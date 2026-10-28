#include "editor_app.hpp"

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
    editor::update_layout();
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    editor::release_editor_resources();
    if (state.font) DeleteObject(state.font);
    if (state.bold_font) DeleteObject(state.bold_font);
    return static_cast<int>(msg.wParam);
}
