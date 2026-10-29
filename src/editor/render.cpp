#include "editor_app.hpp"
#include <algorithm>
#include <chrono>

namespace editor {
namespace { std::chrono::steady_clock::time_point previous_frame; }
void reset_frame_timing() { previous_frame={}; }
void render_frame(HWND hwnd) {
    auto& g=app();
    // Frontend owns scene/renderer/ImGui only while the GPU worker is idle.
    RECT client{}; GetClientRect(hwnd,&client);
    g.width=static_cast<int>(std::max(1L,client.right));
    g.height=static_cast<int>(std::max(1L,client.bottom));
    update_layout();
    const auto now=std::chrono::steady_clock::now();
    const float elapsed=previous_frame.time_since_epoch().count()==0?1.0F/60.0F:
        std::chrono::duration<float>(now-previous_frame).count();
    const float delta=elapsed>0?elapsed:1.0F/60.0F;
    previous_frame=now;
    g.renderer.begin_ui_frame(delta);
    draw_ui();
    const auto view=view_rect();
    const auto& io=ImGui::GetIO();
    const bool focused=GetForegroundWindow()==hwnd && !io.AppFocusLost;
    const bool blocked=ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel);
    const engine::RenderViewport bounds{static_cast<int>(view.left),static_cast<int>(view.top),
        static_cast<int>(view.right-view.left),static_cast<int>(view.bottom-view.top)};
    const auto consume=[&](engine::OrbitInput input) {
        input.focused=input.focused && focused;
        input.blockStart=blocked;
        engine::update_orbit(g.orbit,input,bounds);
    };
    // Replay ordered edges after this frame's UI gating. A press/move/release
    // between two rendered frames must still move the camera, not disappear.
    for (const auto& input:g.orbit_events) consume(input);
    g.orbit_events.clear();
    g.orbit_input.wheel=0;
    g.orbit_input.rightPressed=false;
    consume(g.orbit_input);
    engine::smooth_orbit(g.displayed_orbit,g.orbit,delta);
    engine::apply_orbit(g.camera,g.displayed_orbit);
    // EndFrame/IME/platform work stays on the window thread. Only GPU recording
    // and submission execute on the render worker; no mutable frame is queued.
    g.renderer.finish_ui_frame();
    const auto camera=g.camera;
    const auto selected=g.selected;
    const engine::RenderViewport viewport{static_cast<int>(view.left),static_cast<int>(view.top),
        static_cast<int>(view.right-view.left),static_cast<int>(view.bottom-view.top)};
    g.render_worker->submit([&g,camera,selected,viewport] {
        g.renderer.render(g.scene,camera,selected,viewport);
        g.renderer.present();
    });
}
} // namespace editor
