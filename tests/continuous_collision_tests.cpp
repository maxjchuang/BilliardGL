#include "continuous_collision.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
void expect(bool condition, const char* message)
{
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}

billiardgl::BallState ball(float x, float vx)
{
    billiardgl::BallState value;
    value.position.x = x;
    value.velocity.x = vx;
    return value;
}
}

int main()
{
    using namespace billiardgl;
    const BallState fast = ball(-50.0f, 1000.0f);
    const BallState target = ball(50.0f, 0.0f);
    const ContinuousContactCandidate hit = sweptBallBallCandidate(
        fast, 3, target, 7, 0.1, 5.715);
    expect(hit.valid && std::fabs(hit.timeOfImpactSeconds - 0.094285) < 1e-6,
        "analytic sweep should catch a high-speed crossing");
    expect(hit.firstBall == 3 && hit.secondBall == 7 && hit.normal.x > 0.999f,
        "candidate key and normal should be canonical");

    const ContinuousContactCandidate swapped = sweptBallBallCandidate(
        target, 7, fast, 3, 0.1, 5.715);
    expect(swapped.valid && swapped.firstBall == hit.firstBall &&
        swapped.secondBall == hit.secondBall &&
        std::fabs(swapped.timeOfImpactSeconds - hit.timeOfImpactSeconds) < 1e-12 &&
        std::fabs(swapped.normal.x - hit.normal.x) < 1e-6,
        "input order must not change the canonical candidate");

    BallState overlap = ball(4.0f, 10.0f);
    const ContinuousContactCandidate initial = sweptBallBallCandidate(
        ball(0.0f, 0.0f), 0, overlap, 1, 0.1, 5.715);
    expect(initial.valid && initial.timeOfImpactSeconds == 0.0 &&
        std::fabs(initial.penetrationCm - 1.715) < 1e-6,
        "initial overlap should be a zero-time projection candidate");

    expect(!sweptBallBallCandidate(
        ball(0.0f, -1.0f), 0, ball(10.0f, 1.0f), 1, 1.0, 5.715).valid,
        "separating balls should not create a future impulse");
    expect(!sweptBallBallCandidate(
        ball(0.0f, 0.0f), 0, ball(10.0f, 0.0f), 1, 1.0, 5.715).valid,
        "stationary separated balls have no candidate");

    BallState grazer = ball(-10.0f, 20.0f);
    grazer.position.z = 5.715f;
    BallState origin = ball(0.0f, 0.0f);
    const ContinuousContactCandidate grazing = sweptBallBallCandidate(
        grazer, 0, origin, 1, 1.0, 5.715);
    expect(grazing.valid && std::isfinite(grazing.timeOfImpactSeconds),
        "a grazing discriminant should remain finite");

    GameState state;
    for (BallState& item : state.balls) item.pocketed = true;
    state.balls[4] = fast;
    state.balls[4].pocketed = false;
    state.balls[2] = target;
    state.balls[2].pocketed = false;
    const GameState before = state;
    const std::vector<ContinuousContactCandidate> generated =
        generateBallBallCandidates(state, 0.1, 2.8575);
    expect(generated.size() == 1 && generated[0].firstBall == 2 &&
        generated[0].secondBall == 4,
        "generator should emit one stable active-pair candidate");
    expect(state.balls[4].position.x == before.balls[4].position.x &&
        state.balls[2].velocity.x == before.balls[2].velocity.x,
        "candidate generation must not mutate authoritative state");

    const ContinuousContactCandidate jaw = boundaryContactCandidate(
        2, 5, 0.01, Point3{1.0f, 0.0f, 0.0f},
        PocketBoundaryEventKind::LeftJaw);
    expect(jaw.valid && jaw.kind == ContinuousContactKind::Jaw &&
        jaw.featureId == 5, "boundary wrapper should retain stable feature identity");

    const ContinuousContactCandidate capture = boundaryContactCandidate(
        2, 9, 0.01, Point3{}, PocketBoundaryEventKind::Capture);
    expect(continuousContactStableKey(jaw) < continuousContactStableKey(capture),
        "stable event ordering should place physical jaws before capture transitions");
    return 0;
}
