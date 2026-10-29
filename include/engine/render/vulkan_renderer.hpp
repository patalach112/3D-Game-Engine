#pragma once

#include "engine/scene/scene.hpp"

#include <cstdint>
#include <memory>
#include <string>
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

// Vulkan rasterization, clipping, shading and depth testing. No CPU rasterizer.
// With a Win32 target, present() copies the GPU image to a Vulkan swapchain.
// Without a window, render() reads it back as 0x00RRGGBB for tests/PPM/terminal.
class VulkanRenderer {
public:
    VulkanRenderer();
    ~VulkanRenderer();
    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

#ifdef _WIN32
    // Call before initialize(). The caller owns both native handles.
    void set_window(void* hinstance, void* hwnd);
#endif
    void initialize();
    void shutdown() noexcept;
    void resize(int width, int height);
    void render(const Scene& scene, const EditorCamera& camera, EntityId selected);
    void present();

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    // Only populated by headless rendering, not by the Win32 swapchain path.
    [[nodiscard]] const std::vector<std::uint32_t>& pixels() const noexcept { return pixels_; }
    [[nodiscard]] const std::string& device_name() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    int width_{1};
    int height_{1};
    std::vector<std::uint32_t> pixels_;
};

} // namespace engine
