#include "engine/hud/document.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace engine::hud {
Node* Document::find(Id id) { for (auto& n:nodes) if(n.id==id) return &n; return nullptr; }
const Node* Document::find(Id id) const { for (auto& n:nodes) if(n.id==id) return &n; return nullptr; }
bool accepts(Kind parent,Kind child) {
    if (child==Kind::Bar || child==Kind::Window || child==Kind::Footer) return false;
    if (parent==Kind::Bar || parent==Kind::List) return child==Kind::Button || child==Kind::List || child==Kind::Text;
    if (parent==Kind::Window) return child==Kind::Panel || child==Kind::Input || child==Kind::Text || child==Kind::List;
    return parent==Kind::Panel && child!=Kind::Window && child!=Kind::Bar;
}
Id Document::add(Kind kind,Id parent,const std::string& text) {
    if(nodes.size()>=4096 || next_id==0 || next_id==std::numeric_limits<Id>::max() || kind<Kind::Bar || kind>Kind::Footer) return 0;
    const auto* p=find(parent);
    if(parent?(!p || !accepts(p->kind,kind)):(kind!=Kind::Bar && kind!=Kind::Window && kind!=Kind::Footer)) return 0;
    const Node* ancestor=p;unsigned depth=0;
    while(ancestor) {if(++depth>32) return 0;ancestor=find(ancestor->parent);}
    if(kind==Kind::Bar || kind==Kind::Footer) for(const auto& n:nodes) if(n.kind==kind) return 0;
    Node n; n.id=next_id++; n.kind=kind; n.parent=parent; n.text=text;
    if(kind==Kind::Window) { n.minimum={180,120}; n.rect={.2F,.2F,.4F,.4F}; }
    if(kind==Kind::Panel) n.rect={0,0,1,1};
    if(kind==Kind::Button || kind==Kind::List || kind==Kind::Input || kind==Kind::Text) n.minimum={80,26};
    nodes.push_back(n); return n.id;
}
void Document::erase(Id id) {
    const auto* n=find(id); if(!n || n->kind==Kind::Bar) return;
    std::set<Id> remove{id}; bool more=true;
    while(more) { more=false; for(const auto& item:nodes) if(remove.count(item.parent) && !remove.count(item.id)) { remove.insert(item.id); more=true; } }
    std::erase_if(nodes,[&](const Node& item){ return remove.count(item.id)!=0; });
}
std::string window_name(const Node& n) { return n.text+"###HUD_Window_"+std::to_string(n.id); }
bool Document::validate(std::string& error) const {
    const auto fail=[&](const char* e){error=e;return false;};
    if(!validate_bindings(bindings,error))return false;
    const auto scalar=[](float x,float lo,float hi){return std::isfinite(x)&&x>=lo&&x<=hi;};
    const auto size=[&](Size s,float lo,float hi){return scalar(s.x,lo,hi)&&scalar(s.y,lo,hi);};
    if(nodes.empty()||nodes.size()>4096||!size(window_minimum,300,8192)||!scalar(default_rounding,0,256)||docking.size()>1048576) return fail("Nieprawidłowe ustawienia HUD-u");
    std::size_t budget=docking.size()*2;
    std::set<Id> ids; Id maximum{}; int bars=0, scenes=0, footers=0;
    for(const auto& n:nodes) {
        if(!n.id || !ids.insert(n.id).second) return fail("Powtórzony identyfikator");
        budget+=512+2*(n.text.size()+n.action.size()+n.builtin.size()+n.value.size());
        if(budget>2097152) return fail("HUD przekracza limit rozmiaru pliku");
        maximum=std::max(maximum,n.id);
        if(n.kind<Kind::Bar || n.kind>Kind::Footer || n.dock<Dock::Floating || n.dock>Dock::Bottom) return fail("Nieznany typ elementu");
        if(n.kind==Kind::Bar) ++bars;
        if(n.kind==Kind::Footer) ++footers;
        if(n.builtin=="scene") ++scenes;
        if(n.text.size()>1024||n.action.size()>128||n.builtin.size()>64||n.value.size()>4096) return fail("Zbyt długi tekst");
        if(!size(n.minimum,1,8192)||!size(n.button_minimum,1,2048)||!size(n.gap,0,256)||!size(n.padding,0,256)||!scalar(n.rounding,0,256)) return fail("Nieprawidłowe rozmiary");
        if(!scalar(n.rect.x,0,1)||!scalar(n.rect.y,0,1)||!scalar(n.rect.w,.01F,1)||!scalar(n.rect.h,.01F,1)) return fail("Nieprawidłowy obszar względny");
        if(n.parent) { const auto* p=find(n.parent); if(!p||!accepts(p->kind,n.kind)) return fail("Niedozwolony rodzic elementu"); }
        else if(n.kind!=Kind::Bar && n.kind!=Kind::Window && n.kind!=Kind::Footer) return fail("Element bez kontenera");
        const Node* p=&n; unsigned depth=0;
        while(p->parent) { if(++depth>32) return fail("Cykl lub zbyt głębokie menu"); p=find(p->parent); if(!p) return fail("Brak rodzica"); }
    }
    if(bars!=1||footers>1||scenes>1||next_id<=maximum) return fail("Nieprawidłowy pasek, widok sceny lub licznik ID");
    error.clear(); return true;
}
Document defaults() {
    Document d;
    const auto bar=d.add(Kind::Bar,0,"STUDIO / VULKAN");
    const auto list=[&](Id p,const char* text){return d.add(Kind::List,p,text);};
    const auto button=[&](Id p,const char* text,const char* action){const auto id=d.add(Kind::Button,p,text);d.find(id)->action=action;return id;};
    auto f=list(bar,"Plik");
    d.find(button(f,"Import",""))->enabled=false; d.find(button(f,"Eksport",""))->enabled=false;
    button(f,"Zapisz scenę","scene.save"); button(f,"Ustawienia","settings"); button(f,"Zamknij","app.close");
    auto t=list(bar,"Narzędzia");
    button(t,"Przesuwanie","tool.move");button(t,"Obracanie","tool.rotate");button(t,"Skalowanie","tool.scale");
    button(t,"Reset kamery","camera.reset");button(t,"Wycentruj zaznaczony","camera.frame");
    auto add=list(bar,"Dodaj"),o=list(add,"Obiekty"),p=list(add,"Płaszczyzna");
    button(o,"Kostka","object.cube");button(o,"Kula","object.sphere");button(o,"Walec","object.cylinder");button(o,"Płaszczyzna 3D","object.plane");
    button(p,"Koło 2D","object.circle");button(p,"Kwadrat 2D","object.square");
    auto v=list(bar,"Widok");d.find(v)->builtin="view.windows";button(v,"Przywróć układ","layout.reset");
    const auto window=[&](const char* name,const char* content,Dock dock,Rect rect,bool visible=true) {
        const auto w=d.add(Kind::Window,0,name);auto* n=d.find(w);n->dock=dock;n->rect=rect;n->visible=visible;
        auto panel=d.add(Kind::Panel,w,name);d.find(panel)->builtin=content;
        if(std::string(content)=="scene") {
            button(panel,"Ruch","tool.move");button(panel,"Obrót","tool.rotate");button(panel,"Skala","tool.scale");
        }
    };
    window("Scena 3D","scene",Dock::Center,{.19F,0,.59F,.7F});
    window("Hierarchia","hierarchy",Dock::Left,{0,0,.19F,1});
    window("Inspektor","inspector",Dock::Right,{.78F,0,.22F,1});
    window("Konsola","console",Dock::Bottom,{.19F,.7F,.59F,.3F});
    window("Zasoby","resources",Dock::Bottom,{.19F,.7F,.59F,.3F});
    window("Notatki","notes",Dock::Bottom,{.19F,.7F,.59F,.3F});
    window("Renderer","renderer",Dock::Bottom,{.19F,.7F,.59F,.3F});

    const auto footer=d.add(Kind::Footer,0,"{status} | {objects} obiektów | {fps} FPS | {limit}");
    d.find(footer)->minimum={1,28};
    return d;
}
bool load(const std::filesystem::path& path,Document& out,std::string& error) {
    try {
        std::error_code ec;const auto bytes=std::filesystem::file_size(path,ec);
        if(ec||bytes>2097152) {error="Brak pliku lub plik HUD jest zbyt duży";return false;}
        std::ifstream f(path,std::ios::binary); Document d;std::string magic;int version{};std::size_t count{};
        if(!(f>>magic>>version)||magic!="STUDIO_HUD"||(version!=1&&version!=2)) {error="Nieznany format HUD";return false;}
        if(!(f>>d.next_id>>d.window_minimum.x>>d.window_minimum.y>>d.default_rounding>>count)||count>4096) {error="Uszkodzony nagłówek HUD";return false;}
        for(std::size_t i=0;i<count;++i) {
            Node n;int kind{},dock{};
            if(!(f>>n.id>>n.parent>>kind>>dock>>n.visible>>n.enabled>>n.rounded>>n.shared_rounding>>n.rounding
                >>n.minimum.x>>n.minimum.y>>n.gap.x>>n.gap.y>>n.padding.x>>n.padding.y>>n.button_minimum.x>>n.button_minimum.y
                >>n.rect.x>>n.rect.y>>n.rect.w>>n.rect.h>>std::quoted(n.text)>>std::quoted(n.action)>>std::quoted(n.builtin)>>std::quoted(n.value))) {error="Uszkodzony element HUD";return false;}
            n.kind=static_cast<Kind>(kind);n.dock=static_cast<Dock>(dock);d.nodes.push_back(n);
        }
        if(!(f>>std::quoted(d.docking))) {error="Brak układu dokowania";return false;}
        if(version==2) {
            std::size_t total{};if(!(f>>total)||total>512){error="Błędna liczba bindów";return false;}
            d.bindings.clear();
            for(std::size_t i=0;i<total;++i){Binding b;if(!(f>>std::quoted(b.action)>>b.key>>b.modifiers)){error="Uszkodzony bind HUD";return false;}d.bindings.push_back(b);}
        }
        // Settings is application chrome now, never a deletable/dockable HUD node.
        std::vector<Id> legacy;
        for(auto& n:d.nodes)if(n.builtin=="settings"){n.builtin.clear();legacy.push_back(n.id);}
        for(auto id:legacy) {
            const auto* n=d.find(id);const auto parent=n?n->parent:0;
            const auto children=[&](Id p){return std::any_of(d.nodes.begin(),d.nodes.end(),[&](const Node& c){return c.parent==p;});};
            if(!children(id))d.erase(id);
            if(parent&&!children(parent))d.erase(parent);
        }
        f>>std::ws;if(!f.eof()) {error="Dodatkowe dane po dokumencie";return false;}
        if(!d.validate(error)) return false;
        out=std::move(d);return true;
    } catch(const std::exception& e) {error=e.what();return false;}
}
bool save(const std::filesystem::path& path,const Document& doc,std::string& error) {
    if(!doc.validate(error)) return false;
    auto temp=path;temp+=".tmp";
    try {
        std::ofstream f(temp,std::ios::binary|std::ios::trunc);
        f<<"STUDIO_HUD 2\n"<<doc.next_id<<' '<<doc.window_minimum.x<<' '<<doc.window_minimum.y<<' '<<doc.default_rounding<<' '<<doc.nodes.size()<<'\n'<<std::setprecision(9);
        for(const auto& n:doc.nodes) f<<n.id<<' '<<n.parent<<' '<<static_cast<int>(n.kind)<<' '<<static_cast<int>(n.dock)<<' '
            <<n.visible<<' '<<n.enabled<<' '<<n.rounded<<' '<<n.shared_rounding<<' '<<n.rounding<<' '
            <<n.minimum.x<<' '<<n.minimum.y<<' '<<n.gap.x<<' '<<n.gap.y<<' '<<n.padding.x<<' '<<n.padding.y<<' '<<n.button_minimum.x<<' '<<n.button_minimum.y<<' '
            <<n.rect.x<<' '<<n.rect.y<<' '<<n.rect.w<<' '<<n.rect.h<<' '
            <<std::quoted(n.text)<<' '<<std::quoted(n.action)<<' '<<std::quoted(n.builtin)<<' '<<std::quoted(n.value)<<'\n';
        f<<std::quoted(doc.docking)<<'\n'<<doc.bindings.size()<<'\n';
        for(const auto& b:doc.bindings)f<<std::quoted(b.action)<<' '<<b.key<<' '<<b.modifiers<<'\n';
        f.flush();if(!f) {error="Nie można zapisać HUD";return false;}f.close();if(!f) {error="Błąd zamknięcia pliku HUD";return false;}
#ifdef _WIN32
        if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {error="Nie można zastąpić pliku HUD";return false;}
#else
        std::filesystem::rename(temp,path);
#endif
        error.clear();return true;
    } catch(const std::exception& e) {error=e.what();std::error_code ec;std::filesystem::remove(temp,ec);return false;}
}
}
