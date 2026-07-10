#pragma once

namespace billiardgl {

struct FrameStepResult {
    int steps = 0;
    bool clamped = false;
};

FrameStepResult advanceFixedStepAccumulator(float& accumulatorSeconds,
    float elapsedSeconds,
    float fixedTimeStepSeconds,
    int maxStepsPerFrame);

}  // namespace billiardgl
