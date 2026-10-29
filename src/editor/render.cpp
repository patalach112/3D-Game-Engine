#include "editor_app.hpp"
#include "engine/log.hpp"
#include <algorithm>
#include <exception>
#include <chrono>

namespace editor {
void render_frame(HWND hwnd) {
    PAINTSTRUCT paint{};
    BeginPaint(hwnd,&paint); // Validate damage only: no GDI drawing or bitmap copies.
    auto& g=app();
    try {
        if (g.ready && !g.renderer_failed && !IsIconic(hwnd)) {
            const auto frame_started=FrameClock::now();
            if (!frame_due(g,frame_started)) {
                EndPaint(hwnd,&paint);
                return;
            }
            RECT client{}; GetClientRect(hwnd,&client);
            g.width=static_cast<int>(std::max(1L,client.right));
            g.height=static_cast<int>(std::max(1L,client.bottom));
            update_layout();
            const float elapsed=g.previous_frame.time_since_epoch().count()==0
                ?1.0F/60.0F:std::chrono::duration<float>(frame_started-g.previous_frame).count();
            // ImGui averages FPS over recent frames. Use real elapsed time:
            // capping it at 0.1 s would incorrectly report at least 10 FPS.
            const float delta=elapsed>0.0F?elapsed:1.0F/60.0F;
            g.previous_frame=frame_started;
            g.renderer.begin_ui_frame(delta);
            LOG_DEBUG("render", "Rendering frame at %dx%d", g.width, g.height);
            draw_ui();
            const auto view=view_rect();
            const auto& io=ImGui::GetIO();
            engine::OrbitInput input;
            input.mouse={io.MousePos.x,io.MousePos.y}; input.wheel=io.MouseWheel;
            input.rightDown=ImGui::IsMouseDown(ImGuiMouseButton_Right);
            input.rightPressed=ImGui::IsMouseClicked(ImGuiMouseButton_Right);
            input.focused=GetForegroundWindow()==hwnd && !io.AppFocusLost;
            input.pointerValid=ImGui::IsMousePosValid();
            input.blockStart=ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel);
            engine::update_orbit(g.orbit,input,{static_cast<int>(view.left),static_cast<int>(view.top),
                static_cast<int>(view.right-view.left),static_cast<int>(view.bottom-view.top)});
            update_camera();
            g.renderer.set_grid_visible(g.grid_visible);
            g.renderer.set_clear_color(g.background_color);
            g.renderer.set_vsync(g.vsync);
            g.renderer.render(g.scene,g.camera,g.selected,
                {static_cast<int>(view.left),static_cast<int>(view.top),
                 static_cast<int>(view.right-view.left),static_cast<int>(view.bottom-view.top)});
            g.renderer.present();
            schedule_next_frame(g,frame_started,FrameClock::now());
        }
        EndPaint(hwnd,&paint);
    } catch (const std::exception& error) {
        EndPaint(hwnd,&paint);
        g.renderer_failed=true;
        g.ready=false;
        g.renderer.shutdown();
        MessageBoxA(hwnd,error.what(),"Vulkan renderer error",MB_OK|MB_ICONERROR);
        PostMessageW(hwnd,WM_CLOSE,0,0);
    }
}
} // namespace editor
