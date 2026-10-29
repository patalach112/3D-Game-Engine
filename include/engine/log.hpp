#pragma once

#include "engine/export.hpp"

#include <cstdarg>
#include <cstdio>
#include <string>
#include <cstdint>
#include <vector>

namespace engine {

// Thread-safe file/stderr sink and bounded editor history, including Release.

enum class LogLevel : int {
    Debug = 0,
    Info  = 1,
    Warn  = 2,
    Error = 3,
};

// Minimum severity (default Info).
ENGINE_API void set_log_level(LogLevel level);
ENGINE_API LogLevel log_level();

// Initialize the file sink (Windows) or stderr sink (Linux).
ENGINE_API void log_init(const char* filename);
ENGINE_API void log_shutdown();

struct LogEntry {
    std::uint64_t sequence{};
    LogLevel level{};
    std::string time, tag, message;
};
inline constexpr std::size_t kLogHistoryCapacity = 2048;
// Copies under the logger mutex; caller never retains pointers into live history.
// Sequence numbers are monotonic across clear/shutdown/reinitialization.
ENGINE_API std::vector<LogEntry> log_snapshot(std::uint64_t after_sequence = 0);
ENGINE_API std::uint64_t log_latest_sequence();

// Adds a row to both history and the configured sink.
ENGINE_API void log_write(LogLevel level, const char* tag, const std::string& message);

// Helper do formatowania stringa (printf-style).
inline std::string log_fmt(const char* fmt, ...) {
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    return buffer;
}

} // namespace engine

// Only debug traces disappear in Release. Warnings/errors remain observable.
#ifdef NDEBUG
  #define LOG_DEBUG(tag, ...) ((void)0)
#else
  #define LOG_DEBUG(tag, ...) engine::log_write(engine::LogLevel::Debug, tag, engine::log_fmt(__VA_ARGS__))
#endif
#define LOG_INFO(tag, ...)  engine::log_write(engine::LogLevel::Info, tag, engine::log_fmt(__VA_ARGS__))
#define LOG_WARN(tag, ...)  engine::log_write(engine::LogLevel::Warn, tag, engine::log_fmt(__VA_ARGS__))
#define LOG_ERROR(tag, ...) engine::log_write(engine::LogLevel::Error, tag, engine::log_fmt(__VA_ARGS__))
