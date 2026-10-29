#include "editor_app.hpp"
#include "engine/platform.hpp"
#include "engine/log.hpp"
#include <exception>
#include <fstream>

int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int show) {
    SetProcessDPIAware();
    engine::log_init("engine.log");
    LOG_INFO("editor", "Starter 3D Editor start");
    auto& state=editor::app();
    (void)editor::load_graphics_settings();
    state.scene.create_entity("Cube 0").transform.position={0,0.5F,0};
    state.selected=1;
    editor::update_camera();
    std::ifstream("project_notes.txt").get(state.notes,sizeof(state.notes),'\0');
    std::unique_ptr<engine::Window> main_window;
    HWND hwnd{};
    try {
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
        state.renderer.set_vsync(state.vsync);
        state.renderer.set_window(main_window->native_handle().instance,hwnd);
        LOG_INFO("editor", "Creating Vulkan renderer");
        state.renderer.initialize();
        LOG_INFO("editor", "Vulkan renderer initialized: %s", state.renderer.device_name().c_str());
        state.renderer.enable_ui();
        LOG_INFO("editor", "UI enabled");
        state.ready=true;
        ShowWindow(hwnd,show);
        UpdateWindow(hwnd);

        // Drive painting from a deadline-aware message loop. A Win32 SetTimer is
        // clamped by the system timer resolution and made "unlimited" behave like
        // ~60 FPS; this loop can genuinely run uncapped while still sleeping when
        // a finite limit is selected.
        MSG msg{};
        bool running=true;
        int exit_code=0;
        while (running) {
            while (PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) {
                if (msg.message==WM_QUIT) {
                    running=false;
                    exit_code=static_cast<int>(msg.wParam);
                    break;
                }
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            if (!running) break;

            if (state.ready && !state.renderer_failed && !IsIconic(hwnd)) {
                const auto now=editor::FrameClock::now();
                if (editor::frame_due(state,now)) {
                    InvalidateRect(hwnd,nullptr,FALSE);
                    UpdateWindow(hwnd);
                    continue;
                }
                const DWORD wait=editor::frame_wait_milliseconds(state,now);
                MsgWaitForMultipleObjectsEx(0,nullptr,wait,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            } else {
                MsgWaitForMultipleObjectsEx(0,nullptr,INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            }
        }
        state.ready=false;
        if (state.settings_dirty && !editor::save_graphics_settings())
            LOG_WARN("editor", "Could not save graphics settings");
        state.renderer.shutdown();
        LOG_INFO("editor", "Editor shutdown");
        engine::log_shutdown();
        return exit_code;
    } catch (const std::exception& error) {
        state.ready=false;
        if (state.settings_dirty) (void)editor::save_graphics_settings();
        state.renderer.shutdown();
        LOG_ERROR("editor", "Vulkan error: %s", error.what());
        MessageBoxA(hwnd,error.what(),"Vulkan initialization error",MB_OK|MB_ICONERROR);
        engine::log_shutdown();
        return 1;
    }
}
