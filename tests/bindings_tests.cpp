#include "engine/hud/document.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
int main() {
    using namespace engine::hud;std::string error;
    auto basic=basic_bindings();assert(validate_bindings(basic,error));
    assert(binding_label({"scene.save",'S',Ctrl})=="Ctrl+S");
    assert(binding_label({"",0,0})=="Nieprzypisany");
    auto bad=basic;bad.push_back(basic.front());assert(!validate_bindings(bad,error));
    bad=basic;bad.front().action="execute.arbitrary.script";assert(!validate_bindings(bad,error));
    bad=basic;bad.front().key=999;assert(!validate_bindings(bad,error));
    bad=basic;bad.front().modifiers=16;assert(!validate_bindings(bad,error));
    assert(!valid_key(299)&&valid_key(300)&&valid_key(323)&&!valid_key(324));
    auto hash=bindings_fingerprint(basic);bad=basic;bad.front().key='T';assert(hash!=bindings_fingerprint(bad));
    const auto path=std::filesystem::temp_directory_path()/"studio_bindings_test.cfg";
    assert(save_bindings(path,hash,basic,error));std::vector<Binding> read;
    assert(load_bindings(path,hash,read,error)&&read==basic);
    assert(!load_bindings(path,hash+1,read,error));assert(read==basic);
    auto doc=defaults();doc.bindings.front().key='T';
    assert(save(path,doc,error));Document loaded;assert(load(path,loaded,error));assert(loaded.bindings==doc.bindings);
    // A v1 document migrates basic bindings and removes legacy settings panes.
    auto w=doc.add(Kind::Window,0,"Ustawienia");auto panel=doc.add(Kind::Panel,w,"Ustawienia");doc.find(panel)->builtin="settings";
    assert(save(path,doc,error));std::ifstream input(path,std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(input)),{});input.close();
    text.replace(0,12,"STUDIO_HUD 1");
    const auto tail=text.rfind("\n"+std::to_string(doc.bindings.size())+"\n");assert(tail!=std::string::npos);text.resize(tail+1);
    {std::ofstream output(path,std::ios::binary);output<<text;}
    assert(load(path,loaded,error));assert(loaded.bindings==basic);assert(!loaded.find(w));
    bad=basic;bad.push_back(basic.front());doc.bindings=bad;assert(!save(path,doc,error));
    std::filesystem::remove(path);
    std::cout<<"Bindings defaults, validation, persistence and v1 migration passed\n";
}
