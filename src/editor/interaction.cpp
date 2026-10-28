#include "editor_app.hpp"

#include <algorithm>
#include <cmath>
#include <string>

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

bool hit_gizmo(int sx, int sy, int view_w, int view_h, engine::Entity& entity) {
    App& g = app();
    const float d = magnitude(sub(g.camera.position, entity.transform.position));
    const float handle_len = std::clamp(d * 0.13F, 0.8F, 2.0F);
    const int radius = std::clamp(view_h / 75, 8, 14) + 5;
    const engine::Vec3 axes[3]{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    const auto center = engine::project_to_screen(entity.transform.position, g.camera, view_w, view_h);
    if (!center.visible) return false;

    float best = static_cast<float>(radius * radius);
    int chosen = -1;
    float chosen_sign = 1.0F;
    for (int axis = 0; axis < 3; ++axis) {
        for (float sign : {-1.0F, 1.0F}) {
            const auto p = engine::project_to_screen(
                add(entity.transform.position, mul(axes[axis], handle_len * sign)),
                g.camera, view_w, view_h);
            if (!p.visible) continue;
            const float dx = p.x - static_cast<float>(sx);
            const float dy = p.y - static_cast<float>(sy);
            const float dist = dx * dx + dy * dy;
            if (dist < best) { best = dist; chosen = axis; chosen_sign = sign; }
        }
    }
    if (chosen < 0) return false;

    g.drag_axis = chosen;
    g.drag_sign = chosen_sign;
    const engine::Vec3 axis = mul(axes[chosen], chosen_sign);
    const engine::Vec3 view = normalized(sub(g.camera.target, g.camera.position));
    engine::Vec3 normal = sub(view, mul(axis, dot(view, axis)));
    if (magnitude(normal) < 0.0001F) {
        normal = cross(axis, std::abs(axis.y) < 0.9F ? engine::Vec3{0, 1, 0} : engine::Vec3{1, 0, 0});
    }
    g.drag_plane_normal = normalized(normal);

    const auto ray = engine::screen_ray(static_cast<float>(sx), static_cast<float>(sy), g.camera, view_w, view_h);
    if (!ray_plane(ray, entity.transform.position, g.drag_plane_normal, g.drag_start_hit)) {
        g.drag_axis = -1;
        return false;
    }
    g.drag_start_position = entity.transform.position;
    return true;
}

void add_primitive(engine::PrimitiveType type) {
    App& g = app();
    const char* name = "Cube";
    switch (type) {
    case engine::PrimitiveType::Sphere: name = "Sphere"; break;
    case engine::PrimitiveType::Cylinder: name = "Cylinder"; break;
    case engine::PrimitiveType::Plane: name = "Plane"; break;
    default: break;
    }
    auto& entity = g.scene.create_entity(std::string(name) + " " + std::to_string(g.scene.entity_count()), type);
    const float offset = static_cast<float>(g.scene.entity_count() - 1) * 1.25F;
    entity.transform.position = {offset, type == engine::PrimitiveType::Plane ? 0.0F : 0.5F, 0.0F};
    if (type == engine::PrimitiveType::Plane) entity.transform.scale = {3.0F, 0.05F, 3.0F};
    g.selected = entity.id;
    g.active_field = -1;
    g.status = L"Dodano obiekt";
}

} // namespace editor
