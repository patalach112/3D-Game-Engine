#include "engine/hud/document.hpp"
#include "engine/render/grid.hpp"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <iomanip>
#include <chrono>
#include <atomic>

namespace {
void legacy_save(const std::filesystem::path& path,const engine::hud::Document& doc,int version) {
    std::ofstream f(path);f<<std::setprecision(9)<<"STUDIO_HUD "<<version<<'\n'
        <<doc.next_id<<' '<<doc.window_minimum.x<<' '<<doc.window_minimum.y<<' '<<doc.default_rounding<<' '<<doc.nodes.size()<<'\n';
    for(const auto& n:doc.nodes)f<<n.id<<' '<<n.parent<<' '<<static_cast<int>(n.kind)<<' '<<static_cast<int>(n.dock)<<' '
        <<n.visible<<' '<<n.enabled<<' '<<n.rounded<<' '<<n.shared_rounding<<' '<<n.rounding<<' '
        <<n.minimum.x<<' '<<n.minimum.y<<' '<<n.gap.x<<' '<<n.gap.y<<' '<<n.padding.x<<' '<<n.padding.y<<' '<<n.button_minimum.x<<' '<<n.button_minimum.y<<' '
        <<n.rect.x<<' '<<n.rect.y<<' '<<n.rect.w<<' '<<n.rect.h<<' '
        <<std::quoted(n.text)<<' '<<std::quoted(n.action)<<' '<<std::quoted(n.builtin)<<' '<<std::quoted(n.value)<<'\n';
    f<<std::quoted(doc.docking)<<'\n';
    if(version>=2) {f<<doc.bindings.size()<<'\n';for(const auto& b:doc.bindings)f<<std::quoted(b.action)<<' '<<b.key<<' '<<b.modifiers<<'\n';}
}
void benchmark() {
    using namespace engine::hud;
    Document doc;Node root;root.id=1;doc.nodes.push_back(root);
    for(Id p=2;p<402;++p) {
        Node panel;panel.id=p;panel.parent=1;doc.nodes.push_back(panel);
        for(Id c=0;c<3;++c) {Node child;child.id=1000+p*3+c;child.parent=p;doc.nodes.push_back(child);}
    }
    HierarchyIndex index;index.update(doc);
    std::uint64_t sum{};
    const auto start=std::chrono::steady_clock::now();
    for(int frame=0;frame<100;++frame) {
        std::atomic_signal_fence(std::memory_order_seq_cst);
        for(const auto& parent:doc.nodes)for(const auto& child:doc.nodes)if(child.parent==parent.id)++sum;
    }
    const auto middle=std::chrono::steady_clock::now();
    for(int frame=0;frame<100;++frame) {
        std::atomic_signal_fence(std::memory_order_seq_cst);index.update(doc);
        for(const auto& parent:doc.nodes)sum+=index.children(parent.id).size();
    }
    const auto end=std::chrono::steady_clock::now();
    std::cout<<"Hierarchy CPU / frame, "<<doc.nodes.size()<<" nodes: full scan "
        <<std::chrono::duration<double,std::micro>(middle-start).count()/100<<" us, cached index "
        <<std::chrono::duration<double,std::micro>(end-middle).count()/100<<" us, checksum "<<sum<<'\n';
}
}
int main(int argc,char**) {
    using namespace engine::hud;
    Document d=defaults();std::string error;
    assert(d.validate(error));
    auto bar=d.nodes.front().id;
    assert(!d.add(Kind::Input,bar,"invalid"));
    assert(!d.add(Kind::Window,bar,"invalid"));
    auto w=d.add(Kind::Window,0,"Custom \"Window\" / ąęść");
    auto panel=d.add(Kind::Panel,w,"Panel");
    auto button=d.add(Kind::Button,panel,"Save");d.find(button)->action="scene.save";
    auto list=d.add(Kind::List,panel,"Tools"),nested=d.add(Kind::List,list,"Nested");
    auto leaf=d.add(Kind::Button,nested,"Reset");d.find(leaf)->action="camera.reset";
    d.find(panel)->shared_rounding=false;d.find(panel)->rounding=21;
    d.find(panel)->gap={17,23};d.find(panel)->padding={3,9};
    auto input=d.add(Kind::Input,panel,"Text");d.find(input)->value="hello\n\"world\"\\";
    d.docking="[Docking][Data]\nDockSpace ID=0x1234\n";
    d.bindings[0].key='T'; // v2/v3 must preserve authored bindings, v1 falls back to defaults.
    d.palette[ColorRole::Accent]={.8F,.2F,.7F,.75F};
    d.find(panel)->custom_colors=true;d.find(panel)->palette[ColorRole::Panel]={.2F,.3F,.8F,.5F};
    assert(d.validate(error));
    const auto path=std::filesystem::temp_directory_path()/"studio_hud_roundtrip.layout";
    assert(save(path,d,error));Document read;
    assert(load(path,read,error));assert(read.nodes.size()==d.nodes.size());
    assert(read.find(w)->text==d.find(w)->text);assert(read.docking==d.docking);
    assert(read.find(input)->value==d.find(input)->value);
    assert(read.find(panel)->rounding==21 && read.find(panel)->gap.y==23);
    assert(window_name(*read.find(w))==window_name(*d.find(w)));
    assert(read.custom_colors && read.palette==d.palette);
    assert(read.find(panel)->custom_colors && read.find(panel)->palette==d.find(panel)->palette);
    assert(!read.find(button)->custom_colors);
    // Bad palettes are rejected even when the override is currently disabled.
    auto invalid=d;invalid.palette[ColorRole::Text][0]=std::numeric_limits<float>::infinity();assert(!invalid.validate(error));
    invalid=d;invalid.find(button)->palette[ColorRole::Active][3]=-1;assert(!invalid.validate(error));
    invalid=d;invalid.find(panel)->palette[ColorRole::Border][1]=1.01F;assert(!invalid.validate(error));
    invalid=d;invalid.palette[ColorRole::Text][0]=std::numeric_limits<float>::quiet_NaN();assert(!save(path,invalid,error));
    assert(load(path,read,error) && read.palette==d.palette);
    for(int version:{1,2}) {
        legacy_save(path,d,version);assert(load(path,read,error));
        assert(!read.custom_colors && !read.find(panel)->custom_colors);
        assert(read.find(input)->value==d.find(input)->value && read.docking==d.docking);
        assert(read.bindings==(version==1?basic_bindings():d.bindings));
    }
    assert(save(path,d,error));
    HierarchyIndex index;assert(index.update(d));
    assert(index.children(nested).size()==1 && d.nodes[index.children(nested)[0]].id==leaf);
    assert(index.children(999999).empty());
    for(int i=0;i<20;++i)assert(!index.update(d));
    d.find(button)->text="New label";d.find(button)->visible=false;d.palette[ColorRole::Text][0]=.4F;
    assert(!index.update(d) && index.builds==1 && index.hits==21);
    d.find(button)->parent=list;assert(index.update(d));assert(index.children(list).size()==2);
    d.find(button)->parent=panel;assert(index.update(d));
    std::swap(d.nodes[0],d.nodes[1]);assert(index.update(d));
    auto sparse=d;auto* changed=sparse.find(button);changed->id=std::numeric_limits<Id>::max();
    assert(index.update(sparse));assert(!index.children(panel).empty());
    assert(index.update(d));
    auto broken=d;broken.find(panel)->parent=panel;assert(!broken.validate(error));
    broken=d;broken.find(w)->minimum.x=std::numeric_limits<float>::quiet_NaN();assert(!broken.validate(error));
    broken=d;broken.nodes.back().id=bar;assert(!broken.validate(error));
    broken=d;broken.find(panel)->parent=999999;assert(!broken.validate(error));
    broken=d;broken.find(w)->rect.w=0;assert(!save(path,broken,error));
    assert(load(path,read,error)); // A failed save never destroys the previous document.
    d.erase(list);assert(!d.find(list)&&!d.find(nested)&&!d.find(leaf));assert(d.find(button));
    Document deep=defaults();auto deep_parent=deep.add(Kind::List,deep.nodes.front().id,"0");
    for(int i=0;i<31;++i){deep_parent=deep.add(Kind::List,deep_parent,"Nested");assert(deep_parent);}
    assert(!deep.add(Kind::List,deep_parent,"Too deep"));assert(deep.validate(error));
    assert(!deep.add(Kind::Footer,0,"duplicate"));
    d.erase(bar);assert(d.find(bar));d.erase(w);assert(!d.find(panel));assert(d.validate(error));
    {std::ofstream out(path);out<<"STUDIO_HUD 99\n";}assert(!load(path,read,error));
    {std::ofstream out(path);out<<"STUDIO_HUD 1\n1 900 600 10 999999999\n";}assert(!load(path,read,error));
    std::filesystem::remove(path);
    engine::EditorCamera camera;camera.target={3,-2,7};
    for(float zoom:{2.0F,4.0F,8.0F,16.0F,32.0F}) {
        camera.position={camera.target.x+zoom,camera.target.y,camera.target.z};
        const float spacing=engine::grid_spacing(camera);
        for(int yaw=0;yaw<360;yaw+=3)for(int pitch=-80;pitch<90;pitch+=7) {
            const float y=static_cast<float>(yaw)*.0174532925F,p=static_cast<float>(pitch)*.0174532925F;
            camera.position={camera.target.x+zoom*std::cos(p)*std::cos(y),camera.target.y+zoom*std::sin(p),camera.target.z+zoom*std::cos(p)*std::sin(y)};
            assert(engine::grid_spacing(camera)==spacing);
        }
    }
    camera.target={0,0,0};camera.position={4,0,0};auto a=engine::grid_spacing(camera);
    camera.position={32,0,0};assert(engine::grid_spacing(camera)>a);
    if(argc>1)benchmark();
    std::cout<<"HUD colours, legacy roundtrip, hierarchy invalidation and zoom-only grid passed\n";
}
