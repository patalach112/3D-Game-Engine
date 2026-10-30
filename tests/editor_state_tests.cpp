#include "editor_app.hpp"

#include <cassert>
#include <cfloat>
#include <cmath>
#include <iostream>
#include "dock_layout.hpp"
#include "hud_editor.hpp"
#include "hud_style.hpp"
#include "settings.hpp"
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
    const auto* settings=ImGui::FindWindowByName(editor::layout::settings);
    assert(settings && !settings->DockIsActive && (settings->Flags&ImGuiWindowFlags_NoDocking));
    g.show_settings=false;ui_frame();
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
    const auto original_text_color=ImGui::GetStyleColorVec4(ImGuiCol_Text);
    {
        engine::hud::Palette palette;palette[engine::hud::ColorRole::Text]={1,0,0,1};
        editor::HudColors scope(palette);assert(ImGui::GetStyleColorVec4(ImGuiCol_Text).x==1);
        {palette[engine::hud::ColorRole::Text]={0,1,0,1};editor::HudColors child(palette);assert(ImGui::GetStyleColorVec4(ImGuiCol_Text).y==1);}
        assert(ImGui::GetStyleColorVec4(ImGuiCol_Text).x==1);
    }
    assert(ImGui::GetStyleColorVec4(ImGuiCol_Text).x==original_text_color.x);
    for(int i=0;i<6;++i)hud_frame();
    assert(GImGui->ColorStack.empty());
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
    auto* coloured=g.hud.find(custom);coloured->custom_colors=true;
    coloured->palette[engine::hud::ColorRole::Background]={.8F,.1F,.7F,1};hud_frame();hud_frame();
    const auto* coloured_window=ImGui::FindWindowByName(engine::hud::window_name(*g.hud.find(custom)).c_str());
    const auto expected=ImGui::ColorConvertFloat4ToU32({.8F,.1F,.7F,1});bool found_colour=false;
    for(const auto& vertex:coloured_window->DrawList->VtxBuffer)if(vertex.col==expected)found_colour=true;
    assert(found_colour && GImGui->ColorStack.empty());
    assert(ImGui::GetStyleColorVec4(ImGuiCol_Text).x==original_text_color.x);
    const std::string docking=ImGui::SaveIniSettingsToMemory();assert(!docking.empty());
    // Settings remains reachable in designer mode but is not an editable node.
    const auto node_count=g.hud.nodes.size();editor::hud_action("settings");hud_frame();hud_frame();
    assert(g.show_settings && g.hud.nodes.size()==node_count);
    assert(ImGui::FindWindowByName("Ustawienia")->Flags&ImGuiWindowFlags_NoDocking);
    editor::hud_action("settings.bindings");hud_frame();hud_frame();
    g.binding_capture=0;g.binding_capture_armed=true;g.binding_capture_frame=ImGui::GetFrameCount()-1;
    hud_io.AddKeyEvent(ImGuiMod_Ctrl,true);hud_io.AddKeyEvent(ImGuiKey_T,true);hud_frame();
    assert(g.binding_capture==-1 && g.hud.bindings[0].key=='T' && g.hud.bindings[0].modifiers==engine::hud::Ctrl);
    hud_io.AddKeyEvent(ImGuiMod_Ctrl,false);hud_io.AddKeyEvent(ImGuiKey_T,false);hud_frame();
    g.binding_capture=0;g.binding_capture_armed=true;
    hud_io.AddKeyEvent(ImGuiKey_Delete,true);hud_frame();assert(g.binding_capture==0 && !g.binding_message.empty());
    hud_io.AddKeyEvent(ImGuiKey_Delete,false);hud_frame();hud_io.AddKeyEvent(ImGuiKey_Escape,true);hud_frame();assert(g.binding_capture==-1);
    hud_io.AddKeyEvent(ImGuiKey_Escape,false);hud_frame();
    editor::restore_bindings();assert(g.hud.bindings==engine::hud::basic_bindings());
    g.show_settings=false;g.gizmo_mode=0;
    hud_io.AddKeyEvent(ImGuiKey_E,true);hud_frame();editor::process_bindings();assert(g.gizmo_mode==0);
    hud_io.AddKeyEvent(ImGuiKey_E,false);hud_frame();
    g.hud_designer=false;editor::initialize_bindings();
    hud_io.AddMousePosEvent(1,static_cast<float>(g.height)-2);hud_frame();
    hud_io.AddKeyEvent(ImGuiKey_E,true);hud_frame();assert(g.gizmo_mode==2);
    hud_io.AddKeyEvent(ImGuiKey_E,false);hud_frame();
    g.live_bindings={{"tool.rotate",'T',engine::hud::Ctrl}};
    g.gizmo_mode=0;hud_io.AddKeyEvent(ImGuiKey_E,true);hud_frame();assert(g.gizmo_mode==0);
    hud_io.AddKeyEvent(ImGuiKey_E,false);hud_frame();
    hud_io.AddKeyEvent(ImGuiMod_Ctrl,true);hud_io.AddKeyEvent(ImGuiKey_T,true);hud_frame();assert(g.gizmo_mode==1);
    hud_io.AddKeyEvent(ImGuiMod_Ctrl,false);hud_io.AddKeyEvent(ImGuiKey_T,false);hud_frame();
    // Cold runtime startup must not dispatch launcher's held keys. The designer
    // never enters this dispatch path, which hid this gap in previous tests.
    g.gizmo_mode=0;g.bindings_armed=false;g.live_bindings={{"tool.rotate",'T',0}};
    hud_io.AddKeyEvent(ImGuiKey_T,true);hud_frame();assert(g.gizmo_mode==0 && !g.bindings_armed);
    hud_io.AddKeyEvent(ImGuiKey_T,false);hud_frame();assert(g.gizmo_mode==0 && g.bindings_armed);
    hud_io.AddKeyEvent(ImGuiKey_T,true);hud_frame();assert(g.gizmo_mode==1);
    hud_io.AddKeyEvent(ImGuiKey_T,false);hud_frame();
    g.live_bindings={{"grid.toggle",static_cast<int>(engine::hud::Key::MouseLeft),0}};
    const bool grid=g.grid_visible;
    hud_io.AddMousePosEvent(-FLT_MAX,-FLT_MAX);hud_io.AddMouseButtonEvent(0,true);hud_frame();assert(g.grid_visible==grid);
    hud_io.AddMouseButtonEvent(0,false);hud_frame();
    ImGui::DestroyContext();g.hud_loaded=false;g.hud_designer=false;
    std::cout << "Editor state, flat picking, docked UI and console passed (no GPU)\n";
}
