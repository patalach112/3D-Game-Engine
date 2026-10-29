#include "engine/camera_controller.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace engine {
bool update_orbit(OrbitState& s,const OrbitInput& i,RenderViewport v) {
    if (!i.focused || !i.pointerValid || !std::isfinite(i.mouse.x)||!std::isfinite(i.mouse.y)) { s.dragging=false; return false; }
    const bool inside=i.mouse.x>=v.x && i.mouse.y>=v.y && static_cast<double>(i.mouse.x)<static_cast<double>(v.x)+v.width
        &&static_cast<double>(i.mouse.y)<static_cast<double>(v.y)+v.height &&v.width>0&&v.height>0;
    bool changed=false;
    if (!i.rightDown) s.dragging=false;
    if (i.rightPressed && i.rightDown && inside && !i.blockStart) { s.dragging=true; s.lastMouse=i.mouse; }
    if (s.dragging && i.rightDown) {
        const double dx=static_cast<double>(i.mouse.x)-s.lastMouse.x,dy=static_cast<double>(i.mouse.y)-s.lastMouse.y;
        s.yaw=static_cast<float>(std::remainder(s.yaw-dx*0.006,6.283185307179586));
        s.pitch=static_cast<float>(std::clamp(s.pitch+dy*0.006,-1.4,1.4));
        s.lastMouse=i.mouse; changed=dx!=0||dy!=0;
    }
    if (inside&&!i.blockStart&&std::isfinite(i.wheel)&&i.wheel!=0) {
        const float old=s.distance; s.distance=std::clamp(s.distance-i.wheel*0.7F,2.0F,40.0F); changed|=old!=s.distance;
    }
    return changed;
}
void apply_orbit(EditorCamera& camera,const OrbitState& s) {
    if (!std::isfinite(s.yaw)||!std::isfinite(s.pitch)||!std::isfinite(s.distance)||s.distance<=0)
        throw std::invalid_argument("Invalid orbit state");
    const float cp=std::cos(s.pitch);
    camera.position={camera.target.x+s.distance*cp*std::sin(s.yaw),camera.target.y+s.distance*std::sin(s.pitch),
        camera.target.z+s.distance*cp*std::cos(s.yaw)};
}
}
