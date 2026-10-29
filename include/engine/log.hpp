#pragma once

#include "engine/export.hpp"

#include <cstdarg>
#include <cstdio>
#include <string>
#include <mutex>

namespace engine {

// Prosty logowanie do pliku i/lub konsoli.
// W build-u Release (NDEBUG) cały log jest kompilowany na pustą makrą — zero overhead.

enum class LogLevel : int {
    Debug = 0,
    Info  = 1,
    Warn  = 2,
    Error = 3,
};

// Ustawienie maksymalnego poziomu logowania (domyślnie Info).
ENGINE_API void set_log_level(LogLevel level);
ENGINE_API LogLevel log_level();

// Inicjalizacja logowania do pliku (Windows: engine.log, Linux: stderr).
// W Release te funkcje to puste implementacje.
ENGINE_API void log_init(const char* filename);
ENGINE_API void log_shutdown();

// Interny: dodaje wiersz do logu. Nie używaj bezpośrednio.
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

// Makra logowania — w Release znikają całkowicie.
#ifdef NDEBUG
  #define LOG_DEBUG(tag, ...)   ((void)0)
  #define LOG_INFO(tag, ...)    ((void)0)
  #define LOG_WARN(tag, ...)    ((void)0)
  #define LOG_ERROR(tag, ...)   ((void)0)
#else
  #define LOG_DEBUG(tag, ...)   engine::log_write(engine::LogLevel::Debug, tag, engine::log_fmt(__VA_ARGS__))
  #define LOG_INFO(tag, ...)    engine::log_write(engine::LogLevel::Info,  tag, engine::log_fmt(__VA_ARGS__))
  #define LOG_WARN(tag, ...)    engine::log_write(engine::LogLevel::Warn,  tag, engine::log_fmt(__VA_ARGS__))
  #define LOG_ERROR(tag, ...)   engine::log_write(engine::LogLevel::Error, tag, engine::log_fmt(__VA_ARGS__))
#endif
