#include "game_state.h"
#include "surface_motion.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {

int fail(const char* message)
{
    std::cerr << message << std::endl;
    return EXIT_FAILURE;
}

bool nearlyEqual(float a, float b, float epsilon = 0.001f)
{
    return std::fabs(a - b) < epsilon;
}

}  // namespace

int main()
{
    billiardgl::GameState state;
    if (state.aim.showGuideLine) {
        return fail("aim guide line should be disabled by default");
    }
    billiardgl::initializeBalls(state);
    for (const billiardgl::BallState& ball : state.balls) {
        if (ball.motionState != billiardgl::BallMotionState::Stationary) {
            return fail("initialized balls should have authoritative stationary state");
        }
        if (ball.trajectoryGeneration != 0) {
            return fail("initialized balls should start at trajectory generation zero");
        }
    }

    std::uint64_t parsedGeneration = 99;
    if (!billiardgl::parseCanonicalTrajectoryGeneration("0", parsedGeneration) ||
        parsedGeneration != 0 ||
        !billiardgl::parseCanonicalTrajectoryGeneration(
            "18446744073709551615", parsedGeneration) ||
        parsedGeneration != std::numeric_limits<std::uint64_t>::max() ||
        billiardgl::canonicalTrajectoryGeneration(parsedGeneration) !=
            "18446744073709551615") {
        return fail("trajectory generation codec should round-trip the full uint64 range");
    }
    const std::array<const char*, 8> invalidGenerations = {{
        "", "-1", "+1", "01", "1.0", " 1", "1 ",
        "18446744073709551616"}};
    for (const char* text : invalidGenerations) {
        parsedGeneration = 73;
        if (billiardgl::parseCanonicalTrajectoryGeneration(text, parsedGeneration) ||
            parsedGeneration != 73) {
            return fail("trajectory generation codec should reject noncanonical or overflowing values atomically");
        }
    }

    state.balls[0].trajectoryGeneration =
        std::numeric_limits<std::uint64_t>::max();
    const billiardgl::GameState copied = state;
    if (copied.balls[0].trajectoryGeneration !=
            std::numeric_limits<std::uint64_t>::max()) {
        return fail("GameState copies should preserve nonzero trajectory generations");
    }
    state.balls[0].trajectoryGeneration = 9;
    state.balls[1].outOfPlay = true;
    billiardgl::initializeBalls(state);
    if (state.balls[0].trajectoryGeneration != 0) {
        return fail("initializeBalls should clear stale trajectory generations");
    }
    if (state.balls[1].outOfPlay) return fail("rerack initialization clears prior off-table removal");
    if (!nearlyEqual(billiardgl::kBallRadius, 2.8575f)) {
        return fail("ball radius should use real Chinese billiards radius");
    }
    if (!nearlyEqual(billiardgl::kTableInLength, 254.0f) ||
        !nearlyEqual(billiardgl::kTableInWidth, 127.0f)) {
        return fail("playfield should use real Chinese billiards dimensions");
    }
    if (!nearlyEqual(state.balls[0].position.y, billiardgl::kTableHeight + billiardgl::kBallRadius)) {
        return fail("cue ball should sit on the corrected table surface");
    }
    if (!nearlyEqual(state.balls[0].position.y - billiardgl::kBallRadius, 86.483976f)) {
        return fail("cue ball bottom should align with the visual table surface");
    }
    const float secondRowDistance = std::sqrt(
        (state.balls[2].position.x - state.balls[3].position.x) *
            (state.balls[2].position.x - state.balls[3].position.x) +
        (state.balls[2].position.z - state.balls[3].position.z) *
            (state.balls[2].position.z - state.balls[3].position.z));
    if (!nearlyEqual(secondRowDistance, 2.0f * billiardgl::kBallRadius)) {
        return fail("rack balls in the same row should touch");
    }
    const float firstToSecondRowDistance = std::sqrt(
        (state.balls[1].position.x - state.balls[2].position.x) *
            (state.balls[1].position.x - state.balls[2].position.x) +
        (state.balls[1].position.z - state.balls[2].position.z) *
            (state.balls[1].position.z - state.balls[2].position.z));
    if (!nearlyEqual(firstToSecondRowDistance, 2.0f * billiardgl::kBallRadius)) {
        return fail("adjacent rack rows should touch without overlap");
    }
    state.balls[0].position = billiardgl::Point3{1.0f, 2.0f, 3.0f};
    state.balls[0].velocity = billiardgl::Point3{4.0f, 0.0f, 5.0f};
    state.balls[0].rotationAxis = billiardgl::Point3{0.0f, 1.0f, 0.0f};
    state.balls[0].speed = 6.0f;
    state.balls[0].rotationAngle = 7.0f;
    state.balls[0].pocketed = true;
    state.balls[0].texture = 42;
    state.balls[0].motionState = billiardgl::BallMotionState::Sliding;
    state.balls[0].trajectoryGeneration = 23;
    billiardgl::resetBallMotion(state.balls[0]);
    if (state.balls[0].motionState != billiardgl::BallMotionState::Stationary) {
        return fail("resetBallMotion should reset authoritative motion state");
    }
    if (state.balls[0].trajectoryGeneration != 23) {
        return fail("motion-only reset should preserve trajectory generation");
    }
    state.balls[0].velocity = billiardgl::Point3{4.0f, 0.0f, 5.0f};
    state.balls[0].speed = 6.0f;
    state.balls[0].rotationAngle = 7.0f;
    state.balls[0].pocketed = true;

    std::array<billiardgl::LegacyBallAdapter, billiardgl::kBallCount> legacyBalls;
    billiardgl::copyBallStateToLegacy(state, legacyBalls);

    if (legacyBalls[0].position.x != 1.0f || legacyBalls[0].position.y != 2.0f || legacyBalls[0].position.z != 3.0f) {
        return fail("legacy adapter should copy position from GameState");
    }
    if (legacyBalls[0].velocity.x != 4.0f || legacyBalls[0].velocity.z != 5.0f) {
        return fail("legacy adapter should copy velocity from GameState");
    }
    if (!legacyBalls[0].pocketed || legacyBalls[0].texture != 42) {
        return fail("legacy adapter should copy pocket and texture fields");
    }

    legacyBalls[1].texture = 99;
    billiardgl::copyLegacyTexturesToState(legacyBalls, state);
    if (state.balls[1].texture != 99) {
        return fail("texture copy should preserve render-loaded texture IDs in GameState");
    }

    return EXIT_SUCCESS;
}
