#pragma once

#include "engine/render/vulkan_renderer.hpp"
#include "engine/objects.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace engine {
namespace internal {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kNearPlane = 0.08F;
constexpr std::array<std::uint32_t, 3> kAxisColors{0x00E84C4CU, 0x005ACB70U, 0x004E91F2U};

inline Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 subtract(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 multiply(Vec3 a, float n) { return {a.x * n, a.y * n, a.z * n}; }
inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float length(Vec3 v) { return std::sqrt(dot(v, v)); }
inline Vec3 normalize(Vec3 v) {
    const float magnitude = length(v);
    return magnitude < 1.0e-6F ? Vec3{0, 0, 1} : multiply(v, 1.0F / magnitude);
}

struct Basis { Vec3 right, up, forward; };
inline Basis camera_basis(const EditorCamera& camera) {
    const Vec3 forward = normalize(subtract(camera.target, camera.position));
    const Vec3 reference = std::abs(forward.y) > 0.999F ? Vec3{0, 0, 1} : Vec3{0, 1, 0};
    const Vec3 right = normalize(cross(forward, reference));
    return {right, normalize(cross(right, forward)), forward};
}
struct CameraPoint { float x, y, z; };
inline CameraPoint to_camera(Vec3 world, const EditorCamera& camera, const Basis& basis) {
    const Vec3 delta = subtract(world, camera.position);
    return {dot(delta, basis.right), dot(delta, basis.up), dot(delta, basis.forward)};
}

using MeshData = MeshAsset;

struct DebugVertex {
    Vec3 position;
    Vec3 color;
    float grid{};
};
static_assert(sizeof(DebugVertex) == 28);

struct DebugAttributes { Vec3 color; float grid; };
static_assert(sizeof(DebugAttributes) == 16);

struct CameraPush {
    std::array<float, 4> right, up, forward, eye, projection;
};
static_assert(sizeof(CameraPush) == 80);

struct FrameGeometry {
    std::vector<DebugVertex> vertices;
    // Camera-relative FP16 conversion, colours and sequential indices are part
    // of the scene cache too, not rebuilt on every unchanged Vulkan frame.
    std::vector<Vertex> debug_vertices;
    std::vector<DebugAttributes> debug_attributes;
    std::vector<std::uint32_t> debug_indices;
    float debug_scale{1};
    GeometryBuffer geometry;
    std::uint32_t lines{}, overlay_lines{}, overlay_triangles{};
};

// Cache renderer inputs, not Scene identity: transforms can be edited through
// public Entity references. A scene revision counter alone would miss edits.
struct GeometryEntityKey { EntityId id; PrimitiveType primitive; Transform transform; Vec3 color; const MeshAsset* mesh{}; };
struct FrameGeometryCache {
    std::vector<GeometryEntityKey> entities;
    EditorCamera camera;
    EntityId selected{};
    int width{},height{},mode{};
    bool grid{},valid{};
    FrameGeometry frame;
    std::uint64_t builds{},hits{};
    ENGINE_API const FrameGeometry& update(const Scene& scene,const EditorCamera& view,EntityId selection,
        int w,int h,bool show_grid,int gizmo);
};

// --- Declaracje funkcji z podzielonych plików ---
MeshData make_primitive_mesh(PrimitiveType type);
const MeshData& primitive_mesh(PrimitiveType type);
Vec3 rgb(std::uint32_t value);
bool finite(Vec3 p);
FrameGeometry scene_geometry(const Scene& scene, const EditorCamera& camera, EntityId selected, int width, int height,
                             bool grid_visible, int gizmo_mode);

} // namespace internal
} // namespace engine
