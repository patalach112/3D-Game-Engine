#include "editor_app.hpp"
#include "engine/log.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>

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

void reset_graphics_settings() {
    auto& g=app();
    g.fps_limit=60;
    g.grid_visible=true;
    g.vsync=true;
    g.camera.vertical_fov_degrees=45.0F;
    g.background_color={39.0F/255.0F,39.0F/255.0F,41.0F/255.0F};
    g.next_frame={};
    g.settings_dirty=true;
}

bool load_graphics_settings() {
    std::ifstream file("editor_settings.ini");
    if (!file) return false;

    int version{}, fps_limit=60, grid=1, vsync=1;
    float fov=45.0F;
    engine::Vec3 background{39.0F/255.0F,39.0F/255.0F,41.0F/255.0F};
    unsigned int seen{};
    std::string key;
    while (file>>key) {
        if (key=="version") { if (!(file>>version)) return false; seen|=1U; }
        else if (key=="fps_limit") { if (!(file>>fps_limit)) return false; seen|=2U; }
        else if (key=="grid_visible") { if (!(file>>grid)) return false; seen|=4U; }
        else if (key=="vsync") { if (!(file>>vsync)) return false; seen|=8U; }
        else if (key=="fov") { if (!(file>>fov)) return false; seen|=16U; }
        else if (key=="background") {
            if (!(file>>background.x>>background.y>>background.z)) return false;
            seen|=32U;
        } else {
            std::string ignored;
            std::getline(file,ignored);
        }
    }
    const bool valid=seen==63U && version==1 && (fps_limit==0 || (fps_limit>=15 && fps_limit<=360))
        && (grid==0 || grid==1) && (vsync==0 || vsync==1)
        && std::isfinite(fov) && fov>=30.0F && fov<=100.0F
        && std::isfinite(background.x) && std::isfinite(background.y) && std::isfinite(background.z)
        && background.x>=0.0F && background.x<=1.0F && background.y>=0.0F && background.y<=1.0F
        && background.z>=0.0F && background.z<=1.0F;
    if (!valid) return false;

    auto& g=app();
    g.fps_limit=fps_limit;
    g.grid_visible=grid!=0;
    g.vsync=vsync!=0;
    g.camera.vertical_fov_degrees=fov;
    g.background_color=background;
    g.next_frame={};
    g.settings_dirty=false;
    return true;
}

bool save_graphics_settings() {
    const auto& g=app();
    std::ofstream file("editor_settings.tmp",std::ios::trunc);
    if (!file) return false;
    file<<"version 1\n"
        <<"fps_limit "<<g.fps_limit<<'\n'
        <<"grid_visible "<<(g.grid_visible?1:0)<<'\n'
        <<"vsync "<<(g.vsync?1:0)<<'\n'
        <<"fov "<<std::setprecision(9)<<g.camera.vertical_fov_degrees<<'\n'
        <<"background "<<g.background_color.x<<' '<<g.background_color.y<<' '
        <<g.background_color.z<<'\n';
    file.close();
    if (!file || !MoveFileExW(L"editor_settings.tmp",L"editor_settings.ini",
            MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(L"editor_settings.tmp");
        return false;
    }
    app().settings_dirty=false;
    return true;
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
    const float cp = std::cos(g.orbit.pitch);
    g.camera.position = engine::Vec3{
        target.x + g.orbit.distance * cp * std::sin(g.orbit.yaw),
        target.y + g.orbit.distance * std::sin(g.orbit.pitch),
        target.z + g.orbit.distance * cp * std::cos(g.orbit.yaw)
    };
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
