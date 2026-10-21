#include "render_state.h"
#include "frame_timing.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool close(float first, float second, float tolerance = 0.0001f)
{
    return std::fabs(first - second) <= tolerance;
}

std::string stateSignature(const billiardgl::GameState& state)
{
    std::ostringstream output;
    for (const billiardgl::BallState& ball : state.balls) {
        output << ball.position.x << ',' << ball.position.y << ',' << ball.position.z
            << ';' << ball.velocity.x << ',' << ball.velocity.y << ',' << ball.velocity.z
            << ';' << ball.angularVelocity.x << ',' << ball.angularVelocity.y << ','
            << ball.angularVelocity.z << ';' << ball.rotationAxis.x << ','
            << ball.rotationAxis.y << ',' << ball.rotationAxis.z << ';'
            << ball.speed << ';' << ball.rotationAngle << ';' << ball.pocketed << ';'
            << ball.texture << ';' << static_cast<int>(ball.motionState) << ';'
            << static_cast<int>(ball.pocketInteraction.phase) << ';'
            << ball.pocketInteraction.pocketId << ';'
            << ball.pocketInteraction.captureSequence << '|';
    }
    for (float value : state.camera.target) output << value << ',';
    for (float value : state.camera.eye) output << value << ',';
    output << state.camera.zoom << ',' << state.camera.angleX << ','
        << state.camera.angleY << ',' << state.camera.panX << ','
        << state.camera.panY << ',' << state.camera.panZ << ','
        << static_cast<int>(state.camera.anchorMode) << '|'
        << static_cast<int>(state.aim.mode) << ',' << state.aim.yaw << ','
        << state.aim.sensitivity << ',' << state.aim.showGuideLine << '|'
        << state.players.currentPlayer << ',' << state.players.nextPlayer << ','
        << state.players.illegalShot << ',' << state.players.shotTaken << ','
        << state.players.aimingAtCueBall << '|'
        << state.input.shotPower << ',' << state.input.hitRequested << '|'
        << state.hud.showHelp << '|' << state.config.width << ','
        << state.config.height << ',' << state.config.screenshotPath << '|'
        << state.events.ballCollision << ',' << state.events.railCollision << ','
        << state.events.ballPocketed << ',' << state.events.cueBallPocketed << ','
        << state.events.eightBallPocketed << ',' << state.events.shotEnded << '|'
        << state.ballsMoving << ',' << state.transitionPerspective << ','
        << state.perspectiveRecorded << ',' << state.gameOver;
    return output.str();
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
    current.balls[0].rotationAxis = {0.0f, 1.0f, 0.0f};
    current.balls[0].texture = 71;
    current.players.currentPlayer = 1;
    current.players.aimingAtCueBall = true;
    current.aim.mode = billiardgl::AimMode::Aim;
    current.aim.showGuideLine = true;
    current.input.shotPower = 80.0f;
    current.hud.showHelp = true;
    current.config.width = 1920;
    current.config.height = 1080;
    current.events.ballPocketed = true;

    const std::string previousBefore = stateSignature(previous);
    const std::string currentBefore = stateSignature(current);
    billiardgl::PresentationSnapshot halfway =
        billiardgl::buildPresentationSnapshot(
        previous, current, 0.5);
    expect(close(halfway.balls[0].position.x, 6.0f),
        "ball position is interpolated between physics ticks");
    expect(close(halfway.balls[0].rotationAngleDegrees, 22.0f),
        "ball visual rotation is interpolated");
    expect(close(halfway.camera.target.x, 6.0f),
        "follow camera tracks the interpolated cue ball");
    expect(stateSignature(previous) == previousBefore &&
        stateSignature(current) == currentBefore,
        "snapshot construction cannot modify authoritative states");
    expect(halfway.balls[0].identity == 0 &&
        halfway.balls[0].legacyTexture == 71 && halfway.balls[0].visible &&
        close(halfway.balls[0].rotationAxis.y, 1.0f),
        "snapshot carries current ball identity, visibility, and render identity");
    expect(halfway.cue.visible && halfway.cue.showGuideLine &&
        close(halfway.cue.shotPower, 80.0f) &&
        close(halfway.cue.referencePosition.x, 6.0f) &&
        close(halfway.cue.referencePosition.z, current.balls[0].position.z) &&
        close(halfway.cue.modelOriginBackoffCm,
            billiardgl::kBallRadius + 1.0f + 2.0f),
        "snapshot contains the interpolated cue transform and discrete controls");
    expect(halfway.hud.currentPlayerNumber == 2 && halfway.hud.aimMode &&
        halfway.hud.showHelp && halfway.hud.showPowerMeter &&
        halfway.hud.viewportWidth == 1920 && halfway.hud.viewportHeight == 1080,
        "HUD view model is copied from the current completed state");
    expect(halfway.events.ballPocketed,
        "transient visual events come from the current completed state");

    current.events.railCollision = true;
    halfway = billiardgl::buildPresentationSnapshot(previous, current, 0.5);
    expect(close(halfway.balls[0].position.x, 12.0f),
        "a rail collision tick presents the solved endpoint instead of a chord through geometry");
    expect(close(halfway.camera.target.x, 12.0f),
        "the follow camera uses the collision-safe cue-ball position");
    current.events.railCollision = false;

    current.events.ballCollision = true;
    halfway = billiardgl::buildPresentationSnapshot(previous, current, 0.25);
    expect(close(halfway.balls[0].position.x, 12.0f),
        "a ball collision tick cannot visually interpolate through another ball");
    current.events.ballCollision = false;

    current.balls[1].pocketed = true;
    current.balls[1].position = billiardgl::Point3{100.0f, 0.0f, 100.0f};
    halfway = billiardgl::buildPresentationSnapshot(previous, current, 0.5);
    expect(!halfway.balls[1].visible && !halfway.balls[1].castsShadow &&
        close(halfway.balls[1].position.x, 100.0f),
        "a sparse-scenario pocketed Outside slot stays hidden at its default rack position");

    current.balls[1].pocketInteraction.phase =
        billiardgl::PocketInteractionPhase::ThroatCrossed;
    halfway = billiardgl::buildPresentationSnapshot(previous, current, 0.5);
    expect(halfway.balls[1].visible && !halfway.balls[1].castsShadow &&
        close(halfway.balls[1].position.x, 100.0f),
        "a real pocket-channel ball remains visible without a table shadow");

    // Regression: throat-to-storage can retain pocketed/capture sequence, so
    // neither of those flags alone establishes continuous visible motion.
    previous.balls[1] = current.balls[1];
    previous.balls[1].position = {54.927639f, 89.341476f, -124.114006f};
    current.balls[1].pocketInteraction.phase = billiardgl::PocketInteractionPhase::Captured;
    current.balls[1].position = {54.927639f, -100.0f, 200.0f};
    for (double alpha : {0.0, 0.25, 0.5, 1.0}) {
        const auto capture = billiardgl::buildPresentationSnapshot(previous, current, alpha);
        expect(!capture.balls[1].visible && !capture.balls[1].castsShadow &&
            close(capture.balls[1].position.y, -100.0f) &&
            close(capture.balls[1].position.z, 200.0f),
            "completed capture is hidden without interpolating to its storage coordinate");
    }
    previous.balls[8] = previous.balls[1];
    current.balls[8] = current.balls[1];
    current.balls[8].position.y = 83.0f;
    expect(!billiardgl::buildPresentationSnapshot(previous, current, 0.5).balls[8].visible,
        "eight-ball storage is hidden by semantic capture state, not a magic height");

    current.balls[2].outOfPlay = true;
    current.balls[2].position.x = 400.0f;
    halfway = billiardgl::buildPresentationSnapshot(previous, current, 0.5);
    expect(!halfway.balls[2].visible && !halfway.balls[2].castsShadow &&
        !halfway.balls[2].pocketed && close(halfway.balls[2].position.x, 400.0f),
        "off-table removal hides discretely without inventing a pocket capture");

    halfway = billiardgl::buildPresentationSnapshot(previous, current, -1.0);
    expect(close(halfway.balls[0].position.x, 0.0f),
        "render interpolation clamps negative alpha");
    halfway = billiardgl::buildPresentationSnapshot(previous, current, 2.0);
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
            const billiardgl::PresentationSnapshot frameState =
                billiardgl::buildPresentationSnapshot(prior, latest,
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
        expect(close(latest.balls[0].position.x, 100.0f, 0.001f),
            "display cadence cannot change authoritative 120 Hz tick results");
    }
    return EXIT_SUCCESS;
}
