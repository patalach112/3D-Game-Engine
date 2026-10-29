#include "editor_app.hpp"
#include "engine/platform.hpp"

#include <exception>

namespace {
LRESULT CALLBACK viewport_proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    if (message == WM_NCHITTEST) return HTTRANSPARENT; // Parent keeps mouse/picking/gizmo logic.
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_PAINT) {
        PAINTSTRUCT paint{};
        BeginPaint(hwnd, &paint);
        EndPaint(hwnd, &paint);
        editor::app().viewport_dirty = true;
        // Expose/restore must present even when the scene did not change.
        InvalidateRect(GetParent(hwnd), nullptr, FALSE);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wp, lp);
}
}

// Punkt wejścia aplikacji. Reszta logiki widoku dzielona jest pomiędzy moduły
// w katalogu src/editor: app, ui, render, scene_io, interaction i window.
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int show) {
    SetProcessDPIAware();

    editor::App& state = editor::app();
    state.scene.create_entity("Cube 0").transform.position = {0.0F, 0.5F, 0.0F};
    state.selected = 1;
    editor::update_camera();

    std::unique_ptr<engine::Window> main_window, viewport_window;
    HWND hwnd{};
    try {
        engine::WindowConfig config;
        config.title = "Starter 3D Engine";
        config.width = 1280; config.height = 800; config.visible = false;
        config.native_callback = [](const engine::NativeMessage& message, std::intptr_t& result, void*) {
            result = editor::window_proc(static_cast<HWND>(message.window), message.id,
                static_cast<WPARAM>(message.wparam), static_cast<LPARAM>(message.lparam));
            return true;
        };
        main_window = engine::createnewwindow(config);
        hwnd = reinterpret_cast<HWND>(main_window->native_handle().window);
        config.title.clear(); config.width = config.height = 1;
        config.child = true; config.parent = main_window->native_handle().window;
        config.visible = true;
        config.native_callback = [](const engine::NativeMessage& message, std::intptr_t& result, void*) {
            result = viewport_proc(static_cast<HWND>(message.window), message.id,
                static_cast<WPARAM>(message.wparam), static_cast<LPARAM>(message.lparam));
            return true;
        };
        viewport_window = engine::createnewwindow(config);
        state.viewport_window = reinterpret_cast<HWND>(viewport_window->native_handle().window);
    } catch (const std::exception& error) {
        MessageBoxA(hwnd, error.what(), "Window initialization error", MB_OK | MB_ICONERROR);
        return 1;
    }
    try {
        state.renderer.set_window(viewport_window->native_handle().instance, state.viewport_window);
        state.renderer.initialize();
        editor::update_layout();
    } catch (const std::exception& error) {
        MessageBoxA(hwnd, error.what(), "Vulkan initialization error", MB_OK | MB_ICONERROR);
        state.renderer.shutdown();
        DestroyWindow(hwnd);
        editor::release_editor_resources();
        return 1;
    }
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    state.renderer.shutdown();
    editor::release_editor_resources();
    if (state.font) DeleteObject(state.font);
    if (state.bold_font) DeleteObject(state.bold_font);
    return static_cast<int>(msg.wParam);
}
