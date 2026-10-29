#pragma once
#include "engine/render/vulkan_renderer.hpp"
#include "engine/objects.hpp"
namespace engine {
struct OrbitState { float yaw{0.75F},pitch{0.42F},distance{8}; bool dragging{}; Vec2 lastMouse{}; };
struct OrbitInput {
    Vec2 mouse{}; float wheel{};
    bool rightDown{},rightPressed{},focused{true},pointerValid{true},blockStart{};
};
// Consume the current UI frame, NOT stale WantCaptureMouse from a native message.
// blockStart is for popups/modal UI. Once dragging, crossing a panel doesn't stop it.
ENGINE_API bool update_orbit(OrbitState& state,const OrbitInput& input,RenderViewport viewport);
ENGINE_API void apply_orbit(EditorCamera& camera,const OrbitState& state);
}
