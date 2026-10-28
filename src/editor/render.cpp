#include "editor_app.hpp"

#include <algorithm>
#include <cstddef>

namespace editor {

void ensure_backbuffer(HDC window_dc) {
    App& g = app();
    if (g.backbuffer_width == g.width && g.backbuffer_height == g.height) return;

    if (g.backbuffer_dc) {
        if (g.backbuffer_previous) {
            SelectObject(g.backbuffer_dc, g.backbuffer_previous);
            g.backbuffer_previous = nullptr;
        }
        if (g.backbuffer_bitmap) {
            DeleteObject(g.backbuffer_bitmap);
            g.backbuffer_bitmap = nullptr;
        }
        DeleteDC(g.backbuffer_dc);
        g.backbuffer_dc = nullptr;
    }

    g.backbuffer_width = g.width;
    g.backbuffer_height = g.height;
    g.backbuffer_bitmap = CreateCompatibleBitmap(window_dc, g.backbuffer_width, g.backbuffer_height);
    g.backbuffer_dc = CreateCompatibleDC(window_dc);
    g.backbuffer_previous = static_cast<HBITMAP>(SelectObject(g.backbuffer_dc, g.backbuffer_bitmap));
}

void render_frame(HWND hwnd) {
    App& g = app();
    RECT client{};
    GetClientRect(hwnd, &client);
    g.width = std::max(1L, client.right);
    g.height = std::max(1L, client.bottom);
    update_layout();
    update_camera();
    g.renderer.render(g.scene, g.camera, g.selected);

    // Zachować widok sceny w rozmiarach okna i wypełnić tłem dla paneli.
    const RECT viewport = view_rect();
    g.frame.assign(static_cast<std::size_t>(g.width) * static_cast<std::size_t>(g.height), 0x00272729U);
    const auto& view = g.renderer.pixels();
    const int vw = viewport.right - viewport.left;
    const int vh = viewport.bottom - viewport.top;
    for (int y = 0; y < vh; ++y) {
        const auto src = view.begin() + static_cast<std::ptrdiff_t>(y) * vw;
        auto dst = g.frame.begin() + static_cast<std::ptrdiff_t>(y + viewport.top) * g.width + viewport.left;
        std::copy_n(src, vw, dst);
    }

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = g.width;
    info.bmiHeader.biHeight = -g.height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    PAINTSTRUCT ps{};
    HDC window_dc = BeginPaint(hwnd, &ps);
    ensure_backbuffer(window_dc);
    StretchDIBits(g.backbuffer_dc, 0, 0, g.width, g.height, 0, 0, g.width, g.height,
                  g.frame.data(), &info, DIB_RGB_COLORS, SRCCOPY);
    draw_ui(g.backbuffer_dc);
    BitBlt(window_dc, 0, 0, g.width, g.height, g.backbuffer_dc, 0, 0, SRCCOPY);
    EndPaint(hwnd, &ps);
}

} // namespace editor
