#pragma once

#include "engine/export.hpp"
#include "engine/objects.hpp"

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

// Pixel rectangle inside the full client-area render target.
struct RenderViewport {
    int x{}, y{}, width{}, height{};
};

struct Ray3 {
    Vec3 origin{};
    Vec3 direction{};
};

[[nodiscard]] ENGINE_API ScreenPoint project_to_screen(Vec3 world, const EditorCamera& camera,
                                           int width, int height);
[[nodiscard]] ENGINE_API Ray3 screen_ray(float x, float y, const EditorCamera& camera,
                             int width, int height);

// Vulkan rasterization, clipping, shading and depth testing. No CPU rasterizer.
// With a Win32 target, present() copies the GPU image to a Vulkan swapchain.
// Without a window, render() reads it back as 0x00RRGGBB for tests/PPM/terminal.
class ENGINE_API VulkanRenderer {
public:
    VulkanRenderer();
    ~VulkanRenderer();
    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

#ifdef _WIN32
    // Call before initialize(). The caller owns both native handles.
    void set_window(void* hinstance, void* hwnd);
#endif
    // UI state/backends live in the engine DLL. Native Win32 input is forwarded
    // through handle_ui_message(); headless callers feed ImGuiIO directly.
    void enable_ui(float font_size = 17.0F);
    void begin_ui_frame(float delta_seconds = 1.0F / 60.0F);
#ifdef _WIN32
    bool handle_ui_message(void* hwnd, unsigned int message,
                           std::uintptr_t wparam, std::intptr_t lparam);
#endif
    [[nodiscard]] unsigned int antialiasing_samples() const noexcept;
    void initialize();
    void shutdown() noexcept;
    void resize(int width, int height);
    // Win32: records a scene pass; present() submits it together with the blit.
    // Headless: submits and waits for pixel readback before returning.
    // Preserve the original whole-target entry point for existing DLL consumers.
    void render(const Scene& scene, const EditorCamera& camera, EntityId selected);
    void render(const Scene& scene, const EditorCamera& camera, EntityId selected,
                RenderViewport viewport);
    // One packed geometry buffer: HUD vertices, 3D vertices, HUD indices, 3D indices.
    // ObjectData and indirect commands are 3D-only SSBOs; compute updates visibility.
    void render_geometry(const GeometryBuffer& geometry,const EditorCamera& camera,RenderViewport viewport = {});
    // GPU-produced visibility, available after headless render/readback only.
    [[nodiscard]] const std::vector<DrawIndexedIndirectCommand>& last_draw_commands() const noexcept;
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
