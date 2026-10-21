#pragma once

#include <optional>

namespace billiardgl {

struct FrameStepResult {
    int steps = 0;
    bool clamped = false;
    double droppedSeconds = 0.0;
};

FrameStepResult advanceFixedStepAccumulator(double& accumulatorSeconds,
    double elapsedSeconds,
    double fixedTimeStepSeconds,
    int maxStepsPerFrame);

double frameInterpolationAlpha(double accumulatorSeconds,
    double fixedTimeStepSeconds);

struct PresentedFrameTiming {
    double loopThroughSwapMilliseconds = 0.0;
    double swapMilliseconds = 0.0;
    // Absent on the first frame. This interval includes work after the previous
    // swap (logging/event processing), not just the current renderer submission.
    std::optional<double> swapToSwapMilliseconds;
};

// All timestamps must use the same monotonic clock and millisecond units.
// This measures CPU wall time through swap completion, not compositor scanout.
std::optional<PresentedFrameTiming> measurePresentedFrame(
    double loopStart, double swapStart, double swapEnd,
    std::optional<double> previousSwapEnd = std::nullopt);

}  // namespace billiardgl
