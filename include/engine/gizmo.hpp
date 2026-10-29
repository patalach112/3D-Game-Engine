#pragma once
#include "engine/render/vulkan_renderer.hpp"
#include <algorithm>
#include <cmath>

namespace engine {
inline constexpr float kGizmoPi=3.14159265358979323846F;
inline Vec3 gizmo_axis(int axis) {
    return axis==0?Vec3{1,0,0}:axis==1?Vec3{0,1,0}:Vec3{0,0,1};
}
inline float gizmo_length(Vec3 camera,Vec3 center) {
    const float x=camera.x-center.x,y=camera.y-center.y,z=camera.z-center.z;
    return std::clamp(std::sqrt(x*x+y*y+z*z)*0.13F,0.8F,2.0F);
}
// Shared by Vulkan drawing and editor picking: rotation rings cannot diverge.
inline Vec3 gizmo_ring_point(Vec3 center,int axis,float radius,float angle) {
    const float c=radius*std::cos(angle),s=radius*std::sin(angle);
    if (axis==0) return {center.x,center.y+c,center.z+s};
    if (axis==1) return {center.x+s,center.y,center.z+c};
    return {center.x+c,center.y+s,center.z};
}
}
