#include "engine/platform.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <algorithm>
#include <stdexcept>
#include <string>

extern "C" LRESULT CALLBACK engine_window_proc(HWND, UINT, WPARAM, LPARAM) noexcept;
extern "C" LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
#ifndef ENGINE_HAS_ASM
extern "C" LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    return engine_window_proc(hwnd, message, wp, lp);
}
#endif
namespace {
std::wstring utf16(const std::string& text) {
    if (text.empty()) return {};
    const int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.c_str(),-1,nullptr,0);
    if (!count) throw std::invalid_argument("Window title must be valid UTF-8");
    std::wstring out(static_cast<std::size_t>(count),L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.c_str(),-1,out.data(),count);
    out.pop_back();
    return out;
}
class WindowsWindow final : public engine::Window {
public:
    HWND handle{};
    HINSTANCE instance{GetModuleHandleW(nullptr)};
    engine::WindowConfig config;
    engine::WindowSize extent;
    bool closed{}, iconic{};
    engine::ResizeCallback resize_callback{};
    void* resize_user{};

    explicit WindowsWindow(const engine::WindowConfig& value) : config(value), extent{value.width,value.height} {
        if (value.width<=0 || value.height<=0 || value.width>32767 || value.height>32767)
            throw std::invalid_argument("Invalid window dimensions");
        if (value.child && !value.parent) throw std::invalid_argument("Child window needs a parent");
        static const ATOM registered=[] {
            WNDCLASSEXW wc{};
            wc.cbSize=sizeof(wc); wc.lpfnWndProc=WindowProc;
            wc.hInstance=GetModuleHandleW(nullptr); wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));
            wc.lpszClassName=L"StarterEnginePlatformWindow";
            const ATOM result=RegisterClassExW(&wc);
            if (!result) throw std::runtime_error("RegisterClassExW failed");
            return result;
        }();
        (void)registered;
        const DWORD style=value.child?(WS_CHILD|WS_CLIPSIBLINGS):
            value.borderless?(WS_POPUP|WS_THICKFRAME|WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_CLIPCHILDREN):
            (WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN);
        RECT rect{0,0,value.width,value.height};
        if (!value.child && !value.borderless) AdjustWindowRectEx(&rect,style,FALSE,0);
        const auto title=utf16(value.title);
        handle=CreateWindowExW(0,L"StarterEnginePlatformWindow",title.c_str(),style,
            value.child?0:CW_USEDEFAULT,value.child?0:CW_USEDEFAULT,
            rect.right-rect.left,rect.bottom-rect.top,reinterpret_cast<HWND>(value.parent),nullptr,instance,this);
        if (!handle) throw std::runtime_error("CreateWindowExW failed");
        if (value.visible) show();
    }
    ~WindowsWindow() override { if (handle) DestroyWindow(handle); }
    engine::NativeWindowHandle native_handle() const noexcept override {
        return {nullptr,reinterpret_cast<std::uintptr_t>(handle),instance};
    }
    engine::WindowSize size() const noexcept override { return extent; }
    bool should_close() const noexcept override { return closed; }
    bool minimized() const noexcept override { return iconic; }
    void show() override { if (handle) ShowWindow(handle,SW_SHOW); }
    void poll_events() override {
        MSG message{};
        while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
            if (message.message==WM_QUIT) { closed=true; continue; }
            TranslateMessage(&message); DispatchMessageW(&message);
        }
    }
    void request_close() override { if (handle) PostMessageW(handle,WM_CLOSE,0,0); }
    void set_resize_callback(engine::ResizeCallback callback, void* user) noexcept override {
        resize_callback=callback; resize_user=user;
    }
};
}
extern "C" LRESULT CALLBACK engine_window_proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) noexcept {
    auto* self=reinterpret_cast<WindowsWindow*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if (message==WM_NCCREATE) {
        self=static_cast<WindowsWindow*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        if (!self) return FALSE;
        self->handle=hwnd;
        SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
    }
    if (!self) return DefWindowProcW(hwnd,message,wp,lp);
    try {
        if (message==WM_SIZE) {
            self->iconic=wp==SIZE_MINIMIZED;
            self->extent={static_cast<int>(LOWORD(lp)),static_cast<int>(HIWORD(lp))};
            if (self->resize_callback) self->resize_callback(self->extent,self->iconic,self->resize_user);
        }
        if (message==WM_CLOSE || message==WM_DESTROY) self->closed=true;
        if (message==WM_NCDESTROY) {
            self->handle=nullptr; self->closed=true;
            SetWindowLongPtrW(hwnd,GWLP_USERDATA,0);
        }
        std::intptr_t result{};
        if (self->config.native_callback && self->config.native_callback(
                {hwnd,message,wp,lp},result,self->config.user)) return static_cast<LRESULT>(result);
        if (message==WM_CLOSE) { DestroyWindow(hwnd); return 0; }
        if (self->config.borderless && message==WM_NCCALCSIZE && wp) return 0;
        if (self->config.borderless && message==WM_NCHITTEST) {
            RECT rect{};
            if (GetWindowRect(hwnd,&rect)) {
                const int x=GET_X_LPARAM(lp), y=GET_Y_LPARAM(lp); // signed: negative monitors work.
                const int border=IsZoomed(hwnd)?0:8;
                const bool left=x<rect.left+border, right=x>=rect.right-border;
                const bool top=y<rect.top+border, bottom=y>=rect.bottom-border;
                if (top) return left?HTTOPLEFT:right?HTTOPRIGHT:HTTOP;
                if (bottom) return left?HTBOTTOMLEFT:right?HTBOTTOMRIGHT:HTBOTTOM;
                if (left) return HTLEFT;
                if (right) return HTRIGHT;
                return y<rect.top+32?HTCAPTION:HTCLIENT;
            }
        }
    } catch (...) {
        // Never unwind C++ exceptions through the OS or the assembly bridge.
        OutputDebugStringA("Engine window callback threw an exception\n");
    }
    return DefWindowProcW(hwnd,message,wp,lp);
}
namespace engine::windows {
std::unique_ptr<Window> createnewwindow(const WindowConfig& config) {
    return std::make_unique<WindowsWindow>(config);
}
}
