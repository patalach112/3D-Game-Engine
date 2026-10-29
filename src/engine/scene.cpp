#include "engine/scene/scene.hpp"
#include "engine/log.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace engine {

Entity& Scene::create_entity(std::string name, PrimitiveType primitive) {
    if (name.empty()) {
        throw std::invalid_argument("Obiekt sceny musi mieć nazwę.");
    }

    entities_.push_back(Entity{next_id_++, std::move(name), primitive, {}});
    LOG_DEBUG("scene", "Created entity: " + entities_.back().name);
    return entities_.back();
}

Entity* Scene::find_entity(EntityId id) {
    const auto found = std::find_if(entities_.begin(), entities_.end(),
        [id](const Entity& entity) { return entity.id == id; });
    return found == entities_.end() ? nullptr : &*found;
}

const Entity* Scene::find_entity(EntityId id) const {
    const auto found = std::find_if(entities_.begin(), entities_.end(),
        [id](const Entity& entity) { return entity.id == id; });
    return found == entities_.end() ? nullptr : &*found;
}

bool Scene::remove_entity(EntityId id) {
    const auto found = std::find_if(entities_.begin(), entities_.end(),
        [id](const Entity& entity) { return entity.id == id; });
    if (found == entities_.end()) {
        return false;
    }
    entities_.erase(found);
    LOG_DEBUG("scene", "Removed entity ID " + std::to_string(id));
    return true;
}

} // namespace engine
