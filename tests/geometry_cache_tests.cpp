#include "engine/render/internal_types.hpp"
#include <cassert>
#include <iostream>
int main() {
    engine::Scene scene;const auto id=scene.create_entity("Cube").id;
    engine::EditorCamera camera;engine::internal::FrameGeometryCache cache;
    auto update=[&]{return &cache.update(scene,camera,id,800,600,true,0);};
    const auto* first=update();const auto bytes=first->geometry.bytes;
    for(int i=0;i<100;++i)assert(update()==first);
    assert(cache.builds==1&&cache.hits==100&&cache.frame.geometry.bytes==bytes);
    scene.find_entity(id)->name="Rename does not change geometry";update();assert(cache.builds==1);
    scene.find_entity(id)->transform.position.x=3;update();assert(cache.builds==2);
    scene.find_entity(id)->transform.rotation.y=20;update();assert(cache.builds==3);
    scene.find_entity(id)->transform.scale={2,3,4};update();assert(cache.builds==4);
    camera.position.x+=1;update();assert(cache.builds==5);
    camera.target.y+=1;update();assert(cache.builds==6);
    camera.vertical_fov_degrees=60;update();assert(cache.builds==7);
    (void)cache.update(scene,camera,id,900,600,true,0);assert(cache.builds==8);
    (void)cache.update(scene,camera,id,900,600,false,0);assert(cache.builds==9);
    (void)cache.update(scene,camera,id,900,600,false,1);assert(cache.builds==10);
    (void)cache.update(scene,camera,0,900,600,false,1);assert(cache.builds==11);
    const auto extra=scene.create_entity("Sphere",engine::PrimitiveType::Sphere).id;update();assert(cache.builds==12);
    assert(scene.remove_entity(extra));update();assert(cache.builds==13);
    scene.find_entity(id)->primitive=engine::PrimitiveType::Circle2D;update();assert(cache.builds==14);
    cache.frame.geometry.validate();
    std::cout<<"Geometry cache invalidation passed (no GPU)\n";
}
