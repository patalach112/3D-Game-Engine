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
    App& g = app();
    const auto& target = g.camera.target;
    const float cp = std::cos(g.pitch);
    g.camera.position = {
        target.x + g.camera_distance * cp * std::sin(g.yaw),
        target.y + g.camera_distance * std::sin(g.pitch),
        target.z + g.camera_distance * cp * std::cos(g.yaw)
    };
}

void update_layout() {
    App& g=app();
    g.left_width=std::clamp(g.left_width,180,std::max(180,g.width-g.right_width-240));
    g.right_width=std::clamp(g.right_width,240,std::max(240,g.width-g.left_width-240));
    g.bottom_height=std::clamp(g.bottom_height,100,std::max(100,g.height-320));
    g.viewport_bounds={g.left_width+6,kHeaderHeight,g.width-g.right_width-6,
        g.height-kFooterHeight-g.bottom_height-6};
    g.renderer.resize(g.width,g.height);
}
RECT view_rect() { return app().viewport_bounds; }
} // namespace editor
