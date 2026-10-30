#pragma once
#include "engine/hud/document.hpp"
#include "imgui.h"

namespace editor {
// Scoped rather than mutating ImGuiStyle: project colours must never leak into
// the designer properties, Settings or immutable native window buttons.
class HudColors {
    int count_{};
public:
    explicit HudColors(const engine::hud::Palette& p,bool enabled=true) {
        if(!enabled)return;
        using R=engine::hud::ColorRole;
        const auto push=[&](ImGuiCol slot,R role,float alpha=1) {
            const auto& c=p[role];ImGui::PushStyleColor(slot,{c[0],c[1],c[2],c[3]*alpha});++count_;
        };
        push(ImGuiCol_WindowBg,R::Background);push(ImGuiCol_ChildBg,R::Panel);
        push(ImGuiCol_PopupBg,R::Background);push(ImGuiCol_MenuBarBg,R::Background);
        push(ImGuiCol_Text,R::Text);push(ImGuiCol_TextDisabled,R::Text,.55F);push(ImGuiCol_Border,R::Border);
        for(auto slot:{ImGuiCol_Button,ImGuiCol_FrameBg,ImGuiCol_Header,ImGuiCol_Tab})push(slot,R::Control);
        for(auto slot:{ImGuiCol_ButtonHovered,ImGuiCol_FrameBgHovered,ImGuiCol_HeaderHovered,ImGuiCol_TabHovered})push(slot,R::Hovered);
        for(auto slot:{ImGuiCol_ButtonActive,ImGuiCol_FrameBgActive,ImGuiCol_HeaderActive,ImGuiCol_TabSelected,ImGuiCol_TabDimmedSelected})push(slot,R::Active);
        for(auto slot:{ImGuiCol_CheckMark,ImGuiCol_SliderGrab,ImGuiCol_SliderGrabActive,ImGuiCol_NavCursor})push(slot,R::Accent);
        push(ImGuiCol_Separator,R::Border);push(ImGuiCol_TextSelectedBg,R::Accent,.6F);
    }
    void pop() {if(count_){ImGui::PopStyleColor(count_);count_=0;}}
    ~HudColors(){pop();}
    HudColors(const HudColors&)=delete;
    HudColors& operator=(const HudColors&)=delete;
};
}
