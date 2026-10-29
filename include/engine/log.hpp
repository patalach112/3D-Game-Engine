#pragma once

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
void set_log_level(LogLevel level);
LogLevel log_level();

// Inicjalizacja logowania do pliku (Windows: engine.log, Linux: stderr).
// W Release te funkcje to puste implementacje.
void log_init(const char* filename);
void log_shutdown();

// Interny: dodaje wiersz do logu. Nie używaj bezpośrednio.
void log_write(LogLevel level, const char* tag, const std::string& message);

} // namespace engine

// Makra logowania — w Release znikają całkowicie.
#ifdef NDEBUG
  #define LOG_DEBUG(tag, msg)   ((void)0)
  #define LOG_INFO(tag, msg)    ((void)0)
  #define LOG_WARN(tag, msg)    ((void)0)
  #define LOG_ERROR(tag, msg)   ((void)0)
#else
  #define LOG_DEBUG(tag, msg)   engine::log_write(engine::LogLevel::Debug, tag, msg)
  #define LOG_INFO(tag, msg)    engine::log_write(engine::LogLevel::Info,  tag, msg)
  #define LOG_WARN(tag, msg)    engine::log_write(engine::LogLevel::Warn,  tag, msg)
  #define LOG_ERROR(tag, msg)   engine::log_write(engine::LogLevel::Error, tag, msg)
#endif
