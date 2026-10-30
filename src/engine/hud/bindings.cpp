#include "engine/hud/bindings.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <set>
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
namespace {
constexpr Action registry[]{
    {"","Brak funkcji"},{"scene.save","Zapisz scenę"},{"model.import","Importuj model 3D"},{"settings","Ustawienia"},{"settings.bindings","Ustawienia: bindy"},{"settings.graphics","Ustawienia: grafika"},
    {"tool.move","Przesuwanie"},{"tool.rotate","Obracanie"},{"tool.scale","Skalowanie"},
    {"camera.reset","Reset kamery"},{"camera.frame","Wycentruj zaznaczony"},
    {"camera.front","Kamera: przód"},{"camera.top","Kamera: góra"},{"camera.side","Kamera: bok"},{"camera.iso","Kamera: izometryczna"},
    {"camera.zoom_in","Przybliż"},{"camera.zoom_out","Oddal"},
    {"entity.duplicate","Duplikuj zaznaczony"},{"entity.delete","Usuń zaznaczony"},{"entity.reset","Reset transformacji"},
    {"entity.left","Przesuń w lewo"},{"entity.right","Przesuń w prawo"},{"entity.up","Przesuń w górę"},{"entity.down","Przesuń w dół"},
    {"grid.toggle","Pokaż / ukryj siatkę"},{"console.toggle","Pokaż / ukryj konsolę"},
    {"object.cube","Dodaj kostkę"},{"object.sphere","Dodaj kulę"},{"object.cylinder","Dodaj walec"},
    {"object.cone","Dodaj stożek"},{"object.pyramid","Dodaj piramidę"},{"object.plane","Dodaj płaszczyznę 3D"},{"object.circle","Dodaj koło 2D"},{"object.square","Dodaj kwadrat 2D"},
    {"layout.reset","Przywróć układ"},{"app.close","Zamknij aplikację"}
};
constexpr const char* special[]{"Tab","Left","Right","Up","Down","PageUp","PageDown","Home","End","Insert","Delete","Backspace","Space","Enter","Escape",
    "Minus","Equal","Comma","Period","Slash","Semicolon","Apostrophe","LeftBracket","RightBracket","Backslash","Grave","KeypadAdd","KeypadSubtract","KeypadEnter"};
}
std::span<const Action> actions(){return registry;}
bool known_action(const std::string& id){return std::any_of(std::begin(registry),std::end(registry),[&](auto a){return id==a.id;});}
bool valid_key(int key) {
    return key==0 || (key>='A'&&key<='Z') || (key>='0'&&key<='9') ||
        (key>=256&&key<256+static_cast<int>(std::size(special))) || (key>=300&&key<=323) || (key>=400&&key<=404);
}
std::string key_label(int key) {
    if(!key)return "Nieprzypisany";
    if((key>='A'&&key<='Z')||(key>='0'&&key<='9'))return std::string(1,static_cast<char>(key));
    if(key>=256&&key<256+static_cast<int>(std::size(special)))return special[key-256];
    if(key>=300&&key<=323)return "F"+std::to_string(key-299);
    if(key>=400&&key<=404)return "Mysz "+std::to_string(key-399);
    return "Nieznany";
}
std::string binding_label(const Binding& b) {
    if(!b.key)return key_label(0);
    std::string s;if(b.modifiers&Ctrl)s+="Ctrl+";if(b.modifiers&Shift)s+="Shift+";if(b.modifiers&Alt)s+="Alt+";if(b.modifiers&Super)s+="Win+";
    return s+key_label(b.key);
}
std::vector<Binding> basic_bindings() {
    using K=Key;
    return {{"scene.save",'S',Ctrl},{"model.import",'I',Ctrl},{"entity.duplicate",'D',Ctrl},{"camera.frame",'F',0},
        {"tool.move",'Q',0},{"tool.rotate",'W',0},{"tool.scale",'E',0},{"object.cube",'N',0},
        {"entity.delete",static_cast<int>(K::Delete),0},{"entity.left",static_cast<int>(K::Left),0},
        {"entity.right",static_cast<int>(K::Right),0},{"entity.up",static_cast<int>(K::Up),0},{"entity.down",static_cast<int>(K::Down),0},
        {"camera.zoom_in",static_cast<int>(K::Equal),0},{"camera.zoom_out",static_cast<int>(K::Minus),0},
        {"camera.zoom_in",static_cast<int>(K::KeypadAdd),0},{"camera.zoom_out",static_cast<int>(K::KeypadSubtract),0},
        {"camera.front",300,0},{"camera.top",301,0},{"camera.side",302,0},{"camera.iso",303,0},
        {"settings",309,0},{"grid.toggle",'G',0},{"entity.reset",'R',Ctrl}};
}
bool validate_bindings(const std::vector<Binding>& bindings,std::string& error) {
    if(bindings.size()>512){error="Maksymalnie 512 bindów";return false;}
    std::set<std::pair<int,unsigned>> seen;
    for(const auto& b:bindings) {
        if(!known_action(b.action)||!valid_key(b.key)||b.modifiers>15||(!b.key&&b.modifiers)) {error="Nieprawidłowa funkcja lub klawisz binda";return false;}
        if(b.key&&!seen.emplace(b.key,b.modifiers).second) {error="Ten skrót jest już przypisany";return false;}
    }
    error.clear();return true;
}
std::uint64_t bindings_fingerprint(const std::vector<Binding>& bindings) {
    std::uint64_t h=14695981039346656037ULL;
    const auto add=[&](unsigned char c){h=(h^c)*1099511628211ULL;};
    for(const auto& b:bindings) {
        for(unsigned char c:b.action)add(c);
        add(0);
        for(unsigned i=0;i<4;++i)add(static_cast<unsigned char>(static_cast<unsigned>(b.key)>>(i*8)));
        add(static_cast<unsigned char>(b.modifiers));
    }
    return h;
}
bool load_bindings(const std::filesystem::path& path,std::uint64_t hash,std::vector<Binding>& out,std::string& error) {
    try {
        std::error_code ec;auto size=std::filesystem::file_size(path,ec);
        if(ec||size>65536){error="Brak pliku bindów lub przekroczony limit";return false;}
        std::ifstream f(path,std::ios::binary);std::string magic;int version{};std::uint64_t source{};std::size_t count{};
        if(!(f>>magic>>version>>source>>count)||magic!="STUDIO_BINDINGS"||version!=1||source!=hash||count>512){error="Inna wersja domyślnych bindów lub błędny plik";return false;}
        std::vector<Binding> list;list.reserve(count);
        for(std::size_t i=0;i<count;++i){Binding b;if(!(f>>std::quoted(b.action)>>b.key>>b.modifiers)){error="Uszkodzony bind";return false;}list.push_back(b);}
        f>>std::ws;if(!f.eof()){error="Dodatkowe dane po liście bindów";return false;}
        if(!validate_bindings(list,error))return false;
        out=std::move(list);error.clear();return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
bool save_bindings(const std::filesystem::path& path,std::uint64_t hash,const std::vector<Binding>& bindings,std::string& error) {
    if(!validate_bindings(bindings,error))return false;
    auto temp=path;temp+=".tmp";
    try {
        std::ofstream f(temp,std::ios::binary|std::ios::trunc);
        f<<"STUDIO_BINDINGS 1 "<<hash<<' '<<bindings.size()<<'\n';
        for(const auto& b:bindings)f<<std::quoted(b.action)<<' '<<b.key<<' '<<b.modifiers<<'\n';
        f.flush();f.close();if(!f){error="Nie można zapisać bindów";return false;}
#ifdef _WIN32
        if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){error="Nie można zastąpić pliku bindów";return false;}
#else
        std::filesystem::rename(temp,path);
#endif
        error.clear();return true;
    }catch(const std::exception& e){error=e.what();std::error_code ec;std::filesystem::remove(temp,ec);return false;}
}
}
