#include "editor_app.hpp"

#include <string>

namespace editor {

namespace {
// Przyciski, pola i ikony tworzą GDI-owe obiekty kilka razy na odwrócenie.
// Tworzymy je po raz pierwszy i pamiętamy, aby każde odwrócenie było tanim.
constexpr int kMaxCachedBrushes = 24;
constexpr int kMaxCachedPens = 12;

struct GdiObjectCache {
    HBRUSH brushes[kMaxCachedBrushes]{};
    COLORREF brush_colors[kMaxCachedBrushes]{};
    int brush_count{0};
    HPEN pens[kMaxCachedPens]{};
    COLORREF pen_colors[kMaxCachedPens]{};
    int pen_count{0};

    HBRUSH brush(COLORREF color) {
        for (int i = 0; i < brush_count; ++i)
            if (brush_colors[i] == color) return brushes[i];
        if (brush_count < kMaxCachedBrushes) {
            HBRUSH created = CreateSolidBrush(color);
            if (created) {
                brushes[brush_count] = created;
                brush_colors[brush_count] = color;
                ++brush_count;
                return created;
            }
        }
        return CreateSolidBrush(color);
    }

    HPEN pen(COLORREF color) {
        for (int i = 0; i < pen_count; ++i)
            if (pen_colors[i] == color) return pens[i];
        if (pen_count < kMaxCachedPens) {
            HPEN created = CreatePen(PS_SOLID, 1, color);
            if (created) {
                pens[pen_count] = created;
                pen_colors[pen_count] = color;
                ++pen_count;
                return created;
            }
        }
        return CreatePen(PS_SOLID, 1, color);
    }

    void release() {
        for (int i = 0; i < brush_count; ++i) DeleteObject(brushes[i]);
        for (int i = 0; i < pen_count; ++i) DeleteObject(pens[i]);
        brush_count = 0;
        pen_count = 0;
    }
};

GdiObjectCache& gdi_objects() {
    static GdiObjectCache cache;
    return cache;
}

constexpr COLORREF kPanel = RGB(29, 29, 30);
constexpr COLORREF kChrome = RGB(31, 31, 32);
constexpr COLORREF kSeparator = RGB(34, 34, 35);
constexpr COLORREF kGrip = RGB(104, 104, 106);
constexpr COLORREF kBorder = RGB(57, 57, 59);
constexpr COLORREF kButton = RGB(43, 43, 44);
constexpr COLORREF kButtonPrimary = RGB(51, 51, 53);
constexpr COLORREF kButtonTop = RGB(68, 68, 70);
constexpr COLORREF kButtonBottom = RGB(20, 20, 21);
constexpr COLORREF kInput = RGB(38, 38, 39);
constexpr COLORREF kInputActive = RGB(47, 47, 49);
constexpr COLORREF kInputBorder = RGB(74, 74, 76);
constexpr COLORREF kInputBorderActive = RGB(230, 112, 43);
constexpr COLORREF kRowSelected = RGB(48, 48, 49);
constexpr COLORREF kAccent = RGB(230, 112, 43);
constexpr COLORREF kIconOutline = RGB(20, 20, 21);
constexpr COLORREF kIconGrid = RGB(38, 38, 39);
} // namespace

void set_font(HDC dc, bool bold) {
    const HFONT wanted = bold ? app().bold_font : app().font;
    const HFONT current = reinterpret_cast<HFONT>(GetCurrentObject(dc, OBJ_FONT));
    if (current != wanted) SelectObject(dc, wanted);
    SetBkMode(dc, TRANSPARENT);
}

void fill(HDC dc, RECT rect, COLORREF color) {
    FillRect(dc, &rect, gdi_objects().brush(color));
}

void line(HDC dc, int x1, int y1, int x2, int y2, COLORREF color) {
    HGDIOBJ old = SelectObject(dc, gdi_objects().pen(color));
    MoveToEx(dc, x1, y1, nullptr);
    LineTo(dc, x2, y2);
    SelectObject(dc, old);
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
    fill(dc, rect, primary ? kButtonPrimary : kButton);
    line(dc, rect.left, rect.top, rect.right, rect.top, kButtonTop);
    line(dc, rect.left, rect.bottom - 1, rect.right, rect.bottom - 1, kButtonBottom);
    draw_text(dc, caption, rect.left + 8, rect.top + 8, RGB(236, 236, 238), rect.right - rect.left - 14);
}

void draw_entity_icon(HDC dc, const engine::Entity& entity, int x, int y, COLORREF color) {
    HGDIOBJ old_brush = SelectObject(dc, gdi_objects().brush(color));
    HGDIOBJ old_pen = SelectObject(dc, gdi_objects().pen(kIconOutline));
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
        line(dc, x + 4, y + 2, x + 4, y + 10, kIconGrid);
        line(dc, x + 8, y + 2, x + 8, y + 10, kIconGrid);
        break;
    default:
        Rectangle(dc, x + 1, y + 1, x + 11, y + 11);
        break;
    }
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
}

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

// Napis paska statusu jen zmienia się po zdarzeniu, więc rysujemy go w każdym kadrze.
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

void release_editor_resources() {
    release_ui_cache();
    gdi_objects().release();
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
