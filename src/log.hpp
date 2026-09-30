#pragma once

#include <cstdarg>
#include <cstdio>

namespace de {

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

#define DE_LOG(level, ...) ::de::log_write(level, __VA_ARGS__)
#define DE_DEBUG(...) DE_LOG(::de::LogLevel::Debug, __VA_ARGS__)
#define DE_INFO(...)  DE_LOG(::de::LogLevel::Info, __VA_ARGS__)
#define DE_WARN(...)  DE_LOG(::de::LogLevel::Warn, __VA_ARGS__)
#define DE_ERROR(...) DE_LOG(::de::LogLevel::Error, __VA_ARGS__)

}  // namespace de
