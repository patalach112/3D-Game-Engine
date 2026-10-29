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
struct alignas(64) InputQueue {
    alignas(64) std::atomic<std::uint64_t> write{};
    alignas(64) std::atomic<std::uint64_t> read{};
    alignas(64) std::array<InputEvent,input_queue_capacity> events{};
};
static_assert(sizeof(InputEvent)==320);
static_assert(offsetof(InputQueue,write)==0 && offsetof(InputQueue,read)==64 && offsetof(InputQueue,events)==128);
// SPSC only: one window-thread producer, one input-thread consumer. x86-64
// NASM implements publication/acquisition; the non-ASM build uses C++ atomics.
extern "C" ENGINE_API bool engine_input_push(InputQueue*,const InputEvent*) noexcept;
extern "C" ENGINE_API bool engine_input_pop(InputQueue*,InputEvent*) noexcept;

class ENGINE_API InputProcessor {
public:
    explicit InputProcessor(std::function<void()> wake={});
    ~InputProcessor();
    InputProcessor(const InputProcessor&)=delete;
    InputProcessor& operator=(const InputProcessor&)=delete;
    bool enqueue(InputEvent event); // producer thread only, never blocks on a full ring
    std::vector<InputEvent> drain(); // frontend thread only
    std::uint64_t dropped_events() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
