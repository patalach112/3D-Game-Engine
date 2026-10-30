#include "editor_app.hpp"
#include "engine/objects.hpp"

#include <algorithm>
#include <cmath>

namespace editor {

bool ray_plane(engine::Ray3 ray, engine::Vec3 point, engine::Vec3 normal, engine::Vec3& hit) {
    const float denom = dot(ray.direction, normal);
    if (std::abs(denom) < 0.00001F) return false;
    const float t = dot(sub(point, ray.origin), normal) / denom;
    if (t < 0) return false;
    hit = add(ray.origin, mul(ray.direction, t));
    return true;
}

bool ray_box(engine::Ray3 ray, const engine::Entity& entity, float& distance) {
    // Undo the exact packed Euler rotation used by the GPU, then intersect in
    // local space. Flat primitives have no artificial bounding-box thickness.
    const auto rotation=engine::unpack_rotation(engine::pack_rotation(entity.transform.rotation));
    const auto inverse=[&](engine::Vec3 p) {
        const float rx=-rotation.x*0.01745329252F,ry=-rotation.y*0.01745329252F,rz=-rotation.z*0.01745329252F;
        const engine::Vec3 z{p.x*std::cos(rz)-p.y*std::sin(rz),p.x*std::sin(rz)+p.y*std::cos(rz),p.z};
        const engine::Vec3 y{z.x*std::cos(ry)+z.z*std::sin(ry),z.y,-z.x*std::sin(ry)+z.z*std::cos(ry)};
        return engine::Vec3{y.x,y.y*std::cos(rx)-y.z*std::sin(rx),y.y*std::sin(rx)+y.z*std::cos(rx)};
    };
    ray.origin=inverse(sub(ray.origin,entity.transform.position));
    ray.direction=inverse(ray.direction);
    const bool plane=entity.primitive==engine::PrimitiveType::Plane;
    const bool flat=plane || entity.primitive==engine::PrimitiveType::Circle2D || entity.primitive==engine::PrimitiveType::Square2D;
    if (flat) {
        const float o=plane?ray.origin.y:ray.origin.z,d=plane?ray.direction.y:ray.direction.z;
        const float sx=entity.transform.scale.x,sy=plane?entity.transform.scale.z:entity.transform.scale.y;
        if (std::abs(d)<1e-7F || std::abs(sx)<1e-7F || std::abs(sy)<1e-7F) return false;
        const float t=-o/d;
        if (t<0) return false;
        const auto hit=add(ray.origin,mul(ray.direction,t));
        const float x=hit.x/sx,y=(plane?hit.z:hit.y)/sy;
        if (entity.primitive==engine::PrimitiveType::Circle2D ? x*x+y*y>0.25F : std::abs(x)>0.5F || std::abs(y)>0.5F)
            return false;
        distance=t;
        return true;
    }
    float low = 0.0F, high = 1.0e30F;
    const float origin[3]{ray.origin.x, ray.origin.y, ray.origin.z};
    const float direction[3]{ray.direction.x, ray.direction.y, ray.direction.z};
    const float center[3]{};
    const float half[3]{std::abs(entity.transform.scale.x) * 0.5F,
                         std::abs(entity.transform.scale.y) * 0.5F,
                         std::abs(entity.transform.scale.z) * 0.5F};
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(direction[axis]) < 1.0e-7F) {
            if (origin[axis] < center[axis] - half[axis] || origin[axis] > center[axis] + half[axis]) return false;
            continue;
        }
        float a = (center[axis] - half[axis] - origin[axis]) / direction[axis];
        float b = (center[axis] + half[axis] - origin[axis]) / direction[axis];
        if (a > b) std::swap(a, b);
        low = std::max(low, a);
        high = std::min(high, b);
        if (low > high) return false;
    }
    distance = low;
    return true;
}

} // namespace editor
