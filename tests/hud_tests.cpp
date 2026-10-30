#include "engine/hud/document.hpp"
#include "engine/render/grid.hpp"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>

int main() {
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
    assert(d.validate(error));
    const auto path=std::filesystem::temp_directory_path()/"studio_hud_roundtrip.layout";
    assert(save(path,d,error));Document read;
    assert(load(path,read,error));assert(read.nodes.size()==d.nodes.size());
    assert(read.find(w)->text==d.find(w)->text);assert(read.docking==d.docking);
    assert(read.find(input)->value==d.find(input)->value);
    assert(read.find(panel)->rounding==21 && read.find(panel)->gap.y==23);
    assert(window_name(*read.find(w))==window_name(*d.find(w)));
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
    std::cout<<"HUD validation/roundtrip/nested deletion and zoom-only grid passed\n";
}
