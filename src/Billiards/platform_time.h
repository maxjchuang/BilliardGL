#pragma once

#include <chrono>
#include <thread>

namespace billiardgl {

inline void sleepMilliseconds(int milliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

}  // namespace billiardgl
