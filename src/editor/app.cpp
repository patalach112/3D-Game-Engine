#include "editor_app.hpp"

#include <algorithm>
#include <cmath>
#include <cwchar>

namespace editor {

App& app() {
    static App instance;
    return instance;
}

engine::Vec3 add(engine::Vec3 a, engine::Vec3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

engine::Vec3 sub(engine::Vec3 a, engine::Vec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

engine::Vec3 mul(engine::Vec3 a, float n) {
    return {a.x * n, a.y * n, a.z * n};
}

float dot(engine::Vec3 a, engine::Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

engine::Vec3 cross(engine::Vec3 a, engine::Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

float magnitude(engine::Vec3 a) {
    return std::sqrt(dot(a, a));
}

engine::Vec3 normalized(engine::Vec3 a) {
    const float len = magnitude(a);
    return len < 0.00001F ? engine::Vec3{0, 0, 1} : mul(a, 1.0F / len);
}

void update_camera() {
    const auto& target = app().camera.target;
    const float cp = std::cos(app().pitch);
    app().camera.position = {
        target.x + app().camera_distance * cp * std::sin(app().yaw),
        target.y + app().camera_distance * std::sin(app().pitch),
        target.z + app().camera_distance * cp * std::cos(app().yaw)
    };
}

void update_layout() {
    App& g = app();
    if (!g.layout_initialized) {
        g.left_width = std::clamp(g.width / 7, 180, 245);
        g.right_width = std::clamp(g.width / 4, 280, 360);
        g.layout_initialized = true;
    }
    const int max_left = std::max(170, g.width - g.right_width - 300);
    g.left_width = std::clamp(g.left_width, 170, max_left);
    const int max_right = std::max(240, g.width - g.left_width - 300);
    g.right_width = std::clamp(g.right_width, 240, max_right);

    const int view_width = std::max(1, g.width - g.left_width - g.right_width);
    const int view_height = std::max(1, g.height - kHeaderHeight - kFooterHeight);
    g.renderer.resize(view_width, view_height);

    const int wanted_font_height = std::clamp(g.height / 55, 15, 17);
    if (wanted_font_height != g.font_height) {
        if (g.font) DeleteObject(g.font);
        if (g.bold_font) DeleteObject(g.bold_font);
        g.font_height = wanted_font_height;
        g.font = CreateFontW(-wanted_font_height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g.bold_font = CreateFontW(-wanted_font_height, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    }
}

RECT view_rect() {
    const App& g = app();
    return {g.left_width, kHeaderHeight,
            g.width - g.right_width, g.height - kFooterHeight};
}

RECT inspector_field_rect(int field) {
    const App& g = app();
    const int right_x = g.width - g.right_width;
    const int gap = 6;
    const int col_width = (g.right_width - 30 - 2 * gap) / 3;
    const int group = field / 3;
    const int axis = field % 3;
    const int top = 194 + group * 63;
    const int left = right_x + 15 + axis * (col_width + gap);
    return {left, top, left + col_width, top + 29};
}

float* inspector_value(int field) {
    App& g = app();
    auto* entity = g.scene.find_entity(g.selected);
    if (!entity || field < 0 || field >= 9) return nullptr;
    engine::Vec3* groups[3]{&entity->transform.position, &entity->transform.rotation, &entity->transform.scale};
    engine::Vec3& group = *groups[field / 3];
    switch (field % 3) {
    case 0: return &group.x;
    case 1: return &group.y;
    default: return &group.z;
    }
}

int field_at(int x, int y) {
    if (!app().scene.find_entity(app().selected)) return -1;
    for (int field = 0; field < 9; ++field) {
        const RECT rect = inspector_field_rect(field);
        if (x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom) return field;
    }
    return -1;
}

std::wstring number_text(float value, int precision) {
    wchar_t buffer[32]{};
    std::swprintf(buffer, 32, L"%.*f", precision, static_cast<double>(value));
    return buffer;
}

void begin_field_edit(int field) {
    float* value = inspector_value(field);
    if (!value) return;
    App& g = app();
    g.active_field = field;
    g.field_input = number_text(*value);
    g.replace_field_on_char = true;
}

void commit_field_edit() {
    App& g = app();
    if (g.active_field < 0) return;
    float* value = inspector_value(g.active_field);
    wchar_t* end = nullptr;
    const float parsed = std::wcstof(g.field_input.c_str(), &end);
    if (value && end != g.field_input.c_str() && *end == L'\0' && std::isfinite(parsed)) {
        *value = parsed;
        g.status = L"Zmieniono transformację";
    } else if (!g.field_input.empty()) {
        g.status = L"Niepoprawna liczba — zmiana odrzucona";
    }
    g.active_field = -1;
    g.field_input.clear();
}

} // namespace editor
