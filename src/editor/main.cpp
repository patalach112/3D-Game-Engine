#include "editor_app.hpp"
#include "hud_editor.hpp"
#include "startup.hpp"
#include "engine/platform.hpp"
#include "engine/log.hpp"
#include <array>
#include <exception>
#include <fstream>
#include <sstream>

namespace {
struct Logging {
    explicit Logging(bool designer) { engine::log_init(designer?"editor_hud.log":"engine_editor.log"); }
    ~Logging() { engine::log_shutdown(); }
};
void finish_render() {
    auto& state=editor::app();
    if (!state.render_worker) return;
    while (state.render_worker->busy()) {
        // WSI/driver operations can synchronously message the HWND owner. Never
        // block that thread in join()/wait() while a GPU job still needs it.
        MSG message{};
        for (unsigned count=0;count<256 && PeekMessageW(&message,nullptr,0,0,PM_REMOVE);++count) {
            if (message.message!=WM_QUIT) { TranslateMessage(&message); DispatchMessageW(&message); }
        }
        if (state.render_worker->busy())
            MsgWaitForMultipleObjectsEx(0,nullptr,100,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
    }
    state.render_worker->wait();
}
void drain_input(HWND hwnd) {
    auto& state=editor::app();
    // Replay the exact keyboard state from message receipt. Otherwise a queued
    // Ctrl-down/key/Ctrl-up burst could lose its modifiers before replay.
    std::array<BYTE,256> current{};
    GetKeyboardState(current.data());
    struct Restore {
        std::array<BYTE,256>& keys;
        LPARAM extra;
        ~Restore() { SetKeyboardState(keys.data()); SetMessageExtraInfo(extra); }
    } restore{current,GetMessageExtraInfo()};
    for (auto& event:state.inputs->drain()) {
        if (event.kind!=engine::InputKind::Reset) {
            SetKeyboardState(event.keyboard.data());
            SetMessageExtraInfo(static_cast<LPARAM>(event.extra_info)); // preserve pen/touch source tags
        }
        editor::apply_input_event(hwnd,event);
    }
}
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR command_line,int show) {
    SetProcessDPIAware();
#ifdef EDITOR_HUD_DESIGNER
    constexpr bool designer=true;
#else
    constexpr bool designer=false;
#endif
    editor::configure_startup_diagnostics(designer);
    Logging logging(designer);
    LOG_INFO("startup","Starting %s, build %s %s, %u-bit",designer?"editor_hud.exe":"engine_editor.exe",__DATE__,__TIME__,static_cast<unsigned>(sizeof(void*)*8));
    bool smoke=false,safe=false,save_test_layout=false;
    std::wistringstream args(command_line?command_line:L"");std::wstring arg;
    while(args>>arg) {if(arg==L"--smoke-test")smoke=true;if(arg==L"--safe-mode")safe=true;if(arg==L"--save-test-layout")save_test_layout=true;}
    auto& state=editor::app();state.hud_designer=designer;state.safe_mode=safe;
    std::unique_ptr<engine::Window> main_window;
    HWND hwnd{};
    try {
        LOG_INFO("startup","Loading HUD and bindings (safe mode=%d)",state.safe_mode);
        editor::initialize_hud();
        state.scene.create_entity("Cube 0").transform.position={0,0.5F,0};
        state.selected=1;
        editor::update_camera();
        std::ifstream("project_notes.txt").get(state.notes,sizeof(state.notes),'\0');
        engine::WindowConfig config;
        config.title=state.hud_designer?"HUD Designer | Vulkan Studio":"Starter 3D | Vulkan Studio";
        config.width=1440; config.height=900; config.visible=false;
        config.borderless=true;
        config.native_callback=[](const engine::NativeMessage& msg,std::intptr_t& result,void*) {
            result=editor::window_proc(static_cast<HWND>(msg.window),msg.id,
                static_cast<WPARAM>(msg.wparam),static_cast<LPARAM>(msg.lparam));
            return true;
        };
        LOG_INFO("startup","Creating native window");
        main_window=engine::createnewwindow(config);
        hwnd=reinterpret_cast<HWND>(main_window->native_handle().window);
        state.hwnd=hwnd;
        state.renderer.set_window(main_window->native_handle().instance,hwnd);
        LOG_INFO("startup","Initializing Vulkan");
        state.renderer.initialize();
        state.renderer.enable_ui();
        auto& io=ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigWindowsMoveFromTitleBarOnly=true;
        io.IniFilename=nullptr;
        if (!state.hud.docking.empty()) ImGui::LoadIniSettingsFromMemory(state.hud.docking.c_str());
        // One Vulkan target and one context: do not enable platform viewports.
        LOG_INFO("startup", "Vulkan device/UI initialized: %s",state.renderer.device_name().c_str());
        state.renderer.set_external_mouse_capture(true);
        auto wake=[hwnd] { PostMessageW(hwnd,editor::kWorkerWake,0,0); };
        LOG_INFO("startup","Starting input/render workers");
        state.inputs=std::make_unique<engine::InputProcessor>(wake);
        state.render_worker=std::make_unique<engine::RenderWorker>(wake);
        state.orbit_input.pointerValid=false;
        state.ready=true;
        editor::show_editor_window(hwnd,smoke?SW_HIDE:show);
        bool first_frame_complete=false;

        // Render continuously by default. The graphics settings may explicitly
        // enable a finite limit; native messages/input remain responsive in either mode.
        MSG msg{};
        bool quit=false;
        while (!quit && !state.close_requested) {
            // Bound message draining so mouse floods cannot starve rendering.
            for (unsigned count=0;count<256 && PeekMessageW(&msg,nullptr,0,0,PM_REMOVE);++count) {
                if (msg.message==WM_QUIT) { quit=true; break; }
                TranslateMessage(&msg); DispatchMessageW(&msg);
            }
            if (quit || state.close_requested) break;
            if (state.render_worker->busy()) {
                // Keep pumping native messages while Vulkan records/waits/submits.
                // Completion posts kWorkerWake AFTER publishing worker-idle.
                MsgWaitForMultipleObjectsEx(0,nullptr,100,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
                continue;
            }
            state.render_worker->wait(); // propagate GPU-thread exceptions
            if(state.submitted_frames && !first_frame_complete) {
                first_frame_complete=true;LOG_INFO("startup","First frame completed; editor running");
            }
            if(smoke && state.submitted_frames>=3) {
                if(save_test_layout && designer) {
                    if(std::filesystem::exists(state.hud_path))throw std::runtime_error("Smoke test refuses to overwrite an existing HUD");
                    if(!editor::save_hud())throw std::runtime_error("Smoke test could not save its HUD");
                }
                LOG_INFO("startup","Vulkan window smoke test passed");break;
            }
            drain_input(hwnd);
            if (state.minimized || IsIconic(hwnd)) {
                editor::reset_frame_timing();
                state.orbit_events.clear();
                state.orbit_input.wheel=0; state.orbit_input.rightPressed=false;
                state.orbit.dragging=false;
                MsgWaitForMultipleObjectsEx(0,nullptr,100,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
                continue;
            }
            const auto started=editor::FrameClock::now();
            if (!editor::frame_due(state,started)) {
                MsgWaitForMultipleObjectsEx(0,nullptr,editor::frame_wait_milliseconds(state,started),
                    QS_ALLINPUT,MWMO_INPUTAVAILABLE);
                continue;
            }
            // No timer or default FPS cap; only an explicitly selected limit waits.
            editor::render_frame(hwnd);
            editor::schedule_next_frame(state,started,editor::FrameClock::now());
        }
        LOG_INFO("startup","Closing normally: frames=%llu close_requested=%d quit=%d",static_cast<unsigned long long>(state.submitted_frames),state.close_requested,quit);
        state.ready=false;
        finish_render();
        state.render_worker.reset();
        state.inputs.reset();
        state.renderer.shutdown(); // surface must die before its HWND
        main_window.reset();
        return 0;
    } catch (const std::exception& error) {
        state.ready=false;
        try { finish_render(); } catch (...) {} // preserve the original error
        state.render_worker.reset(); // join before touching renderer or destroying HWND
        state.inputs.reset();
        state.renderer.shutdown();
        LOG_ERROR("editor","Vulkan/input worker error: %s",error.what());
        if(!smoke)MessageBoxA(hwnd,error.what(),"Engine error",MB_OK|MB_ICONERROR);
        return 1;
    }
}
