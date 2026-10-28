#include "engine/render/software_renderer.hpp"
#include "engine/scene/scene.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <stdexcept>

int main() {
    engine::Scene scene;
    auto& first = scene.create_entity("Cube");
    const auto first_id = first.id;
    first.transform.position = {1.0F, 2.0F, 3.0F};

    assert(scene.entity_count() == 1);
    assert(scene.find_entity(first_id) != nullptr);
    assert(scene.find_entity(first_id)->transform.position.y == 2.0F);
    assert(scene.find_entity(9999) == nullptr);
    assert(scene.remove_entity(first_id));
    assert(!scene.remove_entity(first_id));
    assert(scene.entity_count() == 0);

    bool rejected_empty_name = false;
    try {
        (void)scene.create_entity("");
    } catch (const std::invalid_argument&) {
        rejected_empty_name = true;
    }
    assert(rejected_empty_name);

    const engine::Vec3 direction{3.0F, 0.0F, 4.0F};
    assert(direction.normalized().length() > 0.999F);

    engine::SoftwareRenderer renderer;
    renderer.resize(128, 96);
    const engine::EditorCamera camera{{5.0F, 4.0F, 7.0F}, {0.0F, 0.0F, 0.0F}, 45.0F};
    engine::Scene empty_scene;
    renderer.render(empty_scene, camera, 0);
    const auto background_pixels = static_cast<std::size_t>(std::count(
        renderer.pixels().begin(), renderer.pixels().end(), 0x00272729U));
    assert(background_pixels > 0);
    assert(background_pixels < renderer.pixels().size()); // Grid is drawn, but no opaque floor fills the view.

    const engine::EditorCamera zoomed_camera{{0.0F, 0.05F, 0.22F}, {0.0F, 0.0F, 0.0F}, 55.0F};
    renderer.render(empty_scene, zoomed_camera, 0);
    const auto zoomed_grid_pixels = static_cast<std::size_t>(std::count_if(
        renderer.pixels().begin(), renderer.pixels().end(), [](std::uint32_t pixel) {
            return pixel != 0x00272729U;
        }));
    assert(zoomed_grid_pixels > 0); // Grid segments survive when their far endpoint clips behind the camera.

    engine::Scene rendered_scene;
    const auto rendered_id = rendered_scene.create_entity("Visible Cube").id;
    renderer.render(rendered_scene, camera, rendered_id);
    const auto rendered_pixels = static_cast<std::size_t>(std::count_if(
        renderer.pixels().begin(), renderer.pixels().end(), [](std::uint32_t pixel) {
            return pixel != 0x00272729U;
        }));
    assert(rendered_pixels > renderer.pixels().size() / 10);

    engine::Scene primitives;
    const auto sphere = primitives.create_entity("Sphere", engine::PrimitiveType::Sphere).id;
    const auto cylinder = primitives.create_entity("Cylinder", engine::PrimitiveType::Cylinder).id;
    const auto plane = primitives.create_entity("Plane", engine::PrimitiveType::Plane).id;
    assert(primitives.find_entity(sphere)->primitive == engine::PrimitiveType::Sphere);
    assert(primitives.find_entity(cylinder)->primitive == engine::PrimitiveType::Cylinder);
    assert(primitives.find_entity(plane)->primitive == engine::PrimitiveType::Plane);
    renderer.render(primitives, camera, sphere);
    const auto primitive_pixels = static_cast<std::size_t>(std::count_if(
        renderer.pixels().begin(), renderer.pixels().end(), [](std::uint32_t pixel) {
            return pixel != 0x00272729U;
        }));
    assert(primitive_pixels > renderer.pixels().size() / 10);

    // A very large object must keep its faces even when the camera sits inside it:
    // triangles are clipped against the near plane instead of being dropped.
    engine::Scene huge_scene;
    const auto huge_id = huge_scene.create_entity("Huge Cube").id;
    huge_scene.find_entity(huge_id)->transform.position = {0.0F, 0.0F, 0.0F};
    huge_scene.find_entity(huge_id)->transform.scale = {60.0F, 60.0F, 60.0F};
    const engine::EditorCamera inside_camera{{1.0F, 1.0F, 1.0F}, {0.0F, 0.0F, 0.0F}, 45.0F};
    renderer.render(huge_scene, inside_camera, huge_id);
    const auto huge_pixels = static_cast<std::size_t>(std::count_if(
        renderer.pixels().begin(), renderer.pixels().end(), [](std::uint32_t pixel) {
            return pixel != 0x00272729U;
        }));
    assert(huge_pixels > 0); // Kamera wewnątrz: krawędzie są rysowane, fill dużych trójkąt jest skipowany (wireframe).

    assert(engine::project_to_screen({0.0F, 0.0F, 0.0F}, camera, 128, 96).visible);
    return 0;
}
