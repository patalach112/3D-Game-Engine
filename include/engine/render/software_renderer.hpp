#pragma once

#include "engine/scene/scene.hpp"

#include <cstdint>
#include <vector>

namespace engine {

struct EditorCamera {
    Vec3 position{6.0F, 4.0F, 7.0F};
    Vec3 target{0.0F, 0.5F, 0.0F};
    float vertical_fov_degrees{45.0F};
};

struct ScreenPoint {
    float x{};
    float y{};
    float depth{};
    bool visible{};
};

struct Ray3 {
    Vec3 origin{};
    Vec3 direction{};
};

[[nodiscard]] ScreenPoint project_to_screen(Vec3 world, const EditorCamera& camera,
                                            int width, int height);
[[nodiscard]] Ray3 screen_ray(float x, float y, const EditorCamera& camera,
                              int width, int height);

// A small, project-owned CPU renderer. Pixels use 0x00RRGGBB so Win32 can
// present the buffer directly as a 32-bit BI_RGB bitmap.
class SoftwareRenderer {
public:
    void resize(int width, int height);
    void render(const Scene& scene, const EditorCamera& camera, EntityId selected);

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] const std::vector<std::uint32_t>& pixels() const noexcept { return pixels_; }

private:
    int width_{1};
    int height_{1};
    std::vector<std::uint32_t> pixels_;
    std::vector<float> depth_;
};

} // namespace engine
