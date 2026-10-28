#include "engine/render/software_renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace engine {
namespace {
constexpr float kPi = 3.14159265358979323846F;
constexpr float kNearPlane = 0.08F;
constexpr std::uint32_t kBackground = 0x00272729U;
constexpr std::array<std::uint32_t, 3> kAxisColors{0x00E84C4CU, 0x005ACB70U, 0x004E91F2U};

Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 subtract(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 multiply(Vec3 a, float n) { return {a.x * n, a.y * n, a.z * n}; }
float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
float length(Vec3 v) { return std::sqrt(dot(v, v)); }
Vec3 normalize(Vec3 v) {
    const float magnitude = length(v);
    return magnitude < 1.0e-6F ? Vec3{0.0F, 0.0F, 1.0F} : multiply(v, 1.0F / magnitude);
}

struct Basis { Vec3 right; Vec3 up; Vec3 forward; };
Basis camera_basis(const EditorCamera& camera) {
    const Vec3 forward = normalize(subtract(camera.target, camera.position));
    const Vec3 right = normalize(cross(forward, {0.0F, 1.0F, 0.0F}));
    return {right, normalize(cross(right, forward)), forward};
}

struct CameraPoint { float x, y, z; };
CameraPoint to_camera(Vec3 world, const EditorCamera& camera, const Basis& basis) {
    const Vec3 delta = subtract(world, camera.position);
    return {dot(delta, basis.right), dot(delta, basis.up), dot(delta, basis.forward)};
}

std::uint32_t shade(std::uint32_t color, float factor) {
    const auto channel = [color, factor](std::uint32_t shift) {
        return static_cast<std::uint32_t>(std::clamp(
            static_cast<int>(((color >> shift) & 0xffU) * factor), 0, 255));
    };
    return (channel(16) << 16U) | (channel(8) << 8U) | channel(0);
}
} // namespace

ScreenPoint project_to_screen(Vec3 world, const EditorCamera& camera, int width, int height) {
    const Basis basis = camera_basis(camera);
    const CameraPoint point = to_camera(world, camera, basis);
    if (point.z < kNearPlane || width <= 0 || height <= 0) return {};
    const float focal = (static_cast<float>(height) * 0.5F) /
        std::tan(camera.vertical_fov_degrees * kPi / 360.0F);
    return {static_cast<float>(width) * 0.5F + point.x * focal / point.z,
            static_cast<float>(height) * 0.5F - point.y * focal / point.z,
            point.z, true};
}

Ray3 screen_ray(float x, float y, const EditorCamera& camera, int width, int height) {
    const Basis basis = camera_basis(camera);
    const float tangent = std::tan(camera.vertical_fov_degrees * kPi / 360.0F);
    const float nx = (2.0F * x / static_cast<float>(std::max(1, width)) - 1.0F)
        * (static_cast<float>(width) / static_cast<float>(std::max(1, height))) * tangent;
    const float ny = (1.0F - 2.0F * y / static_cast<float>(std::max(1, height))) * tangent;
    return {camera.position, normalize(add(basis.forward, add(multiply(basis.right, nx), multiply(basis.up, ny))))};
}

void SoftwareRenderer::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const auto count = static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_);
    pixels_.resize(count);
    depth_.resize(count);
}

