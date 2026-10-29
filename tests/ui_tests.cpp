#include "engine/render/vulkan_renderer.hpp"
#include "engine/ui.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

int main() {
    engine::VulkanRenderer renderer;
    engine::Scene scene;
    engine::EditorCamera camera;
    bool rejected=false;
    try { renderer.begin_ui_frame(); } catch (const std::logic_error&) { rejected=true; }
    assert(rejected);
    renderer.resize(640,480);
    renderer.enable_ui();
    auto& io=ImGui::GetIO();
    io.AddFocusEvent(true);
    assert(renderer.antialiasing_samples()==1 || renderer.antialiasing_samples()==2 || renderer.antialiasing_samples()==4);
    char text[128]="seed";
    int clicks=0;
    ImVec2 button_center{};
    float scroll_y=0;
    const auto frame=[&](bool focus=false) {
        renderer.begin_ui_frame();
        ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize({240,480});
        ImGui::Begin("Controls",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings);
        if (ImGui::Button("Test button",{150,30})) ++clicks;
        const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();
        button_center={(lo.x+hi.x)/2,(lo.y+hi.y)/2};
        if (focus) ImGui::SetKeyboardFocusHere();
        ImGui::InputText("##writebox",text,sizeof(text));
        ImGui::BeginChild("scroll",{180,120},ImGuiChildFlags_Borders);
        for (int i=0;i<100;++i) ImGui::Text("Object %d",i);
        scroll_y=ImGui::GetScrollY();
        ImGui::EndChild(); ImGui::End();
        auto* draw=ImGui::GetForegroundDrawList();
        draw->AddRectFilled({250,4},{274,28},IM_COL32(255,0,0,255));
        draw->PushClipRect({250,80},{270,100},true);
        draw->AddRectFilled({240,70},{300,110},IM_COL32(0,255,0,255));
        draw->PopClipRect();
        draw->PushClipRect({250,40},{425,65},true);
        draw->AddText({250,40},IM_COL32(255,255,255,255),"Vulkan GPU text");
        draw->PopClipRect();
        // Disable ImGui's geometric AA here to isolate actual Vulkan MSAA resolve.
        const auto flags=draw->Flags;
        draw->Flags&=~ImDrawListFlags_AntiAliasedFill;
        draw->AddTriangleFilled({450.25F,40.25F},{550.25F,40.25F},{450.25F,100.25F},IM_COL32(0,0,255,255));
        draw->Flags=flags;
        renderer.render(scene,camera,0,{300,120,320,340});
        renderer.present(); // Safe headless no-op; pixels come from actual Vulkan.
    };
    frame(true); frame();
    io.AddInputCharactersUTF8(" GPU ą"); frame();
    assert(std::strstr(text,"GPU") && std::strstr(text,"ą"));
    io.AddMousePosEvent(button_center.x,button_center.y); frame();
    io.AddMouseButtonEvent(0,true); frame();
    io.AddMouseButtonEvent(0,false); frame();
    assert(clicks==1);
    io.AddMousePosEvent(50,140); frame();
    io.AddMouseWheelEvent(0,-4); frame(); frame();
    assert(scroll_y>0);
    const auto pixel=[&](int x,int y) { return renderer.pixels()[static_cast<std::size_t>(y*640+x)]; };
    assert(pixel(260,14)==0x00FF0000); // Vulkan blended UI triangles, not CPU-painted bitmap.
    assert(pixel(260,90)==0x0000FF00);
    assert(pixel(280,90)==0x00272729); // GPU scissor clips the oversized green rectangle.
    assert(pixel(500,20)==0x00272729); // Scene viewport does not leak into the UI area.
    if (renderer.antialiasing_samples()>1) {
        int resolved_edge_pixels=0;
        for (int y=40;y<101;++y) for (int x=450;x<551;++x)
            if (pixel(x,y)!=0x00272729 && pixel(x,y)!=0x000000FF) ++resolved_edge_pixels;
        assert(resolved_edge_pixels>0); // Do not assume vendor-specific sample locations.
    }
    int text_pixels=0,partial_coverage=0;
    for (int y=40;y<65;++y) for (int x=250;x<425;++x) {
        const auto p=pixel(x,y);
        if (p!=0x00272729) { ++text_pixels; if (p!=0x00FFFFFF) ++partial_coverage; }
    }
    assert(text_pixels>50 && partial_coverage>10); // Font texture sampling/alpha antialiasing.
    rejected=false;
    try { renderer.render(scene,camera,0,{630,0,50,50}); } catch (const std::invalid_argument&) { rejected=true; }
    assert(rejected);
    rejected=false;
    try { renderer.begin_ui_frame(std::numeric_limits<float>::quiet_NaN()); }
    catch (const std::invalid_argument&) { rejected=true; }
    assert(rejected);
    // Target reallocation while the font texture and UI backend remain alive.
    renderer.resize(800,600); frame();
    assert(renderer.pixels().size()==800*600);
    std::cout<<"Vulkan UI: "<<renderer.device_name()<<", "<<renderer.antialiasing_samples()<<"x MSAA, "
        <<text_pixels<<" glyph pixels, "<<partial_coverage<<" alpha-AA pixels\n";
    renderer.shutdown(); renderer.shutdown();
    renderer.resize(640,480); renderer.enable_ui(); frame();
    renderer.shutdown();
}
