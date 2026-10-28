#include "editor_app.hpp"

#include <string>

namespace editor {

namespace {
constexpr COLORREF kPanelDark = RGB(29, 29, 30);
constexpr COLORREF kBorderDark = RGB(57, 57, 59);
constexpr COLORREF kHandleGrip = RGB(104, 104, 106);
} // namespace

void set_font(HDC dc, bool bold) {
    SelectObject(dc, bold ? app().bold_font : app().font);
    SetBkMode(dc, TRANSPARENT);
}

void fill(HDC dc, RECT rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void line(HDC dc, int x1, int y1, int x2, int y2, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ old = SelectObject(dc, pen);
    MoveToEx(dc, x1, y1, nullptr);
    LineTo(dc, x2, y2);
    SelectObject(dc, old);
    DeleteObject(pen);
}

void draw_text(HDC dc, const std::wstring& value, int x, int y, COLORREF color,
               int max_width, bool bold) {
    set_font(dc, bold);
    SetTextColor(dc, color);
    RECT rect{x, y, max_width > 0 ? x + max_width : app().width, y + 32};
    DrawTextW(dc, value.c_str(), static_cast<int>(value.size()), &rect,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
}

void draw_button(HDC dc, RECT rect, const wchar_t* caption, bool primary) {
    fill(dc, rect, primary ? RGB(51, 51, 53) : RGB(43, 43, 44));
    line(dc, rect.left, rect.top, rect.right, rect.top, RGB(68, 68, 70));
    line(dc, rect.left, rect.bottom - 1, rect.right, rect.bottom - 1, RGB(20, 20, 21));
    draw_text(dc, caption, rect.left + 8, rect.top + 8, RGB(236, 236, 238), rect.right - rect.left - 14);
}

void draw_input(HDC dc, int field) {
    const RECT rect = inspector_field_rect(field);
    const bool active = app().active_field == field;
    fill(dc, rect, active ? RGB(47, 47, 49) : RGB(38, 38, 39));
    const COLORREF border = active ? RGB(230, 112, 43) : RGB(74, 74, 76);
    line(dc, rect.left, rect.top, rect.right, rect.top, border);
    line(dc, rect.left, rect.bottom - 1, rect.right, rect.bottom - 1, border);
    line(dc, rect.left, rect.top, rect.left, rect.bottom, border);
    line(dc, rect.right - 1, rect.top, rect.right - 1, rect.bottom, border);

    static constexpr wchar_t axis_labels[3]{L'X', L'Y', L'Z'};
    std::wstring contents;
    contents.push_back(axis_labels[field % 3]);
    contents += L" ";
    contents += active ? app().field_input : number_text(*inspector_value(field));
    draw_text(dc, contents, rect.left + 7, rect.top + 6,
              active ? RGB(255, 230, 210) : RGB(235, 235, 236), rect.right - rect.left - 12);
}

void draw_entity_icon(HDC dc, const engine::Entity& entity, int x, int y, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    HBRUSH old = reinterpret_cast<HBRUSH>(SelectObject(dc, brush));
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(20, 20, 21));
    HGDIOBJ old_pen = SelectObject(dc, pen);
    switch (entity.primitive) {
    case engine::PrimitiveType::Sphere:
        Ellipse(dc, x, y, x + 12, y + 12);
        break;
    case engine::PrimitiveType::Cylinder:
        Rectangle(dc, x + 1, y + 2, x + 11, y + 11);
        Ellipse(dc, x + 1, y, x + 11, y + 5);
        break;
    case engine::PrimitiveType::Plane:
        Rectangle(dc, x, y + 2, x + 12, y + 10);
        line(dc, x + 4, y + 2, x + 4, y + 10, RGB(38, 38, 39));
        line(dc, x + 8, y + 2, x + 8, y + 10, RGB(38, 38, 39));
        break;
    default:
        Rectangle(dc, x + 1, y + 1, x + 11, y + 11);
        break;
    }
    SelectObject(dc, old_pen);
    SelectObject(dc, old);
    DeleteObject(pen);
    DeleteObject(brush);
}

void draw_ui(HDC dc) {
    App& g = app();
    const int w = g.width, h = g.height;
    const int right_x = w - g.right_width;

    fill(dc, {0, 0, w, kHeaderHeight}, RGB(31, 31, 32));
    fill(dc, {0, kHeaderHeight, g.left_width, h - kFooterHeight}, kPanelDark);
    fill(dc, {right_x, kHeaderHeight, w, h - kFooterHeight}, kPanelDark);
    line(dc, 0, kHeaderHeight - 1, w, kHeaderHeight - 1, kBorderDark);

    // Przeciągalne separatory paneli.
    fill(dc, {g.left_width - 3, kHeaderHeight, g.left_width + 3, h - kFooterHeight}, RGB(34, 34, 35));
    fill(dc, {right_x - 3, kHeaderHeight, right_x + 3, h - kFooterHeight}, RGB(34, 34, 35));
    const int grip_y = (kHeaderHeight + h - kFooterHeight) / 2;
    for (int offset = -12; offset <= 12; offset += 6) {
        fill(dc, {g.left_width - 1, grip_y + offset, g.left_width + 2, grip_y + offset + 2}, kHandleGrip);
        fill(dc, {right_x - 1, grip_y + offset, right_x + 2, grip_y + offset + 2}, kHandleGrip);
    }

    // Nagłówek.
    fill(dc, {14, 10, 40, 36}, RGB(230, 112, 43));
    draw_text(dc, L"S", 21, 13, RGB(255, 255, 255), 20, true);
    draw_text(dc, L"STARTER", 50, 13, RGB(246, 246, 246), 90, true);
    draw_text(dc, L"SCENA", 150, 14, RGB(238, 238, 238), 70);
    line(dc, 148, kHeaderHeight - 2, 198, kHeaderHeight - 2, RGB(230, 112, 43));
    draw_text(dc, L"Własny silnik 3D   •   C++", w - g.right_width - 222, 14, RGB(177, 177, 180), 210);

    // Lewy panel: przyciski i lista obiektów.
    draw_text(dc, L"Hierarchia", 14, 58, RGB(243, 243, 243), g.left_width - 75, true);
    draw_button(dc, {g.left_width - 72, 54, g.left_width - 43, 84}, L"+");
    draw_button(dc, {g.left_width - 37, 54, g.left_width - 10, 84}, L"…");
    draw_button(dc, {10, 96, g.left_width - 10, 132}, L"+ Dodaj kostkę", true);
    draw_button(dc, {10, 137, g.left_width - 10, 173}, L"+ Dodaj kulę", true);
    draw_button(dc, {10, 178, g.left_width - 10, 214}, L"+ Dodaj walec", true);
    draw_button(dc, {10, 219, g.left_width - 10, 255}, L"+ Dodaj płaszczyznę", true);
    draw_button(dc, {10, 260, g.left_width - 10, 298}, L"Zapisz scenę");
    draw_text(dc, L"OBIEKTY SCENY", 14, 310, RGB(165, 165, 168), g.left_width - 24);

    int row_y = 336;
    for (const auto& entity : g.scene.entities()) {
        const bool selected = entity.id == g.selected;
        if (selected) {
            fill(dc, {6, row_y, g.left_width - 6, row_y + 32}, RGB(48, 48, 49));
            fill(dc, {6, row_y, 9, row_y + 32}, RGB(230, 112, 43));
        }
        draw_entity_icon(dc, entity, 16, row_y + 9, selected ? RGB(230, 180, 146) : RGB(155, 155, 158));
        draw_text(dc, std::wstring(entity.name.begin(), entity.name.end()), 36, row_y + 7,
                  selected ? RGB(248, 248, 248) : RGB(190, 190, 193), g.left_width - 52);
        row_y += 34;
    }

    // Prawy panel: inspektor.
    draw_text(dc, L"Inspektor", right_x + 15, 58, RGB(244, 244, 244), g.right_width - 30, true);
    line(dc, right_x + 10, 91, w - 10, 91, kBorderDark);
    if (const auto* selected = g.scene.find_entity(g.selected)) {
        draw_text(dc, std::wstring(selected->name.begin(), selected->name.end()), right_x + 15, 105,
                  RGB(230, 133, 78), g.right_width - 28, true);
        fill(dc, {right_x + 10, 137, w - 10, 166}, RGB(38, 38, 39));
        draw_text(dc, L"TRANSFORMACJA", right_x + 17, 143, RGB(177, 177, 180), g.right_width - 34);
        draw_text(dc, L"Pozycja", right_x + 15, 174, RGB(190, 190, 193), g.right_width - 30);
        draw_input(dc, 0); draw_input(dc, 1); draw_input(dc, 2);
        draw_text(dc, L"Obrót", right_x + 15, 237, RGB(190, 190, 193), g.right_width - 30);
        draw_input(dc, 3); draw_input(dc, 4); draw_input(dc, 5);
        draw_text(dc, L"Skala", right_x + 15, 300, RGB(190, 190, 193), g.right_width - 30);
        draw_input(dc, 6); draw_input(dc, 7); draw_input(dc, 8);
    } else {
        draw_text(dc, L"Brak zaznaczenia", right_x + 15, 108, RGB(185, 185, 188), g.right_width - 28);
        draw_text(dc, L"Kliknij obiekt w widoku sceny.", right_x + 15, 134, RGB(155, 155, 158), g.right_width - 28);
    }

    fill(dc, {right_x + 10, 366, w - 10, 395}, RGB(38, 38, 39));
    draw_text(dc, L"STEROWANIE", right_x + 17, 372, RGB(177, 177, 180), g.right_width - 34);
    draw_text(dc, L"Kliknij pole i wpisz wartość; Enter zatwierdza.", right_x + 15, 407, RGB(190, 190, 193), g.right_width - 28);
    draw_text(dc, L"PPM + przeciąganie: obrót kamery", right_x + 15, 434, RGB(190, 190, 193), g.right_width - 28);
    draw_text(dc, L"Kółko myszy: przybliżenie", right_x + 15, 461, RGB(190, 190, 193), g.right_width - 28);
    draw_text(dc, L"Delete: usuń zaznaczony obiekt", right_x + 15, 488, RGB(190, 190, 193), g.right_width - 28);

    fill(dc, {0, h - kFooterHeight, w, h}, RGB(31, 31, 32));
    line(dc, 0, h - kFooterHeight, w, h - kFooterHeight, kBorderDark);
    draw_text(dc, g.status + L"   |   Obiektów: " + std::to_wstring(g.scene.entity_count()),
              10, h - kFooterHeight + 5, RGB(185, 185, 188), w - 20);
}

} // namespace editor