void SoftwareRenderer::render(const Scene& scene, const EditorCamera& camera, EntityId selected) {
    std::fill(pixels_.begin(), pixels_.end(), kBackground);
    std::fill(depth_.begin(), depth_.end(), std::numeric_limits<float>::infinity());
    const Basis basis = camera_basis(camera);
    const float focal = (static_cast<float>(height_) * 0.5F) /
        std::tan(camera.vertical_fov_degrees * kPi / 360.0F);

    const auto project = [&](Vec3 world) -> ScreenPoint {
        const CameraPoint point = to_camera(world, camera, basis);
        if (point.z < kNearPlane) return {};
        return {static_cast<float>(width_) * 0.5F + point.x * focal / point.z,
                static_cast<float>(height_) * 0.5F - point.y * focal / point.z,
                point.z, true};
    };
    const auto put_pixel = [&](int x, int y, float z, std::uint32_t color, bool depth_test) {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
        const auto index = static_cast<std::size_t>(y) * static_cast<std::size_t>(width_)
                         + static_cast<std::size_t>(x);
        if (!depth_test || z < depth_[index]) {
            pixels_[index] = color;
            if (depth_test) depth_[index] = z;
        }
    };
    const auto draw_line = [&](Vec3 a, Vec3 b, std::uint32_t color, bool depth_test) {
        const ScreenPoint pa = project(a), pb = project(b);
        if (!pa.visible || !pb.visible) return;
        const float dx = pb.x - pa.x, dy = pb.y - pa.y;
        const int steps = std::max(1, static_cast<int>(std::ceil(std::max(std::abs(dx), std::abs(dy)))));
        for (int i = 0; i <= steps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            put_pixel(static_cast<int>(std::lround(pa.x + dx * t)),
                      static_cast<int>(std::lround(pa.y + dy * t)),
                      pa.depth + (pb.depth - pa.depth) * t, color, depth_test);
        }
    };

    // The floor is intentionally only a grid. There is no filled plane below it.
    constexpr int grid_extent = 28;
    for (int i = -grid_extent; i <= grid_extent; ++i) {
        const std::uint32_t color = (i % 5 == 0) ? 0x00525255U : 0x00373739U;
        const float coordinate = static_cast<float>(i);
        draw_line({-static_cast<float>(grid_extent), 0.0F, coordinate},
                  {static_cast<float>(grid_extent), 0.0F, coordinate}, color, true);
        draw_line({coordinate, 0.0F, -static_cast<float>(grid_extent)},
                  {coordinate, 0.0F, static_cast<float>(grid_extent)}, color, true);
    }

    constexpr std::array<std::array<int, 4>, 6> faces{{
        {{0, 1, 3, 2}}, {{4, 6, 7, 5}}, {{0, 4, 5, 1}},
        {{2, 3, 7, 6}}, {{0, 2, 6, 4}}, {{1, 5, 7, 3}}
    }};
    constexpr std::array<float, 6> lighting{{0.66F, 0.86F, 0.74F, 1.0F, 0.58F, 0.9F}};
    constexpr std::array<std::array<int, 2>, 12> edges{{
        {{0,1}}, {{1,3}}, {{3,2}}, {{2,0}}, {{4,5}}, {{5,7}},
        {{7,6}}, {{6,4}}, {{0,4}}, {{1,5}}, {{2,6}}, {{3,7}}
    }};

    for (const Entity& entity : scene.entities()) {
        const Vec3 scale = entity.transform.scale;
        const float rx = entity.transform.rotation.x * kPi / 180.0F;
        const float ry = entity.transform.rotation.y * kPi / 180.0F;
        const float rz = entity.transform.rotation.z * kPi / 180.0F;
        std::array<Vec3, 8> world{};
        std::array<ScreenPoint, 8> screen{};
        for (int i = 0; i < 8; ++i) {
            Vec3 p{(i & 1) ? 0.5F : -0.5F, (i & 2) ? 0.5F : -0.5F, (i & 4) ? 0.5F : -0.5F};
            p = {p.x * scale.x, p.y * scale.y, p.z * scale.z};
            const float cx = std::cos(rx), sx = std::sin(rx);
            const float cy = std::cos(ry), sy = std::sin(ry);
            const float cz = std::cos(rz), sz = std::sin(rz);
            const Vec3 px{p.x, p.y * cx - p.z * sx, p.y * sx + p.z * cx};
            const Vec3 py{px.x * cy + px.z * sy, px.y, -px.x * sy + px.z * cy};
            const Vec3 pz{py.x * cz - py.y * sz, py.x * sz + py.y * cz, py.z};
            world[static_cast<std::size_t>(i)] = add(pz, entity.transform.position);
            screen[static_cast<std::size_t>(i)] = project(world[static_cast<std::size_t>(i)]);
        }

        const bool is_selected = entity.id == selected;
        const std::uint32_t base = is_selected ? 0x00D8D8DAU : 0x00A7A7AAU;
        for (std::size_t face_index = 0; face_index < faces.size(); ++face_index) {
            const auto& face = faces[face_index];
            const ScreenPoint a = screen[static_cast<std::size_t>(face[0])];
            const ScreenPoint b = screen[static_cast<std::size_t>(face[1])];
            const ScreenPoint c = screen[static_cast<std::size_t>(face[2])];
            const ScreenPoint d = screen[static_cast<std::size_t>(face[3])];
            if (!a.visible || !b.visible || !c.visible || !d.visible) continue;
            const std::uint32_t color = shade(base, lighting[face_index]);
            const auto triangle = [&](ScreenPoint p0, ScreenPoint p1, ScreenPoint p2) {
                const float area = (p1.x - p0.x) * (p2.y - p0.y) - (p1.y - p0.y) * (p2.x - p0.x);
                if (std::abs(area) < 1.0e-5F) return;
                const int min_x = std::max(0, static_cast<int>(std::floor(std::min({p0.x, p1.x, p2.x}))));
                const int max_x = std::min(width_ - 1, static_cast<int>(std::ceil(std::max({p0.x, p1.x, p2.x}))));
                const int min_y = std::max(0, static_cast<int>(std::floor(std::min({p0.y, p1.y, p2.y}))));
                const int max_y = std::min(height_ - 1, static_cast<int>(std::ceil(std::max({p0.y, p1.y, p2.y}))));
                for (int y = min_y; y <= max_y; ++y) for (int x = min_x; x <= max_x; ++x) {
                    const float px = static_cast<float>(x) + 0.5F, py = static_cast<float>(y) + 0.5F;
                    const float w0 = ((p1.x - px) * (p2.y - py) - (p1.y - py) * (p2.x - px)) / area;
                    const float w1 = ((p2.x - px) * (p0.y - py) - (p2.y - py) * (p0.x - px)) / area;
                    const float w2 = 1.0F - w0 - w1;
                    if (w0 >= -0.001F && w1 >= -0.001F && w2 >= -0.001F)
                        put_pixel(x, y, w0 * p0.depth + w1 * p1.depth + w2 * p2.depth, color, true);
                }
            };
            triangle(a, b, c);
            triangle(a, c, d);
        }
        for (const auto& edge : edges) {
            const std::uint32_t edge_color = is_selected ? 0x00F1A06BU : 0x00555558U;
            draw_line(world[static_cast<std::size_t>(edge[0])],
                      world[static_cast<std::size_t>(edge[1])], edge_color, true);
        }
    }

    if (const Entity* entity = scene.find_entity(selected)) {
        const Vec3 center = entity->transform.position;
        const float distance = length(subtract(camera.position, center));
        const float handle_length = std::clamp(distance * 0.13F, 0.8F, 2.0F);
        for (int axis = 0; axis < 3; ++axis) for (float sign : {-1.0F, 1.0F}) {
            const Vec3 direction = multiply(axis == 0 ? Vec3{1,0,0} : axis == 1 ? Vec3{0,1,0} : Vec3{0,0,1}, sign);
            const Vec3 handle = add(center, multiply(direction, handle_length));
            const ScreenPoint a = project(center), b = project(handle);
            if (!a.visible || !b.visible) continue;
            draw_line(center, handle, kAxisColors[static_cast<std::size_t>(axis)], false);
            const int radius = std::clamp(static_cast<int>(std::lround(static_cast<float>(height_) * 0.011F)), 7, 12);
            for (int y = -radius; y <= radius; ++y) for (int x = -radius; x <= radius; ++x)
                if (x*x + y*y <= radius*radius)
                    put_pixel(static_cast<int>(std::lround(b.x)) + x,
                              static_cast<int>(std::lround(b.y)) + y, b.depth,
                              kAxisColors[static_cast<std::size_t>(axis)], false);
        }
    }
}

} // namespace engine
