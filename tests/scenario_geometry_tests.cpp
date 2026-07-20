#include "scenario_geometry.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {

void expect(bool value, const char* message)
{
    if (!value) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

billiardgl::PhysicsScenario validScenario()
{
    billiardgl::PhysicsScenario scenario;
    for (billiardgl::BallState& ball : scenario.balls) {
        ball.pocketed = true;
    }
    const float radius = scenario.physicsProfile.ball.radiusCm;
    scenario.balls[0].pocketed = false;
    scenario.balls[0].position = billiardgl::Point3{
        0.0f, billiardgl::kTableHeight + radius, 0.0f};
    scenario.balls[1].pocketed = false;
    scenario.balls[1].position = billiardgl::Point3{
        2.0f * radius, billiardgl::kTableHeight + radius, 0.0f};
    return scenario;
}

}  // namespace

int main()
{
    billiardgl::PhysicsScenario scenario = validScenario();
    expect(billiardgl::validateScenarioGeometry(scenario).ok,
        "exactly touching balls should be a valid initial contact");

    scenario.balls[0].position.x = std::numeric_limits<float>::infinity();
    const billiardgl::ScenarioGeometryResult nonfinite =
        billiardgl::validateScenarioGeometry(scenario);
    expect(!nonfinite.ok && nonfinite.code == "NONFINITE_BALL" &&
        nonfinite.firstBall == 0,
        "nonfinite physical state should identify the first invalid ball");

    scenario = validScenario();
    scenario.balls[1].position.x =
        2.0f * scenario.physicsProfile.ball.radiusCm - 0.01f;
    const billiardgl::ScenarioGeometryResult overlap =
        billiardgl::validateScenarioGeometry(scenario);
    expect(!overlap.ok && overlap.code == "BALL_OVERLAP" &&
        overlap.firstBall == 0 && overlap.secondBall == 1,
        "overlap should identify both balls deterministically");
    scenario.initialContactEpsilonCm = 0.02f;
    expect(billiardgl::validateScenarioGeometry(scenario).ok,
        "declared initial-contact epsilon should admit only its small overlap");

    scenario = validScenario();
    scenario.balls[0].position.x =
        scenario.physicsProfile.tableBoundary.playfieldWidthCm;
    const billiardgl::ScenarioGeometryResult outside =
        billiardgl::validateScenarioGeometry(scenario);
    expect(!outside.ok && outside.code == "OUTSIDE_APPARATUS" &&
        outside.firstBall == 0,
        "production table should reject a center outside the playable domain");
    scenario.boundaryMode = billiardgl::PhysicsBoundaryMode::Unbounded;
    expect(billiardgl::validateScenarioGeometry(scenario).ok,
        "unbounded bench should skip only the table-domain check");

    scenario.balls[0].velocity.z =
        std::numeric_limits<float>::quiet_NaN();
    expect(billiardgl::validateScenarioGeometry(scenario).code ==
        "NONFINITE_BALL",
        "unbounded bench should still reject nonfinite state");

    scenario = validScenario();
    scenario.initialContactEpsilonCm =
        2.0f * scenario.physicsProfile.ball.radiusCm;
    expect(billiardgl::validateScenarioGeometry(scenario).code ==
        "INVALID_CONTACT_EPSILON",
        "contact epsilon may not erase the full non-penetration contract");
    return 0;
}
