#include "engine/render/vulkan_renderer.hpp"
#include "engine/scene/scene.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <cmath>
#include <iostream>
#include <limits>
#include <type_traits>
#include "engine/render/software_renderer.hpp"

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

    static_assert(std::is_same_v<engine::SoftwareRenderer, engine::VulkanRenderer>);
    engine::VulkanRenderer renderer;
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
    assert(huge_pixels > renderer.pixels().size() * 9 / 10); // GPU fills/clips faces even inside the cube.

    // Assert real geometry, not just the grid; exercise every primitive separately.
    renderer.resize(160, 120);
    renderer.render(empty_scene, camera, 0);
    const auto grid_only = renderer.pixels();
    for (auto type : {engine::PrimitiveType::Cube, engine::PrimitiveType::Sphere,
                      engine::PrimitiveType::Cylinder, engine::PrimitiveType::Cone,
                      engine::PrimitiveType::Pyramid, engine::PrimitiveType::Plane}) {
        engine::Scene single;
        auto& object = single.create_entity("Primitive", type);
        object.transform.position = {0, 0.5F, 0};
        object.transform.scale = {2, 2, 2};
        renderer.render(single, camera, 0);
        assert(renderer.pixels() != grid_only);
        const auto unselected = renderer.pixels();
        renderer.render(single, camera, object.id);
        assert(renderer.pixels() != unselected); // Highlight and axis handles.
        object.transform.rotation = {25, 40, 15};
        object.transform.scale = {-2, 1, 3};
        renderer.render(single, camera, 0);
        assert(renderer.pixels() != unselected);
    }

    // Depth test must not depend on insertion order. Background sphere is hidden
    // by the front cube at the center of the viewport.
    const engine::EditorCamera straight{{0, 0.5F, 6}, {0, 0.5F, 0}, 45};
    engine::Scene front_only;
    front_only.create_entity("Front").transform.position = {0, 0.5F, 0};
    renderer.render(front_only, straight, 0);
    const auto center_color = renderer.pixels()[60 * 160 + 80];
    engine::Scene ordered, reversed;
    ordered.create_entity("Front").transform.position = {0, 0.5F, 0};
    ordered.create_entity("Back", engine::PrimitiveType::Sphere).transform.position = {0, 0.5F, -2};
    reversed.create_entity("Back", engine::PrimitiveType::Sphere).transform.position = {0, 0.5F, -2};
    reversed.create_entity("Front").transform.position = {0, 0.5F, 0};
    renderer.render(ordered, straight, 0);
    const auto ordered_image = renderer.pixels();
    assert(ordered_image[60 * 160 + 80] == center_color);
    renderer.render(reversed, straight, 0);
    assert(renderer.pixels() == ordered_image);

    // An object entirely behind the camera must contribute no pixels.
    engine::Scene behind;
    behind.create_entity("Behind").transform.position = {0, 0.5F, 20};
    renderer.render(empty_scene, straight, 0);
    const auto empty_straight = renderer.pixels();
    renderer.render(behind, straight, 0);
    assert(renderer.pixels() == empty_straight);

    // Camera inside a sphere also retains filled faces (no edge-list dependency).
    engine::Scene enclosing_sphere;
    enclosing_sphere.create_entity("Enclosing", engine::PrimitiveType::Sphere).transform.scale = {60, 60, 60};
    renderer.render(enclosing_sphere, inside_camera, 0);
    assert(std::count(renderer.pixels().begin(), renderer.pixels().end(), 0x00272729U)
           < static_cast<std::ptrdiff_t>(renderer.pixels().size() / 10));

    // A near-plane crossing triangle is clipped by Vulkan, not dropped by CPU.
    engine::Scene crossing;
    crossing.create_entity("Crossing").transform.position = {0, 0.5F, 0};
    const engine::EditorCamera close_camera{{0.1F, 0.5F, 0.55F}, {0, 0.5F, 0}, 55};
    renderer.render(empty_scene, close_camera, 0);
    const auto empty_close = renderer.pixels();
    renderer.render(crossing, close_camera, 0);
    assert(renderer.pixels() != empty_close);

    // Target recreation, cached target reuse and tiny viewports.
    for (auto size : {1, 32, 256, 96, 160}) {
        renderer.resize(size, size);
        renderer.render(front_only, camera, 0);
        assert(renderer.pixels().size() == static_cast<std::size_t>(size * size));
        const auto first_image = renderer.pixels();
        renderer.render(front_only, camera, 0);
        assert(renderer.pixels() == first_image);
        renderer.present(); // Safe no-op without a surface.
    }
    renderer.resize(0, -1);
    renderer.render(empty_scene, camera, 0);
    assert(renderer.width() == 1 && renderer.height() == 1 && renderer.pixels().size() == 1);

    bool invalid_camera = false;
    auto invalid = camera;
    invalid.vertical_fov_degrees = std::numeric_limits<float>::quiet_NaN();
    try { renderer.render(empty_scene, invalid, 0); }
    catch (const std::invalid_argument&) { invalid_camera = true; }
    assert(invalid_camera);

    // Ray/pixel coordinates remain consistent with the Vulkan projection.
    const auto target_pixel = engine::project_to_screen(straight.target, straight, 160, 120);
    assert(std::abs(target_pixel.x - 80) < 0.001F && std::abs(target_pixel.y - 60) < 0.001F);
    const auto center_ray = engine::screen_ray(80, 60, straight, 160, 120);
    assert(std::abs(center_ray.direction.z + 1) < 0.001F);
    const engine::EditorCamera pole{{0, 5, 0}, {0, 0, 0}, 45};
    const auto pole_pixel = engine::project_to_screen({1, 0, 0}, pole, 160, 120);
    assert(pole_pixel.visible && std::isfinite(pole_pixel.x) && std::isfinite(pole_pixel.y));

    std::cout << "Vulkan device: " << renderer.device_name() << '\n';
    assert(!renderer.device_name().empty());
    renderer.shutdown();
    renderer.shutdown(); // Idempotent; can initialize again for a new session.
    renderer.render(empty_scene, camera, 0);
    assert(!renderer.device_name().empty());

    assert(engine::project_to_screen({0.0F, 0.0F, 0.0F}, camera, 128, 96).visible);
    return 0;
}
