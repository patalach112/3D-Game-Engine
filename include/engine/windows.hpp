#pragma once
#include "engine/platform_types.hpp"
#if !defined(_WIN32)
#  error "engine/windows.hpp requires a Windows build; include engine/platform.hpp instead"
#endif
namespace engine::windows {
ENGINE_API std::unique_ptr<Window> createnewwindow(const WindowConfig& config);
}
