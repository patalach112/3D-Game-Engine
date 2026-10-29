#include "engine/render/vulkan_renderer.hpp"
#include "engine/ui.hpp"
#include "engine/camera_controller.hpp"
#include <array>
#include <cassert>
#include <iostream>
using namespace engine;
int main() {
    GeometryBuilder builder;
    const std::array<Vertex,3> vertices{{make_vertex({-1,-1,0},{0,0,1}),make_vertex({1,-1,0},{0,0,1}),make_vertex({0,1,0},{0,0,1})}};
    const std::array<std::uint32_t,3> indices{0,1,2};
    const std::array<Vertex_hud,3> hud{{{{0,0},{1,0,0,1}},{{32,0},{1,0,0,1}},{{0,32},{1,0,0,1}}}};
    builder.add_hud(hud,indices);
    const auto mesh=builder.add_mesh(vertices,indices);
    ObjectData front; builder.add_object(mesh,front,{0,0,1,1});
    ObjectData behind; behind.position={0,0,20}; builder.add_object(mesh,behind);
    ObjectData outside; outside.position={10000,0,0}; builder.add_object(mesh,outside);
    const std::array<Vertex,4> quad{{make_vertex({-.25F,-.25F,0},{0,0,1}),make_vertex({.25F,-.25F,0},{0,0,1}),
        make_vertex({.25F,.25F,0},{0,0,1}),make_vertex({-.25F,.25F,0},{0,0,1})}};
    const std::array<std::uint32_t,6> quad_indices{0,1,2,2,3,0};
    const auto second=builder.add_mesh(quad,quad_indices);
    ObjectData right; right.position={1.5F,0,0}; builder.add_object(second,right,{1,1,0,1});
    const auto geometry=builder.build();
    VulkanRenderer renderer; renderer.resize(128,96);
    const EditorCamera camera{{0,0,5},{0,0,0},45};
    renderer.render_geometry(geometry,camera);
    const auto& commands=renderer.last_draw_commands();
    assert(commands.size()==4 && commands[0].instanceCount==1 && commands[1].instanceCount==0 && commands[2].instanceCount==0);
    assert(commands[2].firstInstance==2 && commands[0].indexCount==3);
    const auto pixel=[&](int x,int y) { return renderer.pixels()[static_cast<std::size_t>(y*128+x)]; };
    assert(pixel(4,4)==0xff0000); assert((pixel(64,48)&255)>0 && (pixel(64,48)&0xffff00)==0);
    assert(pixel(127,95)==0x272729);
    assert(commands[3].firstIndex==3&&commands[3].vertexOffset==3&&commands[3].firstInstance==3&&commands[3].instanceCount==1);
    assert((pixel(95,44)&0xffff00)!=0 && (pixel(95,44)&255)==0); // Second quad's second triangle.
    assert(pixel(107,48)==0x272729); // A wrong vertexOffset would fetch the larger first triangle here.
    renderer.enable_ui(); renderer.begin_ui_frame();
    auto* draw=ImGui::GetForegroundDrawList();
    // >65535 vertices forces ImGui VtxOffset and conversion from local uint16 indices to uint32.
    for (int i=0;i<20000;++i) draw->AddRectFilled({140,100},{141,101},IM_COL32_WHITE);
    draw->AddRectFilled({10,10},{20,20},IM_COL32(0,255,0,255));
    renderer.render_geometry(geometry,camera);
    assert(pixel(4,4)==0xff0000 && pixel(15,15)==0x00ff00);
    assert(renderer.last_draw_commands().size()==4); // UI/HUD never becomes an indirect object.
    // Regression: consume queued ImGui input after NewFrame, not a stale native-message capture flag.
    OrbitState orbit;
    auto& io=ImGui::GetIO(); io.AddFocusEvent(true);
    const auto orbit_frame=[&] {
        renderer.begin_ui_frame();
        io.WantCaptureMouse=true; // Global capture must not disable a geometrically empty 3D viewport.
        OrbitInput input;
        input.mouse={io.MousePos.x,io.MousePos.y}; input.rightDown=ImGui::IsMouseDown(ImGuiMouseButton_Right);
        input.rightPressed=ImGui::IsMouseClicked(ImGuiMouseButton_Right);
        input.focused=!io.AppFocusLost; input.pointerValid=ImGui::IsMousePosValid();
        update_orbit(orbit,input,{0,0,128,96});
        renderer.render_geometry(geometry,camera);
    };
    io.AddMousePosEvent(50,50); io.AddMouseButtonEvent(ImGuiMouseButton_Right,true); orbit_frame();
    assert(orbit.dragging); const float old_yaw=orbit.yaw;
    io.AddMousePosEvent(80,60); orbit_frame(); assert(orbit.yaw<old_yaw&&orbit.pitch>.42F);
    io.AddMouseButtonEvent(ImGuiMouseButton_Right,false); orbit_frame(); assert(!orbit.dragging);
    GeometryBuilder hud_only; hud_only.add_hud(hud,indices);
    renderer.render_geometry(hud_only.build(),camera);
    assert(renderer.last_draw_commands().empty()&&pixel(4,4)==0xff0000);
    GeometryBuilder empty;
    renderer.render_geometry(empty.build(),camera);
    assert(renderer.last_draw_commands().empty()&&pixel(4,4)==0x272729);
    std::cout<<"Shared HUD/3D geometry + GPU culling + ImGui baseVertex: "<<renderer.device_name()<<'\n';
}
