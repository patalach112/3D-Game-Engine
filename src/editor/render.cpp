#include "editor_app.hpp"

#include <algorithm>

namespace editor {

void render_frame(HWND hwnd) {
    App& g = app();
    RECT client{};
    GetClientRect(hwnd, &client);
    const int wanted_width = std::max(1L, client.right);
    const int wanted_height = std::max(1L, client.bottom);
    if (wanted_width != g.width || wanted_height != g.height) {
        g.width = wanted_width;
        g.height = wanted_height;
    }

    // Upewnia się, że rozmiar renderera odpowiada aktualnym szerokościom paneli.
    // Przeciąganie separatora zmienia widok, ale nie rozmiar okna.
    update_layout();

    // Jeśli widok się nie zmienił, nie ponawiamy wyciążającej rasteryzacji.
    if (g.scene_dirty) {
        update_camera();
        g.renderer.render(g.scene, g.camera, g.selected);
        g.scene_dirty = false;
    }

    const RECT viewport = view_rect();
    const int view_width = std::max(1, static_cast<int>(viewport.right - viewport.left));
    const int view_height = std::max(1, static_cast<int>(viewport.bottom - viewport.top));

    PAINTSTRUCT ps{};
    HDC window_dc = BeginPaint(hwnd, &ps);

    // Scena trafia prosto na urządzenie. Stary czas przepisywał scenę do bufora
    // pełnowymiarowego, potem StretchDIBits i BitBlt robili po kolejnej kopie.
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = view_width;
    info.bmiHeader.biHeight = -view_height; // Ujemny => pierwszy wiersz to góra.
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(window_dc, viewport.left, viewport.top, view_width, view_height,
                      g.renderer.pixels().data(), &info, DIB_RGB_COLORS, SRCCOPY);

    draw_ui(window_dc);
    EndPaint(hwnd, &ps);
}

} // namespace editor
