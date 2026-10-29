#include "editor_app.hpp"
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
            RECT client{}; GetClientRect(hwnd,&client);
            g.width=static_cast<int>(std::max(1L,client.right));
            g.height=static_cast<int>(std::max(1L,client.bottom));
            update_layout();
            const auto now=std::chrono::steady_clock::now();
            static auto previous=now;
            const float delta=std::clamp(std::chrono::duration<float>(now-previous).count(),0.001F,0.1F);
            previous=now;
            g.renderer.begin_ui_frame(delta);
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
            g.renderer.render(g.scene,g.camera,g.selected,
                {static_cast<int>(view.left),static_cast<int>(view.top),
                 static_cast<int>(view.right-view.left),static_cast<int>(view.bottom-view.top)});
            g.renderer.present();
        }
        EndPaint(hwnd,&paint);
    } catch (const std::exception& error) {
        EndPaint(hwnd,&paint);
        g.renderer_failed=true;
        g.ready=false;
        g.renderer.shutdown();
        KillTimer(hwnd,1);
        MessageBoxA(hwnd,error.what(),"Vulkan renderer error",MB_OK|MB_ICONERROR);
        PostMessageW(hwnd,WM_CLOSE,0,0);
    }
}
} // namespace editor
