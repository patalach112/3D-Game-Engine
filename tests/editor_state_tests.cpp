#include "editor_app.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include "dock_layout.hpp"
#include "hud_editor.hpp"
#include "engine/gizmo.hpp"

namespace {
bool close_enough(float a, float b) { return std::abs(a - b) < 0.0001F; }
void same(engine::Vec3 a, engine::Vec3 b) {
    assert(close_enough(a.x, b.x) && close_enough(a.y, b.y) && close_enough(a.z, b.z));
}
void check_camera() {
    const auto& g = editor::app();
    auto expected = g.camera;
    engine::apply_orbit(expected, g.orbit);
    same(g.camera.position, expected.position);
    assert(close_enough(editor::magnitude(editor::sub(g.camera.position, g.camera.target)), g.orbit.distance));
}
void check_layout() {
    editor::update_layout();
    const auto& g = editor::app();
    const auto view = editor::view_rect();
    assert(g.renderer.width() == g.width && g.renderer.height() == g.height);
    assert(view.left == g.left_width + 6 && view.top == editor::kHeaderHeight);
    assert(view.right == g.width - g.right_width - 6);
    assert(view.bottom == g.height - editor::kFooterHeight - g.bottom_height - 6);
    assert(view.right > view.left && view.bottom > view.top);
    assert(view.left >= 0 && view.top >= 0 && view.right <= g.width && view.bottom <= g.height);
    assert(EqualRect(&view, &g.viewport_bounds));
}
}

