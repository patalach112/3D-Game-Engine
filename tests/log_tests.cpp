#include "engine/log.hpp"
#include <cassert>
#include <thread>
#include <vector>
void log_release_probe();
int main() {
    engine::set_log_level(engine::LogLevel::Debug);
    const auto start=engine::log_latest_sequence();
    log_release_probe();
    auto entries=engine::log_snapshot(start);
    assert(entries.size()==3);
    assert(entries[0].level==engine::LogLevel::Info);
    assert(entries[2].level==engine::LogLevel::Error);
    engine::set_log_level(engine::LogLevel::Warn);
    LOG_INFO("test","filtered");
    assert(engine::log_latest_sequence()==start+3);
    engine::log_write(engine::LogLevel::Error,nullptr,std::string(20000,'x'));
    entries=engine::log_snapshot(start+3);
    assert(entries.size()==1 && entries[0].message.size()==4096 && entries[0].tag=="engine");
    engine::set_log_level(engine::LogLevel::Info);
    std::vector<std::thread> writers;
    for (int t=0;t<4;++t) writers.emplace_back([t] {
        for (int i=0;i<600;++i) LOG_INFO("thread","%d:%d",t,i);
    });
    // Concurrent readers receive owned snapshots, not invalidatable references.
    for (int i=0;i<50;++i) {
        const auto copy=engine::log_snapshot();
        for (std::size_t j=1;j<copy.size();++j) assert(copy[j].sequence>copy[j-1].sequence);
    }
    for (auto& writer:writers) writer.join();
    entries=engine::log_snapshot();
    assert(entries.size()==engine::kLogHistoryCapacity);
    for (std::size_t i=1;i<entries.size();++i) assert(entries[i].sequence==entries[i-1].sequence+1);
    const auto cleared=engine::log_latest_sequence();
    assert(engine::log_snapshot(cleared).empty());
    engine::log_shutdown();
    LOG_INFO("test","still monotonic after shutdown");
    entries=engine::log_snapshot(cleared);
    assert(entries.size()==1 && entries.front().sequence==cleared+1 && entries.front().time.size()==8);
}
