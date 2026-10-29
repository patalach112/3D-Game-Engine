#include "editor_app.hpp"
#include <algorithm>
#include <cmath>

namespace editor {
namespace {
bool over_scene(HWND hwnd,LPARAM lp) {
    POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
    return app().ready && PtInRect(&app().viewport_bounds,p)
        && !ImGui::GetIO().WantCaptureMouse && GetActiveWindow()==hwnd;
}
}
LRESULT CALLBACK window_proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
    auto& g=app();
    // Always feed input/focus/UTF-16 characters to the engine's platform backend.
    // ImGui owns textbox selection, caret, clipboard, undo, UTF-8 and scrollbars.
    const bool handled=g.renderer.handle_ui_message(hwnd,message,wp,lp);
    switch (message) {
    case WM_PAINT: render_frame(hwnd); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_GETMINMAXINFO: {
        auto* limits=reinterpret_cast<MINMAXINFO*>(lp);
        limits->ptMinTrackSize.x=900; limits->ptMinTrackSize.y=600; return 0;
    }
    case WM_TIMER:
        if (wp==1 && g.ready && !IsIconic(hwnd)) InvalidateRect(hwnd,nullptr,FALSE);
        return 0;
    case WM_SIZE:
        if (wp!=SIZE_MINIMIZED) InvalidateRect(hwnd,nullptr,FALSE);
        return 0;
    case WM_LBUTTONDOWN: {
        if (!over_scene(hwnd,lp)) return 0;
        const auto view=view_rect();
        const int width=static_cast<int>(view.right-view.left),height=static_cast<int>(view.bottom-view.top);
        const int x=GET_X_LPARAM(lp)-view.left,y=GET_Y_LPARAM(lp)-view.top;
        bool dragging=false;
        if (auto* entity=g.scene.find_entity(g.selected)) dragging=hit_gizmo(x,y,width,height,*entity);
        if (!dragging) {
            const auto ray=engine::screen_ray(static_cast<float>(x),static_cast<float>(y),g.camera,width,height);
            float closest=1.0e30F; engine::EntityId hit{};
            for (const auto& entity:g.scene.entities()) {
                float distance{};
                if (ray_box(ray,entity,distance) && distance<closest) { closest=distance; hit=entity.id; }
            }
            g.selected=hit;
        }
        SetCapture(hwnd); return 0;
    }
    case WM_LBUTTONUP:
        g.drag_axis=-1;
        if (!g.orbiting && GetCapture()==hwnd) ReleaseCapture();
        return 0;
    case WM_RBUTTONDOWN:
        if (over_scene(hwnd,lp)) {
            g.orbiting=true;
            g.last_mouse={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
            SetCapture(hwnd);
        }
        return 0;
    case WM_RBUTTONUP:
        g.orbiting=false;
        if (GetCapture()==hwnd) ReleaseCapture();
        return 0;
    case WM_MOUSEMOVE: {
        const int x=GET_X_LPARAM(lp),y=GET_Y_LPARAM(lp);
        if (g.orbiting) {
            g.yaw-=static_cast<float>(x-g.last_mouse.x)*0.006F;
            g.pitch=std::clamp(g.pitch+static_cast<float>(y-g.last_mouse.y)*0.006F,-1.4F,1.4F);
            g.last_mouse={x,y}; update_camera();
        } else if (g.drag_axis>=0 && (wp&MK_LBUTTON)) {
            const auto view=view_rect();
            const auto ray=engine::screen_ray(static_cast<float>(x-view.left),static_cast<float>(y-view.top),
                g.camera,static_cast<int>(view.right-view.left),static_cast<int>(view.bottom-view.top));
            engine::Vec3 hit{};
            if (ray_plane(ray,g.drag_start_position,g.drag_plane_normal,hit)) {
                const auto axis=mul(g.drag_axis==0?engine::Vec3{1,0,0}:g.drag_axis==1?engine::Vec3{0,1,0}:engine::Vec3{0,0,1},g.drag_sign);
                if (auto* entity=g.scene.find_entity(g.selected))
                    entity->transform.position=add(g.drag_start_position,mul(axis,dot(sub(hit,g.drag_start_hit),axis)));
            }
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        POINT point{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
        ScreenToClient(hwnd,&point); // WM_MOUSEWHEEL coordinates are screen-relative.
        if (g.ready && PtInRect(&g.viewport_bounds,point) && !ImGui::GetIO().WantCaptureMouse) {
            g.camera_distance=std::clamp(g.camera_distance-static_cast<float>(GET_WHEEL_DELTA_WPARAM(wp))/120*0.7F,2.0F,40.0F);
            update_camera();
        }
        return 0; // Over a panel, the queued ImGui wheel event scrolls that panel only.
    }
    case WM_KEYDOWN: return 0; // App shortcuts are routed after the UI has consumed keys.
    case WM_CHAR: return 0; // Already queued by ImGui's Win32 backend.
    case WM_KILLFOCUS:
    case WM_CAPTURECHANGED:
        g.orbiting=false; g.drag_axis=-1; return 0;
    case WM_CLOSE:
        KillTimer(hwnd,1); g.ready=false;
        // Release Vulkan surface and UI resources before destroying their HWND.
        g.renderer.shutdown(); DestroyWindow(hwnd); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    default: break;
    }
    return handled?1:DefWindowProcW(hwnd,message,wp,lp);
}
} // namespace editor
