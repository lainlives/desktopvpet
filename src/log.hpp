// log.hpp: tiny printf-style logger with severity prefixes (DVP_INFO/WARN/ERROR...).
#pragma once

#include <cstdarg>
#include <cstdio>

namespace dvp {

enum class LogLevel { Debug, Info, Warn, Error };

inline void log_write(LogLevel level, const char *fmt, ...) {
    const char *tag = "info";
    switch (level) {
        case LogLevel::Debug: tag = "debug"; break;
        case LogLevel::Info:  tag = "info";  break;
        case LogLevel::Warn:  tag = "warn";  break;
        case LogLevel::Error: tag = "error"; break;
    }
    std::fprintf(stderr, "[%s] ", tag);
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);
    std::fputc('\n', stderr);
}

#define DVP_LOG(level, ...) ::dvp::log_write(level, __VA_ARGS__)
#define DVP_DEBUG(...) DVP_LOG(::dvp::LogLevel::Debug, __VA_ARGS__)
#define DVP_INFO(...)  DVP_LOG(::dvp::LogLevel::Info, __VA_ARGS__)
#define DVP_WARN(...)  DVP_LOG(::dvp::LogLevel::Warn, __VA_ARGS__)
#define DVP_ERROR(...) DVP_LOG(::dvp::LogLevel::Error, __VA_ARGS__)

}  // namespace dvp
