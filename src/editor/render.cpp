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
            RECT client{}; GetClientRect(hwnd,&client);
            g.width=static_cast<int>(std::max(1L,client.right));
            g.height=static_cast<int>(std::max(1L,client.bottom));
            update_layout();
            const auto now=std::chrono::steady_clock::now();
            static auto previous=now;
            const float delta=std::clamp(std::chrono::duration<float>(now-previous).count(),0.001F,0.1F);
            previous=now;
            g.renderer.begin_ui_frame(delta);
            LOG_DEBUG("render", "Rendering frame at %dx%d", g.width, g.height);
            draw_ui();
            update_camera();
            const auto view=view_rect();
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
