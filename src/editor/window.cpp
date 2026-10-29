#include "editor_app.hpp"
#include "engine/log.hpp"
#include <algorithm>
#include <cmath>

namespace editor {
namespace {
bool over_scene(HWND hwnd,LPARAM lp) {
    POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
    return app().ready && PtInRect(&app().viewport_bounds,p)
        && !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel)
        && GetActiveWindow()==hwnd;
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
        return 0; // Win32 ImGui backend already maintains per-button capture.
    }
    case WM_LBUTTONUP:
        g.drag_axis=-1;
        return 0;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP: return 0; // Orbit input is consumed after ImGui::NewFrame.
    case WM_MOUSEMOVE: {
        if (g.drag_axis >= 0 && (wp & MK_LBUTTON)) {
            const auto view = view_rect();
            const auto ray = engine::screen_ray(
                static_cast<float>(GET_X_LPARAM(lp) - view.left),
                static_cast<float>(GET_Y_LPARAM(lp) - view.top), g.camera,
                static_cast<int>(view.right - view.left), static_cast<int>(view.bottom - view.top));
            engine::Vec3 hit{};
            if (auto* entity = g.scene.find_entity(g.selected);
                entity && ray_plane(ray, g.drag_start_position, g.drag_plane_normal, hit)) {
                const auto axis = mul(g.drag_axis == 0 ? engine::Vec3{1,0,0}
                    : g.drag_axis == 1 ? engine::Vec3{0,1,0} : engine::Vec3{0,0,1}, g.drag_sign);
                if (g.gizmo_mode == 0) {
                    entity->transform.position = add(g.drag_start_position,
                        mul(axis, dot(sub(hit, g.drag_start_hit), axis)));
                } else if (g.gizmo_mode == 1) {
                    const auto v0 = sub(g.drag_start_hit, g.drag_start_position);
                    const auto v1 = sub(hit, g.drag_start_position);
                    const auto p0 = sub(v0, mul(axis, dot(v0, axis)));
                    const auto p1 = sub(v1, mul(axis, dot(v1, axis)));
                    if (magnitude(p0) > 0.0001F && magnitude(p1) > 0.0001F) {
                        // Signed angle, always relative to the initial transform.
                        const float angle = std::atan2(dot(axis, cross(p0, p1)), dot(p0, p1));
                        auto rotation = g.drag_start_rotation;
                        float& value = g.drag_axis == 0 ? rotation.x : g.drag_axis == 1 ? rotation.y : rotation.z;
                        value += angle * g.drag_sign * 57.2957795F;
                        entity->transform.rotation = rotation;
                    }
                } else if (g.gizmo_mode == 2) {
                    const float start = dot(sub(g.drag_start_hit, g.drag_start_position), axis);
                    if (std::abs(start) > 0.0001F) {
                        auto scale = g.drag_start_scale;
                        float& value = g.drag_axis == 0 ? scale.x : g.drag_axis == 1 ? scale.y : scale.z;
                        const float factor = dot(sub(hit, g.drag_start_position), axis) / start;
                        value = std::max(0.01F, std::abs(value) * factor);
                        entity->transform.scale = scale;
                    }
                }
            }
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_MOUSEWHEEL: return 0; // Current-frame input routes scroll vs camera zoom.
    case WM_KEYDOWN: return 0; // App shortcuts are routed after the UI has consumed keys.
    case WM_CHAR: return 0; // Already queued by ImGui's Win32 backend.
    case WM_KILLFOCUS:
    case WM_CAPTURECHANGED:
        g.orbit.dragging=false; g.drag_axis=-1; return 0;
    case WM_CLOSE:
        LOG_INFO("editor", "Window closing");
        KillTimer(hwnd,1); g.ready=false;
        // Release Vulkan surface and UI resources before destroying their HWND.
        g.renderer.shutdown(); DestroyWindow(hwnd); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    default: break;
    }
    return handled?1:DefWindowProcW(hwnd,message,wp,lp);
}
} // namespace editor
