#include "engine/input.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>

using namespace engine;
int main() {
    auto queue=std::make_unique<InputQueue>();
    InputEvent event,out;
    assert(!engine_input_pop(queue.get(),&out));
    for (std::size_t i=0;i<input_queue_capacity;++i) {
        event.sequence=i; event.keyboard[255]=static_cast<std::uint8_t>(i);
        assert(engine_input_push(queue.get(),&event));
    }
    assert(!engine_input_push(queue.get(),&event));
    for (std::size_t i=0;i<input_queue_capacity;++i) {
        assert(engine_input_pop(queue.get(),&out));
        assert(out.sequence==i && out.keyboard[255]==static_cast<std::uint8_t>(i));
    }
    // Both array wrapping and unsigned monotonic-counter overflow.
    queue->write=queue->read=std::numeric_limits<std::uint64_t>::max()-2;
    for (unsigned i=0;i<20;++i) {
        event.sequence=i; assert(engine_input_push(queue.get(),&event));
        assert(engine_input_pop(queue.get(),&out) && out.sequence==i);
    }
    constexpr std::uint64_t count=200000;
    std::thread producer([&] {
        InputEvent packet;
        for (std::uint64_t i=0;i<count;++i) {
            packet.sequence=i; packet.wparam=i^0xabcdef; packet.extra_info=i^0x123456;
            packet.keyboard.fill(static_cast<std::uint8_t>(i));
            while (!engine_input_push(queue.get(),&packet)) std::this_thread::yield();
        }
    });
    for (std::uint64_t i=0;i<count;++i) {
        while (!engine_input_pop(queue.get(),&out)) std::this_thread::yield();
        assert(out.sequence==i && out.wparam==(i^0xabcdef) && out.extra_info==(i^0x123456));
        for (auto byte:out.keyboard) assert(byte==static_cast<std::uint8_t>(i));
    }
    producer.join();
    std::mutex mutex;
    std::condition_variable condition;
    unsigned notifications=0;
    InputProcessor input([&] { std::lock_guard lock(mutex); ++notifications; condition.notify_one(); });
    auto send=[&](InputKind kind,int x,int y,std::uint32_t message) {
        InputEvent packet; packet.kind=kind; packet.message=message;
        packet.lparam=static_cast<std::uint16_t>(x)|(static_cast<std::uint64_t>(static_cast<std::uint16_t>(y))<<16);
        assert(input.enqueue(packet));
    };
    send(InputKind::PointerMove,-10,20,1);
    send(InputKind::PointerMove,-15,-25,2);
    send(InputKind::PointerButton,-15,-25,3);
    send(InputKind::PointerMove,50,60,4);
    send(InputKind::Key,0,0,5);
    std::vector<InputEvent> events;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while (events.empty() || events.back().message!=5) {
        auto batch=input.drain(); events.insert(events.end(),batch.begin(),batch.end());
        if (!events.empty() && events.back().message==5) break;
        std::unique_lock lock(mutex);
        assert(condition.wait_until(lock,deadline,[&]{return notifications!=0;}));
        notifications=0;
    }
    bool button=false,last_move=false,key=false;
    for (const auto& e:events) {
        assert(e.kind!=InputKind::Reset);
        if (e.message==2) assert(e.x==-15 && e.y==-25);
        if (e.message==3) { button=true; assert(e.x==-15 && e.y==-25); }
        if (e.message==4) { assert(button); last_move=true; assert(e.x==50 && e.y==60); }
        if (e.message==5) { assert(last_move); key=true; }
    }
    assert(button && key && input.dropped_events()==0);
    // Stall the frontend and overflow the bounded output buffer. Overflow must
    // publish an explicit reset and never replay stale pre-overflow key-downs.
    std::uint64_t sent=0;
    while (input.dropped_events()==0 && sent<200000) {
        InputEvent packet; packet.kind=InputKind::Key; packet.message=100;
        input.enqueue(packet); ++sent;
    }
    assert(input.dropped_events()>0);
    auto reset=input.drain();
    assert(!reset.empty() && reset.front().kind==InputKind::Reset);
    for (std::size_t i=1;i<reset.size();++i) assert(reset[i].sequence>reset.front().sequence);
    std::cout << "Input ring FIFO/full/wrap/concurrent integrity, normalization and overflow reset passed\n";
}
