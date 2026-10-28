#include "engine/scene/scene.hpp"

#include <cassert>
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
    return 0;
}
