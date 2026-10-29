#include "engine/platform.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
// Exercise all callback arguments through the DLL + ASM bridge without a desktop.
class CallbackWindow final : public engine::Window {
public:
    engine::ResizeCallback callback{};
    void* user{};
    engine::NativeWindowHandle native_handle() const noexcept override { return {}; }
    engine::WindowSize size() const noexcept override { return {}; }
    bool should_close() const noexcept override { return false; }
    bool minimized() const noexcept override { return false; }
    void show() override {}
    void poll_events() override {}
    void request_close() override {}
    void set_resize_callback(engine::ResizeCallback value, void* data) noexcept override { callback=value; user=data; }
};
}

int main(int argc, char**) {
    assert(createheap(0)==nullptr);
    freeheap(nullptr);
    auto* memory=static_cast<unsigned char*>(createheap(8200));
    assert(memory);
    for (int i=0;i<8200;++i) assert(memory[i]==0);
    std::memset(memory,0xA5,8200);
    freeheap(memory);
    WindowResizedCallback(nullptr,nullptr,nullptr); // Safe, no global byte-pointer dereference.
    CallbackWindow one, two;
    int calls=0;
    WindowResizedCallback(&one, [](engine::WindowSize size, bool minimized, void* user) {
        assert(size.width==640 && size.height==480 && minimized);
        ++*static_cast<int*>(user);
    }, &calls);
    assert(one.callback && one.user==&calls && !two.callback);
    one.callback({640,480},true,one.user);
    assert(calls==1);
    WindowResizedCallback(&one,nullptr,nullptr);
    assert(!one.callback && !one.user);
    engine::WindowConfig config;
    config.width=0;
    config.visible=false;
    bool rejected=false;
    try { auto window=engine::createnewwindow(config); }
    catch (const std::exception&) { rejected=true; }
    assert(rejected);
    // Opt-in integration test: requires a desktop or Xvfb, not a Vulkan GPU.
    if (argc>1) {
        config.width=320; config.height=240;
        auto first=engine::createnewwindow(config);
        auto second=engine::createnewwindow(config);
        assert(first->native_handle().window && second->native_handle().window);
        assert(first->native_handle().window!=second->native_handle().window);
        assert(first->size().width==320);
        first->show(); first->poll_events();
        first->request_close(); first->poll_events();
        assert(first->should_close());
        assert(!second->should_close());
    }
    std::cout << "Platform: " << engine::platform_name << '\n';
}
