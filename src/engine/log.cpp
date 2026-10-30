#include "engine/log.hpp"

#include <chrono>
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <deque>
#include <mutex>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <unistd.h>
#endif

namespace engine {
namespace {

std::mutex g_log_mutex;
LogLevel g_level = LogLevel::Info;
FILE* g_file = nullptr; // Windows: plik log. Linux: nullptr → stderr.
bool g_initialized = false;
std::deque<LogEntry> g_history;
std::uint64_t g_sequence{};

const char* level_name(LogLevel level) {
    switch (level) {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info:  return "INFO ";
    case LogLevel::Warn:  return "WARN ";
    case LogLevel::Error: return "ERROR";
    }
    return "?????";
}

bool should_log(LogLevel level) {
    return level >= g_level;
}

std::string timestamp_string() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%H:%M:%S", &tm);
    return buffer;
}

} // namespace

void set_log_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    g_level = level;
}

LogLevel log_level() {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    return g_level;
}

void log_init(const char* filename) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (g_initialized) return;
#ifdef _WIN32
    // Windows: otwiera plik log w tryb append.
    if (filename && filename[0]) {
        (void)fopen_s(&g_file, filename, "a");
    }
    if (!g_file) {
        (void)fopen_s(&g_file, "engine.log", "a");
    }
#else
    // Linux: logowanie do stderr.
    g_file = nullptr;
    (void)filename;
#endif
    g_initialized = true;
}

void log_shutdown() {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (g_file) {
        std::fclose(g_file);
        g_file = nullptr;
    }
    g_initialized = false;
}

void log_write(LogLevel level, const char* tag, const std::string& message) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (!should_log(level)) return;

    const std::string ts = timestamp_string();
    const char* lvl = level_name(level);
    // Bound both row count and individual allocations, including direct callers.
    std::size_t tag_length=0;
    if (tag) while (tag_length<96 && tag[tag_length]) ++tag_length;
    const std::string safe_tag = tag ? std::string(tag,tag_length) : "engine";
    const std::string safe_message = message.substr(0, 4096);
    if (g_history.size() == kLogHistoryCapacity) g_history.pop_front();
    g_history.push_back({++g_sequence, level, ts, safe_tag, safe_message});

    if (g_file) {
        std::fprintf(g_file, "[%s] [%s] [%s] %s\n", ts.c_str(), lvl, safe_tag.c_str(), safe_message.c_str());
        std::fflush(g_file);
    }

    // Linux always has a sink; Windows warnings also reach stderr.
    if (level >= LogLevel::Warn || !g_file) {
        std::fprintf(stderr, "[%s] [%s] [%s] %s\n", ts.c_str(), lvl, safe_tag.c_str(), safe_message.c_str());
    }
}

std::vector<LogEntry> log_snapshot(std::uint64_t after_sequence) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (g_history.empty() || after_sequence>=g_history.back().sequence) return {};
    const auto begin=std::upper_bound(g_history.begin(),g_history.end(),after_sequence,
        [](std::uint64_t sequence,const LogEntry& entry) { return sequence<entry.sequence; });
    return {begin,g_history.end()};
}
std::uint64_t log_latest_sequence() {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    return g_sequence;
}

} // namespace engine
