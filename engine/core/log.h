#pragma once
#include <cstdio>
#include <cstdarg>

namespace eng::log {
    inline void info(const char* fmt, ...) {
        va_list args; va_start(args, fmt);
        std::fputs("[INFO] ", stdout);
        std::vfprintf(stdout, fmt, args);
        std::fputc('\n', stdout);
        va_end(args);
    }
    inline void warn(const char* fmt, ...) {
        va_list args; va_start(args, fmt);
        std::fputs("[WARN] ", stderr);
        std::vfprintf(stderr, fmt, args);
        std::fputc('\n', stderr);
        va_end(args);
    }
    inline void error(const char* fmt, ...) {
        va_list args; va_start(args, fmt);
        std::fputs("[ERROR] ", stderr);
        std::vfprintf(stderr, fmt, args);
        std::fputc('\n', stderr);
        va_end(args);
    }
}
