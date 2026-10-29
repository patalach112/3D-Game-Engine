#pragma once

#include "engine/render/vulkan_renderer.hpp"

namespace engine {
// Source compatibility only: this is the Vulkan backend, not a CPU fallback.
using SoftwareRenderer = VulkanRenderer;
} // namespace engine
