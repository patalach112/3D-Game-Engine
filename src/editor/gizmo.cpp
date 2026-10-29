#include "editor_app.hpp"
#include "engine/log.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace editor {

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
    LOG_DEBUG("editor", "Adding primitive type " + std::to_string(static_cast<int>(type)));
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
    g.status = L"Dodano obiekt";
    LOG_DEBUG("editor", "Added " + entity.name);
}

} // namespace editor
