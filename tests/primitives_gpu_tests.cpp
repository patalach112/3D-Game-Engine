#include "engine/render/vulkan_renderer.hpp"
#include "engine/ui.hpp"
#include "../src/editor/dock_layout.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>
int main() {
    engine::VulkanRenderer renderer;
    renderer.resize(320,240);
    renderer.set_grid_visible(false);
    engine::EditorCamera camera;
    camera.position={0,0,3}; camera.target={0,0,0};
    engine::Scene scene;
    renderer.render(scene,camera,0);
    const auto background=renderer.pixels().at(120*320+160);
    auto& entity=scene.create_entity("flat",engine::PrimitiveType::Circle2D);
    const auto id=entity.id;
    const auto sample=[&](engine::Vec3 p) {
        const auto pixel=engine::project_to_screen(p,camera,320,240);
        return renderer.pixels().at(static_cast<std::size_t>(std::lround(pixel.y))*320+std::lround(pixel.x));
    };
    renderer.render(scene,camera,0);
    assert(sample({0,0,0})!=background);
    assert(sample({0.44F,0.44F,0})==background); // disk has no square corners
    const auto face_on=renderer.pixels();
    scene.find_entity(id)->primitive=engine::PrimitiveType::Square2D;
    renderer.render(scene,camera,0);
    assert(sample({0.44F,0.44F,0})!=background);
    scene.find_entity(id)->primitive=engine::PrimitiveType::Circle2D;
    scene.find_entity(id)->transform.rotation.y=90;
    renderer.render(scene,camera,0);
    const auto count=[&](const auto& pixels) {
        return std::count_if(pixels.begin(),pixels.end(),[&](auto p) { return p!=background; });
    };
    assert(count(renderer.pixels())<count(face_on)/15); // edge-on disk, not a sphere
    scene.find_entity(id)->transform.rotation={};
    std::vector<std::uint32_t> modes[3];
    for (int mode=0;mode<3;++mode) {
        renderer.set_gizmo_mode(mode);
        renderer.render(scene,camera,id);
        modes[mode]=renderer.pixels();
    }
    assert(modes[0]!=modes[1] && modes[1]!=modes[2] && modes[0]!=modes[2]);
    renderer.resize(800,600);
    renderer.enable_ui();
    ImGui::GetIO().ConfigFlags|=ImGuiConfigFlags_DockingEnable;
    engine::RenderViewport view;
    for (int i=0;i<5;++i) {
        renderer.begin_ui_frame();
        editor::layout::dockspace({0,0},{800,600});
        assert(ImGui::Begin(editor::layout::scene,nullptr,ImGuiWindowFlags_NoBackground));
        const auto position=ImGui::GetCursorScreenPos(),size=ImGui::GetContentRegionAvail();
        view={static_cast<int>(std::ceil(position.x)),static_cast<int>(std::ceil(position.y)),
            static_cast<int>(std::floor(size.x)),static_cast<int>(std::floor(size.y))};
        ImGui::End();
        for (auto title:{editor::layout::hierarchy,editor::layout::inspector,editor::layout::console}) {
            ImGui::Begin(title); ImGui::TextUnformatted("GPU docked panel"); ImGui::End();
        }
        renderer.render(scene,camera,0,view);
    }
    const auto center=static_cast<std::size_t>(view.y+view.height/2)*800+view.x+view.width/2;
    const auto with_ui=renderer.pixels().at(center);
    renderer.render(scene,camera,0,view);
    assert(with_ui==renderer.pixels().at(center) && with_ui!=background);
    // Float the scene over an opaque hierarchy panel. Background panels must
    // not cover the 3D view, but a foreground popup/panel must remain visible.
    for (int i=0;i<4;++i) {
        renderer.begin_ui_frame();
        editor::layout::dockspace({0,0},{800,600});
        ImGui::SetNextWindowDockID(0);
        ImGui::SetNextWindowPos({30,80}); ImGui::SetNextWindowSize({360,320});
        ImGui::SetNextWindowFocus();
        ImGui::Begin(editor::layout::scene,nullptr,ImGuiWindowFlags_NoBackground);
        auto* list=ImGui::GetWindowDrawList();
        const auto p=ImGui::GetCursorScreenPos(),size=ImGui::GetContentRegionAvail();
        view={static_cast<int>(std::ceil(p.x)),static_cast<int>(std::ceil(p.y)),
            static_cast<int>(std::floor(size.x)),static_cast<int>(std::floor(size.y))};
        // Keep a draw list for the otherwise empty transparent window.
        ImGui::TextUnformatted("Floating Vulkan scene");
        ImGui::End();
        for (auto title:{editor::layout::hierarchy,editor::layout::inspector,editor::layout::console}) {
            ImGui::Begin(title,nullptr,ImGuiWindowFlags_NoFocusOnAppearing);
            ImGui::TextUnformatted("Behind scene"); ImGui::End();
        }
        renderer.finish_ui_frame();
        editor::layout::cutout_behind_scene(ImGui::GetDrawData(),list,
            {static_cast<float>(view.x),static_cast<float>(view.y),
             static_cast<float>(view.x+view.width),static_cast<float>(view.y+view.height)});
        renderer.render(scene,camera,0,view);
    }
    const auto floated_center=static_cast<std::size_t>(view.y+view.height/2)*800+view.x+view.width/2;
    const auto floated=renderer.pixels().at(floated_center);
    renderer.render(scene,camera,0,view);
    assert(floated==renderer.pixels().at(floated_center) && floated!=background);
}
