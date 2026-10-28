#include "editor_app.hpp"
#include "editor_ui_colors.hpp"

#include <string>

namespace editor {

// Wartości pola inspektora: ktoś może je zmienić w każdym odwróceniu.
void draw_input_text(HDC dc, int field) {
    const RECT rect = inspector_field_rect(field);
    const bool active = app().active_field == field;
    static constexpr wchar_t axis_labels[3]{L'X', L'Y', L'Z'};
    std::wstring contents;
    contents.push_back(axis_labels[field % 3]);
    contents += L" ";
    contents += active ? app().field_input : number_text(*inspector_value(field));
    draw_text(dc, contents, rect.left + 7, rect.top + 6,
              active ? RGB(255, 230, 210) : RGB(235, 235, 236), rect.right - rect.left - 12);
}

void draw_input(HDC dc, int field) {
    const RECT rect = inspector_field_rect(field);
    const bool active = app().active_field == field;
    fill(dc, rect, active ? kInputActive : kInput);
    const COLORREF border = active ? kInputBorderActive : kInputBorder;
    line(dc, rect.left, rect.top, rect.right, rect.top, border);
    line(dc, rect.left, rect.bottom - 1, rect.right, rect.bottom - 1, border);
    line(dc, rect.left, rect.top, rect.left, rect.bottom, border);
    line(dc, rect.right - 1, rect.top, rect.right - 1, rect.bottom, border);
    draw_input_text(dc, field);
}

// Część interfejsu zależna tylko od rozmiaru okna i paneli.
void draw_chrome(HDC dc) {
    App& g = app();
    const int w = g.width, h = g.height;
    const int right_x = w - g.right_width;

    fill(dc, {0, 0, w, kHeaderHeight}, kChrome);
    fill(dc, {0, kHeaderHeight, g.left_width, h - kFooterHeight}, kPanel);
    fill(dc, {right_x, kHeaderHeight, w, h - kFooterHeight}, kPanel);
    line(dc, 0, kHeaderHeight - 1, w, kHeaderHeight - 1, kBorder);

    // Przeciągalne separatory paneli.
    fill(dc, {g.left_width - 3, kHeaderHeight, g.left_width + 3, h - kFooterHeight}, kSeparator);
    fill(dc, {right_x - 3, kHeaderHeight, right_x + 3, h - kFooterHeight}, kSeparator);
    const int grip_y = (kHeaderHeight + h - kFooterHeight) / 2;
    for (int offset = -12; offset <= 12; offset += 6) {
        fill(dc, {g.left_width - 1, grip_y + offset, g.left_width + 2, grip_y + offset + 2}, kGrip);
        fill(dc, {right_x - 1, grip_y + offset, right_x + 2, grip_y + offset + 2}, kGrip);
    }

    // Nagłówek.
    fill(dc, {14, 10, 40, 36}, kAccent);
    draw_text(dc, L"S", 21, 13, RGB(255, 255, 255), 20, true);
    draw_text(dc, L"STARTER", 50, 13, RGB(246, 246, 246), 90, true);
    draw_text(dc, L"SCENA", 150, 14, RGB(238, 238, 238), 70);
    line(dc, 148, kHeaderHeight - 2, 198, kHeaderHeight - 2, kAccent);
    draw_text(dc, L"Własny silnik 3D   •   C++", w - g.right_width - 222, 14, RGB(177, 177, 180), 210);

    // Lewy panel: tytuły i przyciski.
    draw_text(dc, L"Hierarchia", 14, 58, RGB(243, 243, 243), g.left_width - 75, true);
    draw_button(dc, {g.left_width - 72, 54, g.left_width - 43, 84}, L"+");
    draw_button(dc, {g.left_width - 37, 54, g.left_width - 10, 84}, L"…");
    draw_button(dc, {10, 96, g.left_width - 10, 132}, L"+ Dodaj kostkę", true);
    draw_button(dc, {10, 137, g.left_width - 10, 173}, L"+ Dodaj kulę", true);
    draw_button(dc, {10, 178, g.left_width - 10, 214}, L"+ Dodaj walec", true);
    draw_button(dc, {10, 219, g.left_width - 10, 255}, L"+ Dodaj płaszczyznę", true);
    draw_button(dc, {10, 260, g.left_width - 10, 298}, L"Zapisz scenę");
    draw_text(dc, L"OBIEKTY SCENY", 14, 310, RGB(165, 165, 168), g.left_width - 24);

    // Prawy panel: tytuły i opisy.
    draw_text(dc, L"Inspektor", right_x + 15, 58, RGB(244, 244, 244), g.right_width - 30, true);
    line(dc, right_x + 10, 91, w - 10, 91, kBorder);
    fill(dc, {right_x + 10, 137, w - 10, 166}, kInput);
    draw_text(dc, L"TRANSFORMACJA", right_x + 17, 143, RGB(177, 177, 180), g.right_width - 34);
    draw_text(dc, L"Pozycja", right_x + 15, 174, RGB(190, 190, 193), g.right_width - 30);
    draw_text(dc, L"Obrót", right_x + 15, 237, RGB(190, 190, 193), g.right_width - 30);
    draw_text(dc, L"Skala", right_x + 15, 300, RGB(190, 190, 193), g.right_width - 30);

    fill(dc, {right_x + 10, 366, w - 10, 395}, kInput);
    draw_text(dc, L"STEROWANIE", right_x + 17, 372, RGB(177, 177, 180), g.right_width - 34);
    draw_text(dc, L"Kliknij pole i wpisz wartość; Enter zatwierdza.", right_x + 15, 407, RGB(190, 190, 193), g.right_width - 28);
    draw_text(dc, L"PPM + przeciąganie: obrót kamery", right_x + 15, 434, RGB(190, 190, 193), g.right_width - 28);
    draw_text(dc, L"Kółko myszy: przybliżenie", right_x + 15, 461, RGB(190, 190, 193), g.right_width - 28);
    draw_text(dc, L"Delete: usuń zaznaczony obiekt", right_x + 15, 488, RGB(190, 190, 193), g.right_width - 28);

    fill(dc, {0, h - kFooterHeight, w, h}, kChrome);
    line(dc, 0, h - kFooterHeight, w, h - kFooterHeight, kBorder);
}

// Część zależna od sceny, zaznaczenia i edycji pól.
void draw_dynamic(HDC dc) {
    App& g = app();
    const int right_x = g.width - g.right_width;

    int row_y = 336;
    for (const auto& entity : g.scene.entities()) {
        const bool selected = entity.id == g.selected;
        if (selected) {
            fill(dc, {6, row_y, g.left_width - 6, row_y + 32}, kRowSelected);
            fill(dc, {6, row_y, 9, row_y + 32}, kAccent);
        }
        draw_entity_icon(dc, entity, 16, row_y + 9, selected ? RGB(230, 180, 146) : RGB(155, 155, 158));
        draw_text(dc, std::wstring(entity.name.begin(), entity.name.end()), 36, row_y + 7,
                  selected ? RGB(248, 248, 248) : RGB(190, 190, 193), g.left_width - 52);
        row_y += 34;
    }

    if (const auto* selected = g.scene.find_entity(g.selected)) {
        draw_text(dc, std::wstring(selected->name.begin(), selected->name.end()), right_x + 15, 105,
                  RGB(230, 133, 78), g.right_width - 28, true);
        draw_input(dc, 0); draw_input(dc, 1); draw_input(dc, 2);
        draw_input(dc, 3); draw_input(dc, 4); draw_input(dc, 5);
        draw_input(dc, 6); draw_input(dc, 7); draw_input(dc, 8);
    } else {
        draw_text(dc, L"Brak zaznaczenia", right_x + 15, 108, RGB(185, 185, 188), g.right_width - 28);
        draw_text(dc, L"Kliknij obiekt w widoku sceny.", right_x + 15, 134, RGB(155, 155, 158), g.right_width - 28);
    }
}

// Napis paska statusu jest zmienia po zdarzeniu, więc rysujemy go w każdym kadrze.
void draw_status(HDC dc) {
    const App& g = app();
    draw_text(dc, g.status + L"   |   Obiektów: " + std::to_wstring(g.scene.entity_count()),
              10, g.height - kFooterHeight + 5, RGB(185, 185, 188), g.width - 20);
}

std::wstring ui_signature() {
    const App& g = app();
    std::wstring signature;
    signature.reserve(128);
    signature += std::to_wstring(g.width); signature += L'|';
    signature += std::to_wstring(g.height); signature += L'|';
    signature += std::to_wstring(g.left_width); signature += L'|';
    signature += std::to_wstring(g.right_width); signature += L'|';
    signature += std::to_wstring(g.font_height); signature += L'|';
    signature += std::to_wstring(g.scene.entity_count()); signature += L'|';
    signature += std::to_wstring(g.selected); signature += L'|';
    signature += std::to_wstring(g.active_field); signature += L'|';
    signature += g.field_input; signature += L'|';
    for (const auto& entity : g.scene.entities()) {
        // Nazwa jest kodowana tak samo jak w draw_dynamic, więc sygnatura
        // odzwierciedla dokładnie to, co jest wyświetlone.
        signature += std::wstring(entity.name.begin(), entity.name.end());
        signature += std::to_wstring(static_cast<int>(entity.primitive));
        signature += L',';
    }
    return signature;
}

void ensure_ui_cache(HDC source_dc) {
    App& g = app();
    if (!source_dc) return;

    if (g.ui_width != g.width || g.ui_height != g.height) {
        if (g.ui_dc) {
            if (g.ui_previous) {
                SelectObject(g.ui_dc, g.ui_previous);
                g.ui_previous = nullptr;
            }
            if (g.ui_bitmap) {
                DeleteObject(g.ui_bitmap);
                g.ui_bitmap = nullptr;
            }
            DeleteDC(g.ui_dc);
            g.ui_dc = nullptr;
        }
        g.ui_width = g.width;
        g.ui_height = g.height;
        g.ui_bitmap = CreateCompatibleBitmap(source_dc, g.ui_width, g.ui_height);
        g.ui_dc = CreateCompatibleDC(source_dc);
        g.ui_previous = static_cast<HBITMAP>(SelectObject(g.ui_dc, g.ui_bitmap));
        g.ui_signature.clear();
    }

    const std::wstring signature = ui_signature();
    if (!g.ui_dc || g.ui_signature == signature) return;
    g.ui_signature = signature;
    draw_chrome(g.ui_dc);
    draw_dynamic(g.ui_dc);
}

void release_ui_cache() {
    App& g = app();
    if (g.ui_dc) {
        if (g.ui_previous) SelectObject(g.ui_dc, g.ui_previous);
        if (g.ui_bitmap) DeleteObject(g.ui_bitmap);
        DeleteDC(g.ui_dc);
        g.ui_dc = nullptr;
        g.ui_previous = nullptr;
        g.ui_bitmap = nullptr;
    }
    g.ui_signature.clear();
}

void draw_ui(HDC dc) {
    App& g = app();
    ensure_ui_cache(dc);

    if (!g.ui_dc) {
        // Bez zapasowego bufora: rysujemy cały interfejs bezpośrednio.
        draw_chrome(dc);
        draw_dynamic(dc);
        draw_status(dc);
        return;
    }

    // Kopiujemy dokładnie panele i paski, ale nie nad widok sceny.
    const int panel_top = kHeaderHeight;
    const int panel_height = g.height - kFooterHeight - kHeaderHeight;
    if (panel_height > 0) {
        BitBlt(dc, 0, panel_top, g.left_width, panel_height,
               g.ui_dc, 0, panel_top, SRCCOPY);
        BitBlt(dc, g.width - g.right_width, panel_top, g.right_width, panel_height,
               g.ui_dc, g.width - g.right_width, panel_top, SRCCOPY);
    }
    BitBlt(dc, 0, 0, g.width, kHeaderHeight, g.ui_dc, 0, 0, SRCCOPY);
    BitBlt(dc, 0, g.height - kFooterHeight, g.width, kFooterHeight,
           g.ui_dc, 0, g.height - kFooterHeight, SRCCOPY);
    draw_status(dc);
}

} // namespace editor
