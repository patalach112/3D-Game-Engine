#include "editor_app.hpp"
#include "dock_layout.hpp"
#include "engine/log.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace editor {
namespace {
constexpr ImGuiWindowFlags fixed=ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize
    |ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings
    |ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoDocking;
void sidebar(const char* id,ImGuiDir direction,float height,ImGuiWindowFlags extra=0) {
    ImGui::BeginViewportSideBar(id,ImGui::GetMainViewport(),direction,height,fixed|extra);
    // App chrome stays above floating panels without stealing keyboard focus.
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
}
void heading(const char* text) {
    ImGui::TextColored({0.32F,0.85F,0.80F,1},"%s",text);
    ImGui::Separator(); ImGui::Spacing();
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
void add_menu_contents();
void hierarchy() {
    auto& g=app();
    heading("HIERARCHIA");
    ImGui::TextDisabled("%zu obiektów w scenie",g.scene.entity_count());
    ImGui::Spacing();
    if (ImGui::Button("+ Dodaj obiekt",{-1,34})) ImGui::OpenPopup("create");
    if (ImGui::BeginPopup("create")) {
        add_menu_contents();
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
void set_tool(int mode) {
    auto& g=app();
    if (g.gizmo_mode!=mode) { g.gizmo_mode=mode; g.drag_axis=-1; }
}
void add_menu_contents() {
    if (ImGui::BeginMenu("Obiekty")) {
        if (ImGui::MenuItem("Kostka")) add_primitive(engine::PrimitiveType::Cube);
        if (ImGui::MenuItem("Kula")) add_primitive(engine::PrimitiveType::Sphere);
        if (ImGui::MenuItem("Walec")) add_primitive(engine::PrimitiveType::Cylinder);
        if (ImGui::MenuItem("Płaszczyzna 3D")) add_primitive(engine::PrimitiveType::Plane);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Płaszczyzna")) {
        if (ImGui::MenuItem("Koło 2D")) add_primitive(engine::PrimitiveType::Circle2D);
        if (ImGui::MenuItem("Kwadrat 2D")) add_primitive(engine::PrimitiveType::Square2D);
        ImGui::EndMenu();
    }
}
void title_bar() {
    auto& g=app();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{9,6});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{8,0});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{4,4});
    sidebar("##StudioTitle",ImGuiDir_Up,40,
          ImGuiWindowFlags_MenuBar|ImGuiWindowFlags_NoScrollbar);
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Plik")) {
            ImGui::MenuItem("Import",nullptr,false,false);
            ImGui::MenuItem("Eksport",nullptr,false,false);
            ImGui::Separator();
            if (ImGui::MenuItem("Zapisz scenę","Ctrl+S")) save_scene();
            if (ImGui::MenuItem("Ustawienia")) {
                g.show_settings=true; g.focus_settings=true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Zamknij")) PostMessageW(g.hwnd,WM_CLOSE,0,0);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Narzędzia")) {
            if (ImGui::MenuItem("Przesuwanie","Q",g.gizmo_mode==0)) set_tool(0);
            if (ImGui::MenuItem("Obracanie","W",g.gizmo_mode==1)) set_tool(1);
            if (ImGui::MenuItem("Skalowanie","E",g.gizmo_mode==2)) set_tool(2);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset kamery")) g.orbit={};
            if (ImGui::MenuItem("Wycentruj zaznaczony","F")) frame_selected();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Dodaj")) { add_menu_contents(); ImGui::EndMenu(); }
        if (ImGui::BeginMenu("Widok")) {
            ImGui::MenuItem(layout::scene,nullptr,&g.show_scene);
            ImGui::MenuItem(layout::hierarchy,nullptr,&g.show_hierarchy);
            ImGui::MenuItem(layout::inspector,nullptr,&g.show_inspector);
            ImGui::MenuItem(layout::console,nullptr,&g.show_console);
            ImGui::MenuItem(layout::resources,nullptr,&g.show_resources);
            ImGui::MenuItem(layout::notes,nullptr,&g.show_notes);
            ImGui::MenuItem(layout::renderer,nullptr,&g.show_renderer);
            ImGui::Separator();
            if (ImGui::MenuItem("Przywróć układ")) {
                g.reset_layout=true;
                g.show_scene=g.show_hierarchy=g.show_inspector=g.show_console=true;
                g.show_resources=g.show_notes=g.show_renderer=true;
            }
            ImGui::EndMenu();
        }
        ImGui::TextDisabled("  STUDIO / VULKAN");
        // Only this blank strip becomes HTCAPTION. Native messages must not
        // inspect ImGui while the GPU worker owns its context.
        g.title_drag_bounds={static_cast<LONG>(std::ceil(ImGui::GetCursorScreenPos().x)),
            0,std::max(0,g.width-146),40};
        ImGui::SetCursorPosX(static_cast<float>(g.width-144));
        if (ImGui::Button("-##minimize",{44,28})) PostMessageW(g.hwnd,WM_SYSCOMMAND,SC_MINIMIZE,0);
        if (ImGui::Button(IsZoomed(g.hwnd)?"<>##restore":"[]##maximize",{44,28}))
            PostMessageW(g.hwnd,WM_SYSCOMMAND,IsZoomed(g.hwnd)?SC_RESTORE:SC_MAXIMIZE,0);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{0.75F,0.18F,0.22F,1});
        if (ImGui::Button("X##close",{44,28})) PostMessageW(g.hwnd,WM_CLOSE,0,0);
        ImGui::PopStyleColor();
        ImGui::EndMenuBar();
    }
    ImGui::End(); ImGui::PopStyleVar(3);
}
void clear_console() {
    auto& g=app();
    g.console_clear_sequence=g.console_loaded_sequence=engine::log_latest_sequence();
    g.console_rows.clear();
}
void console_command() {
    auto& g=app();
    const std::string command=g.console_command;
    LOG_INFO("console", "> %s",command.c_str());
    if (command=="clear") clear_console();
    else if (command=="help") LOG_INFO("console", "help | clear | stats | grid on | grid off | camera reset");
    else if (command=="stats") LOG_INFO("console", "%zu objects | %.1f FPS | GPU: %s",
        g.scene.entity_count(),static_cast<double>(ImGui::GetIO().Framerate),g.renderer.device_name().c_str());
    else if (command=="grid on") g.grid_visible=true;
    else if (command=="grid off") g.grid_visible=false;
    else if (command=="camera reset") g.orbit={};
    else LOG_WARN("console", "Nieznana komenda. Wpisz help.");
    g.console_command[0]='\0';
}
void console() {
    auto& g=app();
    if (ImGui::Button("Wyczyść")) clear_console();
    ImGui::SameLine(); const bool copy=ImGui::Button("Kopiuj");
    ImGui::SameLine(); ImGui::Checkbox("Auto-scroll",&g.console_auto_scroll);
    const char* names[]{"DEBUG","INFO","WARN","ERROR"};
    const ImVec4 colors[]{{0.55F,0.6F,0.65F,1},{0.7F,0.85F,0.85F,1},
        {1,0.75F,0.3F,1},{1,0.35F,0.35F,1}};
    for (int i=0;i<4;++i) {
        if (i) ImGui::SameLine();
        ImGui::Checkbox(names[i],&g.console_levels[i]);
    }
    g.console_filter.Draw("Filtr",-1);
    if (g.console_loaded_sequence<g.console_clear_sequence) {
        g.console_rows.clear(); g.console_loaded_sequence=g.console_clear_sequence;
    }
    // Copy/format only newly arrived entries; never re-copy megabytes of logs
    // on every uncapped frame. Both the engine history and UI cache are bounded.
    for (const auto& entry:engine::log_snapshot(g.console_loaded_sequence)) {
        g.console_loaded_sequence=entry.sequence;
        const int level=static_cast<int>(entry.level);
        if (level<0 || level>3) continue;
        std::string line="["+entry.time+"] ["+names[level]+"] ["+entry.tag+"] "+entry.message;
        // One physical row per log entry keeps the clipper correct.
        for (auto& ch:line) if (ch=='\n' || ch=='\r') ch=' ';
        if (g.console_rows.size()==engine::kLogHistoryCapacity) g.console_rows.pop_front();
        g.console_rows.push_back({std::move(line),level});
    }
    const auto latest=g.console_loaded_sequence;
    std::vector<const App::ConsoleRow*> rows;
    std::string clipboard;
    for (const auto& row:g.console_rows) {
        if (!g.console_levels[row.level] || !g.console_filter.PassFilter(row.text.c_str())) continue;
        if (copy) clipboard+=row.text+'\n';
        rows.push_back(&row);
    }
    if (copy) ImGui::SetClipboardText(clipboard.c_str());
    const float reserve=ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("##LogHistory",{0,std::max(1.0F,ImGui::GetContentRegionAvail().y-reserve)},
        ImGuiChildFlags_Borders,ImGuiWindowFlags_HorizontalScrollbar);
    const bool at_bottom=ImGui::GetScrollY()>=ImGui::GetScrollMaxY()-1;
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(rows.size()));
    while (clipper.Step()) for (int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i) {
        ImGui::PushStyleColor(ImGuiCol_Text,colors[rows[i]->level]);
        ImGui::TextUnformatted(rows[i]->text.c_str()); ImGui::PopStyleColor();
    }
    if (g.console_auto_scroll && at_bottom && latest>g.console_seen_sequence) ImGui::SetScrollHereY(1);
    g.console_seen_sequence=latest;
    ImGui::EndChild();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##ConsoleCommand","Komenda (help)",g.console_command,
        sizeof(g.console_command),ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (g.console_command[0]) console_command();
        ImGui::SetKeyboardFocusHere(-1);
    }
}
void scene_window() {
    auto& g=app();
    g.viewport_bounds={}; g.viewport_visible=false; g.scene_drawlist=nullptr;
    if (!g.show_scene) return;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    if (ImGui::Begin(layout::scene,&g.show_scene,ImGuiWindowFlags_NoBackground
        |ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse)) {
        g.scene_floating=!ImGui::IsWindowDocked();
        g.scene_drawlist=ImGui::GetWindowDrawList();
        const auto origin=ImGui::GetCursorScreenPos();
        const auto available=ImGui::GetContentRegionAvail();
        const float toolbar_height=ImGui::GetFrameHeight()+12;
        ImGui::GetWindowDrawList()->AddRectFilled(origin,{origin.x+available.x,origin.y+toolbar_height},
            ImGui::GetColorU32(ImGuiCol_WindowBg));
        ImGui::SetCursorScreenPos({origin.x+8,origin.y+6});
        if (ImGui::RadioButton("Ruch",g.gizmo_mode==0)) set_tool(0);
        ImGui::SameLine(); if (ImGui::RadioButton("Obrót",g.gizmo_mode==1)) set_tool(1);
        ImGui::SameLine(); if (ImGui::RadioButton("Skala",g.gizmo_mode==2)) set_tool(2);
        ImGui::SetCursorScreenPos({origin.x,origin.y+toolbar_height});
        const auto size=ImGui::GetContentRegionAvail();
        const auto position=ImGui::GetCursorScreenPos();
        g.viewport_bounds={std::clamp(static_cast<LONG>(std::ceil(position.x)),0L,static_cast<LONG>(g.width)),
            std::clamp(static_cast<LONG>(std::ceil(position.y)),0L,static_cast<LONG>(g.height)),
            std::clamp(static_cast<LONG>(std::floor(position.x+size.x)),0L,static_cast<LONG>(g.width)),
            std::clamp(static_cast<LONG>(std::floor(position.y+size.y)),0L,static_cast<LONG>(g.height))};
        g.viewport_visible=g.viewport_bounds.right>g.viewport_bounds.left && g.viewport_bounds.bottom>g.viewport_bounds.top;
        if (size.x>0 && size.y>0) ImGui::Dummy(size);
    }
    ImGui::End(); ImGui::PopStyleVar();
}

