#include "editor_app.hpp"

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
    float low = 0.0F, high = 1.0e30F;
    const float origin[3]{ray.origin.x, ray.origin.y, ray.origin.z};
    const float direction[3]{ray.direction.x, ray.direction.y, ray.direction.z};
    const float center[3]{entity.transform.position.x, entity.transform.position.y, entity.transform.position.z};
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
