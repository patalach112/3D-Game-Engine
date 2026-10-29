#include "editor_app.hpp"

#include <algorithm>
#include <exception>

namespace editor {

void render_frame(HWND hwnd) {
    App& g = app();
    PAINTSTRUCT ps{};
    HDC window_dc = BeginPaint(hwnd, &ps);
    try {
        RECT client{};
        GetClientRect(hwnd, &client);
        g.width = std::max(1L, client.right);
        g.height = std::max(1L, client.bottom);
        update_layout();
        const RECT viewport = view_rect();
        if (g.viewport_window && !EqualRect(&g.viewport_bounds, &viewport)) {
            SetWindowPos(g.viewport_window, nullptr, viewport.left, viewport.top,
                std::max(1L, viewport.right - viewport.left),
                std::max(1L, viewport.bottom - viewport.top), SWP_NOZORDER | SWP_NOACTIVATE);
            g.viewport_bounds = viewport;
            g.viewport_dirty = true;
        }
        if (!g.renderer_failed && !IsIconic(hwnd)) {
            if (g.scene_dirty) {
                update_camera();
                g.renderer.render(g.scene, g.camera, g.selected);
                g.scene_dirty = false;
                g.viewport_dirty = true;
            }
            // Pure GPU image -> swapchain -> present; no SetDIBitsToDevice or
            // readback on Windows. Cached image is reused for UI-only repaints.
            if (g.viewport_dirty) {
                // Clear before present so an out-of-date retry can request a
                // new presentation without this assignment discarding it.
                g.viewport_dirty = false;
                g.renderer.present();
            }
        }
        draw_ui(window_dc);
        EndPaint(hwnd, &ps);
    } catch (const std::exception& error) {
        EndPaint(hwnd, &ps);
        g.renderer_failed = true;
        g.renderer.shutdown();
        MessageBoxA(hwnd, error.what(), "Vulkan renderer error", MB_OK | MB_ICONERROR);
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
    }
}

} // namespace editor