int main() {
    auto& g = editor::app();
    assert(g.fps_limit == 0); // threaded editor starts uncapped
    // The frame limiter uses deadlines, supports arbitrary limits and treats 0 as
    // genuinely uncapped rather than silently falling back to a 16 ms timer.
    const auto start = editor::FrameClock::time_point(std::chrono::seconds(1));
    g.fps_limit = 60; g.next_frame = {};
    assert(editor::frame_due(g, start));
    editor::schedule_next_frame(g, start, start + std::chrono::milliseconds(2));
    assert(!editor::frame_due(g, start + std::chrono::milliseconds(15)));
    assert(editor::frame_due(g, start + std::chrono::milliseconds(17)));
    assert(editor::frame_wait_milliseconds(g, start + std::chrono::milliseconds(15)) == 2);
    g.fps_limit = 0;
    editor::schedule_next_frame(g, start, start + std::chrono::milliseconds(2));
    assert(editor::frame_due(g, start + std::chrono::milliseconds(2)));
    assert(editor::frame_wait_milliseconds(g, start) == 0);
    g.fps_limit = 60; g.next_frame = {}; g.previous_frame = {};

    // Constructing App and resizing must not initialize Vulkan or require a GPU.
    check_layout();
    g.width = 900; g.height = 600;
    g.left_width = 10000; g.right_width = 10000; g.bottom_height = 10000;
    check_layout();
    g.left_width = -100; g.right_width = -100; g.bottom_height = -100;
    check_layout();
    assert(g.left_width == 170 && g.right_width == 240 && g.bottom_height == 100);
    g.width = 1920; g.height = 1080;
    g.left_width = 270; g.right_width = 350; g.bottom_height = 220;
    check_layout(); check_layout();
    assert(g.left_width == 270 && g.right_width == 350 && g.bottom_height == 220);

    editor::duplicate_entity(); editor::frame_selected();
    assert(g.scene.entity_count() == 0 && g.selected == 0);
    const auto id = g.scene.create_entity("Original", engine::PrimitiveType::Cylinder).id;
    g.scene.find_entity(id)->transform = {{3, 4, 5}, {10, 20, 30}, {2, 3, 4}};
    const auto original = *g.scene.find_entity(id);
    // Retain all copies to force repeated vector growth during duplication.
    for (int i = 0; i < 300; ++i) {
        g.selected = id; g.drag_axis = 1;
        editor::duplicate_entity();
        const auto* copy = g.scene.find_entity(g.selected);
        assert(copy && copy->id != id && g.drag_axis == -1);
        assert(copy->name == "Original copy" && copy->primitive == original.primitive);
        same(copy->transform.position, {5, 4, 5});
        same(copy->transform.rotation, original.transform.rotation);
        same(copy->transform.scale, original.transform.scale);
        same(g.scene.find_entity(id)->transform.position, original.transform.position);
    }
    assert(g.scene.entity_count() == 301);
    g.selected = id;
    g.orbit.dragging = true;
    editor::frame_selected();
    same(g.camera.target, original.transform.position);
    assert(!g.orbit.dragging && close_enough(g.orbit.distance, 6));
    assert(close_enough(g.orbit.yaw, .75F) && close_enough(g.orbit.pitch, .42F));
    check_camera();
    const float yaw[]{0, 0, 1.57F, .75F};
    const float pitch[]{0, 1.4F, 0, .42F};
    for (int view = 0; view < 4; ++view) {
        g.orbit.dragging = true;
        editor::snap_camera(view);
        same(g.camera.target, original.transform.position);
        assert(!g.orbit.dragging && close_enough(g.orbit.distance, 8));
        assert(close_enough(g.orbit.yaw, yaw[view]) && close_enough(g.orbit.pitch, pitch[view]));
        check_camera();
    }
    g.selected = 999999;
    editor::duplicate_entity(); editor::frame_selected();
    assert(g.scene.entity_count() == 301 && g.selected == 999999);
    // Actual flat primitive picking, including rotated/non-uniformly scaled shapes.
    engine::Entity flat{1,"flat",engine::PrimitiveType::Circle2D,{}};
    float hit{};
    assert(editor::ray_box({{0,0,3},{0,0,-1}},flat,hit) && close_enough(hit,3));
    assert(!editor::ray_box({{0.44F,0.44F,3},{0,0,-1}},flat,hit));
    flat.primitive=engine::PrimitiveType::Square2D;
    assert(editor::ray_box({{0.44F,0.44F,3},{0,0,-1}},flat,hit));
    flat.transform.rotation={0,90,0}; flat.transform.scale={2,3,1};
    assert(editor::ray_box({{3,0,0},{-1,0,0}},flat,hit));
    assert(!editor::ray_box({{3,2,0},{-1,0,0}},flat,hit));
    flat.primitive=engine::PrimitiveType::Plane; flat.transform={};
    assert(editor::ray_box({{0,3,0},{0,-1,0}},flat,hit));
    assert(!editor::ray_box({{0.6F,3,0},{0,-1,0}},flat,hit));

    // Picking uses the same world-space ring/handle geometry as the renderer.
    g.camera.position={3,2,4}; g.camera.target={0,0,0};
    flat.transform={};
    const float radius=engine::gizmo_length(g.camera.position,flat.transform.position);
    const auto endpoint=engine::project_to_screen({radius,0,0},g.camera,600,400);
    for (int mode:{0,2}) {
        g.gizmo_mode=mode;
        assert(editor::hit_gizmo(static_cast<int>(std::lround(endpoint.x)),static_cast<int>(std::lround(endpoint.y)),600,400,flat));
        assert(g.drag_axis==0 && std::abs(editor::dot(g.drag_plane_normal,{1,0,0}))<0.001F);
    }
    const auto ring=engine::project_to_screen(engine::gizmo_ring_point({},2,radius,0.7F),g.camera,600,400);
    g.gizmo_mode=1;
    assert(editor::hit_gizmo(static_cast<int>(std::lround(ring.x)),static_cast<int>(std::lround(ring.y)),600,400,flat));
    assert(std::abs(editor::dot(g.drag_plane_normal,engine::gizmo_axis(g.drag_axis)))>0.99F);
    g.drag_axis=-1;

    // Build the real editor UI without initializing Vulkan or opening a window.
    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    auto& io=ImGui::GetIO();
    io.IniFilename=nullptr; io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;
    io.ConfigWindowsMoveFromTitleBarOnly=true;
    io.DisplaySize={static_cast<float>(g.width),static_cast<float>(g.height)};
    io.Fonts->AddFontDefault(); io.Fonts->Build();
    const auto ui_frame=[&] { ImGui::NewFrame(); editor::draw_ui(); ImGui::Render(); };
    for (int i=0;i<5;++i) ui_frame();
    assert(g.docking_initialized && g.viewport_visible);
    auto* console=ImGui::FindWindowByName(editor::layout::console);
    assert(console && console->DockTabIsVisible);
    const auto view=editor::view_rect();
    assert(view.left>0 && view.right<g.width && view.bottom<g.height);
    assert(editor::scene_input_at((view.left+view.right)/2,(view.top+view.bottom)/2));
    g.show_scene=false; ui_frame();
    assert(!g.viewport_visible && g.viewport_bounds.right==0);
    g.show_scene=true; ui_frame(); ui_frame();
    assert(g.viewport_visible);

    // Execute an actual console command through ImGui text/key events.
    const float command_x=console->Pos.x+40;
    const float command_y=console->Pos.y+console->Size.y-ImGui::GetStyle().WindowPadding.y-ImGui::GetFrameHeight()/2;
    io.AddMousePosEvent(command_x,command_y); ui_frame();
    io.AddMouseButtonEvent(0,true); ui_frame();
    io.AddMouseButtonEvent(0,false); ui_frame();
    io.AddInputCharactersUTF8("grid off"); ui_frame();
    io.AddKeyEvent(ImGuiKey_Enter,true); ui_frame();
    io.AddKeyEvent(ImGuiKey_Enter,false); ui_frame();
    assert(!g.grid_visible && !g.console_command[0]);
    g.show_settings=true; g.focus_settings=true; ui_frame(); ui_frame();
    assert(ImGui::FindWindowByName(editor::layout::settings)->DockTabIsVisible);
    ImGui::DestroyContext();
    // Same document drives engine_editor and editor_hud; native controls are
    // deliberately not document nodes. Exercise both modes without a GPU.
    ImGui::CreateContext();
    auto& hud_io=ImGui::GetIO();
    hud_io.IniFilename=nullptr;hud_io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;
    hud_io.DisplaySize={static_cast<float>(g.width),static_cast<float>(g.height)};
    hud_io.Fonts->AddFontDefault();hud_io.Fonts->Build();
    g.hud=engine::hud::defaults();g.hud_loaded=true;
    const auto hud_frame=[&]{ImGui::NewFrame();editor::draw_ui();ImGui::Render();};
    for(int i=0;i<6;++i)hud_frame();
    assert(g.viewport_visible && g.scene_drawlist);
    auto hud_view=editor::view_rect();
    assert(editor::scene_input_at((hud_view.left+hud_view.right)/2,(hud_view.top+hud_view.bottom)/2));
    engine::hud::Id scene_window{},scene_panel{};
    for(const auto& n:g.hud.nodes)if(n.builtin=="scene"){scene_window=n.parent;scene_panel=n.id;}
    assert(scene_window && scene_panel);
    g.hud.find(scene_window)->visible=false;hud_frame();assert(!g.viewport_visible);
    g.hud.find(scene_window)->visible=true;hud_frame();hud_frame();assert(g.viewport_visible);
    g.hud_designer=true;
    for(int i=0;i<4;++i)hud_frame();
    assert(ImGui::FindWindowByName("HUD / Właściwości") && g.viewport_visible);
    // Select via a real preview hit, rather than only the model tree.
    hud_view=editor::view_rect();hud_io.AddMousePosEvent(static_cast<float>((hud_view.left+hud_view.right)/2),static_cast<float>((hud_view.top+hud_view.bottom)/2));
    hud_frame();hud_io.AddMouseButtonEvent(0,true);hud_frame();hud_io.AddMouseButtonEvent(0,false);hud_frame();
    assert(g.hud_selected==scene_panel);
    g.hud.find(scene_panel)->shared_rounding=false;g.hud.find(scene_panel)->rounding=23;
    g.hud.default_rounding=17;hud_frame();
    const auto custom=g.hud.add(engine::hud::Kind::Window,0,"Custom");
    auto panel=g.hud.add(engine::hud::Kind::Panel,custom,"Panel");
    auto list=g.hud.add(engine::hud::Kind::List,panel,"List");
    auto nested=g.hud.add(engine::hud::Kind::List,list,"Nested");
    g.hud.add(engine::hud::Kind::Button,nested,"Action");
    for(int i=0;i<3;++i)hud_frame();
    assert(ImGui::FindWindowByName(engine::hud::window_name(*g.hud.find(custom)).c_str()));
    const std::string docking=ImGui::SaveIniSettingsToMemory();assert(!docking.empty());
    ImGui::DestroyContext();g.hud_loaded=false;g.hud_designer=false;
    std::cout << "Editor state, flat picking, docked UI and console passed (no GPU)\n";
}
