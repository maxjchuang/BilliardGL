#pragma once

#include <cstdint>

namespace fullgame {

class XorShift32 {
public:
    explicit XorShift32(std::uint32_t seed)
        : state_(seed == 0 ? 0x6d2b79f5u : seed) {}

    std::uint32_t next()
    {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return state_;
    }

    float unit()
    {
        return static_cast<float>(next()) / 4294967295.0f;
    }

private:
    std::uint32_t state_;
};

}  // namespace fullgame
