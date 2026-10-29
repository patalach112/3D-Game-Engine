#pragma once
#include "engine/platform_types.hpp"
#if !defined(__linux__)
#  error "engine/linux.hpp requires a Linux build; include engine/platform.hpp instead"
#endif
namespace engine::linux_platform {
ENGINE_API std::unique_ptr<Window> createnewwindow(const WindowConfig& config);
ENGINE_API bool windowing_available() noexcept;
}
