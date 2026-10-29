#include "engine/objects.hpp"
#include "engine/camera_controller.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace engine;
template<class F> void rejects(F f) { bool rejected=false; try { f(); } catch (const std::exception&) { rejected=true; } assert(rejected); }
int main() {
    for (unsigned h=0;h<65536;++h) if ((h&0x7c00)!=0x7c00)
        assert(float_to_half(half_to_float(static_cast<std::uint16_t>(h)))==h);
    assert(float_to_half(1)==0x3c00 && float_to_half(-2)==0xc000);
    assert(float_to_half(1.0F+1.0F/2048)==0x3c00); // round-to-nearest, ties-to-even
    rejects([] { float_to_half(65505); }); rejects([] { float_to_half(std::numeric_limits<float>::quiet_NaN()); });
    const auto n=unpack_normal(pack_normal({-1,0,0})); assert(n.x==-1 && std::abs(n.y)<0.002F);
    rejects([] { pack_normal({0,0,0}); });
    const auto r=unpack_rotation(pack_rotation({-90,450,360}));
    assert(r.x==270 && r.y==90 && r.z==0);
    const std::array<Vertex,3> vertices{{make_vertex({-.5F,-.5F,0},{0,0,1}),make_vertex({.5F,-.5F,0},{0,0,1}),make_vertex({0,.5F,0},{0,0,1})}};
    const std::array<std::uint32_t,3> indices{0,1,2};
    const std::array<Vertex_hud,3> hud{{{{0,0},{1,0,0,1}},{{32,0},{1,0,0,1}},{{0,32},{1,0,0,1}}}};
    GeometryBuilder builder;
    const auto a=builder.add_mesh(vertices,indices);
    builder.add_hud(hud,indices);
    const auto b=builder.add_mesh(vertices,indices);
    builder.add_hud(hud,indices);
    ObjectData object; object.scale=-2; object.texturePair=texture_pair(12,34);
    builder.add_object(a,object); builder.add_object(b,{});
    const auto packed=builder.build(); packed.validate();
    assert(packed.layout.hudVertices==0 && packed.layout.sceneVertices==6*24);
    assert(packed.layout.hudIndices==6*24+6*12 && packed.layout.sceneIndices==6*24+6*12+6*4);
    assert(packed.bytes.size()==6*24+6*12+6*4+6*4);
    assert(packed.commands.size()==2 && packed.objects.size()==2);
    assert(packed.commands[1].firstIndex==3 && packed.commands[1].vertexOffset==3 && packed.commands[1].firstInstance==1);
    assert(packed.objects[0].texturePair==texture_pair(12,34)&&packed.objects[0].radius>=std::sqrt(0.5F)*2);
    std::uint32_t rebased{}; std::memcpy(&rebased,packed.bytes.data()+packed.layout.hudIndices+3*4,4); assert(rebased==3);
    const auto rebuilt=GeometryBuilder::from_buffer(packed).build(); assert(rebuilt.bytes==packed.bytes);
    auto damaged=packed; ++damaged.layout.sceneIndices; rejects([&] { damaged.validate(); });
    damaged=packed; damaged.commands[0].vertexOffset=-999; rejects([&] { damaged.validate(); });
    damaged=packed; damaged.objects[0].position.x=std::numeric_limits<float>::infinity(); rejects([&] { damaged.validate(); });
    rejects([&] { const std::array<std::uint32_t,3> bad{0,1,3}; builder.add_mesh(vertices,bad); });
    rejects([&] { ObjectData bad; bad.radius=.01F; builder.add_object(a,bad); });
    GeometryBuilder empty; empty.build().validate();
    GeometryBuilder onlyHud; onlyHud.add_hud(hud,indices); assert(onlyHud.build().commands.empty());
    OrbitState orbit; OrbitInput input;
    const RenderViewport view{100,50,400,300};
    input.mouse={150,100}; input.rightPressed=true; input.rightDown=true;
    update_orbit(orbit,input,view); assert(orbit.dragging);
    input.rightPressed=false; input.mouse={210,130};
    assert(update_orbit(orbit,input,view)); assert(orbit.yaw<.75F&&orbit.pitch>.42F);
    input.mouse={20,20}; assert(update_orbit(orbit,input,view)); assert(orbit.dragging); // capture outside viewport
    input.focused=false; update_orbit(orbit,input,view); assert(!orbit.dragging);
    input.focused=true; input.mouse={150,100}; input.rightPressed=true; input.blockStart=true;
    update_orbit(orbit,input,view); assert(!orbit.dragging);
    input.rightDown=false; input.rightPressed=false; input.wheel=4;
    const auto distance=orbit.distance; update_orbit(orbit,input,view); assert(orbit.distance==distance);
    input.blockStart=false; update_orbit(orbit,input,view); assert(orbit.distance<distance);
    EditorCamera camera; apply_orbit(camera,orbit); assert(std::isfinite(camera.position.x));
    std::cout<<"Geometry ABI/packing, binary16 exhaustive roundtrip and orbit input passed\n";
}
