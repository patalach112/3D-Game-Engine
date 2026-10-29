#include "editor_app.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {
bool close_enough(float a, float b) { return std::abs(a - b) < 0.0001F; }
void same(engine::Vec3 a, engine::Vec3 b) {
    assert(close_enough(a.x, b.x) && close_enough(a.y, b.y) && close_enough(a.z, b.z));
}
void check_camera() {
    const auto& g = editor::app();
    auto expected = g.camera;
    engine::apply_orbit(expected, g.orbit);
    same(g.camera.position, expected.position);
    assert(close_enough(editor::magnitude(editor::sub(g.camera.position, g.camera.target)), g.orbit.distance));
}
void check_layout() {
    editor::update_layout();
    const auto& g = editor::app();
    const auto view = editor::view_rect();
    assert(g.renderer.width() == g.width && g.renderer.height() == g.height);
    assert(view.left == g.left_width + 6 && view.top == editor::kHeaderHeight);
    assert(view.right == g.width - g.right_width - 6);
    assert(view.bottom == g.height - editor::kFooterHeight - g.bottom_height - 6);
    assert(view.right > view.left && view.bottom > view.top);
    assert(view.left >= 0 && view.top >= 0 && view.right <= g.width && view.bottom <= g.height);
    assert(EqualRect(&view, &g.viewport_bounds));
}
}

int main() {
    auto& g = editor::app();
    // The frame limiter uses deadlines, supports arbitrary limits and treats 0 as
    // genuinely uncapped rather than silently falling back to a 16 ms timer.
    const auto start = editor::FrameClock::time_point(std::chrono::seconds(1));
    g.fps_limit = 60; g.next_frame = {};
    assert(editor::frame_due(g, start));
    editor::schedule_next_frame(g, start, start + std::chrono::milliseconds(2));
    assert(!editor::frame_due(g, start + std::chrono::milliseconds(15)));
    assert(editor::frame_due(g, start + std::chrono::milliseconds(17)));
    assert(editor::frame_wait_milliseconds(g, start + std::chrono::milliseconds(15)) == 2);
    g.fps_limit = 0;
    editor::schedule_next_frame(g, start, start + std::chrono::milliseconds(2));
    assert(editor::frame_due(g, start + std::chrono::milliseconds(2)));
    assert(editor::frame_wait_milliseconds(g, start) == 0);
    g.fps_limit = 60; g.next_frame = {}; g.previous_frame = {};

    // Constructing App and resizing must not initialize Vulkan or require a GPU.
    check_layout();
    g.width = 900; g.height = 600;
    g.left_width = 10000; g.right_width = 10000; g.bottom_height = 10000;
    check_layout();
    g.left_width = -100; g.right_width = -100; g.bottom_height = -100;
    check_layout();
    assert(g.left_width == 170 && g.right_width == 240 && g.bottom_height == 100);
    g.width = 1920; g.height = 1080;
    g.left_width = 270; g.right_width = 350; g.bottom_height = 220;
    check_layout(); check_layout();
    assert(g.left_width == 270 && g.right_width == 350 && g.bottom_height == 220);

    editor::duplicate_entity(); editor::frame_selected();
    assert(g.scene.entity_count() == 0 && g.selected == 0);
    const auto id = g.scene.create_entity("Original", engine::PrimitiveType::Cylinder).id;
    g.scene.find_entity(id)->transform = {{3, 4, 5}, {10, 20, 30}, {2, 3, 4}};
    const auto original = *g.scene.find_entity(id);
    // Retain all copies to force repeated vector growth during duplication.
    for (int i = 0; i < 300; ++i) {
        g.selected = id; g.drag_axis = 1;
        editor::duplicate_entity();
        const auto* copy = g.scene.find_entity(g.selected);
        assert(copy && copy->id != id && g.drag_axis == -1);
        assert(copy->name == "Original copy" && copy->primitive == original.primitive);
        same(copy->transform.position, {5, 4, 5});
        same(copy->transform.rotation, original.transform.rotation);
        same(copy->transform.scale, original.transform.scale);
        same(g.scene.find_entity(id)->transform.position, original.transform.position);
    }
    assert(g.scene.entity_count() == 301);
    g.selected = id;
    g.orbit.dragging = true;
    editor::frame_selected();
    same(g.camera.target, original.transform.position);
    assert(!g.orbit.dragging && close_enough(g.orbit.distance, 6));
    assert(close_enough(g.orbit.yaw, .75F) && close_enough(g.orbit.pitch, .42F));
    check_camera();
    const float yaw[]{0, 0, 1.57F, .75F};
    const float pitch[]{0, 1.4F, 0, .42F};
    for (int view = 0; view < 4; ++view) {
        g.orbit.dragging = true;
        editor::snap_camera(view);
        same(g.camera.target, original.transform.position);
        assert(!g.orbit.dragging && close_enough(g.orbit.distance, 8));
        assert(close_enough(g.orbit.yaw, yaw[view]) && close_enough(g.orbit.pitch, pitch[view]));
        check_camera();
    }
    g.selected = 999999;
    editor::duplicate_entity(); editor::frame_selected();
    assert(g.scene.entity_count() == 301 && g.selected == 999999);
    std::cout << "Editor layout, duplication and OrbitState camera helpers passed (no GPU)\n";
}
