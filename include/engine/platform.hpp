#pragma once

#if defined(_WIN32)
#  define ENGINE_PLATFORM_WINDOWS 1
#  include "engine/windows.hpp"
namespace engine {
inline constexpr const char* platform_name = "Windows";
inline std::unique_ptr<Window> createnewwindow(const WindowConfig& config = {}) {
    return windows::createnewwindow(config);
}
}
#elif defined(__linux__)
#  define ENGINE_PLATFORM_LINUX 1
#  include "engine/linux.hpp"
namespace engine {
inline constexpr const char* platform_name = "Linux";
inline std::unique_ptr<Window> createnewwindow(const WindowConfig& config = {}) {
    return linux_platform::createnewwindow(config);
}
}
#else
#  error "Unsupported platform: this engine supports Windows and Linux"
#endif
