#pragma once

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

}  // namespace billiardgl
