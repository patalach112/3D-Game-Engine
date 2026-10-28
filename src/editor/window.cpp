#include "editor_app.hpp"

#include <algorithm>
#include <cmath>

namespace editor {

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    App& g = app();
    switch (message) {
    case WM_PAINT:
        render_frame(hwnd);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_GETMINMAXINFO: {
        auto* limits = reinterpret_cast<MINMAXINFO*>(lp);
        limits->ptMinTrackSize.x = 900;
        limits->ptMinTrackSize.y = 600;
        return 0;
    }
    case WM_SETCURSOR: {
        if (LOWORD(lp) == HTCLIENT) {
            POINT cursor{};
            GetCursorPos(&cursor);
            ScreenToClient(hwnd, &cursor);
            const int right_x = g.width - g.right_width;
            if ((std::abs(cursor.x - g.left_width) <= 5 || std::abs(cursor.x - right_x) <= 5)
                && cursor.y >= kHeaderHeight && cursor.y < g.height - kFooterHeight) {
                SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
                return TRUE;
            }
        }
        break;
    }
    case WM_SIZE:
        g.width = LOWORD(lp);
        g.height = HIWORD(lp);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_LBUTTONDOWN: {
        const int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        const int right_x = g.width - g.right_width;

        // Edycja pól inspektora po zaznaczeniu obiektu.
        const int clicked_field = field_at(x, y);
        if (clicked_field >= 0) {
            if (g.active_field >= 0) commit_field_edit();
            begin_field_edit(clicked_field);
            SetFocus(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (g.active_field >= 0) commit_field_edit();

        if (y >= kHeaderHeight && y < g.height - kFooterHeight && std::abs(x - g.left_width) <= 5) {
            g.resizing_left_panel = true;
            SetCapture(hwnd);
            return 0;
        }
        if (y >= kHeaderHeight && y < g.height - kFooterHeight && std::abs(x - right_x) <= 5) {
            g.resizing_right_panel = true;
            SetCapture(hwnd);
            return 0;
        }

        const int view_w = std::max(1, g.width - g.left_width - g.right_width);
        const int view_h = std::max(1, g.height - kHeaderHeight - kFooterHeight);
        if (x < g.left_width) {
            if (y >= 54 && y < 84 && x >= g.left_width - 72 && x < g.left_width - 43)
                add_primitive(engine::PrimitiveType::Cube);
            else if (y >= 96 && y < 132) add_primitive(engine::PrimitiveType::Cube);
            else if (y >= 137 && y < 173) add_primitive(engine::PrimitiveType::Sphere);
            else if (y >= 178 && y < 214) add_primitive(engine::PrimitiveType::Cylinder);
            else if (y >= 219 && y < 255) add_primitive(engine::PrimitiveType::Plane);
            else if (y >= 260 && y < 298) save_scene();
            else if (y >= 336) {
                const int row = (y - 336) / 34;
                if (row >= 0 && static_cast<std::size_t>(row) < g.scene.entity_count()) {
                    g.selected = g.scene.entities()[static_cast<std::size_t>(row)].id;
                    g.active_field = -1;
                    g.scene_dirty = true;
                }
            }
        } else if (x >= g.left_width && x < g.width - g.right_width
                   && y >= kHeaderHeight && y < g.height - kFooterHeight) {
            const int vx = x - g.left_width, vy = y - kHeaderHeight;
            bool dragging = false;
            if (auto* selected = g.scene.find_entity(g.selected))
                dragging = hit_gizmo(vx, vy, view_w, view_h, *selected);
            if (!dragging) {
                const auto ray = engine::screen_ray(static_cast<float>(vx), static_cast<float>(vy), g.camera, view_w, view_h);
                float closest = 1.0e30F;
                engine::EntityId hit = 0;
                for (const auto& entity : g.scene.entities()) {
                    float t{};
                    if (ray_box(ray, entity, t) && t < closest) { closest = t; hit = entity.id; }
                }
                g.selected = hit;
                g.scene_dirty = true; // Zaznaczenie widać w gizmie, więc widok się zmienia.
            }
            SetCapture(hwnd);
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_LBUTTONUP:
        g.drag_axis = -1;
        g.resizing_left_panel = false;
        g.resizing_right_panel = false;
        if (GetCapture() == hwnd) ReleaseCapture();
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
                const engine::Vec3 axis = mul(
                    g.drag_axis == 0 ? engine::Vec3{1, 0, 0} : g.drag_axis == 1 ? engine::Vec3{0, 1, 0} : engine::Vec3{0, 0, 1},
                    g.drag_sign);
                const float amount = dot(sub(hit, g.drag_start_hit), axis);
                if (auto* entity = g.scene.find_entity(g.selected)) {
                    entity->transform.position = add(g.drag_start_position, mul(axis, amount));
                    scene_changed = true;
                    g.scene_dirty = true;
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
            g.last_mouse = {x, y};
        }
        return 0;
    }
    case WM_RBUTTONUP:
        g.orbiting = false;
        if (GetCapture() == hwnd) ReleaseCapture();
        return 0;
    case WM_MOUSEWHEEL:
        g.camera_distance = std::clamp(
            g.camera_distance - static_cast<float>(GET_WHEEL_DELTA_WPARAM(wp)) / 120.0F * 0.7F,
            2.0F, 40.0F);
        update_camera();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_CHAR:
        if (g.active_field >= 0) {
            const wchar_t ch = static_cast<wchar_t>(wp);
            if (ch == L'\r') {
                commit_field_edit();
            } else if (ch == L'\b') {
                if (g.replace_field_on_char) g.field_input.clear();
                else if (!g.field_input.empty()) g.field_input.pop_back();
                g.replace_field_on_char = false;
            } else if ((ch >= L'0' && ch <= L'9') || ch == L'-' || ch == L'.' || ch == L',') {
                if (g.replace_field_on_char) g.field_input.clear();
                g.field_input.push_back(ch == L',' ? L'.' : ch);
                g.replace_field_on_char = false;
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        break;
    case WM_KEYDOWN:
        if (g.active_field >= 0) {
            if (wp == VK_ESCAPE) {
                g.active_field = -1;
                g.field_input.clear();
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        if (wp == 'N') add_primitive(engine::PrimitiveType::Cube);
        else if (wp == VK_DELETE && g.selected) {
            (void)g.scene.remove_entity(g.selected);
            g.selected = g.scene.entity_count() ? g.scene.entities().front().id : 0;
            g.status = L"Usunięto obiekt";
            g.scene_dirty = true;
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_CAPTURECHANGED:
        g.orbiting = false;
        g.drag_axis = -1;
        g.resizing_left_panel = false;
        g.resizing_right_panel = false;
        return 0;
    case WM_DESTROY:
        // Bufor interfejsu sprzątaje release_editor_resources() po cyklu message.
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wp, lp);
    }
    // WM_SETCURSOR może wykonać break z switcha, gdy wskaźnik nie leży na separatorze.
    return DefWindowProcW(hwnd, message, wp, lp);
}

} // namespace editor
