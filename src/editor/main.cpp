#include "editor_app.hpp"

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
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    SetProcessDPIAware();

    editor::App& state = editor::app();
    state.scene.create_entity("Cube 0").transform.position = {0.0F, 0.5F, 0.0F};
    state.selected = 1;
    editor::update_camera();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = editor::window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.lpszClassName = L"Starter3DEngineWindow";
    if (!RegisterClassExW(&wc)) return 1;

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Starter 3D Engine",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800,
        nullptr, nullptr, instance, nullptr);
    if (!hwnd) return 1;
    WNDCLASSEXW viewport_class{};
    viewport_class.cbSize = sizeof(viewport_class);
    viewport_class.lpfnWndProc = viewport_proc;
    viewport_class.hInstance = instance;
    viewport_class.lpszClassName = L"StarterVulkanViewport";
    if (!RegisterClassExW(&viewport_class)) { DestroyWindow(hwnd); return 1; }
    state.viewport_window = CreateWindowExW(0, viewport_class.lpszClassName, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, 1, 1,
        hwnd, nullptr, instance, nullptr);
    if (!state.viewport_window) { DestroyWindow(hwnd); return 1; }
    try {
        state.renderer.set_window(instance, state.viewport_window);
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
