#include "frame_timing.h"

#include <cstdlib>
#include <iostream>
#include <limits>

namespace {

int fail(const char* message)
{
    std::cerr << message << std::endl;
    return EXIT_FAILURE;
}

bool closeEnough(double a, double b)
{
    const float difference = a > b ? a - b : b - a;
    return difference < 0.0001f;
}

}  // namespace

int main()
{
    const auto firstFrame = billiardgl::measurePresentedFrame(10.0, 22.0, 26.0);
    if (!firstFrame || firstFrame->swapToSwapMilliseconds ||
        !closeEnough(firstFrame->loopThroughSwapMilliseconds, 16.0) ||
        !closeEnough(firstFrame->swapMilliseconds, 4.0))
        return fail("first frame measures full loop and swap without inventing cadence");
    const auto nextFrame = billiardgl::measurePresentedFrame(29.0, 39.0, 43.0, 26.0);
    if (!nextFrame || !nextFrame->swapToSwapMilliseconds ||
        !closeEnough(*nextFrame->swapToSwapMilliseconds, 17.0) ||
        !closeEnough(nextFrame->loopThroughSwapMilliseconds, 14.0))
        return fail("swap cadence must include inter-frame logging and event overhead");
    const double invalid = std::numeric_limits<double>::quiet_NaN();
    if (billiardgl::measurePresentedFrame(10, 9, 12) ||
        billiardgl::measurePresentedFrame(10, 12, 11) ||
        billiardgl::measurePresentedFrame(10, 12, 13, 11) ||
        billiardgl::measurePresentedFrame(invalid, 12, 13) ||
        billiardgl::measurePresentedFrame(10, invalid, 13) ||
        billiardgl::measurePresentedFrame(10, 12, invalid) ||
        billiardgl::measurePresentedFrame(10, 12, 13, invalid))
        return fail("invalid or nonmonotonic timestamps must not yield performance evidence");

    double accumulator = 0.0;
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
    if (result.steps != 4 || !result.clamped ||
        !closeEnough(result.droppedSeconds, 1.6) ||
        !closeEnough(accumulator, 0.0f)) {
        return fail("large elapsed time should clamp catch-up work and discard excess");
    }

    accumulator = 0.0;
    result = billiardgl::advanceFixedStepAccumulator(accumulator, 1.0, 1.0 / 120.0, 8);
    if (result.steps != 8 || !result.clamped || accumulator >= 1.0 / 120.0 ||
        !closeEnough(result.droppedSeconds, 1.0 - 8.0 / 120.0)) {
        return fail("one-second product stall must not leave a catch-up backlog");
    }
    result = billiardgl::advanceFixedStepAccumulator(accumulator, 1.0 / 60.0, 1.0 / 120.0, 8);
    if (result.steps != 2 || result.clamped)
        return fail("frame following a stall returns to normal fixed-step cadence");

    if (!closeEnough(billiardgl::frameInterpolationAlpha(0.004, 0.008), 0.5) ||
        !closeEnough(billiardgl::frameInterpolationAlpha(-1.0, 0.008), 0.0) ||
        !closeEnough(billiardgl::frameInterpolationAlpha(1.0, 0.008), 1.0)) {
        return fail("interpolation alpha should be normalized and clamped");
    }

    for (double displayHz : {60.0, 120.0, 144.0}) {
        accumulator = 0.0;
        int steps = 0;
        const int frames = static_cast<int>(displayHz * 10.0);
        for (int frame = 0; frame < frames; ++frame) {
            const billiardgl::FrameStepResult frameResult =
                billiardgl::advanceFixedStepAccumulator(
                    accumulator, 1.0 / displayHz, 1.0 / 120.0, 12);
            steps += frameResult.steps;
            if (frameResult.clamped) {
                return fail("normal display cadences must not drop simulation time");
            }
        }
        if (steps != 1200 || !closeEnough(accumulator, 0.0)) {
            return fail("ten real seconds must advance exactly ten simulated seconds");
        }
    }

    return EXIT_SUCCESS;
}
