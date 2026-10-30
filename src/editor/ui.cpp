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
void tool_button(const char* label,int mode,const char* hint) {
    auto& g=app();
    if (ImGui::RadioButton(label,g.gizmo_mode==mode)) {
        g.gizmo_mode=mode;
        g.drag_axis=-1;
    }
    ImGui::SetItemTooltip("%s",hint);
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
    ImGui::PushStyleColor(ImGuiCol_Button,{0.38F,0.12F,0.14F,1});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{0.62F,0.18F,0.21F,1});
    if (ImGui::Button("Usuń obiekt",{-1,0})) {
        (void)g.scene.remove_entity(g.selected); g.selected=0; g.name_target=0;
        g.status=L"Usunięto obiekt";
    }
    ImGui::PopStyleColor(2);
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
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored({0.32F,0.85F,0.80F,1},"STARTER / STUDIO");
    ImGui::SameLine(185);
    if (ImGui::Button("Zapisz  Ctrl+S")) save_scene();
    ImGui::SetItemTooltip("Zapisz bieżącą scenę do scene.txt");
    ImGui::SameLine();
    if (ImGui::Button("Reset kamery")) { g.orbit={}; g.orbit.dragging=false; }
    ImGui::SetItemTooltip("Przywróć domyślne ustawienie kamery");
    ImGui::SameLine(); ImGui::TextDisabled("|"); ImGui::SameLine();
    tool_button("Ruch Q",0,"Przesuwanie zaznaczonego obiektu (Q)");
    ImGui::SameLine(); tool_button("Obrót W",1,"Obracanie zaznaczonego obiektu (W)");
    ImGui::SameLine(); tool_button("Skala E",2,"Skalowanie zaznaczonego obiektu (E)");
    if (ImGui::GetContentRegionAvail().x>220) {
        ImGui::SameLine();
        ImGui::TextDisabled("Vulkan  |  %u× MSAA",g.renderer.antialiasing_samples());
    }
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
        if (ImGui::BeginTabItem("Ustawienia grafiki")) {
            ImGui::TextDisabled("WYŚWIETLANIE");
            bool unlimited=g.fps_limit==0;
            if (ImGui::Checkbox("Bez limitu FPS",&unlimited)) {
                g.fps_limit=unlimited?0:60;
                g.next_frame={}; // Apply the new mode without waiting for the old deadline.
                g.status=unlimited?L"Limit FPS: wyłączony":L"Limit FPS: 60";
            }
            if (!unlimited) {
                ImGui::SetNextItemWidth(240);
                if (ImGui::SliderInt("Limit FPS",&g.fps_limit,15,360,"%d FPS")) {
                    g.next_frame={};
                    g.status=L"Zmieniono limit FPS";
                }
                ImGui::SameLine();
                const int presets[]{30,60,120,144,240};
                for (const int preset:presets) {
                    ImGui::PushID(preset);
                    if (ImGui::SmallButton(std::to_string(preset).c_str())) {
                        g.fps_limit=preset; g.next_frame={};
                    }
                    ImGui::PopID(); ImGui::SameLine();
                }
                ImGui::NewLine();
            } else {
                ImGui::TextDisabled("Renderer tworzy klatki tak szybko, jak pozwala CPU/GPU.");
            }
            ImGui::Separator();
            ImGui::TextDisabled("WIDOK 3D");
            ImGui::Checkbox("Pokaż siatkę",&g.grid_visible);
            ImGui::SetNextItemWidth(240);
            ImGui::SliderFloat("Pole widzenia",&g.camera.vertical_fov_degrees,30.0F,100.0F,"%.0f°");
            float background[]{g.background_color.x,g.background_color.y,g.background_color.z};
            if (ImGui::ColorEdit3("Kolor tła",background,
                ImGuiColorEditFlags_NoInputs|ImGuiColorEditFlags_DisplayRGB))
                g.background_color={background[0],background[1],background[2]};
            ImGui::TextDisabled("MSAA: %u× (wybierane automatycznie dla GPU)",g.renderer.antialiasing_samples());
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{10,4});
    panel("status",0,height-kFooterHeight,width,kFooterHeight,ImGuiWindowFlags_NoScrollbar);
    const auto status=utf8(g.status);
    ImGui::TextColored({0.35F,0.82F,0.58F,1},"●");
    ImGui::SameLine();
    if (g.fps_limit==0)
        ImGui::TextDisabled("%s  |  %zu obiektów  |  %.1f FPS  |  bez limitu",status.c_str(),
            g.scene.entity_count(),static_cast<double>(ImGui::GetIO().Framerate));
    else
        ImGui::TextDisabled("%s  |  %zu obiektów  |  %.1f / %d FPS",status.c_str(),
            g.scene.entity_count(),static_cast<double>(ImGui::GetIO().Framerate),g.fps_limit);
    ImGui::End(); ImGui::PopStyleVar();
    // A focused hierarchy is not a textbox. Keep editor shortcuts available
    // there, but never delete/spawn while typing, dragging a field or in a popup.
    const auto popup_flags=ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel;
    if (!ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() && !ImGui::IsPopupOpen(nullptr,popup_flags)) {
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S,false)) save_scene();
        else if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D,false)) duplicate_entity();
        else if (!ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F,false)) frame_selected();
        else if (!ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Q,false)) g.gizmo_mode=0;
        else if (!ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_W,false)) g.gizmo_mode=1;
        else if (!ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_E,false)) g.gizmo_mode=2;
        else if (!ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N,false)) add_primitive(engine::PrimitiveType::Cube);
        else if (ImGui::IsKeyPressed(ImGuiKey_Delete,false) && g.selected) {
            (void)g.scene.remove_entity(g.selected); g.selected=0; g.name_target=0;
            g.status=L"Usunięto obiekt";
        }
        if (!ImGui::GetIO().WantCaptureKeyboard) {
            if (auto* entity=g.scene.find_entity(g.selected)) {
                if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) entity->transform.position.x-=1;
                if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) entity->transform.position.x+=1;
                if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) entity->transform.position.y+=1;
                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) entity->transform.position.y-=1;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd))
                g.orbit.distance=std::max(2.0F,g.orbit.distance-1.0F);
            if (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract))
                g.orbit.distance=std::min(40.0F,g.orbit.distance+1.0F);
        }
    }
}
} // namespace editor
