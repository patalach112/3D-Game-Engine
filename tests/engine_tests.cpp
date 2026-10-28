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

    engine::Scene rendered_scene;
    const auto rendered_id = rendered_scene.create_entity("Visible Cube").id;
    renderer.render(rendered_scene, camera, rendered_id);
    const auto rendered_pixels = static_cast<std::size_t>(std::count_if(
        renderer.pixels().begin(), renderer.pixels().end(), [](std::uint32_t pixel) {
            return pixel != 0x00272729U;
        }));
    assert(rendered_pixels > renderer.pixels().size() / 10);
    assert(engine::project_to_screen({0.0F, 0.0F, 0.0F}, camera, 128, 96).visible);
    return 0;
}
