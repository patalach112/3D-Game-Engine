#include "engine/platform.hpp"
#include <cstdlib>
#ifdef ENGINE_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

engine::Window::~Window() = default;

// ABI bridges called by NASM. No exception can escape a C/ASM boundary.
extern "C" void* engine_platform_allocate(std::size_t bytes) noexcept {
    if (!bytes) return nullptr;
#ifdef ENGINE_PLATFORM_WINDOWS
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bytes);
#else
    return std::calloc(1, bytes);
#endif
}
extern "C" void engine_platform_free(void* memory) noexcept {
#ifdef ENGINE_PLATFORM_WINDOWS
    if (memory) HeapFree(GetProcessHeap(), 0, memory);
#else
    std::free(memory);
#endif
}
extern "C" void engine_register_resize_callback(engine::Window* window,
    engine::ResizeCallback callback, void* user) noexcept {
    if (window) window->set_resize_callback(callback, user);
}
#ifndef ENGINE_HAS_ASM
extern "C" void* createheap(std::size_t bytes) noexcept { return engine_platform_allocate(bytes); }
extern "C" void freeheap(void* memory) noexcept { engine_platform_free(memory); }
extern "C" void WindowResizedCallback(engine::Window* window, engine::ResizeCallback callback, void* user) noexcept {
    engine_register_resize_callback(window, callback, user);
}
#endif
