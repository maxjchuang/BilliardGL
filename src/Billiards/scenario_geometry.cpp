#include "scenario_geometry.h"

#include <cmath>

namespace billiardgl {
namespace {

bool finitePoint(const Point3& point)
{
    return std::isfinite(point.x) && std::isfinite(point.y) &&
        std::isfinite(point.z);
}

bool finiteBall(const BallState& ball)
{
    return finitePoint(ball.position) && finitePoint(ball.velocity) &&
        finitePoint(ball.angularVelocity) && finitePoint(ball.rotationAxis) &&
        std::isfinite(ball.speed) && std::isfinite(ball.rotationAngle);
}

bool insideProductionTable(const BallState& ball,
    const PhysicsProfile& profile)
{
    const float radius = profile.ball.radiusCm;
    const float xLimit = 0.5f * profile.tableBoundary.playfieldWidthCm - radius;
    const float zLimit = 0.5f * profile.tableBoundary.playfieldLengthCm - radius;
    constexpr float kComparisonToleranceCm = 0.00001f;
    return std::fabs(ball.position.x) <= xLimit + kComparisonToleranceCm &&
        std::fabs(ball.position.z) <= zLimit + kComparisonToleranceCm;
}

double distanceSquared(const Point3& first, const Point3& second)
{
    const double dx = static_cast<double>(first.x) - second.x;
    const double dy = static_cast<double>(first.y) - second.y;
    const double dz = static_cast<double>(first.z) - second.z;
    return dx * dx + dy * dy + dz * dz;
}

}  // namespace

ScenarioGeometryResult validateScenarioGeometry(
    const PhysicsScenario& scenario)
{
    const double diameter = 2.0 * scenario.physicsProfile.ball.radiusCm;
    const double epsilon = scenario.initialContactEpsilonCm;
    if (!std::isfinite(epsilon) || epsilon < 0.0 || epsilon >= diameter) {
        return ScenarioGeometryResult(false, "INVALID_CONTACT_EPSILON");
    }
    const double minimumSeparation = diameter - epsilon;
    const double minimumSeparationSquared =
        minimumSeparation * minimumSeparation;
    for (int first = 0; first < kBallCount; ++first) {
        const BallState& firstBall = scenario.balls[first];
        if (firstBall.pocketed) continue;
        if (!finiteBall(firstBall)) {
            return ScenarioGeometryResult(false, "NONFINITE_BALL", first);
        }
        if (scenario.boundaryMode == PhysicsBoundaryMode::ProductionTable &&
            !insideProductionTable(firstBall, scenario.physicsProfile)) {
            return ScenarioGeometryResult(false, "OUTSIDE_APPARATUS", first);
        }
        for (int second = first + 1; second < kBallCount; ++second) {
            const BallState& secondBall = scenario.balls[second];
            if (secondBall.pocketed) continue;
            if (!finiteBall(secondBall)) {
                return ScenarioGeometryResult(false, "NONFINITE_BALL", second);
            }
            if (distanceSquared(firstBall.position, secondBall.position) <
                minimumSeparationSquared) {
                return ScenarioGeometryResult(
                    false, "BALL_OVERLAP", first, second);
            }
        }
    }
    return ScenarioGeometryResult(true);
}

}  // namespace billiardgl
