#pragma once

#include "engine/export.hpp"

#include "engine/math/vec3.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace engine {

using EntityId = std::uint64_t;

enum class PrimitiveType : std::uint8_t {
    Cube,
    Sphere,
    Cylinder,
    Plane,
    Circle2D, // Flat XY disk. Append values to preserve existing scene IDs.
    Square2D, // Flat XY square.
    Cone,
    Pyramid,
};

// Immutable CPU mesh shared by imported-object instances. Faces are triangles;
// edges are retained for selection outlines in the editor.
struct MeshAsset {
    std::vector<Vec3> vertices;
    std::vector<std::array<int,3>> triangles;
    std::vector<std::array<int,2>> edges;
    std::string source;
    Vec3 suggested_color{0.72F,0.72F,0.74F};
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
    Vec3 color{0.65F,0.65F,0.67F};
    std::shared_ptr<const MeshAsset> mesh; // null for a built-in primitive
};

class Scene {
public:
    [[nodiscard]] ENGINE_API Entity& create_entity(std::string name, PrimitiveType primitive = PrimitiveType::Cube);
    [[nodiscard]] ENGINE_API Entity* find_entity(EntityId id);
    [[nodiscard]] ENGINE_API const Entity* find_entity(EntityId id) const;
    [[nodiscard]] ENGINE_API bool remove_entity(EntityId id);
    [[nodiscard]] const std::vector<Entity>& entities() const noexcept { return entities_; }
    [[nodiscard]] std::size_t entity_count() const noexcept { return entities_.size(); }

private:
    EntityId next_id_{1};
    std::vector<Entity> entities_;
};

} // namespace engine
