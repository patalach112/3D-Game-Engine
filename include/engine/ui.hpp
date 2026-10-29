#pragma once

// Dear ImGui 1.91.9b is built into engine.dll/libengine.so, not the editor.
// Do not link another ImGui copy or change imconfig.h in a consumer.
// Call VulkanRenderer::enable_ui(), then each frame: begin_ui_frame(), build
// widgets, render(scene, camera, selection, viewport), present(). No GDI paint.
#include <imgui.h>
