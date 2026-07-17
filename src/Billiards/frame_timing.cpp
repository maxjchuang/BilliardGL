#include "frame_timing.h"

#include <algorithm>
#include <cmath>

namespace billiardgl {

FrameStepResult advanceFixedStepAccumulator(double& accumulatorSeconds,
    double elapsedSeconds,
    double fixedTimeStepSeconds,
    int maxStepsPerFrame)
{
    FrameStepResult result;
    if (elapsedSeconds <= 0.0f || fixedTimeStepSeconds <= 0.0f || maxStepsPerFrame <= 0) {
        return result;
    }

    accumulatorSeconds += elapsedSeconds;
    const double comparisonEpsilon = fixedTimeStepSeconds * 1e-9;
    while (accumulatorSeconds + comparisonEpsilon >= fixedTimeStepSeconds &&
        result.steps < maxStepsPerFrame) {
        accumulatorSeconds = std::max(
            0.0, accumulatorSeconds - fixedTimeStepSeconds);
        result.steps += 1;
    }

    if (accumulatorSeconds + comparisonEpsilon >= fixedTimeStepSeconds) {
        result.droppedSeconds = accumulatorSeconds;
        accumulatorSeconds = 0.0f;
        result.clamped = true;
    }

    return result;
}

double frameInterpolationAlpha(double accumulatorSeconds,
    double fixedTimeStepSeconds)
{
    if (!std::isfinite(accumulatorSeconds) ||
        !std::isfinite(fixedTimeStepSeconds) ||
        fixedTimeStepSeconds <= 0.0) {
        return 0.0;
    }
    return std::clamp(accumulatorSeconds / fixedTimeStepSeconds, 0.0, 1.0);
}

}  // namespace billiardgl
