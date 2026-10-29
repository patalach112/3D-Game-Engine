#include "engine/render/render_worker.hpp"
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <memory>
#include <stdexcept>
#include <thread>
int main() {
    const auto frontend=std::this_thread::get_id();
    std::atomic<int> completed{};
    std::mutex mutex;
    std::condition_variable condition;
    bool release=false;
    {
        engine::RenderWorker worker;
        worker.submit([&] {
            assert(std::this_thread::get_id()!=frontend);
            std::unique_lock lock(mutex);
            condition.wait(lock,[&]{return release;});
            ++completed;
        });
        assert(worker.busy());
        bool rejected=false;
        try { worker.submit([]{}); } catch (const std::logic_error&) { rejected=true; }
        assert(rejected);
        { std::lock_guard lock(mutex); release=true; }
        condition.notify_one();
        worker.wait();
        assert(!worker.busy() && completed==1);
        worker.submit([] { throw std::runtime_error("GPU failure"); });
        bool propagated=false;
        try { worker.wait(); } catch (const std::runtime_error&) { propagated=true; }
        assert(propagated);
        worker.submit([&]{++completed;}); worker.wait();
        assert(completed==2);
        std::atomic<bool> destroyed{};
        struct Token {
            std::atomic<bool>& destroyed;
            explicit Token(std::atomic<bool>& flag):destroyed(flag) {}
            ~Token() { destroyed.store(true); }
        };
        auto token=std::make_shared<Token>(destroyed);
        worker.submit([token]{});
        token.reset();
        worker.wait();
        assert(destroyed);
        // Destruction joins an outstanding job rather than detaching it.
        worker.submit([&]{++completed;});
    }
    assert(completed==3);
    std::cout << "Render worker ownership, backpressure, exceptions and join passed\n";
}
