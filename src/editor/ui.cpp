#include "editor_app.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace editor {
namespace {
constexpr ImGuiWindowFlags fixed=ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize
    |ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings
    |ImGuiWindowFlags_NoBringToFrontOnFocus;
void panel(const char* id,float x,float y,float width,float height,ImGuiWindowFlags extra=0) {
    ImGui::SetNextWindowPos({x,y});
    ImGui::SetNextWindowSize({std::max(1.0F,width),std::max(1.0F,height)});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize,{1,1});
    ImGui::Begin(id,nullptr,fixed|extra);
    ImGui::PopStyleVar();
}
void heading(const char* text) {
    ImGui::TextColored({0.32F,0.85F,0.80F,1},"%s",text);
    ImGui::Separator(); ImGui::Spacing();
}
void splitter(const char* id,float x,float y,float w,float h,int& value,bool reverse) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    panel(id,x,y,w,h,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoNav);
    ImGui::InvisibleButton("##split",{w,h});
    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
        ImGui::SetMouseCursor(w<h?ImGuiMouseCursor_ResizeEW:ImGuiMouseCursor_ResizeNS);
        ImGui::GetWindowDrawList()->AddRectFilled({x,y},{x+w,y+h},IM_COL32(65,185,178,160));
    }
    if (ImGui::IsItemActive()) {
        const auto delta=ImGui::GetIO().MouseDelta;
        value+=static_cast<int>((w<h?delta.x:delta.y)*(reverse?-1:1));
    }
    ImGui::End(); ImGui::PopStyleVar();
}
std::string utf8(const std::wstring& value) {
    const int count=WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);
    std::string result(static_cast<std::size_t>(std::max(0,count)),'\0');
    if (count) WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),result.data(),count,nullptr,nullptr);
    return result;
}
void vector_input(const char* label,engine::Vec3& vector,float speed) {
    float values[]{vector.x,vector.y,vector.z};
    ImGui::TextUnformatted(label);
    ImGui::SameLine(); ImGui::TextDisabled("X / Y / Z");
    ImGui::PushID(label); ImGui::SetNextItemWidth(-1);
    if (ImGui::DragFloat3("##value",values,speed,0,0,"%.3f")
        && std::isfinite(values[0]) && std::isfinite(values[1]) && std::isfinite(values[2]))
        vector={values[0],values[1],values[2]};
    ImGui::PopID();
}
void hierarchy() {
    auto& g=app();
    heading("HIERARCHIA");
    ImGui::TextDisabled("%zu obiektów w scenie",g.scene.entity_count());
    ImGui::Spacing();
    if (ImGui::Button("+ Dodaj obiekt",{-1,34})) ImGui::OpenPopup("create");
    if (ImGui::BeginPopup("create")) {
        if (ImGui::MenuItem("Kostka")) add_primitive(engine::PrimitiveType::Cube);
        if (ImGui::MenuItem("Kula")) add_primitive(engine::PrimitiveType::Sphere);
        if (ImGui::MenuItem("Walec")) add_primitive(engine::PrimitiveType::Cylinder);
        if (ImGui::MenuItem("Płaszczyzna")) add_primitive(engine::PrimitiveType::Plane);
        ImGui::EndPopup();
    }
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##search","Szukaj obiektu…",g.search,sizeof(g.search));
    ImGui::Spacing();
    ImGui::BeginChild("objects",{0,0},ImGuiChildFlags_Borders,ImGuiWindowFlags_AlwaysVerticalScrollbar);
    for (const auto& entity:g.scene.entities()) {
        if (g.search[0] && entity.name.find(g.search)==std::string::npos) continue;
        ImGui::PushID(reinterpret_cast<void*>(static_cast<std::uintptr_t>(entity.id)));
        const auto pos=ImGui::GetCursorScreenPos();
        if (ImGui::Selectable("##entity",entity.id==g.selected,0,{0,28})) g.selected=entity.id;
        ImGui::SetItemTooltip("%s",entity.name.c_str());
        // Raw text: an object name containing ## must not become a widget ID.
        ImGui::GetWindowDrawList()->AddRectFilled({pos.x+3,pos.y+10},{pos.x+11,pos.y+18},IM_COL32(82,187,179,255),2);
        ImGui::GetWindowDrawList()->AddText({pos.x+19,pos.y+(28-ImGui::GetFontSize())/2},
            ImGui::GetColorU32(ImGuiCol_Text),entity.name.c_str());
        ImGui::PopID();
    }
    ImGui::EndChild();
}
void inspector() {
    auto& g=app();
    heading("INSPEKTOR");
    auto* entity=g.scene.find_entity(g.selected);
    if (!entity) { ImGui::TextWrapped("Wybierz obiekt w hierarchii lub w widoku 3D."); return; }
    if (g.name_target!=entity->id) {
        std::snprintf(g.entity_name,sizeof(g.entity_name),"%s",entity->name.c_str());
        g.name_target=entity->id;
    }
    ImGui::TextDisabled("OBIEKT / %llu",static_cast<unsigned long long>(entity->id));
    ImGui::SetNextItemWidth(-1);
    const bool enter=ImGui::InputText("##name",g.entity_name,sizeof(g.entity_name),ImGuiInputTextFlags_EnterReturnsTrue);
    if (enter || ImGui::IsItemDeactivatedAfterEdit()) {
        if (g.entity_name[0]) entity->name=g.entity_name;
        else {
            g.status=L"Nazwa obiektu nie może być pusta";
            std::snprintf(g.entity_name,sizeof(g.entity_name),"%s",entity->name.c_str());
        }
    }
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Transformacja",ImGuiTreeNodeFlags_DefaultOpen)) {
        vector_input("Pozycja",entity->transform.position,0.025F);
        vector_input("Obrót",entity->transform.rotation,0.25F);
        vector_input("Skala",entity->transform.scale,0.01F);
        ImGui::TextDisabled("Przeciągnij wartość / Ctrl+klik, aby wpisać");
        if (ImGui::Button("Resetuj transformację",{-1,0})) entity->transform={};
    }
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Sterowanie",ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped("PPM + ruch: obrót kamery\nKółko nad 3D: zoom\nLPM: wybór i gizmo\nDelete: usuń obiekt\nCtrl+S: zapisz scenę\nTab: kolejne pole\nCtrl+A/C/V: zaznacz / kopiuj / wklej");
    }
    ImGui::Spacing();
    if (ImGui::Button("Usuń obiekt",{-1,0})) {
        (void)g.scene.remove_entity(g.selected); g.selected=0; g.name_target=0;
    }
}
}
void draw_ui() {
    auto& g=app();
    update_layout();
    const float width=static_cast<float>(g.width), height=static_cast<float>(g.height);
    // Splitters are GPU widgets too; their drag state stays entirely in ImGui.
    splitter("left-split",static_cast<float>(g.left_width),kHeaderHeight,6,
        height-kHeaderHeight-kFooterHeight,g.left_width,false);
    splitter("right-split",width-g.right_width-6,kHeaderHeight,6,
        height-kHeaderHeight-kFooterHeight,g.right_width,true);
    splitter("bottom-split",static_cast<float>(g.left_width+6),height-kFooterHeight-g.bottom_height-6,
        width-g.left_width-g.right_width-12,6,g.bottom_height,true);
    update_layout();
    const float left=static_cast<float>(g.left_width),right=static_cast<float>(g.right_width);
    panel("toolbar",0,0,width,kHeaderHeight,ImGuiWindowFlags_NoScrollbar);
    ImGui::TextColored({0.32F,0.85F,0.80F,1},"STARTER / STUDIO");
    ImGui::SameLine(215);
    if (ImGui::Button("Zapisz scenę")) save_scene();
    ImGui::SameLine();
    if (ImGui::Button("Reset kamery")) { g.yaw=0.75F;g.pitch=0.42F;g.camera_distance=8; }
    ImGui::SameLine();
    ImGui::TextDisabled("VULKAN  |  %u× MSAA  |  Perspektywa",g.renderer.antialiasing_samples());
    ImGui::End();
    panel("hierarchy",0,kHeaderHeight,left,height-kHeaderHeight-kFooterHeight);
    hierarchy(); ImGui::End();
    panel("inspector",width-right,kHeaderHeight,right,height-kHeaderHeight-kFooterHeight,
        ImGuiWindowFlags_AlwaysVerticalScrollbar);
    inspector(); ImGui::End();
    panel("workspace",left+6,height-kFooterHeight-g.bottom_height,
        width-left-right-12,static_cast<float>(g.bottom_height));
    if (ImGui::BeginTabBar("workspace-tabs")) {
        if (ImGui::BeginTabItem("Zasoby")) {
            ImGui::TextDisabled("PRYMITYWY / kliknij, aby dodać do sceny");
            const engine::PrimitiveType types[]{engine::PrimitiveType::Cube,engine::PrimitiveType::Sphere,
                engine::PrimitiveType::Cylinder,engine::PrimitiveType::Plane};
            const char* labels[]{"Kostka","Kula","Walec","Płaszczyzna"};
            const int columns=std::max(1,static_cast<int>((ImGui::GetContentRegionAvail().x+8)/108));
            for (int i=0;i<4;++i) {
                if (i%columns) ImGui::SameLine();
                if (ImGui::Button(labels[i],{100,42})) add_primitive(types[i]);
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Notatki")) {
            if (ImGui::Button("Zapisz do project_notes.txt")) {
                std::ofstream file("project_notes.txt");
                if (file) { file<<g.notes; file.flush(); }
                g.status=file?L"Zapisano notatki":L"Błąd zapisu notatek";
            }
            ImGui::InputTextMultiline("##notes",g.notes,sizeof(g.notes),{ -1,-1 },ImGuiInputTextFlags_AllowTabInput);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Renderer")) {
            ImGui::TextWrapped("GPU: %s",g.renderer.device_name().c_str());
            ImGui::TextWrapped("Cały obszar klienta: jeden render pass Vulkan. UI: trójkąty, scissor i atlas fontu. MSAA: %u×. Tekst: filtrowany atlas + pokrycie alpha. Bez bitmap GDI i bez odczytu pikseli na CPU.",g.renderer.antialiasing_samples());
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{10,4});
    panel("status",0,height-kFooterHeight,width,kFooterHeight,ImGuiWindowFlags_NoScrollbar);
    const auto status=utf8(g.status);
    ImGui::TextDisabled("%s  |  %zu obiektów",status.c_str(),g.scene.entity_count());
    ImGui::End(); ImGui::PopStyleVar();
    // A focused hierarchy is not a textbox. Keep editor shortcuts available
    // there, but never delete/spawn while typing, dragging a field or in a popup.
    const auto popup_flags=ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel;
    if (!ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() && !ImGui::IsPopupOpen(nullptr,popup_flags)) {
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S,false)) save_scene();
        else if (!ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N,false)) add_primitive(engine::PrimitiveType::Cube);
        else if (ImGui::IsKeyPressed(ImGuiKey_Delete,false) && g.selected) {
            (void)g.scene.remove_entity(g.selected); g.selected=0; g.name_target=0;
            g.status=L"Usunięto obiekt";
        }
    }
}
} // namespace editor
