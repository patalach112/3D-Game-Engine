#include "engine/camera_controller.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
int main() {
    engine::OrbitState target;
    target.yaw=2; target.pitch=-.8F; target.distance=20;
    auto at30=engine::OrbitState{},at240=at30;
    for (int i=0;i<30;++i) engine::smooth_orbit(at30,target,1.0F/30);
    for (int i=0;i<240;++i) engine::smooth_orbit(at240,target,1.0F/240);
    assert(std::abs(at30.yaw-at240.yaw)<1e-5F);
    assert(std::abs(at30.pitch-at240.pitch)<1e-5F);
    assert(std::abs(at30.distance-at240.distance)<1e-4F);
    engine::OrbitState displayed;
    const auto original=displayed;
    engine::smooth_orbit(displayed,target,0);
    assert(displayed.yaw==original.yaw && displayed.distance==original.distance);
    engine::smooth_orbit(displayed,target,1.0F/60);
    assert(displayed.yaw>original.yaw && displayed.yaw<target.yaw);
    assert(displayed.distance>original.distance && displayed.distance<target.distance);
    displayed.yaw=3.13F; target.yaw=-3.13F;
    engine::smooth_orbit(displayed,target,.01F);
    assert(std::abs(displayed.yaw)>3.12F); // shortest path across +/-pi, not through zero
    engine::smooth_orbit(displayed,target,100);
    assert(std::abs(displayed.yaw-target.yaw)<1e-5F && displayed.distance==target.distance);
    // A full gesture between rendered frames still contributes its movement.
    engine::OrbitState gesture;
    engine::OrbitInput press;
    press.mouse={100,100}; press.rightDown=true; press.rightPressed=true;
    engine::update_orbit(gesture,press,{0,0,500,500});
    press.rightPressed=false; press.mouse={140,120};
    engine::update_orbit(gesture,press,{0,0,500,500});
    press.rightDown=false;
    engine::update_orbit(gesture,press,{0,0,500,500});
    assert(!gesture.dragging && gesture.yaw<.75F && gesture.pitch>.42F);
    bool rejected=false;
    try { engine::smooth_orbit(displayed,target,std::numeric_limits<float>::quiet_NaN()); }
    catch (const std::invalid_argument&) { rejected=true; }
    assert(rejected);
    std::cout << "Camera damping: 30/240 FPS equivalence, yaw wrap, bounds and invalid delta passed\n";
}
