#include "editor_app.hpp"
#include "editor_ui_colors.hpp"

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

void release_editor_resources() {
    release_ui_cache();
    gdi_objects().release();
}

} // namespace editor
