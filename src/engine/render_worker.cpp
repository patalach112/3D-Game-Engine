#include "engine/render/render_worker.hpp"
#include "worker_affinity.hpp"
#include <atomic>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
namespace engine {
struct RenderWorker::Impl {
    std::mutex mutex;
    std::condition_variable condition;
    std::function<void()> job,completion;
    std::exception_ptr error;
    std::atomic<bool> busy{};
    bool stopping{};
    std::thread worker;
    explicit Impl(std::function<void()> callback):completion(std::move(callback)),worker([this]{run();}) {}
    ~Impl() {
        { std::lock_guard lock(mutex); stopping=true; }
        condition.notify_all();
        worker.join();
    }
    void run() {
        detail::worker_affinity(false);
        for (;;) {
            std::function<void()> frame;
            {
                std::unique_lock lock(mutex);
                condition.wait(lock,[&]{return stopping || bool(job);});
                if (!job && stopping) return;
                frame=std::exchange(job,{});
            }
            std::exception_ptr failure;
            try { frame(); } catch (...) { failure=std::current_exception(); }
            frame={}; // release job captures before publishing ownership back to frontend
            {
                std::lock_guard lock(mutex);
                error=failure;
                busy.store(false,std::memory_order_release);
            }
            condition.notify_all();
            if (completion) { try { completion(); } catch (...) {} }
        }
    }
};
RenderWorker::RenderWorker(std::function<void()> callback):impl_(std::make_unique<Impl>(std::move(callback))) {}
RenderWorker::~RenderWorker()=default;
void RenderWorker::submit(std::function<void()> frame) {
    if (!frame) throw std::invalid_argument("Empty render job");
    {
        std::lock_guard lock(impl_->mutex);
        if (impl_->busy.load()) throw std::logic_error("Render frame already in flight");
        if (impl_->error) std::rethrow_exception(impl_->error);
        impl_->job=std::move(frame);
        impl_->busy.store(true,std::memory_order_release);
    }
    impl_->condition.notify_one();
}
bool RenderWorker::busy() const noexcept { return impl_->busy.load(std::memory_order_acquire); }
void RenderWorker::wait() {
    std::unique_lock lock(impl_->mutex);
    impl_->condition.wait(lock,[&]{return !impl_->busy.load(std::memory_order_acquire);});
    if (auto error=std::exchange(impl_->error,{})) std::rethrow_exception(error);
}
}
