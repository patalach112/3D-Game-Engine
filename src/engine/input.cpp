#include "engine/input.hpp"
#include "worker_affinity.hpp"
#include <atomic>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <thread>
#include <utility>

namespace engine {
#ifndef ENGINE_HAS_ASM
extern "C" bool engine_input_push(InputQueue* q,const InputEvent* event) noexcept {
    const auto w=q->write.load(std::memory_order_relaxed);
    if (w-q->read.load(std::memory_order_acquire)>=input_queue_capacity) return false;
    q->events[w&(input_queue_capacity-1)]=*event;
    q->write.store(w+1,std::memory_order_release);
    return true;
}
extern "C" bool engine_input_pop(InputQueue* q,InputEvent* event) noexcept {
    const auto r=q->read.load(std::memory_order_relaxed);
    if (r==q->write.load(std::memory_order_acquire)) return false;
    *event=q->events[r&(input_queue_capacity-1)];
    q->read.store(r+1,std::memory_order_release);
    return true;
}
#endif
struct InputProcessor::Impl {
    InputQueue queue;
    std::mutex wake_mutex,output_mutex;
    std::condition_variable condition;
    bool stopping{};
    std::uint64_t posted{},sequence{},last_reset{};
    std::atomic<std::uint64_t> cutoff{},dropped{};
    std::vector<InputEvent> output;
    std::exception_ptr error;
    std::function<void()> wake;
    std::thread worker;
    explicit Impl(std::function<void()> callback):wake(std::move(callback)),worker([this]{run();}) {}
    ~Impl() {
        { std::lock_guard lock(wake_mutex); stopping=true; }
        condition.notify_one();
        worker.join();
    }
    void drop_through(std::uint64_t seq) {
        auto old=cutoff.load();
        while (old<seq && !cutoff.compare_exchange_weak(old,seq)) {}
        dropped.fetch_add(1);
    }
    void run() noexcept {
        try { process(); }
        catch (...) {
            { std::lock_guard lock(output_mutex); error=std::current_exception(); }
            if (wake) { try { wake(); } catch (...) {} }
        }
    }
    void process() {
        detail::worker_affinity(true);
        std::uint64_t seen{};
        for (;;) {
            {
                std::unique_lock lock(wake_mutex);
                condition.wait(lock,[&]{return stopping || posted!=seen;});
                if (stopping) return;
                seen=posted;
            }
            InputEvent event;
            while (engine_input_pop(&queue,&event)) {
                if (event.kind==InputKind::PointerMove || event.kind==InputKind::PointerButton) {
                    const auto bits=static_cast<std::uint64_t>(event.lparam);
                    event.x=static_cast<float>(static_cast<std::int16_t>(bits&0xffff));
                    event.y=static_cast<float>(static_cast<std::int16_t>((bits>>16)&0xffff));
                }
                std::lock_guard lock(output_mutex);
                if (event.sequence<=cutoff.load()) continue;
                // Only adjacent moves may coalesce: never cross key/button/wheel/focus edges.
                if (!output.empty() && event.kind==InputKind::PointerMove &&
                    output.back().kind==InputKind::PointerMove && output.back().window==event.window)
                    output.back()=event;
                else if (output.size()<4096) output.push_back(event);
                else { output.clear(); drop_through(event.sequence); }
            }
            if (wake) { try { wake(); } catch (...) {} }
        }
    }
};
InputProcessor::InputProcessor(std::function<void()> wake):impl_(std::make_unique<Impl>(std::move(wake))) {}
InputProcessor::~InputProcessor()=default;
bool InputProcessor::enqueue(InputEvent event) {
    event.sequence=++impl_->sequence;
    const bool accepted=engine_input_push(&impl_->queue,&event);
    if (!accepted) impl_->drop_through(event.sequence);
    { std::lock_guard lock(impl_->wake_mutex); ++impl_->posted; }
    impl_->condition.notify_one();
    return accepted;
}
std::vector<InputEvent> InputProcessor::drain() {
    std::vector<InputEvent> pending,result;
    {
        std::lock_guard lock(impl_->output_mutex);
        if (impl_->error) std::rethrow_exception(impl_->error);
        pending.swap(impl_->output);
    }
    const auto cutoff=impl_->cutoff.load();
    result.reserve(pending.size()+1);
    if (cutoff!=impl_->last_reset) {
        InputEvent reset; reset.kind=InputKind::Reset; reset.sequence=cutoff;
        result.push_back(reset);
        impl_->last_reset=cutoff;
    }
    for (const auto& event:pending) if (event.sequence>cutoff) result.push_back(event);
    return result;
}
std::uint64_t InputProcessor::dropped_events() const noexcept { return impl_->dropped.load(); }
}
