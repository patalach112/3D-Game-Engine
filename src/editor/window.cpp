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
void apply_input_event(HWND hwnd,const engine::InputEvent& event) {
    auto& g=app();
    if (event.kind==engine::InputKind::Reset) {
        auto& io=ImGui::GetIO();
        io.ClearEventsQueue(); io.ClearInputKeys(); io.ClearInputMouse();
        io.AddFocusEvent(false); io.AddFocusEvent(GetForegroundWindow()==hwnd);
        g.orbit_input={}; g.orbit_input.pointerValid=false; g.orbit_events.clear();
        g.orbit.dragging=false; g.drag_axis=-1;
        g.native_buttons=0;
        if (GetCapture()==hwnd) ReleaseCapture();
        g.status=L"Przeciążenie kolejki input: zresetowano przyciski";
        return;
    }
    const auto message=static_cast<UINT>(event.message);
    const auto wp=static_cast<WPARAM>(event.wparam);
    const auto lp=static_cast<LPARAM>(event.lparam);
    g.renderer.handle_ui_message(hwnd,message,wp,lp);
    g.orbit_input.rightPressed=false;
    g.orbit_input.wheel=0;
    if (event.kind==engine::InputKind::PointerMove || event.kind==engine::InputKind::PointerButton
        || event.kind==engine::InputKind::Wheel) {
        g.orbit_input.mouse={event.x,event.y};
        g.orbit_input.pointerValid=true;
    }
    if (message==WM_RBUTTONDOWN || message==WM_RBUTTONDBLCLK) {
        g.orbit_input.rightDown=true; g.orbit_input.rightPressed=true;
    } else if (message==WM_RBUTTONUP) g.orbit_input.rightDown=false;
    else if (message==WM_MOUSEWHEEL) g.orbit_input.wheel+=static_cast<float>(GET_WHEEL_DELTA_WPARAM(wp))/WHEEL_DELTA;
    else if (message==WM_KILLFOCUS || message==WM_CAPTURECHANGED) {
        g.orbit_input.rightDown=false; g.orbit_input.rightPressed=false;
    }
    if (message==WM_KILLFOCUS) g.orbit_input.focused=false;
    else if (message==WM_SETFOCUS) g.orbit_input.focused=true;
    if (event.kind==engine::InputKind::PointerMove || event.kind==engine::InputKind::PointerButton
        || event.kind==engine::InputKind::Wheel || event.kind==engine::InputKind::Focus)
        g.orbit_events.push_back(g.orbit_input);
    switch (message) {
    case WM_LBUTTONDOWN: {
        if (!over_scene(hwnd,lp)) return;
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
        return; // Native capture is maintained immediately by window_proc.
    }
    case WM_LBUTTONUP:
        g.drag_axis=-1;
        return;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP: return; // Orbit input is consumed after ImGui::NewFrame.
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
        return;
    }
    case WM_MOUSEWHEEL: return; // Current-frame input routes scroll vs camera zoom.
    case WM_KEYDOWN: return; // App shortcuts are routed after the UI has consumed keys.
    case WM_CHAR: return; // Already queued by ImGui's Win32 backend.
    case WM_KILLFOCUS:
    case WM_CAPTURECHANGED:
        g.orbit.dragging=false; g.drag_axis=-1; return;
    default: return;
    }
}

LRESULT CALLBACK window_proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
    auto& g=app();
    engine::InputKind kind=engine::InputKind::Other;
    bool queued=true;
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT paint{}; BeginPaint(hwnd,&paint); EndPaint(hwnd,&paint); return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_GETMINMAXINFO: {
        // Let Windows calculate the maximized rectangle from the monitor work
        // area first. Returning immediately with only ptMinTrackSize filled can
        // make a maximized window cover the taskbar, especially on a secondary
        // monitor or when the taskbar is placed on the top/side of the screen.
        const auto result=DefWindowProcW(hwnd,message,wp,lp);
        auto* limits=reinterpret_cast<MINMAXINFO*>(lp);
        limits->ptMinTrackSize.x=900;
        limits->ptMinTrackSize.y=600;
        return result;
    }
    case WM_SIZE: g.minimized=wp==SIZE_MINIMIZED; return 0;
    case WM_CLOSE: g.close_requested=true; return 0;
    case WM_DESTROY: g.close_requested=true; PostQuitMessage(0); return 0;
    case kWorkerWake: return 0;
    case WM_MOUSEMOVE: kind=engine::InputKind::PointerMove; break;
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: case WM_LBUTTONUP:
    case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK: case WM_RBUTTONUP:
    case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK: case WM_MBUTTONUP:
    case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK: case WM_XBUTTONUP:
        kind=engine::InputKind::PointerButton; break;
    case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL: kind=engine::InputKind::Wheel; break;
    case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP:
        kind=engine::InputKind::Key; break;
    case WM_CHAR: kind=engine::InputKind::Text; break;
    case WM_KILLFOCUS: case WM_SETFOCUS: case WM_CAPTURECHANGED: kind=engine::InputKind::Focus; break;
    case WM_SETCURSOR: case WM_MOUSELEAVE: case WM_INPUTLANGCHANGE: case WM_DEVICECHANGE: break;
    default: queued=false; break;
    }
    if (queued && g.inputs) {
        engine::InputEvent event;
        event.window=reinterpret_cast<std::uintptr_t>(hwnd);
        event.message=message; event.kind=kind; event.wparam=wp; event.lparam=lp;
        event.extra_info=static_cast<std::uint64_t>(GetMessageExtraInfo());
        GetKeyboardState(event.keyboard.data());
        if (kind==engine::InputKind::Wheel) {
            POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)}; ScreenToClient(hwnd,&p);
            event.x=static_cast<float>(p.x); event.y=static_cast<float>(p.y);
        }
        g.inputs->enqueue(event); // NASM producer; input.cpp is the sole consumer.
    }
    // Capture must happen immediately on the HWND-owning thread, not in the
    // worker. Keep all buttons captured until the final release.
    if (kind==engine::InputKind::PointerButton) {
        unsigned button=0; bool down=false;
        switch (message) {
        case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: button=1; down=true; break;
        case WM_LBUTTONUP: button=1; break;
        case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK: button=2; down=true; break;
        case WM_RBUTTONUP: button=2; break;
        case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK: button=4; down=true; break;
        case WM_MBUTTONUP: button=4; break;
        default: button=GET_XBUTTON_WPARAM(wp)==XBUTTON1?8:16;
            down=message!=WM_XBUTTONUP; break;
        }
        if (down) { g.native_buttons|=button; SetCapture(hwnd); }
        else { g.native_buttons&=~button; if (!g.native_buttons && GetCapture()==hwnd) ReleaseCapture(); }
        return message==WM_XBUTTONDOWN || message==WM_XBUTTONUP || message==WM_XBUTTONDBLCLK?TRUE:0;
    }
    if (message==WM_KILLFOCUS || message==WM_CAPTURECHANGED) g.native_buttons=0;
    if (message==WM_KILLFOCUS && GetCapture()==hwnd) ReleaseCapture();
    if (message==WM_SETCURSOR && LOWORD(lp)==HTCLIENT) return TRUE;
    if (message==WM_KEYDOWN || message==WM_KEYUP || message==WM_CHAR || message==WM_MOUSEWHEEL || message==WM_MOUSEHWHEEL)
        return 0;
    return DefWindowProcW(hwnd,message,wp,lp);
}
} // namespace editor