void resources() {

    ImGui::TextDisabled("PRYMITYWY / kliknij, aby dodać do sceny");
    const engine::PrimitiveType types[]{engine::PrimitiveType::Cube,engine::PrimitiveType::Sphere,
        engine::PrimitiveType::Cylinder,engine::PrimitiveType::Plane,
        engine::PrimitiveType::Circle2D,engine::PrimitiveType::Square2D};
    const char* labels[]{"Kostka","Kula","Walec","Płaszczyzna","Koło 2D","Kwadrat 2D"};
    const int columns=std::max(1,static_cast<int>((ImGui::GetContentRegionAvail().x+8)/108));
    for (int i=0;i<6;++i) {
        if (i%columns) ImGui::SameLine();
        if (ImGui::Button(labels[i],{100,42})) add_primitive(types[i]);
    }
}

void notes() {
    auto& g=app();

    if (ImGui::Button("Zapisz do project_notes.txt")) {
        std::ofstream file("project_notes.txt");
        if (file) { file<<g.notes; file.flush(); }
        g.status=file?L"Zapisano notatki":L"Błąd zapisu notatek";
        if (file) LOG_INFO("editor","Zapisano project_notes.txt");
        else LOG_ERROR("editor","Błąd zapisu project_notes.txt");
    }
    ImGui::InputTextMultiline("##notes",g.notes,sizeof(g.notes),{ -1,-1 },ImGuiInputTextFlags_AllowTabInput);
}

