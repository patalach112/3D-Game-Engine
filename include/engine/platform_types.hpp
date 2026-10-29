#pragma once

#include "engine/export.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace engine {
struct WindowSize { int width{}, height{}; };
struct NativeWindowHandle {
    void* display{};           // X11 Display*, null on Windows.
    std::uintptr_t window{};   // HWND on Windows, X11 Window ID on Linux.
    void* instance{};          // HINSTANCE on Windows, null on Linux.
};
struct NativeMessage {
    void* window{};
    std::uint32_t id{};
    std::uintptr_t wparam{};
    std::intptr_t lparam{};
};
// Return true if handled. Win32-specific editor input remains outside the engine.
using NativeMessageCallback = bool (*)(const NativeMessage&, std::intptr_t& result, void* user);
using ResizeCallback = void (*)(WindowSize size, bool minimized, void* user);
struct WindowConfig {
    std::string title{"Starter 3D Engine"}; // UTF-8 on both platforms.
    int width{1280}, height{800};
    bool visible{true};
    bool child{};
    bool borderless{};
    std::uintptr_t parent{};
    NativeMessageCallback native_callback{}; // Optional Win32 message hook.
    void* user{};
};

// Objects are allocated AND destroyed in the engine library (virtual destructor).
// Native handles are borrowed; the Window owns the underlying native window.
class ENGINE_API Window {
public:
    virtual ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    virtual NativeWindowHandle native_handle() const noexcept = 0;
    virtual WindowSize size() const noexcept = 0;
    virtual bool should_close() const noexcept = 0;
    virtual bool minimized() const noexcept = 0;
    virtual void show() = 0;
    virtual void poll_events() = 0;
    virtual void request_close() = 0;
    virtual void set_resize_callback(ResizeCallback callback, void* user) noexcept = 0;
protected:
    Window() = default;
};
} // namespace engine

// Shared C entry points implemented by call.asm, or the C++ fallback. Memory
// must be returned to freeheap(), never to delete/free in another module.
extern "C" {
ENGINE_API void* createheap(std::size_t bytes) noexcept;
ENGINE_API void freeheap(void* memory) noexcept;
ENGINE_API void WindowResizedCallback(engine::Window* window, engine::ResizeCallback callback, void* user) noexcept;
}
