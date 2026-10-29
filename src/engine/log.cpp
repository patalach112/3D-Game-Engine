#include "engine/log.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
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
        g_file = std::fopen(filename, "a");
    }
    if (!g_file) {
        g_file = std::fopen("engine.log", "a");
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
    if (!should_log(level)) return;

    std::lock_guard<std::mutex> lock(g_log_mutex);

    const std::string ts = timestamp_string();
    const char* lvl = level_name(level);

    if (g_file) {
        std::fprintf(g_file, "[%s] [%s] [%s] %s\n", ts.c_str(), lvl, tag, message.c_str());
        std::fflush(g_file);
    }

#ifndef NDEBUG
    // Debug build: też wypisz do stderr.
    if (level >= LogLevel::Warn || !g_file) {
        std::fprintf(stderr, "[%s] [%s] [%s] %s\n", ts.c_str(), lvl, tag, message.c_str());
    }
#endif
}

} // namespace engine
