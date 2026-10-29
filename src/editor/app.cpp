#include "editor_app.hpp"
#include "engine/log.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace editor {

App& app() {
    static App instance;
    return instance;
}

bool frame_due(const App& state, FrameClock::time_point now) noexcept {
    return state.fps_limit <= 0 || state.next_frame.time_since_epoch().count() == 0
        || now >= state.next_frame;
}

void schedule_next_frame(App& state, FrameClock::time_point started,
                         FrameClock::time_point completed) noexcept {
    if (state.fps_limit <= 0) {
        state.next_frame = completed;
        return;
    }
    const int limit = std::clamp(state.fps_limit, 1, 1000);
    const auto interval = std::chrono::duration_cast<FrameClock::duration>(
        std::chrono::duration<double>(1.0 / static_cast<double>(limit)));
    state.next_frame = started + interval;
    // Never queue catch-up frames after a slow render.
    if (state.next_frame < completed) state.next_frame = completed;
}

unsigned long frame_wait_milliseconds(const App& state, FrameClock::time_point now) noexcept {
    if (frame_due(state, now)) return 0;
    const auto remaining = state.next_frame - now;
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(remaining);
    if (std::chrono::duration_cast<FrameClock::duration>(milliseconds) < remaining)
        milliseconds += std::chrono::milliseconds(1);
    return static_cast<unsigned long>(std::max<std::int64_t>(1, milliseconds.count()));
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
    engine::apply_orbit(g.camera,g.orbit);
    g.displayed_orbit=g.orbit;
}

void update_layout() {
    App& g = app();
    // The Vulkan target covers the whole client area, including GPU UI panels.
    g.left_width = std::clamp(g.left_width, 170, std::max(170, g.width - g.right_width - 300));
    g.right_width = std::clamp(g.right_width, 240, std::max(240, g.width - g.left_width - 300));
    g.bottom_height = std::clamp(g.bottom_height, 100,
        std::max(100, g.height - kHeaderHeight - kFooterHeight - 200));
    g.viewport_bounds = view_rect();
    g.renderer.resize(g.width, g.height);
}

RECT view_rect() {
    const App& g = app();
    // Exclude both vertical splitters and the workspace's horizontal splitter.
    return {g.left_width + 6, kHeaderHeight,
            g.width - g.right_width - 6, g.height - kFooterHeight - g.bottom_height - 6};
}

void duplicate_entity() {
    App& g = app();
    LOG_DEBUG("editor", "Duplicating entity");
    auto* source = g.scene.find_entity(g.selected);
    if (!source) return;
    // create_entity may reallocate Scene's vector and invalidate source.
    const auto original = *source;
    auto& copy = g.scene.create_entity(original.name + " copy", original.primitive);
    copy.transform = original.transform;
    // Rzucamy kopię o 2 jednostki w kierunku X, aby była widoczna.
    copy.transform.position.x += 2.0F;
    g.selected = copy.id;
    g.drag_axis = -1;
    g.status = L"Powielono obiekt";
}

void frame_selected() {
    App& g = app();
    auto* entity = g.scene.find_entity(g.selected);
    if (!entity) return;
    // Ustawiamy cel kamery na środek obiektu i przybliżamy.
    const engine::Vec3 center = entity->transform.position;
    g.camera.target = center;
    g.orbit = {};
    g.orbit.distance = 6.0F;
    update_camera();
    g.status = L"Ustawiono widok na obiekt";
}

void snap_camera(int view) {
    App& g = app();
    g.orbit = {};
    // 0=front (patrzy z tyłu), 1=top (patrzy z góry), 2=side (patrzy z prawej), 3=iso
    switch (view) {
    case 0: g.orbit.yaw = 0.0F;  g.orbit.pitch = 0.0F; break;
    case 1: g.orbit.yaw = 0.0F;  g.orbit.pitch = 1.4F; break;
    case 2: g.orbit.yaw = 1.57F; g.orbit.pitch = 0.0F; break;
    default: g.orbit.yaw = 0.75F; g.orbit.pitch = 0.42F; break;
    }
    g.orbit.distance = 8.0F;
    update_camera();
    g.status = L"Ustawiono widok";
}

} // namespace editor
