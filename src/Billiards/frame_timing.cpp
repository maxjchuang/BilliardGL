#include "frame_timing.h"

namespace billiardgl {

FrameStepResult advanceFixedStepAccumulator(float& accumulatorSeconds,
    float elapsedSeconds,
    float fixedTimeStepSeconds,
    int maxStepsPerFrame)
{
    FrameStepResult result;
    if (elapsedSeconds <= 0.0f || fixedTimeStepSeconds <= 0.0f || maxStepsPerFrame <= 0) {
        return result;
    }

    accumulatorSeconds += elapsedSeconds;
    while (accumulatorSeconds >= fixedTimeStepSeconds && result.steps < maxStepsPerFrame) {
        accumulatorSeconds -= fixedTimeStepSeconds;
        result.steps += 1;
    }

    if (accumulatorSeconds >= fixedTimeStepSeconds) {
        accumulatorSeconds = 0.0f;
        result.clamped = true;
    }

    return result;
}

}  // namespace billiardgl
