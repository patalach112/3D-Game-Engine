#include "engine/platform.hpp"
#include <stdexcept>
#ifdef ENGINE_HAS_X11
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif

namespace engine::linux_platform {
#ifdef ENGINE_HAS_X11
namespace {
class LinuxWindow final : public engine::Window {
    Display* display_{};
    ::Window window_{};
    Atom delete_message_{};
    WindowSize extent_{};
    bool closed_{}, minimized_{};
    ResizeCallback resize_{};
    void* user_{};
public:
    explicit LinuxWindow(const WindowConfig& config) : extent_{config.width,config.height} {
        if (config.width<=0 || config.height<=0 || config.width>32767 || config.height>32767)
            throw std::invalid_argument("Invalid window dimensions");
        // Native Win32 hooks are deliberately not reinterpreted as X11 callbacks.
        if (config.native_callback || config.child || config.parent)
            throw std::invalid_argument("Linux window backend does not support Win32 hooks/child handles");
        display_=XOpenDisplay(nullptr);
        if (!display_) throw std::runtime_error("Cannot open X11 display (DISPLAY/XWayland required)");
        const int screen=DefaultScreen(display_);
        window_=XCreateSimpleWindow(display_,RootWindow(display_,screen),0,0,
            static_cast<unsigned>(config.width),static_cast<unsigned>(config.height),0,
            BlackPixel(display_,screen),BlackPixel(display_,screen));
        if (!window_) { XCloseDisplay(display_); display_=nullptr; throw std::runtime_error("XCreateSimpleWindow failed"); }
        XSelectInput(display_,window_,StructureNotifyMask|ExposureMask);
        XStoreName(display_,window_,config.title.c_str());
        const Atom utf8=XInternAtom(display_,"UTF8_STRING",False);
        const Atom name=XInternAtom(display_,"_NET_WM_NAME",False);
        XChangeProperty(display_,window_,name,utf8,8,PropModeReplace,
            reinterpret_cast<const unsigned char*>(config.title.c_str()),static_cast<int>(config.title.size()));
        if (config.borderless) {
            struct { unsigned long flags,functions,decorations; long input_mode; unsigned long status; } hints{2,0,0,0,0};
            const Atom motif=XInternAtom(display_,"_MOTIF_WM_HINTS",False);
            XChangeProperty(display_,window_,motif,motif,32,PropModeReplace,
                reinterpret_cast<const unsigned char*>(&hints),5);
        }
        delete_message_=XInternAtom(display_,"WM_DELETE_WINDOW",False);
        XSetWMProtocols(display_,window_,&delete_message_,1);
        if (config.visible) show();
    }
    ~LinuxWindow() override {
        if (display_) { if (window_) XDestroyWindow(display_,window_); XCloseDisplay(display_); }
    }
    NativeWindowHandle native_handle() const noexcept override { return {display_,window_,nullptr}; }
    WindowSize size() const noexcept override { return extent_; }
    bool should_close() const noexcept override { return closed_; }
    bool minimized() const noexcept override { return minimized_; }
    void show() override { if (window_) { XMapWindow(display_,window_); XFlush(display_); } }
    void request_close() override { closed_=true; } // Owner destroys native resources on destruction.
    void set_resize_callback(ResizeCallback callback, void* user) noexcept override { resize_=callback; user_=user; }
    void poll_events() override {
        while (XPending(display_)) {
            XEvent event{}; XNextEvent(display_,&event);
            if (event.xany.window!=window_) continue;
            bool notify=false;
            if (event.type==ClientMessage && static_cast<Atom>(event.xclient.data.l[0])==delete_message_) closed_=true;
            if (event.type==DestroyNotify) { window_=0; closed_=true; }
            if (event.type==ConfigureNotify) {
                const WindowSize next{event.xconfigure.width,event.xconfigure.height};
                notify=next.width!=extent_.width || next.height!=extent_.height;
                extent_=next;
            }
            if (event.type==UnmapNotify || event.type==MapNotify) {
                minimized_=event.type==UnmapNotify; notify=true;
            }
            if (notify && resize_) resize_(extent_,minimized_,user_);
        }
    }
};
}
bool windowing_available() noexcept { return true; }
std::unique_ptr<Window> createnewwindow(const WindowConfig& config) { return std::make_unique<LinuxWindow>(config); }
#else
bool windowing_available() noexcept { return false; }
std::unique_ptr<Window> createnewwindow(const WindowConfig&) {
    throw std::runtime_error("Linux window support disabled: rebuild with ENGINE_LINUX_X11=ON and libx11-dev");
}
#endif
} // namespace engine::linux_platform
