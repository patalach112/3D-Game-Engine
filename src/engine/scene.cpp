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

    entities_.push_back(Entity{next_id_++,std::move(name),primitive,{},Vec3{0.65F,0.65F,0.67F},{}});
    auto& entity=entities_.back();
    switch(primitive) {
    case PrimitiveType::Sphere: entity.color={0.50F,0.66F,0.85F}; break;
    case PrimitiveType::Cylinder: entity.color={0.49F,0.69F,0.51F}; break;
    case PrimitiveType::Plane: entity.color={0.70F,0.64F,0.49F}; break;
    case PrimitiveType::Cone: entity.color={0.78F,0.48F,0.34F}; break;
    case PrimitiveType::Pyramid: entity.color={0.69F,0.52F,0.82F}; break;
    default: break;
    }
    LOG_INFO("scene", "Created entity: %s", entity.name.c_str());
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
    LOG_INFO("scene", "Removed entity ID %llu", static_cast<unsigned long long>(id));
    return true;
}

} // namespace engine
