#include "../src/editor/dock_layout.hpp"
#include <cassert>
#include <cmath>
#include <string>
namespace {
void context() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io=ImGui::GetIO();
    io.IniFilename=nullptr;
    io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;
    io.DisplaySize={1440,900};
    io.Fonts->AddFontDefault();
    io.Fonts->Build();
}
float side(ImVec2 a,ImVec2 b,ImVec2 p) { return (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x); }
bool covered(ImVec2 point) {
    const auto* data=ImGui::GetDrawData();
    for (const auto* list:data->CmdLists) for (const auto& cmd:list->CmdBuffer) {
        if (cmd.UserCallback || point.x<cmd.ClipRect.x || point.y<cmd.ClipRect.y
            || point.x>=cmd.ClipRect.z || point.y>=cmd.ClipRect.w) continue;
        for (unsigned i=0;i+2<cmd.ElemCount;i+=3) {
            const auto& a=list->VtxBuffer[cmd.VtxOffset+list->IdxBuffer[cmd.IdxOffset+i]];
            const auto& b=list->VtxBuffer[cmd.VtxOffset+list->IdxBuffer[cmd.IdxOffset+i+1]];
            const auto& c=list->VtxBuffer[cmd.VtxOffset+list->IdxBuffer[cmd.IdxOffset+i+2]];
            if (!(a.col&IM_COL32_A_MASK) || !(b.col&IM_COL32_A_MASK) || !(c.col&IM_COL32_A_MASK)) continue;
            const float x=side(a.pos,b.pos,point),y=side(b.pos,c.pos,point),z=side(c.pos,a.pos,point);
            if (std::abs(side(a.pos,b.pos,c.pos))>0.001F && ((x>=0&&y>=0&&z>=0)||(x<=0&&y<=0&&z<=0))) return true;
        }
    }
    return false;
}
void frame(bool reset=false) {
    ImGui::NewFrame();
    editor::layout::dockspace({0,40},{1440,832},reset);
    for (auto name:{editor::layout::scene,editor::layout::hierarchy,editor::layout::inspector,
            editor::layout::console,editor::layout::resources,editor::layout::notes,editor::layout::renderer}) {
        ImGui::Begin(name,nullptr,ImGuiWindowFlags_NoFocusOnAppearing|(name==editor::layout::scene?ImGuiWindowFlags_NoBackground:0));
        ImGui::End();
    }
    ImGui::Render();
}
}
int main() {
    context();
    for (int i=0;i<5;++i) frame();
    auto* scene=ImGui::FindWindowByName(editor::layout::scene);
    auto* console=ImGui::FindWindowByName(editor::layout::console);
    auto* hierarchy=ImGui::FindWindowByName(editor::layout::hierarchy);
    auto* inspector=ImGui::FindWindowByName(editor::layout::inspector);
    assert(scene->DockId && scene->DockTabIsVisible && console->DockTabIsVisible);
    assert(hierarchy->Pos.x<scene->Pos.x && inspector->Pos.x>scene->Pos.x);
    assert(console->Pos.y>scene->Pos.y);
    const ImVec2 center{scene->Pos.x+scene->Size.x/2,scene->Pos.y+scene->Size.y/2};
    assert(!covered(center)); // Do not paint over the directly rendered Vulkan scene.
    const auto changed_dock=hierarchy->DockId;
    ImGui::DockBuilderDockWindow(editor::layout::console,changed_dock);
    for (int i=0;i<3;++i) frame();
    assert(console->DockId==changed_dock); // Submitting the root does not reset user docking.
    const std::string saved=ImGui::SaveIniSettingsToMemory();
    ImGui::DestroyContext();
    context();
    ImGui::LoadIniSettingsFromMemory(saved.c_str());
    for (int i=0;i<4;++i) frame();
    assert(ImGui::FindWindowByName(editor::layout::console)->DockId==changed_dock);
    frame(true); frame(); frame();
    assert(ImGui::FindWindowByName(editor::layout::console)->DockId!=changed_dock);
    assert(!(ImGui::GetIO().ConfigFlags&ImGuiConfigFlags_ViewportsEnable));
    ImGui::DestroyContext();
}
