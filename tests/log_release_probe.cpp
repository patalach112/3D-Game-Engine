// Intentionally compile the macros as Release even when the test uses asserts.
#ifndef NDEBUG
#define NDEBUG
#endif
#include "engine/log.hpp"
void log_release_probe() {
    LOG_INFO("release", "info stays visible");
    LOG_WARN("release", "warning stays visible");
    LOG_ERROR("release", "error stays visible");
    LOG_DEBUG("release", "debug is compiled out");
}
