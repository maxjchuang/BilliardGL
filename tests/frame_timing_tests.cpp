#include "frame_timing.h"

#include <cstdlib>
#include <iostream>

namespace {

int fail(const char* message)
{
    std::cerr << message << std::endl;
    return EXIT_FAILURE;
}

bool closeEnough(float a, float b)
{
    const float difference = a > b ? a - b : b - a;
    return difference < 0.0001f;
}

}  // namespace

int main()
{
    float accumulator = 0.0f;
    billiardgl::FrameStepResult result = billiardgl::advanceFixedStepAccumulator(accumulator, 0.0f, 0.1f, 5);
    if (result.steps != 0 || !closeEnough(accumulator, 0.0f)) {
        return fail("zero elapsed time should not advance simulation");
    }

    result = billiardgl::advanceFixedStepAccumulator(accumulator, -1.0f, 0.1f, 5);
    if (result.steps != 0 || !closeEnough(accumulator, 0.0f)) {
        return fail("negative elapsed time should not advance simulation");
    }

    result = billiardgl::advanceFixedStepAccumulator(accumulator, 0.04f, 0.1f, 5);
    if (result.steps != 0 || !closeEnough(accumulator, 0.04f)) {
        return fail("sub-step elapsed time should accumulate without stepping");
    }

    result = billiardgl::advanceFixedStepAccumulator(accumulator, 0.07f, 0.1f, 5);
    if (result.steps != 1 || !closeEnough(accumulator, 0.01f)) {
        return fail("accumulated elapsed time should produce one fixed step");
    }

    result = billiardgl::advanceFixedStepAccumulator(accumulator, 0.32f, 0.1f, 5);
    if (result.steps != 3 || !closeEnough(accumulator, 0.03f)) {
        return fail("elapsed time should drain multiple fixed steps and preserve remainder");
    }

    accumulator = 0.0f;
    result = billiardgl::advanceFixedStepAccumulator(accumulator, 2.0f, 0.1f, 4);
    if (result.steps != 4 || !result.clamped || !closeEnough(accumulator, 0.0f)) {
        return fail("large elapsed time should clamp catch-up work and discard excess");
    }

    return EXIT_SUCCESS;
}
