#pragma once
#include "engine/ui.hpp"
#include <imgui_internal.h>
#include <initializer_list>

// OS-independent so the exact editor dock layout can also be tested headlessly.
namespace editor::layout {
inline constexpr auto scene="Scena 3D";
inline constexpr auto hierarchy="Hierarchia";
inline constexpr auto inspector="Inspektor";
inline constexpr auto console="Konsola";
inline constexpr auto resources="Zasoby";
inline constexpr auto notes="Notatki";
inline constexpr auto renderer="Renderer";
inline constexpr auto settings="Ustawienia";

inline ImGuiID dockspace(ImVec2 position, ImVec2 size, bool reset=false) {
    const ImGuiID id=ImHashStr("StudioDockspace");
    if (reset || !ImGui::DockBuilderGetNode(id)) {
        ImGui::DockBuilderRemoveNode(id);
        ImGui::DockBuilderAddNode(id,ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodePos(id,position);
        ImGui::DockBuilderSetNodeSize(id,size);
        ImGuiID center=id,left{},right{},bottom{};
        ImGui::DockBuilderSplitNode(center,ImGuiDir_Left,0.19F,&left,&center);
        ImGui::DockBuilderSplitNode(center,ImGuiDir_Right,0.27F,&right,&center);
        ImGui::DockBuilderSplitNode(center,ImGuiDir_Down,0.30F,&bottom,&center);
        ImGui::DockBuilderDockWindow(scene,center);
        ImGui::DockBuilderDockWindow(hierarchy,left);
        ImGui::DockBuilderDockWindow(inspector,right);
        for (auto name : {console,resources,notes,renderer,settings})
            ImGui::DockBuilderDockWindow(name,bottom);
        ImGui::DockBuilderGetNode(bottom)->SelectedTabId=ImHashStr("#TAB",0,ImHashStr(console));
        ImGui::DockBuilderFinish(id);
    }
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    const auto flags=ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize
        |ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoDocking
        |ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoNavFocus
        |ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("##StudioDockHost",nullptr,flags);
    ImGui::PopStyleVar(2);
    // No PassthruCentralNode: with a docked scene it paints an opaque rectangle
    // across the root. The host AND scene are explicitly NoBackground instead.
    ImGui::DockSpace(id,{0,0});
    ImGui::End();
    return id;
}
}

namespace editor::layout {
// The scene is drawn directly by Vulkan before the UI, not copied through a CPU
// bitmap. An undocked transparent scene must punch through UI windows BEHIND it,
// while windows/popups in front keep their usual z-order. Split only scissors:
// vertices/indices, texture IDs, callbacks and GPU ownership remain unchanged.
inline void cutout_behind_scene(ImDrawData* data,ImDrawList* scene_list,ImVec4 hole) {
    if (!data || !scene_list || hole.z<=hole.x || hole.w<=hole.y) return;
    int scene_index=-1;
    for (int i=0;i<data->CmdListsCount;++i) if (data->CmdLists[i]==scene_list) { scene_index=i; break; }
    if (scene_index<0) return;
    for (int i=0;i<scene_index;++i) {
        auto* list=data->CmdLists[i];
        ImVector<ImDrawCmd> commands;
        for (const auto& original:list->CmdBuffer) {
            const auto r=original.ClipRect;
            if (original.UserCallback || r.z<=hole.x || r.x>=hole.z || r.w<=hole.y || r.y>=hole.w) {
                commands.push_back(original); continue;
            }
            const auto append=[&](float x0,float y0,float x1,float y1) {
                if (x1<=x0 || y1<=y0) return;
                auto cmd=original; cmd.ClipRect={x0,y0,x1,y1}; commands.push_back(cmd);
            };
            const float y0=ImMax(r.y,hole.y),y1=ImMin(r.w,hole.w);
            append(r.x,r.y,r.z,ImMin(r.w,hole.y));
            append(r.x,ImMax(r.y,hole.w),r.z,r.w);
            append(r.x,y0,ImMin(r.z,hole.x),y1);
            append(ImMax(r.x,hole.z),y0,r.z,y1);
        }
        list->CmdBuffer.swap(commands);
    }
}
}
