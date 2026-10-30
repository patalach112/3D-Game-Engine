#pragma once
#include "engine/export.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace engine {
enum class InputKind : std::uint32_t { Other, PointerMove, PointerButton, Wheel, Key, Text, Focus, Reset };
// Value-only native packet: never enqueue pointers to transient OS structures.
struct InputEvent {
    std::uint64_t window{};
    std::uint32_t message{};
    InputKind kind{};
    std::uint64_t wparam{};
    std::int64_t lparam{};
    std::uint64_t sequence{};
    std::uint64_t extra_info{};
    std::array<std::uint8_t,256> keyboard{};
    float x{}, y{};
    std::uint64_t reserved{};
};
constexpr std::size_t input_queue_capacity=1024;
// Deliberate cache-line padding is part of the NASM ABI, checked below.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324)
#endif
struct alignas(64) InputQueue {
    alignas(64) std::atomic<std::uint64_t> write{};
    alignas(64) std::atomic<std::uint64_t> read{};
    alignas(64) std::array<InputEvent,input_queue_capacity> events{};
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif
static_assert(sizeof(InputEvent)==320);
static_assert(offsetof(InputQueue,write)==0 && offsetof(InputQueue,read)==64 && offsetof(InputQueue,events)==128);
// SPSC only: one window-thread producer, one input-thread consumer. x86-64
// NASM implements publication/acquisition; the non-ASM build uses C++ atomics.
extern "C" ENGINE_API bool engine_input_push(InputQueue*,const InputEvent*) noexcept;
extern "C" ENGINE_API bool engine_input_pop(InputQueue*,InputEvent*) noexcept;

class InputProcessor {
public:
    ENGINE_API explicit InputProcessor(std::function<void()> wake={});
    ENGINE_API ~InputProcessor();
    InputProcessor(const InputProcessor&)=delete;
    InputProcessor& operator=(const InputProcessor&)=delete;
    ENGINE_API bool enqueue(InputEvent event); // producer thread only, never blocks on a full ring
    ENGINE_API std::vector<InputEvent> drain(); // frontend thread only
    ENGINE_API std::uint64_t dropped_events() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
