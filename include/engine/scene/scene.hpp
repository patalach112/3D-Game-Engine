#pragma once

#include "engine/export.hpp"

#include "engine/math/vec3.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace engine {

using EntityId = std::uint64_t;

enum class PrimitiveType : std::uint8_t {
    Cube,
    Sphere,
    Cylinder,
    Plane
};

struct Transform {
    Vec3 position{};
    Vec3 rotation{}; // Kąty Eulera w stopniach.
    Vec3 scale{1.0F, 1.0F, 1.0F};
};

struct Entity {
    EntityId id{};
    std::string name;
    PrimitiveType primitive{PrimitiveType::Cube};
    Transform transform{};
};

class ENGINE_API Scene {
public:
    [[nodiscard]] Entity& create_entity(std::string name, PrimitiveType primitive = PrimitiveType::Cube);
    [[nodiscard]] Entity* find_entity(EntityId id);
    [[nodiscard]] const Entity* find_entity(EntityId id) const;
    [[nodiscard]] bool remove_entity(EntityId id);
    [[nodiscard]] const std::vector<Entity>& entities() const noexcept { return entities_; }
    [[nodiscard]] std::size_t entity_count() const noexcept { return entities_.size(); }

private:
    EntityId next_id_{1};
    std::vector<Entity> entities_;
};

} // namespace engine
