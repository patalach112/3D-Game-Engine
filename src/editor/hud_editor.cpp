#include "editor_app.hpp"
#include "hud_editor.hpp"
#include "settings.hpp"
#include "dock_layout.hpp"
#include "engine/log.hpp"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace editor {
namespace {
using namespace engine::hud;
constexpr float title_height=40;
struct Deferred { Kind kind{Kind::Panel};Id parent{},erase{};bool add{}; } pending;
ImVec2 last_canvas{};
int hit_priority{};
float radius(const Node& n) {return n.rounded?(n.shared_rounding?app().hud.default_rounding:n.rounding):0;}
const char* kind_name(Kind k) {
    switch(k) {case Kind::Bar:return "Pasek górny";case Kind::Window:return "Okno";case Kind::Panel:return "Panel";
    case Kind::Button:return "Przycisk";case Kind::Input:return "Wpisywarka";case Kind::Text:return "Tekst";case Kind::List:return "Lista";case Kind::Footer:return "Pasek stanu";}
    return "?";
}
void change(bool changed) {if(changed) app().hud_dirty=true;}
void text_edit(const char* label,std::string& value) {
    char buffer[4097]{};std::snprintf(buffer,sizeof(buffer),"%s",value.c_str());
    if(ImGui::InputText(label,buffer,sizeof(buffer))) {value=buffer;app().hud_dirty=true;}
}
void size_edit(const char* label,Size& value,float lo,float hi) {
    float v[]{value.x,value.y};
    if(ImGui::DragFloat2(label,v,1,lo,hi,"%.0f",ImGuiSliderFlags_AlwaysClamp)) {value={v[0],v[1]};app().hud_dirty=true;}
}
void rounding(Node& n) {
    change(ImGui::Checkbox("Zaokrąglone",&n.rounded));
    if(n.rounded) {
        change(ImGui::Checkbox("Podstawowe zaokrąglenie (wspólne)",&n.shared_rounding));
        if(!n.shared_rounding) change(ImGui::SliderFloat("Wielkość zaokrąglenia",&n.rounding,0,100,"%.0f px"));
        change(ImGui::SliderFloat("Wspólne zaokrąglenie",&app().hud.default_rounding,0,100,"%.0f px"));
    }
}
void layout_properties(Node& n) {
    size_edit("Odstęp przycisków X / Y",n.gap,0,256);
    size_edit("Odstęp od krawędzi X / Y",n.padding,0,256);
    size_edit("Minimum przycisków X / Y",n.button_minimum,1,2048);
}
void button_properties(Node& n) {
    size_edit("Minimalna wielkość",n.minimum,1,2048);
    change(ImGui::Checkbox("Aktywny",&n.enabled));
    if(n.kind==Kind::Button || n.kind==Kind::List) {
        bool submenu=n.kind==Kind::List;
        bool has_children=false;for(const auto& c:app().hud.nodes) has_children|=c.parent==n.id;
        ImGui::BeginDisabled(has_children);
        if(ImGui::Checkbox("Wysuwana lista",&submenu)) {n.kind=submenu?Kind::List:Kind::Button;app().hud_dirty=true;}
        ImGui::EndDisabled();
        if(ImGui::BeginCombo("Funkcja",n.action.empty()?"Brak":n.action.c_str())) {
            for(auto action:engine::hud::actions()) if(ImGui::Selectable(action.label,n.action==action.id)) {n.action=action.id;app().hud_dirty=true;}
            ImGui::EndCombo();
        }
        if(n.kind==Kind::List && ImGui::Button("Dodaj pozycję listy"))pending={Kind::Button,n.id,0,true};
        if(n.kind==Kind::List) ImGui::TextWrapped("Lista otwiera podmenu. Jej funkcja jest dostępna jako pierwsza pozycja Uruchom.");
    }
    rounding(n);
}
void container_properties(Node& n,bool menu=false) {
    size_edit("Minimum panelu / okna",n.minimum,1,8192);
    if(menu) {if(ImGui::BeginMenu("Zaokrąglenie")){rounding(n);ImGui::EndMenu();}}
    else rounding(n);
}
void hit(Id id,Id panel,Id window) {
    auto& g=app();
    if(!g.hud_designer) return;
    const auto* current=ImGui::GetCurrentWindow();
    const int priority=(current && (current->Flags&ImGuiWindowFlags_Popup))?8:(panel?4:window?3:2);
    if(priority<hit_priority)return;
    hit_priority=priority;g.hud_hit=id;g.hud_hit_panel=panel;g.hud_hit_window=window;
}
void item_hit(const Node& n,Id panel,Id window) {
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled|ImGuiHoveredFlags_AllowWhenBlockedByPopup)) hit(n.id,panel,window);
}
bool design() {return app().hud_designer && !app().hud_preview;}
void execute(const Node& n) {
    auto& g=app();
    if(n.action=="settings"){hud_action(n.action);return;}
    if(design()) g.hud_selected=n.id;
    else if(n.enabled) hud_action(n.action);
}
void menu_children(Id parent,Id window);
void menu_node(Node& n,Id window) {
    auto& g=app();
    ImGui::PushID(static_cast<int>(n.id));
    const auto spacing=ImGui::GetStyle().ItemSpacing;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{spacing.x,std::max(spacing.y,n.minimum.y-ImGui::GetFontSize())});
    if(n.kind==Kind::List) {
        float width=n.minimum.x;
        for(const auto& child:g.hud.nodes)if(child.parent==n.id && child.visible)width=std::max(width,child.minimum.x);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding,radius(n));
        ImGui::SetNextWindowSizeConstraints({width,0},{FLT_MAX,FLT_MAX});
        const bool open=ImGui::BeginMenu(n.text.c_str(),n.enabled);item_hit(n,0,window);
        if(open) {
            if(!n.action.empty() && ImGui::MenuItem("Uruchom")) execute(n);
            if(n.builtin=="view.windows") {
                for(auto& w:g.hud.nodes) if(w.kind==Kind::Window) {
                    ImGui::PushID(static_cast<int>(w.id));
                    if(ImGui::MenuItem(w.text.c_str(),nullptr,&w.visible)) {if(g.hud_designer)g.hud_dirty=true; if(w.visible)g.hud_focus=w.id;}
                    ImGui::PopID();
                }
            }
            menu_children(n.id,window);ImGui::EndMenu();
        }
        ImGui::PopStyleVar();
    } else if(n.kind==Kind::Button) {
        if(ImGui::MenuItem(n.text.c_str(),nullptr,false,n.enabled)) execute(n);
        item_hit(n,0,window);
    } else {ImGui::TextUnformatted(n.text.c_str());item_hit(n,0,window);}
    ImGui::PopStyleVar();ImGui::PopID();
}
void menu_children(Id parent,Id window) {
    for(auto& n:app().hud.nodes) if(n.parent==parent && n.visible) menu_node(n,window);
}
void chrome() {
    auto& g=app();Node* bar=nullptr;for(auto& n:g.hud.nodes) if(n.kind==Kind::Bar)bar=&n;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{8,6});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{4,4});
    ImGui::BeginViewportSideBar("##HUD_Title",ImGui::GetMainViewport(),ImGuiDir_Up,title_height,
        ImGuiWindowFlags_MenuBar|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollbar);
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    if(ImGui::BeginMenuBar()) {
        // Reserve immutable native controls. Narrow windows get a scrollable
        // overflow popup, rather than allowing project menus over the controls.
        if(bar && bar->visible) {
            if(ImGui::IsWindowHovered() && ImGui::GetMousePos().x<g.width-146) hit(bar->id,0,0);
            const float right=static_cast<float>(g.width)-160;
            bool overflow=false;
            for(auto& n:g.hud.nodes) if(n.parent==bar->id && n.visible) {
                if(ImGui::GetCursorPosX()+ImGui::CalcTextSize(n.text.c_str()).x+32>right) {overflow=true;break;}
                menu_node(n,0);
            }
            if(overflow && ImGui::BeginMenu("...")) {menu_children(bar->id,0);ImGui::EndMenu();}
            if(ImGui::GetCursorPosX()+ImGui::CalcTextSize(bar->text.c_str()).x+20<right) ImGui::TextDisabled("  %s",bar->text.c_str());
        }
        g.title_drag_bounds={static_cast<LONG>(std::ceil(ImGui::GetCursorScreenPos().x)),0,std::max(0,g.width-146),40};
        ImGui::SetCursorPosX(static_cast<float>(g.width-144));
        if(ImGui::Button("-##native_min",{44,28})) PostMessageW(g.hwnd,WM_SYSCOMMAND,SC_MINIMIZE,0);
        if(ImGui::Button(IsZoomed(g.hwnd)?"<>##native_max":"[]##native_max",{44,28})) PostMessageW(g.hwnd,WM_SYSCOMMAND,IsZoomed(g.hwnd)?SC_RESTORE:SC_MAXIMIZE,0);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{.8F,.15F,.2F,1});
        if(ImGui::Button("X##native_close",{44,28})) PostMessageW(g.hwnd,WM_CLOSE,0,0);
        ImGui::PopStyleColor();ImGui::EndMenuBar();
    }
    ImGui::End();ImGui::PopStyleVar(2);
}
void children(Node& parent,Id window);
void widget(Node& n,Id panel,Id window,ImVec2 size) {
    auto& g=app();ImGui::PushID(static_cast<int>(n.id));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,radius(n));
    if(n.kind==Kind::Panel) {
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,radius(n));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{n.padding.x,n.padding.y});
        ImGui::PushStyleColor(ImGuiCol_ChildBg,n.builtin=="scene"?ImVec4{0,0,0,0}:ImVec4{.14F,.15F,.17F,1});
        const bool open=ImGui::BeginChild("##panel",size,ImGuiChildFlags_Borders);
        if(ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows|ImGuiHoveredFlags_AllowWhenBlockedByPopup)) hit(n.id,n.id,window);
        if(open) {
            children(n,window);
            if(!n.builtin.empty()) {
                ImGui::BeginDisabled(design());hud_builtin(n.builtin);ImGui::EndDisabled();
            }
        }
        ImGui::EndChild();ImGui::PopStyleColor();ImGui::PopStyleVar(2);
    } else {
        ImGui::BeginDisabled(!n.enabled);
        if(n.kind==Kind::Button) {if(ImGui::Button(n.text.c_str(),size)) execute(n);}
        else if(n.kind==Kind::List) {
            if(ImGui::Button(n.text.c_str(),size)) {if(design())g.hud_selected=n.id;ImGui::OpenPopup("##list");}
            item_hit(n,panel,window);
            float width=n.minimum.x;
            for(const auto& child:g.hud.nodes)if(child.parent==n.id && child.visible)width=std::max(width,child.minimum.x);
            ImGui::SetNextWindowSizeConstraints({width,0},{FLT_MAX,FLT_MAX});
            ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding,radius(n));
            if(ImGui::BeginPopup("##list")) {
                if(!n.action.empty() && ImGui::MenuItem("Uruchom")) execute(n);
                menu_children(n.id,window);ImGui::EndPopup();
            }
            ImGui::PopStyleVar();
        } else if(n.kind==Kind::Input) {
            auto [it,inserted]=g.hud_inputs.try_emplace(n.id);
            if(inserted) std::snprintf(it->second.data(),it->second.size(),"%s",n.value.c_str());
            ImGui::SetNextItemWidth(size.x);
            ImGui::InputTextWithHint("##input",n.text.c_str(),it->second.data(),it->second.size());
        } else ImGui::TextUnformatted(n.text.c_str());
        if(n.kind!=Kind::List) item_hit(n,panel,window);
        ImGui::EndDisabled();
    }
    if(g.hud_designer && g.hud_selected==n.id) {
        const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddRect(lo,hi,IM_COL32(65,215,195,255),radius(n),0,2);
    }
    ImGui::PopStyleVar();ImGui::PopID();
}
void children(Node& parent,Id window) {
    const ImVec2 origin=ImGui::GetCursorScreenPos(),avail=ImGui::GetContentRegionAvail();
    float x=0,y=0,row=0;bool any=false;
    for(auto& n:app().hud.nodes) if(n.parent==parent.id && n.visible) {
        any=true;
        float w=std::max(parent.button_minimum.x,n.minimum.x),h=std::max(parent.button_minimum.y,n.minimum.y);
        if(n.kind==Kind::Panel) {w=std::max(n.minimum.x,avail.x*n.rect.w);h=std::max(n.minimum.y,avail.y*n.rect.h);}
        if(n.kind==Kind::Text) {w=std::max(w,ImGui::CalcTextSize(n.text.c_str()).x);h=ImGui::GetTextLineHeight();}
        w=std::min(w,std::max(1.0F,avail.x));
        if(x>0 && x+w>avail.x+1) {x=0;y+=row+parent.gap.y;row=0;}
        ImGui::SetCursorScreenPos({origin.x+x,origin.y+y});
        widget(n,parent.kind==Kind::Panel?parent.id:0,window,{w,h});
        row=std::max(row,std::max(h,ImGui::GetItemRectSize().y));x+=w+parent.gap.x;
    }
    if(any) {ImGui::SetCursorScreenPos({origin.x,origin.y+y+row+parent.gap.y});ImGui::Dummy({0,0});}
}
bool scene_window(const Node& w) {for(const auto& n:app().hud.nodes) if(n.parent==w.id && n.builtin=="scene")return true;return false;}
Size window_minimum(const Node& w) {
    Size s=w.minimum;
    for(const auto& n:app().hud.nodes) if(n.parent==w.id && n.visible) {
        s.x=std::max(s.x,std::max(n.minimum.x,n.button_minimum.x+2*n.padding.x)+2*w.padding.x);
        s.y=std::max(s.y,std::min(8192.0F,n.minimum.y+2*w.padding.y+30));
    }
    return s;
}
void dockspace(ImVec2 pos,ImVec2 size) {
    auto& g=app();const auto id=ImHashStr("HUD_Project_Dockspace");
    if(g.hud_reset || !ImGui::DockBuilderGetNode(id)) {
        ImGui::DockBuilderRemoveNode(id);ImGui::DockBuilderAddNode(id,ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodePos(id,pos);ImGui::DockBuilderSetNodeSize(id,size);
        ImGuiID center=id,left{},right{},bottom{};
        ImGui::DockBuilderSplitNode(center,ImGuiDir_Left,.19F,&left,&center);
        ImGui::DockBuilderSplitNode(center,ImGuiDir_Right,.27F,&right,&center);
        ImGui::DockBuilderSplitNode(center,ImGuiDir_Down,.30F,&bottom,&center);
        for(const auto& n:g.hud.nodes) if(n.kind==Kind::Window && n.dock!=Dock::Floating) {
            const auto target=n.dock==Dock::Left?left:n.dock==Dock::Right?right:n.dock==Dock::Bottom?bottom:center;
            const auto name=window_name(n);ImGui::DockBuilderDockWindow(name.c_str(),target);
            for(const auto& p:g.hud.nodes) if(p.parent==n.id && p.builtin=="console")
                ImGui::DockBuilderGetNode(bottom)->SelectedTabId=ImHashStr("#TAB",0,ImHashStr(name.c_str()));
        }
        ImGui::DockBuilderFinish(id);g.hud_reset=false;
    }
    ImGui::SetNextWindowPos(pos);ImGui::SetNextWindowSize(size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    ImGui::Begin("##HUD_Project_Host",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoDocking
        |ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar(2);ImGui::DockSpace(id,{0,0});ImGui::End();
}
void windows(ImVec2 pos,ImVec2 canvas) {
    auto& g=app();dockspace(pos,canvas);
    const bool resized=canvas.x!=last_canvas.x || canvas.y!=last_canvas.y;last_canvas=canvas;
    g.viewport_bounds={};g.viewport_visible=false;g.scene_drawlist=nullptr;
    for(auto& n:g.hud.nodes) if(n.kind==Kind::Window && n.visible) {
        const auto name=window_name(n);const bool scene=scene_window(n);
        const auto* existing=ImGui::FindWindowByName(name.c_str());
        const auto min=window_minimum(n);
        if(!existing || (resized && !existing->DockIsActive)) {
            ImGui::SetNextWindowPos({pos.x+canvas.x*n.rect.x,pos.y+canvas.y*n.rect.y});
            ImGui::SetNextWindowSize({std::max(min.x,canvas.x*n.rect.w),std::max(min.y,canvas.y*n.rect.h)});
        }
        ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize,{min.x,min.y});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,radius(n));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{n.padding.x,n.padding.y});
        if(g.hud_focus==n.id) {ImGui::SetNextWindowFocus();g.hud_focus=0;}
        const bool was_open=n.visible;
        const bool visible=ImGui::Begin(name.c_str(),&n.visible,ImGuiWindowFlags_NoFocusOnAppearing|(scene?ImGuiWindowFlags_NoBackground:0));
        if(n.visible!=was_open && g.hud_designer)g.hud_dirty=true;
        const bool docked=ImGui::IsWindowDocked();
        if(ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows|ImGuiHoveredFlags_AllowWhenBlockedByPopup)) hit(n.id,0,n.id);
        if(scene)g.scene_window_name=name;
        if(visible)children(n,n.id);
        if(scene && g.viewport_visible)g.scene_floating=!docked;
        if(!docked && canvas.x>0 && canvas.y>0) {
            const auto p=ImGui::GetWindowPos(),s=ImGui::GetWindowSize();
            n.rect={std::clamp((p.x-pos.x)/canvas.x,0.0F,1.0F),std::clamp((p.y-pos.y)/canvas.y,0.0F,1.0F),
                std::clamp(s.x/canvas.x,.01F,1.0F),std::clamp(s.y/canvas.y,.01F,1.0F)};
        }
        if(g.hud_designer && ImGui::IsWindowFocused() && ImGui::IsMouseDragging(0))g.hud_dirty=true;
        ImGui::End();ImGui::PopStyleVar(3);
    }
    if(!g.viewport_visible){g.drag_axis=-1;g.orbit.dragging=false;}
}
void tree(Id parent) {
    auto& g=app();
    for(const auto& n:g.hud.nodes) if(n.parent==parent) {
        ImGui::PushID(static_cast<int>(n.id));
        bool has_child=false;for(const auto& c:g.hud.nodes)has_child|=c.parent==n.id;
        auto flags=ImGuiTreeNodeFlags_OpenOnArrow|ImGuiTreeNodeFlags_SpanAvailWidth;
        if(!has_child)flags|=ImGuiTreeNodeFlags_Leaf|ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if(g.hud_selected==n.id)flags|=ImGuiTreeNodeFlags_Selected;
        const bool open=ImGui::TreeNodeEx("##node",flags,"%s / %s",kind_name(n.kind),n.text.c_str());
        if(ImGui::IsItemClicked())g.hud_selected=n.id;
        if(open && has_child){tree(n.id);ImGui::TreePop();}ImGui::PopID();
    }
}
void inspector(float top,float height) {
    auto& g=app();
    ImGui::SetNextWindowPos({static_cast<float>(g.width)-330,top});ImGui::SetNextWindowSize({330,height});
    ImGui::Begin("HUD / Właściwości",nullptr,ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextWrapped("LPM: zaznacz. PPM: dodaj / ustawienia. Przeciągnij zakładkę, aby dokować.");
    if(ImGui::Button("Całe okno aplikacji"))g.hud_selected=0;
    ImGui::BeginChild("##tree",{0,170},ImGuiChildFlags_Borders);tree(0);ImGui::EndChild();
    auto* n=g.hud.find(g.hud_selected);
    if(!n) {
        size_edit("Minimum okna aplikacji",g.hud.window_minimum,300,8192);
        change(ImGui::SliderFloat("Podstawowe zaokrąglenie",&g.hud.default_rounding,0,100,"%.0f px"));
        bool status=false;for(const auto& c:g.hud.nodes)status|=c.kind==Kind::Footer;
        if(!status && ImGui::Button("Dodaj pasek stanu"))pending={Kind::Footer,0,0,true};
    } else {
        ImGui::PushID(static_cast<int>(n->id));ImGui::Text("ID: %u / %s",n->id,kind_name(n->kind));
        text_edit("Tekst / nazwa",n->text);change(ImGui::Checkbox("Początkowo widoczny",&n->visible));
        if(n->kind==Kind::Window || n->kind==Kind::Panel) {
            if(ImGui::CollapsingHeader("Layout",ImGuiTreeNodeFlags_DefaultOpen))layout_properties(*n);
            if(ImGui::CollapsingHeader("Ustawienia panelu / okna",ImGuiTreeNodeFlags_DefaultOpen))container_properties(*n);
            if(n->kind==Kind::Panel) {
                float fractions[]{n->rect.w,n->rect.h};
                if(ImGui::SliderFloat2("Część obszaru X / Y",fractions,.05F,1)) {n->rect.w=fractions[0];n->rect.h=fractions[1];g.hud_dirty=true;}
                if(!n->builtin.empty())ImGui::TextWrapped("Wbudowana zawartość: %s (funkcjonalność silnika)",n->builtin.c_str());
            }
        } else if(n->kind==Kind::Footer) {
            change(ImGui::SliderFloat("Wysokość paska",&n->minimum.y,20,100,"%.0f px"));
            ImGui::TextWrapped("Znaczniki: {status}, {objects}, {fps}, {limit}. Pasek jest częścią zapisanego HUD-u.");
        } else if(n->kind!=Kind::Bar) button_properties(*n);
        if(n->kind==Kind::Input) {text_edit("Tekst początkowy",n->value);g.hud_inputs.erase(n->id);}
        if(n->kind!=Kind::Bar && ImGui::Button("Usuń element i zawartość"))pending.erase=n->id;
        ImGui::PopID();
    }
    ImGui::End();
}
void context_menu(ImVec2 canvas) {
    auto& g=app();
    if(g.settings_hovered)return;
    if(ImGui::IsMouseClicked(ImGuiMouseButton_Left) && g.hud_hit && !g.hud_preview)g.hud_selected=g.hud_hit;
    if(ImGui::IsMouseClicked(ImGuiMouseButton_Right) && (g.hud_hit || ImGui::IsMouseHoveringRect({0,74},{canvas.x,74+canvas.y}))) {
        g.hud_context=g.hud_hit;g.hud_context_panel=g.hud_hit_panel;g.hud_context_window=g.hud_hit_window;
        g.hud_selected=g.hud_context;ImGui::OpenPopup("HUD / kontekst");
    }
    if(!ImGui::BeginPopup("HUD / kontekst")) return;
    auto* n=g.hud.find(g.hud_context);auto* panel=g.hud.find(g.hud_context_panel);auto* window=g.hud.find(g.hud_context_window);
    if(ImGui::MenuItem("Dodaj okno"))pending={Kind::Window,0,0,true};
    if(ImGui::BeginMenu("Dodaj")) {
        if(window && ImGui::MenuItem("Panel"))pending={Kind::Panel,window->id,0,true};
        const bool menu=n && (n->kind==Kind::List || n->kind==Kind::Bar);
        if(panel || menu || window) {
            const Id target=menu?n->id:panel?panel->id:window->id;
            // The Button entry is absent unless the hit belongs to a panel.
            // Menu lists also have an explicit Add list item in Properties.
            if(panel && ImGui::MenuItem("Przycisk"))pending={Kind::Button,target,0,true};
            if(ImGui::MenuItem("Lista"))pending={Kind::List,target,0,true};
            if(ImGui::MenuItem("Tekst"))pending={Kind::Text,target,0,true};
            if(!menu && ImGui::MenuItem("Wpisywarka"))pending={Kind::Input,target,0,true};
        }
        ImGui::EndMenu();
    }
    if(panel && ImGui::BeginMenu("Layout")) {layout_properties(*panel);ImGui::EndMenu();}
    if(n && (n->kind==Kind::Button || n->kind==Kind::List) && ImGui::BeginMenu("Ustawienia przycisku / listy")) {button_properties(*n);ImGui::EndMenu();}
    ImGui::Separator();
    if((panel || window) && ImGui::BeginMenu("Ustawienia panelu i okna")) {
        if(panel && ImGui::BeginMenu("Panel")) {container_properties(*panel,true);ImGui::EndMenu();}
        if(window && ImGui::BeginMenu("Okno")) {container_properties(*window,true);ImGui::EndMenu();}
        ImGui::EndMenu();
    }
    ImGui::EndPopup();
}
void reload_project() {
    auto& g=app();
        Document candidate;std::string error;
        if(load(g.hud_path,candidate,error)) {g.hud=std::move(candidate);g.hud_inputs.clear();g.hud_dirty=false;g.hud_selected=0;g.hud_reset=true;
            if(!g.hud.docking.empty()) {ImGui::LoadIniSettingsFromMemory(g.hud.docking.c_str());g.hud_reset=false;}}
        else g.hud_message=error;
        initialize_bindings();
}
void toolbar() {
    auto& g=app();
    ImGui::BeginViewportSideBar("##HUD_Designer_Tools",ImGui::GetMainViewport(),ImGuiDir_Up,34,
        ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollbar);
    if(ImGui::Button("Zapisz HUD (Ctrl+S)"))g.hud_save_requested=true;
    ImGui::SameLine();if(ImGui::Button("Wczytaj")) {
        if(g.hud_dirty)ImGui::OpenPopup("Wczytać zapisany HUD?");else reload_project();
    }
    if(ImGui::BeginPopup("Wczytać zapisany HUD?")) {
        ImGui::TextUnformatted("Odrzucić niezapisane zmiany i wczytać plik?");
        if(ImGui::Button("Wczytaj i odrzuć zmiany")){reload_project();ImGui::CloseCurrentPopup();}
        ImGui::SameLine();if(ImGui::Button("Anuluj"))ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::SameLine();if(ImGui::Button("Ustawienia"))hud_action("settings");
    ImGui::SameLine();ImGui::Checkbox("Interaktywny podgląd",&g.hud_preview);
    ImGui::SameLine();ImGui::Checkbox("Właściwości",&g.hud_inspector);
    ImGui::SameLine();if(ImGui::Button("Domyślny HUD"))ImGui::OpenPopup("Przywrócić HUD?");
    if(ImGui::BeginPopup("Przywrócić HUD?")) {
        ImGui::TextUnformatted("Bieżący projekt zostanie zastąpiony (plik pozostaje bez zmian).");
        if(ImGui::Button("Przywróć")) {g.hud=defaults();g.hud_inputs.clear();g.hud_selected=0;g.hud_reset=true;g.hud_dirty=true;initialize_bindings();ImGui::CloseCurrentPopup();}
        ImGui::EndPopup();
    }
    ImGui::End();
}
Node* project_footer() {for(auto& n:app().hud.nodes)if(n.kind==Kind::Footer && n.visible)return &n;return nullptr;}
std::string footer_text(const Node& node) {
    auto& g=app();std::string text=node.text;
    const auto replace=[&](const char* key,const std::string& value) {
        std::size_t pos=0;while((pos=text.find(key,pos))!=std::string::npos){text.replace(pos,std::strlen(key),value);pos+=value.size();}
    };
    char fps[32]{};std::snprintf(fps,sizeof(fps),"%.1f",static_cast<double>(ImGui::GetIO().Framerate));
    const int bytes=WideCharToMultiByte(CP_UTF8,0,g.status.c_str(),static_cast<int>(g.status.size()),nullptr,0,nullptr,nullptr);
    std::string status(static_cast<std::size_t>(std::max(0,bytes)),'\0');
    if(bytes)WideCharToMultiByte(CP_UTF8,0,g.status.c_str(),static_cast<int>(g.status.size()),status.data(),bytes,nullptr,nullptr);
    replace("{objects}",std::to_string(g.scene.entity_count()));replace("{fps}",fps);
    replace("{limit}",g.fps_limit?std::to_string(g.fps_limit)+" FPS":"unlimited");replace("{status}",status);
    return text;
}
void footer() {
    auto& g=app();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{10,4});
    if(g.hud_designer) {
        ImGui::BeginViewportSideBar("##HUD_Designer_Status",ImGui::GetMainViewport(),ImGuiDir_Down,24,
            ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollbar);
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        ImGui::TextDisabled("%s%s | %s",g.hud_dirty?"* ":"","editor_hud.layout",g.hud_message.c_str());ImGui::End();
    }
    if(auto* node=project_footer()) {
        ImGui::BeginViewportSideBar("##HUD_Project_Status",ImGui::GetMainViewport(),ImGuiDir_Down,std::clamp(node->minimum.y,20.0F,100.0F),
            ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollbar);
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        const auto text=footer_text(*node);ImGui::TextDisabled("%s",text.c_str());
        if(ImGui::IsWindowHovered())hit(node->id,0,0);
        ImGui::End();
    }
    ImGui::PopStyleVar();
}
void confirm_close() {
    auto& g=app();
    if(g.hud_confirm_close)ImGui::OpenPopup("Niezapisany HUD");
    if(ImGui::BeginPopupModal("Niezapisany HUD",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Zapisać zmiany przed zamknięciem?");
        if(ImGui::Button("Zapisz i zamknij")) {if(save_hud()){g.close_requested=true;g.hud_confirm_close=false;ImGui::CloseCurrentPopup();}}
        ImGui::SameLine();if(ImGui::Button("Odrzuć")) {g.close_requested=true;g.hud_confirm_close=false;ImGui::CloseCurrentPopup();}
        ImGui::SameLine();if(ImGui::Button("Anuluj")) {g.hud_confirm_close=false;ImGui::CloseCurrentPopup();}
        ImGui::EndPopup();
    }
}
}
void initialize_hud() {
    auto& g=app();
    std::vector<wchar_t> path(32768);const DWORD count=GetModuleFileNameW(nullptr,path.data(),static_cast<DWORD>(path.size()));
    g.hud_path=count?std::filesystem::path(std::wstring(path.data(),count)).parent_path()/L"editor_hud.layout":std::filesystem::path(L"editor_hud.layout");
    std::string error;
    if(!engine::hud::load(g.hud_path,g.hud,error)) {
        g.hud=engine::hud::defaults();
        std::error_code ec;
        if(std::filesystem::exists(g.hud_path,ec)) {g.hud_message="Nie wczytano HUD: "+error;LOG_WARN("hud","%s",g.hud_message.c_str());}
    }
    g.hud_loaded=true;
    initialize_bindings();
}
bool save_hud() {
    auto& g=app();g.hud.docking=ImGui::SaveIniSettingsToMemory();std::string error;
    if(!engine::hud::save(g.hud_path,g.hud,error)) {g.hud_message=error;LOG_ERROR("hud","%s",error.c_str());return false;}
    g.hud_dirty=false;g.hud_message="Zapisano editor_hud.layout. engine_editor wczyta go przy następnym uruchomieniu.";
    LOG_INFO("hud","Saved editor_hud.layout");return true;
}
void draw_document_ui() {
    auto& g=app();g.hud_hit=g.hud_hit_panel=g.hud_hit_window=0;hit_priority=0;
    chrome();
    auto* status=project_footer();
    const float bottom=(status?std::clamp(status->minimum.y,20.0F,100.0F):0)+(g.hud_designer?24:0);
    const float top=title_height+(g.hud_designer?34:0),height=std::max(1.0F,static_cast<float>(g.height)-top-bottom);
    const float width=std::max(1.0F,static_cast<float>(g.width)-(g.hud_designer&&g.hud_inspector?330:0));
    if(g.hud_designer) {toolbar();if(g.hud_inspector)inspector(top,height);}
    windows({0,top},{width,height});g.docking_initialized=true;
    footer();
    draw_settings_window();
    if(g.hud_designer) {
        context_menu({width,height});
        if(pending.erase) {g.hud.erase(pending.erase);g.hud_inputs.clear();g.hud_selected=0;g.hud_dirty=true;}
        if(pending.add) {
            auto id=g.hud.add(pending.kind,pending.parent,std::string(kind_name(pending.kind))+" "+std::to_string(g.hud.next_id));
            if(id) {
                g.hud_selected=id;g.hud_dirty=true;
                if(pending.kind==Kind::Footer){auto* n=g.hud.find(id);n->text="{status} | {objects} obiektów | {fps} FPS";n->minimum={1,28};}
                if(pending.kind==Kind::Window) {
                    bool view=false;Id bar{};
                    for(const auto& n:g.hud.nodes){view|=n.builtin=="view.windows";if(n.kind==Kind::Bar)bar=n.id;}
                    if(!view){auto menu=g.hud.add(Kind::List,bar,"Widok");if(auto* n=g.hud.find(menu))n->builtin="view.windows";}
                }
            }
        }
        pending={};
        if(!g.show_settings && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S,false))g.hud_save_requested=true;
        if(g.hud_save_requested){save_hud();g.hud_save_requested=false;}
        confirm_close();
    } else editor_shortcuts();
    g.menu_open=ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel);
}
}
