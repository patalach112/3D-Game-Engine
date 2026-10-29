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
    engine::OrbitState orbit;
    int drag_axis{-1};
    float drag_sign{1};
    engine::Vec3 drag_start_position{}, drag_start_hit{}, drag_plane_normal{};
    std::wstring status{L"Gotowe"};
    char search[128]{};
    char entity_name[512]{};
    engine::EntityId name_target{};
    char notes[8192]{};
};
App& app();
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
void save_scene();
void add_primitive(engine::PrimitiveType type);
bool ray_plane(engine::Ray3 ray,engine::Vec3 point,engine::Vec3 normal,engine::Vec3& hit);
bool ray_box(engine::Ray3 ray,const engine::Entity& entity,float& distance);
bool hit_gizmo(int sx,int sy,int view_w,int view_h,engine::Entity& entity);
LRESULT CALLBACK window_proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp);
} // namespace editor
