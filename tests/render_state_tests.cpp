#include "render_state.h"
#include "frame_timing.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool close(float first, float second)
{
    return std::fabs(first - second) <= 0.0001f;
}

}  // namespace

int main()
{
    billiardgl::GameState previous;
    billiardgl::initializeBalls(previous);
    billiardgl::GameState current = previous;
    previous.balls[0].position.x = 0.0f;
    current.balls[0].position.x = 12.0f;
    previous.balls[0].rotationAngle = 10.0f;
    current.balls[0].rotationAngle = 34.0f;

    billiardgl::GameState halfway = billiardgl::interpolateRenderState(
        previous, current, 0.5);
    expect(close(halfway.balls[0].position.x, 6.0f),
        "ball position is interpolated between physics ticks");
    expect(close(halfway.balls[0].rotationAngle, 22.0f),
        "ball visual rotation is interpolated");
    expect(close(halfway.camera.target[0], 6.0f),
        "follow camera tracks the interpolated cue ball");

    current.balls[1].pocketed = true;
    current.balls[1].position = billiardgl::Point3{100.0f, 0.0f, 100.0f};
    halfway = billiardgl::interpolateRenderState(previous, current, 0.5);
    expect(halfway.balls[1].pocketed &&
        close(halfway.balls[1].position.x, 100.0f),
        "pocket transitions are discrete and never interpolate through geometry");

    halfway = billiardgl::interpolateRenderState(previous, current, -1.0);
    expect(close(halfway.balls[0].position.x, 0.0f),
        "render interpolation clamps negative alpha");
    halfway = billiardgl::interpolateRenderState(previous, current, 2.0);
    expect(close(halfway.balls[0].position.x, 12.0f),
        "render interpolation clamps alpha above one");

    for (double displayHz : {60.0, 120.0, 144.0}) {
        billiardgl::GameState prior;
        billiardgl::initializeBalls(prior);
        billiardgl::GameState latest = prior;
        double accumulator = 0.0;
        float lastRenderedX = prior.balls[0].position.x;
        int repeatedMovingFrames = 0;
        float maximumFrameDistance = 0.0f;
        const double physicsStep = 1.0 / 120.0;
        const int frameCount = static_cast<int>(displayHz);
        for (int frame = 0; frame < frameCount; ++frame) {
            const billiardgl::FrameStepResult steps =
                billiardgl::advanceFixedStepAccumulator(
                    accumulator, 1.0 / displayHz, physicsStep, 12);
            expect(!steps.clamped,
                "normal visual cadence cannot drop simulation time");
            for (int step = 0; step < steps.steps; ++step) {
                prior = latest;
                latest.balls[0].position.x +=
                    static_cast<float>(100.0 * physicsStep);
            }
            const billiardgl::GameState frameState =
                billiardgl::interpolateRenderState(prior, latest,
                    billiardgl::frameInterpolationAlpha(
                        accumulator, physicsStep));
            const float distance = std::fabs(
                frameState.balls[0].position.x - lastRenderedX);
            if (frame > 0 && distance <= 0.00001f) ++repeatedMovingFrames;
            maximumFrameDistance = std::max(maximumFrameDistance, distance);
            lastRenderedX = frameState.balls[0].position.x;
        }
        expect(repeatedMovingFrames == 0,
            "a moving ball must produce a new visual position every display frame");
        expect(maximumFrameDistance <= static_cast<float>(200.0 / displayHz),
            "interpolated frame displacement must remain bounded and smooth");
    }
    return EXIT_SUCCESS;
}
