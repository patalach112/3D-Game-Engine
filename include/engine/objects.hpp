#pragma once
#include "engine/export.hpp"
#include "engine/math/vec3.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

namespace engine {
// Explicit unaligned GPU ABI. No dependency on GLM alignment configuration.
struct Vec2 { float x{}, y{}; };
struct Vec4 { float x{}, y{}, z{}, w{}; };
struct hvec4 { std::uint16_t x{}, y{}, z{}, w{}; }; // IEEE-754 binary16 bits.
struct Vertex { hvec4 pos; std::uint32_t Normal{}; };
struct Vertex_hud { Vec2 pos; Vec4 color; };
struct ObjectData {
    Vec3 position{};
    std::int32_t rotation{}; // XYZ: 10 unsigned bits/axis, one turn = 1024.
    float scale{1};
    std::uint32_t texturePair{}; // low/high 16 bits = texture handles; not a color.
    float radius{}; // World-space bounding sphere, centered on position.
    std::uint32_t padding{};
};
struct DrawIndexedIndirectCommand {
    std::uint32_t indexCount{}, instanceCount{}, firstIndex{};
    std::int32_t vertexOffset{}; // Vulkan specifies SIGNED int32, not uint32.
    std::uint32_t firstInstance{};
};
static_assert(sizeof(hvec4)==8 && sizeof(Vertex)==12 && offsetof(Vertex,Normal)==8);
static_assert(sizeof(Vertex_hud)==24 && offsetof(Vertex_hud,color)==8);
static_assert(sizeof(ObjectData)==32 && offsetof(ObjectData,rotation)==12 && offsetof(ObjectData,scale)==16
    && offsetof(ObjectData,texturePair)==20 && offsetof(ObjectData,radius)==24 && offsetof(ObjectData,padding)==28);
static_assert(sizeof(DrawIndexedIndirectCommand)==20 && offsetof(DrawIndexedIndirectCommand,vertexOffset)==12);
static_assert(std::is_trivially_copyable_v<Vertex> && std::is_standard_layout_v<ObjectData>);

ENGINE_API std::uint16_t float_to_half(float value);
ENGINE_API float half_to_float(std::uint16_t value) noexcept;
ENGINE_API std::uint32_t pack_normal(Vec3 value);
ENGINE_API Vec3 unpack_normal(std::uint32_t value) noexcept;
ENGINE_API Vertex make_vertex(Vec3 position, Vec3 normal);
ENGINE_API std::int32_t pack_rotation(Vec3 degrees);
ENGINE_API Vec3 unpack_rotation(std::int32_t packed) noexcept;
constexpr std::uint32_t texture_pair(std::uint16_t first,std::uint16_t second) noexcept {
    return first|(static_cast<std::uint32_t>(second)<<16);
}
struct GeometryLayout {
    std::size_t hudVertices{}, sceneVertices{}, hudIndices{}, sceneIndices{}, totalBytes{};
    std::uint32_t hudVertexCount{}, sceneVertexCount{}, hudIndexCount{}, sceneIndexCount{};
};
// Exactly four tightly concatenated sections, all offsets in BYTES. Indices are uint32.
// firstIndex/vertexOffset are ELEMENT offsets relative to the scene sections.
struct GeometryBuffer {
    std::vector<std::byte> bytes;
    GeometryLayout layout;
    std::vector<ObjectData> objects;
    std::vector<DrawIndexedIndirectCommand> commands; // 3D only, never HUD.
    std::vector<Vec4> tints; // CPU draw parameters; not texturePair or ObjectData padding.
    ENGINE_API void validate() const;
};
using MeshId=std::uint32_t;
class GeometryBuilder {
public:
    ENGINE_API void add_hud(std::span<const Vertex_hud> vertices,std::span<const std::uint32_t> indices);
    // For the font backend: keep per-list indices local; baseVertex is supplied per draw.
    ENGINE_API void append_ui(std::span<const Vertex_hud> vertices,std::span<const std::uint32_t> indices);
    ENGINE_API MeshId add_mesh(std::span<const Vertex> vertices,std::span<const std::uint32_t> indices);
    ENGINE_API std::uint32_t add_object(MeshId mesh,ObjectData data,Vec4 tint={0.65F,0.65F,0.67F,1});
    [[nodiscard]] ENGINE_API GeometryBuffer build() const;
    static ENGINE_API GeometryBuilder from_buffer(const GeometryBuffer& buffer);
private:
    struct Mesh { std::uint32_t firstIndex{}, count{}; std::int32_t baseVertex{}; float radius{}; };
    std::vector<Vertex_hud> hud_;
    std::vector<Vertex> vertices_;
    std::vector<std::uint32_t> hudIndices_,indices_;
    std::vector<Mesh> meshes_;
    std::vector<ObjectData> objects_;
    std::vector<DrawIndexedIndirectCommand> commands_;
    std::vector<Vec4> tints_;
};
} // namespace engine
