#include "engine/platform.hpp"
#include <chrono>
#include <exception>
#include <iostream>
#include <thread>

int main() {
    try {
        engine::WindowConfig config;
        config.title=std::string("Native window: ")+engine::platform_name;
        auto window=engine::createnewwindow(config);
        WindowResizedCallback(window.get(), [](engine::WindowSize size, bool minimized, void*) {
            std::cout << size.width << 'x' << size.height << " minimized=" << minimized << '\n';
        },nullptr);
        while (!window->should_close()) {
            window->poll_events();
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
