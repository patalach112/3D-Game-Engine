#include "editor_app.hpp"
#include "engine/platform.hpp"
#include "engine/log.hpp"
#include <array>
#include <exception>
#include <fstream>

namespace {
struct Logging {
    Logging() { engine::log_init("engine.log"); }
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
            MsgWaitForMultipleObjectsEx(0,nullptr,INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
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
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int show) {
    SetProcessDPIAware();
    Logging logging;
    auto& state=editor::app();
    std::unique_ptr<engine::Window> main_window;
    HWND hwnd{};
    try {
        state.scene.create_entity("Cube 0").transform.position={0,0.5F,0};
        state.selected=1;
        editor::update_camera();
        std::ifstream("project_notes.txt").get(state.notes,sizeof(state.notes),'\0');
        engine::WindowConfig config;
        config.title="Starter 3D | Vulkan Studio";
        config.width=1440; config.height=900; config.visible=false;
        config.native_callback=[](const engine::NativeMessage& msg,std::intptr_t& result,void*) {
            result=editor::window_proc(static_cast<HWND>(msg.window),msg.id,
                static_cast<WPARAM>(msg.wparam),static_cast<LPARAM>(msg.lparam));
            return true;
        };
        main_window=engine::createnewwindow(config);
        hwnd=reinterpret_cast<HWND>(main_window->native_handle().window);
        state.renderer.set_window(main_window->native_handle().instance,hwnd);
        state.renderer.initialize();
        state.renderer.enable_ui();
        state.renderer.set_external_mouse_capture(true);
        auto wake=[hwnd] { PostMessageW(hwnd,editor::kWorkerWake,0,0); };
        state.inputs=std::make_unique<engine::InputProcessor>(wake);
        state.render_worker=std::make_unique<engine::RenderWorker>(wake);
        state.orbit_input.pointerValid=false;
        state.ready=true;
        ShowWindow(hwnd,show);
        UpdateWindow(hwnd);
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
                MsgWaitForMultipleObjectsEx(0,nullptr,INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
                continue;
            }
            state.render_worker->wait(); // propagate GPU-thread exceptions
            drain_input(hwnd);
            if (state.minimized || IsIconic(hwnd)) {
                editor::reset_frame_timing();
                state.orbit_events.clear();
                state.orbit_input.wheel=0; state.orbit_input.rightPressed=false;
                state.orbit.dragging=false;
                MsgWaitForMultipleObjectsEx(0,nullptr,INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
                continue;
            }
            // Continuous rendering: no timer, sleep or fixed FPS target.
            editor::render_frame(hwnd);
        }
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
        LOG_ERROR("editor",std::string("Vulkan/input worker error: ")+error.what());
        MessageBoxA(hwnd,error.what(),"Engine error",MB_OK|MB_ICONERROR);
        return 1;
    }
}
