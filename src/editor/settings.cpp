#include "editor_app.hpp"
#include "settings.hpp"
#include "hud_editor.hpp"
#include "engine/log.hpp"
#include <algorithm>

namespace editor {
namespace {
using namespace engine::hud;
ImGuiKey gui_key(int key) {
    if(key>='A'&&key<='Z')return static_cast<ImGuiKey>(ImGuiKey_A+key-'A');
    if(key>='0'&&key<='9')return static_cast<ImGuiKey>(ImGuiKey_0+key-'0');
    if(key>=300&&key<=323)return static_cast<ImGuiKey>(ImGuiKey_F1+key-300);
    if(key>=400&&key<=404)return static_cast<ImGuiKey>(ImGuiKey_MouseLeft+key-400);
    constexpr ImGuiKey named[]{ImGuiKey_Tab,ImGuiKey_LeftArrow,ImGuiKey_RightArrow,ImGuiKey_UpArrow,ImGuiKey_DownArrow,
        ImGuiKey_PageUp,ImGuiKey_PageDown,ImGuiKey_Home,ImGuiKey_End,ImGuiKey_Insert,ImGuiKey_Delete,ImGuiKey_Backspace,
        ImGuiKey_Space,ImGuiKey_Enter,ImGuiKey_Escape,ImGuiKey_Minus,ImGuiKey_Equal,ImGuiKey_Comma,ImGuiKey_Period,
        ImGuiKey_Slash,ImGuiKey_Semicolon,ImGuiKey_Apostrophe,ImGuiKey_LeftBracket,ImGuiKey_RightBracket,ImGuiKey_Backslash,
        ImGuiKey_GraveAccent,ImGuiKey_KeypadAdd,ImGuiKey_KeypadSubtract,ImGuiKey_KeypadEnter};
    static_assert(static_cast<int>(Key::KeypadEnter)-256+1==std::size(named));
    if(key>=256&&key<256+static_cast<int>(std::size(named)))return named[key-256];
    return ImGuiKey_None;
}
unsigned modifiers() {
    const auto& io=ImGui::GetIO();
    return (io.KeyCtrl?Ctrl:0U)|(io.KeyShift?Shift:0U)|(io.KeyAlt?Alt:0U)|(io.KeySuper?Super:0U);
}
std::vector<Binding>& editable() {return app().hud_designer?app().hud.bindings:app().live_bindings;}
void changed() {
    auto& g=app();g.binding_message.clear();
    if(g.hud_designer){g.hud_dirty=true;return;}
    std::string error;
    if(!save_bindings(g.hud_path.parent_path()/L"editor_bindings.cfg",bindings_fingerprint(g.hud.bindings),g.live_bindings,error)) {
        g.binding_message=error;LOG_ERROR("bindings","%s",error.c_str());
    }
}
const char* action_label(const std::string& id) {for(auto a:actions())if(id==a.id)return a.label;return "Nieznana funkcja";}
void capture() {
    auto& g=app();auto& list=editable();
    if(g.binding_capture<0)return;
    if(static_cast<std::size_t>(g.binding_capture)>=list.size()){g.binding_capture=-1;return;}
    if(ImGui::IsKeyPressed(ImGuiKey_Escape,false)){g.binding_capture=-1;g.binding_consumed_frame=ImGui::GetFrameCount();return;}
    if(!g.binding_capture_armed) {
        if(ImGui::GetFrameCount()>g.binding_capture_frame && !ImGui::IsMouseDown(0))g.binding_capture_armed=true;
        return;
    }
    for(int key=1;key<=404;++key) {
        const auto gui=gui_key(key);if(gui==ImGuiKey_None||!ImGui::IsKeyPressed(gui,false))continue;
        if(key>=400 && ImGui::IsAnyItemActive())continue;
        const auto mods=modifiers();
        bool conflict=false;
        for(std::size_t i=0;i<list.size();++i)if(static_cast<int>(i)!=g.binding_capture && list[i].key==key && list[i].modifiers==mods)conflict=true;
        if(conflict) {g.binding_message="Ten skrót jest już zajęty. Zmień lub usuń istniejącego binda.";return;}
        auto& b=list[static_cast<std::size_t>(g.binding_capture)];b.key=key;b.modifiers=mods;
        g.binding_capture=-1;g.binding_consumed_frame=ImGui::GetFrameCount();g.binding_message.clear();changed();return;
    }
}
void bindings_tab() {
    auto& g=app();auto& list=editable();
    if(ImGui::Button("Przywróć domyślne"))ImGui::OpenPopup("Przywrócić bindy?");
    if(ImGui::BeginPopup("Przywrócić bindy?")) {
        ImGui::TextUnformatted("Zastąpić bieżące przypisania domyślnymi?");
        if(ImGui::Button("Przywróć")){restore_bindings();ImGui::CloseCurrentPopup();}
        ImGui::SameLine();if(ImGui::Button("Anuluj"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
    }
    ImGui::TextWrapped(g.hud_designer?"Edytujesz domyślne bindy projektu. LPM w polu skrótu rozpoczyna przypisywanie. Bindy NIE wykonują funkcji w editor_hud.":
        "LPM w polu skrótu: przypisz klawisz. PPM w tabeli: funkcja, dodaj lub usuń binda. Zmiany zapisują się osobno od HUD-u.");
    if(g.binding_capture>=0) {
        ImGui::TextColored({1,.75F,.2F,1},"Naciśnij klawisz lub przycisk myszy (Esc: anuluj).");
        if(ImGui::Button("Anuluj przypisywanie"))g.binding_capture=-1;
        ImGui::SameLine();if(ImGui::Button("Wyczyść przypisanie")) {
            auto& b=list[static_cast<std::size_t>(g.binding_capture)];b.key=0;b.modifiers=0;g.binding_capture=-1;changed();
        }
    }
    int context_row=-1;bool context_clicked=false;
    if(ImGui::BeginChild("##BindingsArea",{0,-65},ImGuiChildFlags_Borders)) {
        if(ImGui::BeginTable("##Bindings",g.hud_designer?2:3,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerV|ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("Funkcja",ImGuiTableColumnFlags_WidthStretch,1.4F);
            ImGui::TableSetupColumn(g.hud_designer?"Domyślne przypisanie (LPM)":"Przypisanie (LPM)",ImGuiTableColumnFlags_WidthStretch);
            if(!g.hud_designer)ImGui::TableSetupColumn("Domyślne",ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
            ImGuiListClipper clipper;clipper.Begin(static_cast<int>(list.size()),ImGui::GetFrameHeight()+ImGui::GetStyle().CellPadding.y*2);
            if(g.binding_scroll_last && !list.empty())clipper.IncludeItemByIndex(static_cast<int>(list.size()-1));
            while(clipper.Step())for(int row=clipper.DisplayStart;row<clipper.DisplayEnd;++row) {
                const auto i=static_cast<std::size_t>(row);
                ImGui::PushID(static_cast<int>(i));ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);
                const float row_top=ImGui::GetCursorScreenPos().y-ImGui::GetStyle().CellPadding.y;
                const float row_height=ImGui::GetFrameHeight()+2*ImGui::GetStyle().CellPadding.y;
                ImGui::TextUnformatted(action_label(list[i].action));
                if(ImGui::IsItemHovered()&&ImGui::IsMouseClicked(1)){context_row=static_cast<int>(i);context_clicked=true;}
                ImGui::TableSetColumnIndex(1);
                const auto label=g.binding_capture==static_cast<int>(i)?std::string("Naciśnij klawisz..."):binding_label(list[i]);
                ImGui::BeginDisabled(g.binding_capture>=0);
                if(ImGui::Button(label.c_str(),{-1,0})) {g.binding_capture_armed=false;g.binding_capture=static_cast<int>(i);g.binding_capture_frame=ImGui::GetFrameCount();g.binding_message.clear();}
                ImGui::EndDisabled();
                if(ImGui::IsItemHovered()&&ImGui::IsMouseClicked(1)){context_row=static_cast<int>(i);context_clicked=true;}
                if(!g.hud_designer) {
                    ImGui::TableSetColumnIndex(2);std::string text;
                    for(const auto& b:g.hud.bindings)if(!list[i].action.empty() && b.action==list[i].action) {
                        if(!text.empty())text+=" / ";
                        text+=binding_label(b);
                    }
                    if(text.empty())text="—";
                    ImGui::TextDisabled("%s",text.c_str());
                    ImGui::SetItemTooltip("%s",text.c_str());
                    if(ImGui::IsItemHovered()&&ImGui::IsMouseClicked(1)){context_row=static_cast<int>(i);context_clicked=true;}
                }
                // Include blank space in each table row, not only its text/button.
                if(ImGui::IsMouseClicked(1)&&ImGui::IsWindowHovered()&&ImGui::GetMousePos().y>=row_top&&ImGui::GetMousePos().y<row_top+row_height) {
                    context_row=static_cast<int>(i);context_clicked=true;
                }
                if(g.binding_scroll_last && i+1==list.size()){ImGui::SetScrollHereY(1);g.binding_scroll_last=false;}
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    const bool area_hovered=ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
    ImGui::EndChild();
    // Context is opened at the settings-window ID stack level, not per row.
    static int selected=-1;
    if(g.binding_capture<0 && (context_clicked || (area_hovered&&ImGui::IsMouseClicked(1)))) {
        selected=context_clicked?context_row:-1;g.binding_capture=-1;ImGui::OpenPopup("Bind / kontekst");
    }
    if(ImGui::BeginPopup("Bind / kontekst")) {
        const bool valid=selected>=0&&static_cast<std::size_t>(selected)<list.size();
        if(ImGui::BeginMenu("Przypisz funkcję",valid)) {
            for(auto a:actions())if(ImGui::MenuItem(a.label,nullptr,valid&&list[static_cast<std::size_t>(selected)].action==a.id)) {
                list[static_cast<std::size_t>(selected)].action=a.id;changed();
            }
            ImGui::EndMenu();
        }
        if(ImGui::MenuItem("Dodaj binda",nullptr,false,list.size()<512)){list.push_back({});g.binding_scroll_last=true;changed();}
        if(ImGui::MenuItem("Usuń binda",nullptr,false,valid)){list.erase(list.begin()+selected);selected=-1;g.binding_capture=-1;changed();}
        ImGui::EndPopup();
    }
    capture();
    if(!g.binding_message.empty())ImGui::TextWrapped("%s",g.binding_message.c_str());
    if(g.hud_designer && ImGui::Button("Zapisz domyślne w HUD"))g.hud_save_requested=true;
}
void graphics() {
    auto& g=app();bool unlimited=g.fps_limit==0;
    if(ImGui::Checkbox("Bez limitu FPS",&unlimited)){g.fps_limit=unlimited?0:60;g.next_frame={};}
    ImGui::BeginDisabled(unlimited);
    int value=g.fps_limit?g.fps_limit:60;
    if(ImGui::SliderInt("Limit FPS",&value,15,360)){g.fps_limit=value;g.next_frame={};}
    ImGui::EndDisabled();ImGui::TextDisabled("Presety:");
    for(int preset:{30,60,120,144,240,360}) {
        ImGui::SameLine();ImGui::PushID(preset);
        if(ImGui::SmallButton(std::to_string(preset).c_str())){g.fps_limit=preset;g.next_frame={};}ImGui::PopID();
    }
    ImGui::BeginDisabled();
    if(ImGui::BeginCombo("Renderer","Vulkan")){ImGui::Selectable("Vulkan",true);ImGui::EndCombo();}ImGui::EndDisabled();
    ImGui::Text("%.1f FPS / %.3f ms na klatkę",static_cast<double>(ImGui::GetIO().Framerate),1000.0/static_cast<double>(std::max(1.0F,ImGui::GetIO().Framerate)));
    const auto stats=g.renderer.statistics();
    ImGui::Text("CPU geometrii: %.2f us (nie czas GPU)",stats.geometry_cpu_us);
    ImGui::Text("Geometria: %llu przebudów / %llu trafień cache",static_cast<unsigned long long>(stats.geometry_builds),static_cast<unsigned long long>(stats.geometry_cache_hits));
    ImGui::Text("Aktualizacje deskryptorów: %llu",static_cast<unsigned long long>(stats.descriptor_updates));
    ImGui::Text("Indirect: %llu obiektów / %llu wywołań (łącznie)",static_cast<unsigned long long>(stats.indirect_objects),static_cast<unsigned long long>(stats.indirect_draw_calls));
    ImGui::TextDisabled("Multi-draw indirect: %s",stats.indirect_batching?"aktywny":"fallback pojedynczych wywołań");
    if(ImGui::CollapsingHeader("Zaawansowane")) {
        ImGui::Checkbox("Pokaż siatkę",&g.grid_visible);ImGui::SliderFloat("Pole widzenia",&g.camera.vertical_fov_degrees,30,100,"%.0f°");
        float bg[]{g.background_color.x,g.background_color.y,g.background_color.z};
        if(ImGui::ColorEdit3("Kolor tła",bg,ImGuiColorEditFlags_NoInputs))g.background_color={bg[0],bg[1],bg[2]};
        ImGui::Text("MSAA: %u×",g.renderer.antialiasing_samples());
    }
}
}
void initialize_bindings() {
    auto& g=app();g.live_bindings=g.hud.bindings;g.binding_capture=-1;g.bindings_initialized=true;g.bindings_armed=false;
    if(g.hud_designer || g.safe_mode)return; // Never load runtime overrides into authored defaults.
    const auto path=g.hud_path.parent_path()/L"editor_bindings.cfg";std::string error;std::error_code ec;
    if(std::filesystem::exists(path,ec)&&!load_bindings(path,bindings_fingerprint(g.hud.bindings),g.live_bindings,error))LOG_WARN("bindings","%s",error.c_str());
}
void restore_bindings() {
    auto& g=app();editable()=g.hud_designer?basic_bindings():g.hud.bindings;
    g.binding_capture=-1;g.binding_message.clear();changed();
}
void process_bindings() {
    auto& g=app();if(!g.bindings_initialized)initialize_bindings();
    if(g.hud_designer||g.show_settings||g.binding_capture>=0||g.binding_consumed_frame==ImGui::GetFrameCount())return;
    const auto& io=ImGui::GetIO();
    if(!g.bindings_armed) {
        // Ignore keys/buttons held by Explorer/IDE at launch (including binds
        // such as app.close). Require a neutral frame before dispatching.
        for(int key=ImGuiKey_NamedKey_BEGIN;key<ImGuiKey_NamedKey_END;++key)
            if(ImGui::IsKeyDown(static_cast<ImGuiKey>(key)))return;
        if(g.hwnd)for(int key=1;key<256;++key)if(GetAsyncKeyState(key)&0x8000)return;
        g.bindings_armed=true;return;
    }
    if(io.WantTextInput||ImGui::IsAnyItemActive()||ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel))return;
    const auto current_modifiers=modifiers();
    for(const auto& b:g.live_bindings) {
        if(!b.key||b.action.empty()||current_modifiers!=b.modifiers)continue;
        const auto key=gui_key(b.key);
        // Mouse binds must not consume clicks in unrelated editor controls.
        if(b.key>=400 && (!g.viewport_visible||!ImGui::IsMousePosValid()||io.MousePos.x<0||io.MousePos.y<0||io.MousePos.x>=g.width||io.MousePos.y>=g.height||!scene_input_at(static_cast<int>(io.MousePos.x),static_cast<int>(io.MousePos.y))))continue;
        if(key!=ImGuiKey_None&&ImGui::IsKeyPressed(key,false)) {
            if(b.key>=400){g.binding_pointer_frame=ImGui::GetFrameCount();g.orbit.dragging=false;}
            LOG_INFO("bindings","Dispatch: %s",b.action.c_str());
            hud_action(b.action);break;
        }
    }
}
void draw_settings_window() {
    auto& g=app();g.settings_hovered=false;
    if(!g.bindings_initialized)initialize_bindings();
    if(!g.show_settings){g.binding_capture=-1;return;}
    if(g.focus_settings){ImGui::SetNextWindowFocus();g.focus_settings=false;}
    const auto* viewport=ImGui::GetMainViewport();
    const ImVec2 available{std::max(1.0F,viewport->WorkSize.x),std::max(1.0F,viewport->WorkSize.y)};
    ImGui::SetNextWindowSize({std::min(720.0F,available.x),std::min(500.0F,available.y)},ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos({viewport->WorkPos.x+available.x*.5F,viewport->WorkPos.y+available.y*.5F},ImGuiCond_Appearing,{.5F,.5F});
    ImGui::SetNextWindowSizeConstraints({std::min(620.0F,available.x),std::min(380.0F,available.y)},available);
    if(ImGui::Begin("Ustawienia",&g.show_settings,ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings)) {
        if(ImGui::BeginTabBar("##SettingsTabs")) {
            if(ImGui::BeginTabItem("Podstawowe",nullptr,g.settings_tab==0?ImGuiTabItemFlags_SetSelected:0)) {
                ImGui::BeginDisabled();ImGui::Button("Pomoc");
                if(ImGui::BeginCombo("Język","Polski")){ImGui::Selectable("Polski",true);ImGui::EndCombo();}ImGui::EndDisabled();
                ImGui::TextDisabled("Na razie dostępny jest wyłącznie język polski.");ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem("Grafika",nullptr,g.settings_tab==1?ImGuiTabItemFlags_SetSelected:0)){graphics();ImGui::EndTabItem();}
            if(ImGui::BeginTabItem("Bindy",nullptr,g.settings_tab==2?ImGuiTabItemFlags_SetSelected:0)){bindings_tab();ImGui::EndTabItem();}
            ImGui::EndTabBar();g.settings_tab=-1;
        }
    }
    g.settings_hovered=ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows|ImGuiHoveredFlags_AllowWhenBlockedByPopup);
    ImGui::End();if(!g.show_settings)g.binding_capture=-1;
}
}
