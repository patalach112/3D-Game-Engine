#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include "engine/render/vulkan_renderer.hpp"
#include "engine/ui.hpp"
#include "engine/camera_controller.hpp"
#include "engine/input.hpp"
#include "engine/render/render_worker.hpp"
#include <chrono>
#include <string>

namespace editor {
constexpr int kHeaderHeight=56;
constexpr int kFooterHeight=28;
struct App {
    engine::Scene scene;
    engine::VulkanRenderer renderer;
    engine::EditorCamera camera;
    engine::EntityId selected{};
    RECT viewport_bounds{};
    bool ready{}, renderer_failed{};
    int width{1280}, height{800};
    int left_width{250}, right_width{320}, bottom_height{180};
    engine::OrbitState orbit, displayed_orbit;
    engine::OrbitInput orbit_input;
    std::vector<engine::OrbitInput> orbit_events;
    std::unique_ptr<engine::InputProcessor> inputs;
    std::unique_ptr<engine::RenderWorker> render_worker;
    bool close_requested{}, minimized{};
    unsigned native_buttons{};

    // 0 means unlimited. The deadline is managed by the message-loop frame pacer.
    int fps_limit{0};
    std::chrono::steady_clock::time_point next_frame{};
    std::chrono::steady_clock::time_point previous_frame{};
    bool grid_visible{true};
    engine::Vec3 background_color{39.0F / 255.0F, 39.0F / 255.0F, 41.0F / 255.0F};

    int drag_axis{-1};
    float drag_sign{1.0F};
    engine::Vec3 drag_start_position{};
    engine::Vec3 drag_start_rotation{};
    engine::Vec3 drag_start_scale{1, 1, 1};
    engine::Vec3 drag_start_hit{};
    engine::Vec3 drag_plane_normal{};
    POINT drag_start_mouse{};

    // Tryb gizma: 0=pozycja, 1=obrót, 2=skala.
    int gizmo_mode{0};
    // Wskaźnik do przeciągania przy wyborze typu primitywu.
    bool dragging_type_selector{false};

    std::wstring status{L"Gotowe"};
    char search[128]{};
    char entity_name[512]{};
    engine::EntityId name_target{};
    char notes[8192]{};
};
App& app();
using FrameClock = std::chrono::steady_clock;
[[nodiscard]] bool frame_due(const App& state, FrameClock::time_point now) noexcept;
void schedule_next_frame(App& state, FrameClock::time_point started,
                         FrameClock::time_point completed) noexcept;
[[nodiscard]] unsigned long frame_wait_milliseconds(const App& state,
                                                     FrameClock::time_point now) noexcept;
engine::Vec3 add(engine::Vec3 a,engine::Vec3 b);
engine::Vec3 sub(engine::Vec3 a,engine::Vec3 b);
engine::Vec3 mul(engine::Vec3 a,float n);
float dot(engine::Vec3 a,engine::Vec3 b);
engine::Vec3 cross(engine::Vec3 a,engine::Vec3 b);
float magnitude(engine::Vec3 a);
engine::Vec3 normalized(engine::Vec3 a);
void update_camera();
void update_layout();
RECT view_rect();
void draw_ui();
void render_frame(HWND hwnd);
void reset_frame_timing();
void apply_input_event(HWND hwnd,const engine::InputEvent& event);
constexpr UINT kWorkerWake=WM_APP+1;
void save_scene();
void add_primitive(engine::PrimitiveType type);
void duplicate_entity();
void frame_selected();
void snap_camera(int view); // 0=front, 1=top, 2=side, 3=iso
bool ray_plane(engine::Ray3 ray, engine::Vec3 point, engine::Vec3 normal, engine::Vec3& hit);
bool ray_box(engine::Ray3 ray, const engine::Entity& entity, float& distance);
bool hit_gizmo(int sx, int sy, int view_w, int view_h, engine::Entity& entity);
int gizmo_hit(int sx, int sy, int view_w, int view_h, engine::Entity& entity);

// Procedurowa procedura okna.
LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp);

} // namespace editor
