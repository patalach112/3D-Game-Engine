#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>

#include "engine/render/software_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <fstream>
#include <string>
#include <vector>

namespace {
constexpr int kHeaderHeight = 42;
constexpr int kFooterHeight = 27;

struct App {
    engine::Scene scene;
    engine::SoftwareRenderer renderer;
    engine::EditorCamera camera;
    engine::EntityId selected{};
    int width{1280};
    int height{800};
    int left_width{210};
    int right_width{270};
    bool layout_initialized{};
    bool resizing_left_panel{};
    bool resizing_right_panel{};
    float yaw{0.75F};
    float pitch{0.42F};
    float camera_distance{8.0F};
    bool orbiting{};
    POINT last_mouse{};
    int drag_axis{-1};
    float drag_sign{1.0F};
    engine::Vec3 drag_start_position{};
    engine::Vec3 drag_start_hit{};
    engine::Vec3 drag_plane_normal{};
    std::wstring status{L"Gotowe"};
    std::vector<std::uint32_t> frame;
    HDC backbuffer_dc{};
    HBITMAP backbuffer_bitmap{};
    HBITMAP backbuffer_previous{};
    int backbuffer_width{};
    int backbuffer_height{};
    HFONT font{};
    HFONT bold_font{};
    int font_height{};
};

App g_app;

engine::Vec3 add(engine::Vec3 a, engine::Vec3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
engine::Vec3 sub(engine::Vec3 a, engine::Vec3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
engine::Vec3 mul(engine::Vec3 a, float n) { return {a.x*n, a.y*n, a.z*n}; }
float dot(engine::Vec3 a, engine::Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
engine::Vec3 cross(engine::Vec3 a, engine::Vec3 b) {
    return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
}
float magnitude(engine::Vec3 a) { return std::sqrt(dot(a,a)); }
engine::Vec3 normalized(engine::Vec3 a) {
    const float len = magnitude(a);
    return len < 0.00001F ? engine::Vec3{0,0,1} : mul(a,1.0F/len);
}

void update_camera() {
    const auto target = g_app.camera.target;
    const float cp = std::cos(g_app.pitch);
    g_app.camera.position = {
        target.x + g_app.camera_distance * cp * std::sin(g_app.yaw),
        target.y + g_app.camera_distance * std::sin(g_app.pitch),
        target.z + g_app.camera_distance * cp * std::cos(g_app.yaw)
    };
}

void update_layout() {
    if (!g_app.layout_initialized) {
        g_app.left_width = std::clamp(g_app.width / 7, 180, 245);
        g_app.right_width = std::clamp(g_app.width / 4, 280, 360);
        g_app.layout_initialized = true;
    }
    const int max_left = std::max(170, g_app.width - g_app.right_width - 300);
    g_app.left_width = std::clamp(g_app.left_width, 170, max_left);
    const int max_right = std::max(240, g_app.width - g_app.left_width - 300);
    g_app.right_width = std::clamp(g_app.right_width, 240, max_right);
    const int view_width = std::max(1, g_app.width - g_app.left_width - g_app.right_width);
    const int view_height = std::max(1, g_app.height - kHeaderHeight - kFooterHeight);
    g_app.renderer.resize(view_width, view_height);
    const int wanted_font_height = std::clamp(g_app.height / 55, 15, 17);
    if (wanted_font_height != g_app.font_height) {
        if (g_app.font) DeleteObject(g_app.font);
        if (g_app.bold_font) DeleteObject(g_app.bold_font);
        g_app.font_height = wanted_font_height;
        g_app.font = CreateFontW(-wanted_font_height,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
            DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
            DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
        g_app.bold_font = CreateFontW(-wanted_font_height,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,
            DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
            DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
    }
}

RECT view_rect() {
    return {g_app.left_width, kHeaderHeight,
            g_app.width - g_app.right_width, g_app.height - kFooterHeight};
}

void set_font(HDC dc, bool bold = false) {
    SelectObject(dc, bold ? g_app.bold_font : g_app.font);
    SetBkMode(dc, TRANSPARENT);
}

void fill(HDC dc, RECT rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void line(HDC dc, int x1, int y1, int x2, int y2, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ old = SelectObject(dc, pen);
    MoveToEx(dc, x1, y1, nullptr);
    LineTo(dc, x2, y2);
    SelectObject(dc, old);
    DeleteObject(pen);
}

void draw_text(HDC dc, const std::wstring& value, int x, int y, COLORREF color,
               int max_width = 0, bool bold = false) {
    set_font(dc, bold);
    SetTextColor(dc, color);
    RECT rect{x, y, max_width > 0 ? x + max_width : g_app.width, y + 32};
    DrawTextW(dc, value.c_str(), static_cast<int>(value.size()), &rect,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
}

std::wstring number_text(float value, int precision = 2) {
    wchar_t buffer[32]{};
    std::swprintf(buffer, 32, L"%.*f", precision, static_cast<double>(value));
    return buffer;
}

void draw_button(HDC dc, RECT rect, const wchar_t* caption, bool primary = false) {
    fill(dc, rect, primary ? RGB(51,51,53) : RGB(43,43,44));
    line(dc, rect.left, rect.top, rect.right, rect.top, RGB(68,68,70));
    line(dc, rect.left, rect.bottom-1, rect.right, rect.bottom-1, RGB(20,20,21));
    draw_text(dc, caption, rect.left + 12, rect.top + 8, RGB(236,236,238), rect.right-rect.left-20);
}

void draw_ui(HDC dc) {
    const int w = g_app.width, h = g_app.height;
    const int right_x = w - g_app.right_width;
    fill(dc, {0,0,w,kHeaderHeight}, RGB(31,31,32));
    fill(dc, {0,kHeaderHeight,g_app.left_width,h-kFooterHeight}, RGB(29,29,30));
    fill(dc, {right_x,kHeaderHeight,w,h-kFooterHeight}, RGB(29,29,30));
    line(dc,0,kHeaderHeight-1,w,kHeaderHeight-1,RGB(57,57,59));
    fill(dc,{g_app.left_width-3,kHeaderHeight,g_app.left_width+3,h-kFooterHeight},RGB(34,34,35));
    fill(dc,{right_x-3,kHeaderHeight,right_x+3,h-kFooterHeight},RGB(34,34,35));
    const int grip_y = (kHeaderHeight + h - kFooterHeight) / 2;
    for (int offset=-12; offset<=12; offset+=6) {
        fill(dc,{g_app.left_width-1,grip_y+offset,g_app.left_width+2,grip_y+offset+2},RGB(104,104,106));
        fill(dc,{right_x-1,grip_y+offset,right_x+2,grip_y+offset+2},RGB(104,104,106));
    }

    fill(dc,{14,10,40,36},RGB(230,112,43));
    draw_text(dc,L"S",21,13,RGB(255,255,255),20,true);
    draw_text(dc,L"STARTER",50,13,RGB(246,246,246),90,true);
    draw_text(dc,L"SCENA",150,14,RGB(238,238,238),70);
    line(dc,148,kHeaderHeight-2,198,kHeaderHeight-2,RGB(230,112,43));
    draw_text(dc,L"Własny silnik 3D   •   C++",w-g_app.right_width-222,14,RGB(177,177,180),210);

    draw_text(dc,L"Hierarchia",14,58,RGB(243,243,243),g_app.left_width-75,true);
    draw_button(dc,{g_app.left_width-72,54,g_app.left_width-43,84},L"+");
    draw_button(dc,{g_app.left_width-37,54,g_app.left_width-10,84},L"…");
    draw_button(dc,{10,96,g_app.left_width-10,134},L"+  Dodaj kostkę",true);
    draw_button(dc,{10,140,g_app.left_width-10,178},L"Zapisz scenę");
    draw_text(dc,L"OBIEKTY SCENY",14,190,RGB(165,165,168),g_app.left_width-24);

    int row_y = 216;
    for (const auto& entity : g_app.scene.entities()) {
        const bool selected = entity.id == g_app.selected;
        if (selected) {
            fill(dc,{6,row_y,g_app.left_width-6,row_y+32},RGB(48,48,49));
            fill(dc,{6,row_y,9,row_y+32},RGB(230,112,43));
        }
        fill(dc,{17,row_y+10,27,row_y+20},selected?RGB(230,180,146):RGB(155,155,158));
        draw_text(dc,std::wstring(entity.name.begin(),entity.name.end()),36,row_y+7,
                  selected?RGB(248,248,248):RGB(190,190,193),g_app.left_width-52);
        row_y += 34;
    }

    draw_text(dc,L"Inspektor",right_x+15,58,RGB(244,244,244),g_app.right_width-30,true);
    line(dc,right_x+10,91,w-10,91,RGB(57,57,59));
    if (const auto* selected = g_app.scene.find_entity(g_app.selected)) {
        draw_text(dc,std::wstring(selected->name.begin(),selected->name.end()),right_x+15,105,
                  RGB(230,133,78),g_app.right_width-28,true);
        fill(dc,{right_x+10,137,w-10,166},RGB(38,38,39));
        draw_text(dc,L"TRANSFORMACJA",right_x+17,143,RGB(177,177,180),g_app.right_width-34);
        const auto& t = selected->transform;
        const auto draw_vector = [&](const wchar_t* title, int y, engine::Vec3 value, int precision) {
            constexpr int label_width = 59;
            const int column_width = (g_app.right_width - 30 - label_width) / 3;
            const int values_x = right_x + 15 + label_width;
            draw_text(dc,title,right_x+15,y,RGB(165,165,168),label_width);
            draw_text(dc,L"X  "+number_text(value.x,precision),values_x,y,RGB(235,235,236),column_width);
            draw_text(dc,L"Y  "+number_text(value.y,precision),values_x+column_width,y,RGB(235,235,236),column_width);
            draw_text(dc,L"Z  "+number_text(value.z,precision),values_x+2*column_width,y,RGB(235,235,236),column_width);
        };
        draw_vector(L"Pozycja",176,t.position,2);
        draw_vector(L"Obrót",207,t.rotation,1);
        draw_vector(L"Skala",238,t.scale,2);
    } else {
        draw_text(dc,L"Brak zaznaczenia",right_x+15,108,RGB(185,185,188),g_app.right_width-28);
        draw_text(dc,L"Kliknij obiekt w widoku sceny.",right_x+15,134,RGB(155,155,158),g_app.right_width-28);
    }
    fill(dc,{right_x+10,270,w-10,299},RGB(38,38,39));
    draw_text(dc,L"STEROWANIE",right_x+17,276,RGB(177,177,180),g_app.right_width-34);
    draw_text(dc,L"Przeciągnij kolorowe uchwyty osi",right_x+15,311,RGB(190,190,193),g_app.right_width-28);
    draw_text(dc,L"PPM + przeciąganie: obrót kamery",right_x+15,338,RGB(190,190,193),g_app.right_width-28);
    draw_text(dc,L"Kółko myszy: przybliżenie",right_x+15,365,RGB(190,190,193),g_app.right_width-28);
    draw_text(dc,L"Delete: usuń zaznaczony obiekt",right_x+15,392,RGB(190,190,193),g_app.right_width-28);
    draw_text(dc,L"Przeciągnij separator, by zmienić szerokość panelu.",
              right_x+15,419,RGB(160,160,163),g_app.right_width-28);

    fill(dc,{0,h-kFooterHeight,w,h},RGB(31,31,32));
    line(dc,0,h-kFooterHeight,w,h-kFooterHeight,RGB(57,57,59));
    draw_text(dc,g_app.status+L"   |   Obiektów: "+std::to_wstring(g_app.scene.entity_count()),
              10,h-kFooterHeight+5,RGB(185,185,188),w-20);
}

void ensure_backbuffer(HWND hwnd, HDC reference_dc) {
    if (g_app.backbuffer_dc && g_app.backbuffer_width == g_app.width
        && g_app.backbuffer_height == g_app.height) return;
    if (g_app.backbuffer_dc) {
        if (g_app.backbuffer_previous) SelectObject(g_app.backbuffer_dc, g_app.backbuffer_previous);
        if (g_app.backbuffer_bitmap) DeleteObject(g_app.backbuffer_bitmap);
        DeleteDC(g_app.backbuffer_dc);
    }
    g_app.backbuffer_dc = CreateCompatibleDC(reference_dc);
    g_app.backbuffer_bitmap = CreateCompatibleBitmap(reference_dc, g_app.width, g_app.height);
    g_app.backbuffer_previous = reinterpret_cast<HBITMAP>(SelectObject(g_app.backbuffer_dc, g_app.backbuffer_bitmap));
    g_app.backbuffer_width = g_app.width;
    g_app.backbuffer_height = g_app.height;
    (void)hwnd;
}

void render_frame(HWND hwnd) {
    RECT client{};
    GetClientRect(hwnd,&client);
    g_app.width = std::max(1L,client.right);
    g_app.height = std::max(1L,client.bottom);
    update_layout();
    update_camera();
    g_app.renderer.render(g_app.scene,g_app.camera,g_app.selected);
    const RECT viewport = view_rect();
    g_app.frame.assign(static_cast<std::size_t>(g_app.width)*static_cast<std::size_t>(g_app.height),0x00272729U);
    const auto& view = g_app.renderer.pixels();
    const int vw = viewport.right-viewport.left, vh = viewport.bottom-viewport.top;
    for (int y=0;y<vh;++y) {
        const auto src = view.begin()+static_cast<std::ptrdiff_t>(y)*vw;
        auto dst = g_app.frame.begin()+static_cast<std::ptrdiff_t>(y+viewport.top)*g_app.width+viewport.left;
        std::copy_n(src,vw,dst);
    }
    BITMAPINFO info{};
    info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=g_app.width;
    info.bmiHeader.biHeight=-g_app.height;
    info.bmiHeader.biPlanes=1;
    info.bmiHeader.biBitCount=32;
    info.bmiHeader.biCompression=BI_RGB;

    PAINTSTRUCT ps{};
    HDC window_dc=BeginPaint(hwnd,&ps);
    ensure_backbuffer(hwnd,window_dc);
    StretchDIBits(g_app.backbuffer_dc,0,0,g_app.width,g_app.height,0,0,g_app.width,g_app.height,
                  g_app.frame.data(),&info,DIB_RGB_COLORS,SRCCOPY);
    draw_ui(g_app.backbuffer_dc);
    BitBlt(window_dc,0,0,g_app.width,g_app.height,g_app.backbuffer_dc,0,0,SRCCOPY);
    EndPaint(hwnd,&ps);
}

void save_scene() {
    std::ofstream file("scene.txt");
    if (!file) { g_app.status=L"Nie udało się zapisać sceny"; return; }
    file << "# Starter 3D Engine scene v2\n";
    for (const auto& e:g_app.scene.entities()) {
        file<<e.id<<' '<<e.name<<' '<<e.transform.position.x<<' '<<e.transform.position.y<<' '
            <<e.transform.position.z<<' '<<e.transform.rotation.x<<' '<<e.transform.rotation.y<<' '
            <<e.transform.rotation.z<<' '<<e.transform.scale.x<<' '<<e.transform.scale.y<<' '
            <<e.transform.scale.z<<'\n';
    }
    g_app.status=L"Zapisano scenę do scene.txt";
}

bool ray_plane(engine::Ray3 ray,engine::Vec3 point,engine::Vec3 normal,engine::Vec3& hit) {
    const float denom=dot(ray.direction,normal);
    if(std::abs(denom)<0.00001F) return false;
    const float t=dot(sub(point,ray.origin),normal)/denom;
    if(t<0) return false;
    hit=add(ray.origin,mul(ray.direction,t));
    return true;
}

bool ray_box(engine::Ray3 ray,const engine::Entity& entity,float& distance) {
    float low=0.0F, high=1.0e30F;
    const float origin[3]{ray.origin.x,ray.origin.y,ray.origin.z};
    const float direction[3]{ray.direction.x,ray.direction.y,ray.direction.z};
    const float center[3]{entity.transform.position.x,entity.transform.position.y,entity.transform.position.z};
    const float half[3]{std::abs(entity.transform.scale.x)*0.5F,std::abs(entity.transform.scale.y)*0.5F,std::abs(entity.transform.scale.z)*0.5F};
    for(int axis=0;axis<3;++axis) {
        if(std::abs(direction[axis])<1.0e-7F) {
            if(origin[axis]<center[axis]-half[axis]||origin[axis]>center[axis]+half[axis]) return false;
            continue;
        }
        float a=(center[axis]-half[axis]-origin[axis])/direction[axis];
        float b=(center[axis]+half[axis]-origin[axis])/direction[axis];
        if(a>b) std::swap(a,b);
        low=std::max(low,a); high=std::min(high,b);
        if(low>high) return false;
    }
    distance=low;
    return true;
}

bool hit_gizmo(int sx,int sy,int view_w,int view_h,engine::Entity& entity) {
    const float d=magnitude(sub(g_app.camera.position,entity.transform.position));
    const float handle_len=std::clamp(d*0.13F,0.8F,2.0F);
    const int radius=std::clamp(view_h/75,8,14)+5;
    const engine::Vec3 axes[3]{{1,0,0},{0,1,0},{0,0,1}};
    const auto center=engine::project_to_screen(entity.transform.position,g_app.camera,view_w,view_h);
    if(!center.visible) return false;
    float best=static_cast<float>(radius*radius);
    int chosen=-1; float chosen_sign=1;
    for(int axis=0;axis<3;++axis) for(float sign:{-1.0F,1.0F}) {
        const auto p=engine::project_to_screen(add(entity.transform.position,mul(axes[axis],handle_len*sign)),g_app.camera,view_w,view_h);
        if(!p.visible) continue;
        const float dx=p.x-static_cast<float>(sx),dy=p.y-static_cast<float>(sy),dist=dx*dx+dy*dy;
        if(dist<best) {best=dist;chosen=axis;chosen_sign=sign;}
    }
    if(chosen<0) return false;
    g_app.drag_axis=chosen; g_app.drag_sign=chosen_sign;
    const engine::Vec3 axis=mul(axes[chosen],chosen_sign);
    auto view=normalized(sub(g_app.camera.target,g_app.camera.position));
    auto normal=sub(view,mul(axis,dot(view,axis)));
    if(magnitude(normal)<0.0001F) normal=cross(axis,std::abs(axis.y)<0.9F?engine::Vec3{0,1,0}:engine::Vec3{1,0,0});
    g_app.drag_plane_normal=normalized(normal);
    const auto ray=engine::screen_ray(static_cast<float>(sx),static_cast<float>(sy),g_app.camera,view_w,view_h);
    if(!ray_plane(ray,entity.transform.position,g_app.drag_plane_normal,g_app.drag_start_hit)) {g_app.drag_axis=-1;return false;}
    g_app.drag_start_position=entity.transform.position;
    return true;
}

void add_cube() {
    auto& cube=g_app.scene.create_entity("Cube "+std::to_string(g_app.scene.entity_count()));
    cube.transform.position={0.0F,0.5F,0.0F};
    g_app.selected=cube.id;
    g_app.status=L"Dodano kostkę";
}

LRESULT CALLBACK window_proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
    switch(message) {
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
            const int right_x = g_app.width - g_app.right_width;
            if ((std::abs(cursor.x - g_app.left_width) <= 5 || std::abs(cursor.x - right_x) <= 5)
                && cursor.y >= kHeaderHeight && cursor.y < g_app.height - kFooterHeight) {
                SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
                return TRUE;
            }
        }
        break;
    }
    case WM_SIZE:
        g_app.width=LOWORD(lp); g_app.height=HIWORD(lp);
        InvalidateRect(hwnd,nullptr,FALSE);
        return 0;
    case WM_LBUTTONDOWN: {
        const int x=GET_X_LPARAM(lp),y=GET_Y_LPARAM(lp);
        const int right_x = g_app.width - g_app.right_width;
        if (y >= kHeaderHeight && y < g_app.height - kFooterHeight
            && std::abs(x - g_app.left_width) <= 5) {
            g_app.resizing_left_panel = true;
            SetCapture(hwnd);
            return 0;
        }
        if (y >= kHeaderHeight && y < g_app.height - kFooterHeight
            && std::abs(x - right_x) <= 5) {
            g_app.resizing_right_panel = true;
            SetCapture(hwnd);
            return 0;
        }
        const int view_w=std::max(1,g_app.width-g_app.left_width-g_app.right_width);
        const int view_h=std::max(1,g_app.height-kHeaderHeight-kFooterHeight);
        if(x<g_app.left_width) {
            if(y>=54&&y<84&&x>=g_app.left_width-72&&x<g_app.left_width-43) add_cube();
            else if(y>=96&&y<134) add_cube();
            else if(y>=140&&y<178) save_scene();
            else if(y>=216) {
                const int row=(y-216)/34;
                if(row>=0&&static_cast<std::size_t>(row)<g_app.scene.entity_count())
                    g_app.selected=g_app.scene.entities()[static_cast<std::size_t>(row)].id;
            }
        } else if(x>=g_app.left_width&&x<g_app.width-g_app.right_width&&y>=kHeaderHeight&&y<g_app.height-kFooterHeight) {
            const int vx=x-g_app.left_width,vy=y-kHeaderHeight;
            bool dragging=false;
            if(auto* selected=g_app.scene.find_entity(g_app.selected)) dragging=hit_gizmo(vx,vy,view_w,view_h,*selected);
            if(!dragging) {
                const auto ray=engine::screen_ray(static_cast<float>(vx),static_cast<float>(vy),g_app.camera,view_w,view_h);
                float closest=1.0e30F; engine::EntityId hit=0;
                for(const auto& entity:g_app.scene.entities()) {float t{};if(ray_box(ray,entity,t)&&t<closest){closest=t;hit=entity.id;}}
                g_app.selected=hit;
            }
            SetCapture(hwnd);
        }
        InvalidateRect(hwnd,nullptr,FALSE);
        return 0;
    }
    case WM_LBUTTONUP:
        g_app.drag_axis=-1;
        g_app.resizing_left_panel=false;
        g_app.resizing_right_panel=false;
        if(GetCapture()==hwnd) ReleaseCapture();
        return 0;
    case WM_MOUSEMOVE: {
        const int x=GET_X_LPARAM(lp),y=GET_Y_LPARAM(lp);
        bool scene_changed=false;
        if(g_app.resizing_left_panel && (wp & MK_LBUTTON)) {
            const int maximum = std::max(170, g_app.width - g_app.right_width - 300);
            const int resized = std::clamp(x, 170, maximum);
            scene_changed = resized != g_app.left_width;
            g_app.left_width = resized;
        } else if(g_app.resizing_right_panel && (wp & MK_LBUTTON)) {
            const int maximum = std::max(240, g_app.width - g_app.left_width - 300);
            const int resized = std::clamp(g_app.width - x, 240, maximum);
            scene_changed = resized != g_app.right_width;
            g_app.right_width = resized;
        } else if(g_app.orbiting) {
            const float dx=static_cast<float>(x-g_app.last_mouse.x),dy=static_cast<float>(y-g_app.last_mouse.y);
            g_app.yaw-=dx*0.006F;
            // Upward mouse motion (negative dy) now looks upward.
            g_app.pitch=std::clamp(g_app.pitch+dy*0.006F,-1.4F,1.4F);
            g_app.last_mouse={x,y};
            update_camera();
            scene_changed=true;
        } else if(g_app.drag_axis>=0&&(wp&MK_LBUTTON)) {
            const int vw=std::max(1,g_app.width-g_app.left_width-g_app.right_width);
            const int vh=std::max(1,g_app.height-kHeaderHeight-kFooterHeight);
            const auto ray=engine::screen_ray(static_cast<float>(x-g_app.left_width),static_cast<float>(y-kHeaderHeight),g_app.camera,vw,vh);
            engine::Vec3 hit{};
            if(ray_plane(ray,g_app.drag_start_position,g_app.drag_plane_normal,hit)) {
                const engine::Vec3 axis=mul(g_app.drag_axis==0?engine::Vec3{1,0,0}:g_app.drag_axis==1?engine::Vec3{0,1,0}:engine::Vec3{0,0,1},g_app.drag_sign);
                const float amount=dot(sub(hit,g_app.drag_start_hit),axis);
                if(auto* entity=g_app.scene.find_entity(g_app.selected)) {
                    entity->transform.position=add(g_app.drag_start_position,mul(axis,amount));
                    scene_changed=true;
                }
            }
        }
        if(scene_changed) InvalidateRect(hwnd,nullptr,FALSE);
        return 0;
    }
    case WM_RBUTTONDOWN: {
        const int x=GET_X_LPARAM(lp),y=GET_Y_LPARAM(lp);
        if(x>=g_app.left_width&&x<g_app.width-g_app.right_width&&y>=kHeaderHeight&&y<g_app.height-kFooterHeight) {
            g_app.orbiting=true; SetCapture(hwnd);
            g_app.last_mouse={x,y};
        }
        return 0;
    }
    case WM_RBUTTONUP:
        g_app.orbiting=false;
        if(GetCapture()==hwnd) ReleaseCapture();
        return 0;
    case WM_MOUSEWHEEL:
        g_app.camera_distance=std::clamp(g_app.camera_distance-static_cast<float>(GET_WHEEL_DELTA_WPARAM(wp))/120.0F*0.7F,2.0F,40.0F);
        update_camera(); InvalidateRect(hwnd,nullptr,FALSE);
        return 0;
    case WM_KEYDOWN:
        if(wp=='N') add_cube();
        else if(wp==VK_DELETE&&g_app.selected) {
            (void)g_app.scene.remove_entity(g_app.selected);
            g_app.selected=g_app.scene.entity_count()?g_app.scene.entities().front().id:0;
            g_app.status=L"Usunięto obiekt";
        }
        InvalidateRect(hwnd,nullptr,FALSE);
        return 0;
    case WM_CAPTURECHANGED:
        g_app.orbiting = false;
        g_app.drag_axis = -1;
        g_app.resizing_left_panel = false;
        g_app.resizing_right_panel = false;
        return 0;
    case WM_DESTROY:
        if(g_app.backbuffer_dc) {
            if(g_app.backbuffer_previous) SelectObject(g_app.backbuffer_dc,g_app.backbuffer_previous);
            if(g_app.backbuffer_bitmap) DeleteObject(g_app.backbuffer_bitmap);
            DeleteDC(g_app.backbuffer_dc);
            g_app.backbuffer_dc=nullptr;
        }
        PostQuitMessage(0);
        return 0;
    default: return DefWindowProcW(hwnd,message,wp,lp);
    }
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE, PWSTR,int show) {
    SetProcessDPIAware();
    g_app.scene.create_entity("Cube 0").transform.position={0.0F,0.5F,0.0F};
    g_app.selected=1;
    update_camera();
    WNDCLASSEXW wc{};
    wc.cbSize=sizeof(wc); wc.style=CS_HREDRAW|CS_VREDRAW; wc.lpfnWndProc=window_proc;
    wc.hInstance=instance; wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    wc.hIcon=LoadIconW(nullptr,IDI_APPLICATION); wc.lpszClassName=L"Starter3DEngineWindow";
    if(!RegisterClassExW(&wc)) return 1;
    HWND hwnd=CreateWindowExW(0,wc.lpszClassName,L"Starter 3D Engine",
        WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1280,800,
        nullptr,nullptr,instance,nullptr);
    if(!hwnd) return 1;
    update_layout();
    ShowWindow(hwnd,show); UpdateWindow(hwnd);
    MSG msg{};
    while(GetMessageW(&msg,nullptr,0,0)>0) {TranslateMessage(&msg);DispatchMessageW(&msg);}
    if(g_app.font) DeleteObject(g_app.font);
    if(g_app.bold_font) DeleteObject(g_app.bold_font);
    return static_cast<int>(msg.wParam);
}
