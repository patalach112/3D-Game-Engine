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
        state.renderer.set_window(main_window->native_handle().instance,hwnd);
        LOG_INFO("editor", "Creating Vulkan renderer");
        state.renderer.initialize();
        LOG_INFO("editor", "Vulkan renderer initialized: " + state.renderer.device_name());
        state.renderer.enable_ui();
        LOG_INFO("editor", "UI enabled");
        state.ready=true;
        // Refresh hover, caret, keyboard navigation and dragging even in an idle scene.
        SetTimer(hwnd,1,16,nullptr);
        ShowWindow(hwnd,show);
        UpdateWindow(hwnd);
        MSG msg{};
        BOOL result{};
        while ((result=GetMessageW(&msg,nullptr,0,0))>0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        state.ready=false;
        state.renderer.shutdown();
        LOG_INFO("editor", "Editor shutdown");
        return result<0?1:static_cast<int>(msg.wParam);
    } catch (const std::exception& error) {
        state.ready=false;
        state.renderer.shutdown();
        LOG_ERROR("editor", std::string("Vulkan error: ") + error.what());
        MessageBoxA(hwnd,error.what(),"Vulkan initialization error",MB_OK|MB_ICONERROR);
        return 1;
    } finally {
        engine::log_shutdown();
    }
}
