#pragma once
#include <chrono>

namespace eng::time {
    using clock = std::chrono::steady_clock;
    using seconds_f = std::chrono::duration<float>;

    struct DeltaTimer {
        clock::time_point last = clock::now();
        float tick() {
            auto now = clock::now();
            seconds_f dt = now - last;
            last = now;
            return dt.count();
        }
    };
}

