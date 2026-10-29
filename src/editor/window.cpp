#include "editor_app.hpp"
#include "engine/log.hpp"
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
    case WM_MOUSEMOVE: {
        const int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        bool scene_changed = false;
        if (g.resizing_left_panel && (wp & MK_LBUTTON)) {
            const int maximum = std::max(170, g.width - g.right_width - 300);
            const int resized = std::clamp(x, 170, maximum);
            scene_changed = resized != g.left_width;
            g.left_width = resized;
        } else if (g.resizing_right_panel && (wp & MK_LBUTTON)) {
            const int maximum = std::max(240, g.width - g.left_width - 300);
            const int resized = std::clamp(g.width - x, 240, maximum);
            scene_changed = resized != g.right_width;
            g.right_width = resized;
        } else if (g.orbiting) {
            const float dx = static_cast<float>(x - g.last_mouse.x);
            const float dy = static_cast<float>(y - g.last_mouse.y);
            g.yaw -= dx * 0.006F;
            // Ruch myszy w górę (negative dy) pokazuje wyżej.
            g.pitch = std::clamp(g.pitch + dy * 0.006F, -1.4F, 1.4F);
            g.last_mouse = {x, y};
            update_camera();
            scene_changed = true;
        } else if (g.drag_axis >= 0 && (wp & MK_LBUTTON)) {
            const int vw = std::max(1, g.width - g.left_width - g.right_width);
            const int vh = std::max(1, g.height - kHeaderHeight - kFooterHeight);
            const auto ray = engine::screen_ray(
                static_cast<float>(x - g.left_width), static_cast<float>(y - kHeaderHeight),
                g.camera, vw, vh);
            engine::Vec3 hit{};
            if (ray_plane(ray, g.drag_start_position, g.drag_plane_normal, hit)) {
                if (auto* entity = g.scene.find_entity(g.selected)) {
                    if (g.gizmo_mode == 0) {
                        // Pozycja: ruch wzdłuż osi.
                        const engine::Vec3 axis = mul(
                            g.drag_axis == 0 ? engine::Vec3{1, 0, 0} : g.drag_axis == 1 ? engine::Vec3{0, 1, 0} : engine::Vec3{0, 0, 1},
                            g.drag_sign);
                        const float amount = dot(sub(hit, g.drag_start_hit), axis);
                        entity->transform.position = add(g.drag_start_position, mul(axis, amount));
                        scene_changed = true;
                        g.scene_dirty = true;
                    } else if (g.gizmo_mode == 1) {
                        // Obrót: kąt między początkowym a bieżącym punktem, patrząc od osi.
                        const engine::Vec3 axis = mul(
                            g.drag_axis == 0 ? engine::Vec3{1, 0, 0} : g.drag_axis == 1 ? engine::Vec3{0, 1, 0} : engine::Vec3{0, 0, 1},
                            g.drag_sign);
                        const engine::Vec3 v0 = sub(g.drag_start_hit, entity->transform.position);
                        const engine::Vec3 v1 = sub(hit, entity->transform.position);
                        // Proektujemy na płaszczyznę prostopadłą do osi.
                        const engine::Vec3 p0 = sub(v0, mul(axis, dot(v0, axis)));
                        const engine::Vec3 p1 = sub(v1, mul(axis, dot(v1, axis)));
                        const float mag0 = magnitude(p0);
                        const float mag1 = magnitude(p1);
                        if (mag0 > 0.0001F && mag1 > 0.0001F) {
                            float cos_angle = dot(p0, p1) / (mag0 * mag1);
                            cos_angle = std::clamp(cos_angle, -1.0F, 1.0F);
                            const float angle = std::acos(cos_angle) * (g.drag_sign > 0 ? 1.0F : -1.0F);
                            const float rad = angle * g.drag_sign;
                            // Obrót wokół osi przez kąt w stopniach.
                            const engine::Vec3& r = entity->transform.rotation;
                            const float& slot = (g.drag_axis == 0) ? r.x : (g.drag_axis == 1) ? r.y : r.z;
                            slot = std::clamp(slot + rad * 57.2958F, -360.0F, 360.0F);
                            scene_changed = true;
                            g.scene_dirty = true;
                        }
                    } else if (g.gizmo_mode == 2) {
                        // Skala: współczynnik = odległość bieżąca / początkowa.
                        const engine::Vec3 axis = mul(
                            g.drag_axis == 0 ? engine::Vec3{1, 0, 0} : g.drag_axis == 1 ? engine::Vec3{0, 1, 0} : engine::Vec3{0, 0, 1},
                            g.drag_sign);
                        const engine::Vec3 v0 = sub(g.drag_start_hit, entity->transform.position);
                        const engine::Vec3 v1 = sub(hit, entity->transform.position);
                        const float d0 = dot(v0, axis);
                        const float d1 = dot(v1, axis);
                        if (std::abs(d0) > 0.0001F) {
                            const float factor = d1 / d0;
                            const float min_scale = 0.01F;
                            const engine::Vec3& s = entity->transform.scale;
                            float& slot = (g.drag_axis == 0) ? s.x : (g.drag_axis == 1) ? s.y : s.z;
                            slot = std::max(min_scale, std::abs(slot) * factor);
                            scene_changed = true;
                            g.scene_dirty = true;
                        }
                    }
                }
            }
        }
        if (scene_changed) InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_RBUTTONDOWN: {
        const int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        if (x >= g.left_width && x < g.width - g.right_width
            && y >= kHeaderHeight && y < g.height - kFooterHeight) {
            g.orbiting = true;
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
            return 0;
        }
        if (wp == 'D' && (GetKeyState(VK_CONTROL) & 0x8000)) duplicate_entity();
        else if (wp == 'S' && (GetKeyState(VK_CONTROL) & 0x8000)) save_scene();
        else if (wp == 'N') add_primitive(engine::PrimitiveType::Cube);
        else if (wp == 'Q') { g.gizmo_mode = 0; g.status = L"Gizmo: pozycja"; }
        else if (wp == 'W') { g.gizmo_mode = 1; g.status = L"Gizmo: obrót"; }
        else if (wp == 'E') { g.gizmo_mode = 2; g.status = L"Gizmo: skala"; }
        else if (wp == 'F') frame_selected();
        else if (wp == VK_DELETE && g.selected) {
            (void)g.scene.remove_entity(g.selected);
            g.selected = g.scene.entity_count() ? g.scene.entities().front().id : 0;
            g.status = L"Usunięto obiekt";
            g.scene_dirty = true;
        }
        else if (wp == VK_LEFT || wp == VK_RIGHT || wp == VK_UP || wp == VK_DOWN) {
            // Przesunięcie zaznaczonego obiektu strzałkami (1 jednostkę).
            if (auto* entity = g.scene.find_entity(g.selected)) {
                float dx = 0.0F, dy = 0.0F, dz = 0.0F;
                if (wp == VK_LEFT) dx = -1.0F;
                else if (wp == VK_RIGHT) dx = 1.0F;
                else if (wp == VK_UP) dy = 1.0F;
                else if (wp == VK_DOWN) dy = -1.0F;
                entity->transform.position.x += dx;
                entity->transform.position.y += dy;
                entity->transform.position.z += dz;
                g.status = L"Przesunięto obiekt";
                g.scene_dirty = true;
            }
        }
        else if (wp == VK_ADD || wp == VK_OEM_PLUS) {
            g.camera_distance = std::max(2.0F, g.camera_distance - 1.0F);
            update_camera();
            g.status = L"Przybliżenie";
        }
        else if (wp == VK_SUBTRACT || wp == VK_OEM_MINUS) {
            g.camera_distance = std::min(40.0F, g.camera_distance + 1.0F);
            update_camera();
            g.status = L"Oddalenie";
        }
        InvalidateRect(hwnd, nullptr, FALSE);
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
