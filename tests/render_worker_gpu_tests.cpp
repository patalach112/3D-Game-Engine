#include "engine/render/render_worker.hpp"
#include "engine/render/vulkan_renderer.hpp"
#include "engine/ui.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
int main() {
    engine::VulkanRenderer renderer;
    engine::RenderWorker worker;
    engine::Scene scene;
    (void)scene.create_entity("Cube");
    engine::EditorCamera camera;
    renderer.resize(160,120);
    renderer.enable_ui();
    const auto geometry=engine::GeometryBuilder{}.build();
    for (int frame=0;frame<12;++frame) {
        worker.wait();
        const int width=frame%2?192:160,height=frame%2?144:120;
        renderer.resize(width,height);
        renderer.begin_ui_frame(1.0F/120);
        ImGui::GetForegroundDrawList()->AddRectFilled({4,4},{28,28},IM_COL32(255,0,0,255));
        ImGui::GetForegroundDrawList()->AddText({30,8},IM_COL32_WHITE,"GPU worker");
        renderer.finish_ui_frame();
        bool rejected=false;
        try { renderer.begin_ui_frame(); } catch (const std::logic_error&) { rejected=true; }
        assert(rejected); // frontend cannot overwrite draw data pending consumption
        worker.submit([&,frame] {
            if (frame%2) renderer.render_geometry(geometry,camera);
            else renderer.render(scene,camera,0);
            renderer.present();
        });
        worker.wait();
        assert(renderer.pixels().size()==static_cast<std::size_t>(width*height));
        assert(renderer.pixels()[10*width+10]==0xff0000);
    }
    worker.wait();
    std::cout << "UI/frontend -> Vulkan render worker, scene/geometry paths and resize: "
              << renderer.device_name() << '\n';
}
