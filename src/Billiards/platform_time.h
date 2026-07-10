#pragma once

#include <chrono>
#include <thread>

namespace billiardgl {

inline double monotonicSeconds()
{
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now.time_since_epoch()).count();
}

inline void sleepMilliseconds(int milliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

}  // namespace billiardgl