void renderer() {
    auto& g=app();

    ImGui::TextWrapped("GPU: %s",g.renderer.device_name().c_str());
    ImGui::TextWrapped("Cały obszar klienta: jeden render pass Vulkan. UI: trójkąty, scissor i atlas fontu. MSAA: %u×. Tekst: filtrowany atlas + pokrycie alpha. Bez bitmap GDI i bez odczytu pikseli na CPU.",g.renderer.antialiasing_samples());
}

void settings() {
    auto& g=app();

    ImGui::TextDisabled("WYŚWIETLANIE");
    bool unlimited=g.fps_limit==0;
    if (ImGui::Checkbox("Unlimited FPS",&unlimited)) {
        g.fps_limit=unlimited?0:60;
        g.next_frame={}; // Apply the new mode without waiting for the old deadline.
        g.status=unlimited?L"Limit FPS: unlimited":L"Limit FPS: 60";
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
}

} // namespace
bool scene_input_at(int x,int y) {
    auto& g=app();
    const POINT point{x,y};
    if (!g.viewport_visible || !PtInRect(&g.viewport_bounds,point)) return false;
    ImGuiWindow* hovered{};
    ImGui::FindHoveredWindowEx({static_cast<float>(x),static_cast<float>(y)},false,&hovered,nullptr);
    return hovered==ImGui::FindWindowByName(layout::scene);
}
void draw_ui() {
    auto& g=app();
    const float width=static_cast<float>(g.width),height=static_cast<float>(g.height);
    title_bar();
    layout::dockspace({0,40},{width,std::max(1.0F,height-40-kFooterHeight)},g.reset_layout);
    g.reset_layout=false; g.docking_initialized=true;
    scene_window();
    if (!g.viewport_visible) { g.drag_axis=-1; g.orbit.dragging=false; }
    const auto window=[](const char* name,bool& open,void (*contents)()) {
        if (!open) return;
        if (ImGui::Begin(name,&open,ImGuiWindowFlags_NoFocusOnAppearing)) contents();
        ImGui::End();
    };
    window(layout::hierarchy,g.show_hierarchy,hierarchy);
    window(layout::inspector,g.show_inspector,inspector);
    window(layout::console,g.show_console,console);
    window(layout::resources,g.show_resources,resources);
    window(layout::notes,g.show_notes,notes);
    window(layout::renderer,g.show_renderer,renderer);
    if (g.focus_settings) { ImGui::SetNextWindowFocus(); g.focus_settings=false; }
    window(layout::settings,g.show_settings,settings);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{10,4});
    sidebar("status",ImGuiDir_Down,kFooterHeight,ImGuiWindowFlags_NoScrollbar);
    const auto status=utf8(g.status);
    ImGui::TextDisabled("%s | %zu obiektów | %.1f FPS | %s",status.c_str(),g.scene.entity_count(),
        static_cast<double>(ImGui::GetIO().Framerate),g.fps_limit==0?"unlimited":"limit FPS");
    ImGui::End(); ImGui::PopStyleVar();
    g.menu_open=ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel);
    // A focused hierarchy is not a textbox. Keep editor shortcuts available
    // there, but never delete/spawn while typing, dragging a field or in a popup.
    const auto popup_flags=ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel;
    if (!ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() && !ImGui::IsPopupOpen(nullptr,popup_flags)) {
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S,false)) save_scene();
        else if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D,false)) duplicate_entity();
        else if (!ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F,false)) frame_selected();
        else if (!ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Q,false)) set_tool(0);
        else if (!ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_W,false)) set_tool(1);
        else if (!ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_E,false)) set_tool(2);
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
