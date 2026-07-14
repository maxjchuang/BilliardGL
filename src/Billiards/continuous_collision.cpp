#include "continuous_collision.h"

#include <algorithm>
#include <cmath>
#include <tuple>

namespace billiardgl {
namespace {

double dot(const Point3& first, const Point3& second)
{
    return first.x * second.x + first.y * second.y + first.z * second.z;
}

Point3 difference(const Point3& first, const Point3& second)
{
    return Point3{first.x - second.x, first.y - second.y, first.z - second.z};
}

Point3 normalized(const Point3& value)
{
    const double length = std::sqrt(dot(value, value));
    if (!(length > 1e-12) || !std::isfinite(length)) return Point3{1.0f, 0.0f, 0.0f};
    return Point3{static_cast<float>(value.x / length),
                  static_cast<float>(value.y / length),
                  static_cast<float>(value.z / length)};
}

int kindOrder(ContinuousContactKind kind)
{
    switch (kind) {
    case ContinuousContactKind::BallBall: return 0;
    case ContinuousContactKind::StraightRail: return 1;
    case ContinuousContactKind::Jaw: return 2;
    case ContinuousContactKind::Throat: return 3;
    case ContinuousContactKind::Capture: return 4;
    }
    return 5;
}

}  // namespace

ContinuousContactCandidate sweptBallBallCandidate(
    const BallState& first, int firstIndex,
    const BallState& second, int secondIndex,
    double maximumTimeSeconds, double combinedRadiusCm,
    double toleranceSeconds)
{
    ContinuousContactCandidate result;
    result.kind = ContinuousContactKind::BallBall;
    result.firstBall = std::min(firstIndex, secondIndex);
    result.secondBall = std::max(firstIndex, secondIndex);
    if (firstIndex < 0 || secondIndex < 0 || firstIndex == secondIndex ||
        !std::isfinite(maximumTimeSeconds) || maximumTimeSeconds < 0.0 ||
        !std::isfinite(combinedRadiusCm) || combinedRadiusCm <= 0.0 ||
        !std::isfinite(toleranceSeconds) || toleranceSeconds < 0.0) return result;

    const Point3 relativePosition = difference(second.position, first.position);
    const Point3 relativeVelocity = difference(second.velocity, first.velocity);
    const double a = dot(relativeVelocity, relativeVelocity);
    const double b = 2.0 * dot(relativePosition, relativeVelocity);
    const double c = dot(relativePosition, relativePosition) -
        combinedRadiusCm * combinedRadiusCm;
    const double distance = std::sqrt(std::max(0.0,
        dot(relativePosition, relativePosition)));

    double toi = 0.0;
    if (c <= 0.0) {
        result.penetrationCm = std::max(0.0, combinedRadiusCm - distance);
    } else {
        if (a <= 1e-18 || b >= 0.0) return result;
        const double discriminant = b * b - 4.0 * a * c;
        if (discriminant < -1e-10) return result;
        toi = (-b - std::sqrt(std::max(0.0, discriminant))) / (2.0 * a);
        if (toi < -toleranceSeconds ||
            toi > maximumTimeSeconds + toleranceSeconds) return result;
        toi = std::max(0.0, std::min(maximumTimeSeconds, toi));
    }

    Point3 separation{
        static_cast<float>(relativePosition.x + relativeVelocity.x * toi),
        static_cast<float>(relativePosition.y + relativeVelocity.y * toi),
        static_cast<float>(relativePosition.z + relativeVelocity.z * toi)};
    Point3 normal = normalized(separation);
    if (firstIndex > secondIndex) {
        normal.x = -normal.x;
        normal.y = -normal.y;
        normal.z = -normal.z;
    }
    result.valid = true;
    result.timeOfImpactSeconds = toi;
    result.normal = normal;
    return result;
}

std::vector<ContinuousContactCandidate> generateBallBallCandidates(
    const GameState& state, double maximumTimeSeconds,
    double ballRadiusCm, double toleranceSeconds)
{
    std::vector<ContinuousContactCandidate> result;
    for (int first = 0; first < kBallCount; ++first) {
        if (state.balls[first].pocketed) continue;
        for (int second = first + 1; second < kBallCount; ++second) {
            if (state.balls[second].pocketed) continue;
            const ContinuousContactCandidate candidate = sweptBallBallCandidate(
                state.balls[first], first, state.balls[second], second,
                maximumTimeSeconds, 2.0 * ballRadiusCm, toleranceSeconds);
            if (candidate.valid) result.push_back(candidate);
        }
    }
    std::sort(result.begin(), result.end(), continuousContactLess);
    return result;
}

ContinuousContactCandidate boundaryContactCandidate(
    int ballIndex, int featureId, double timeOfImpactSeconds,
    const Point3& normal, PocketBoundaryEventKind event)
{
    ContinuousContactCandidate result;
    if (ballIndex < 0 || featureId < 0 || !std::isfinite(timeOfImpactSeconds) ||
        timeOfImpactSeconds < 0.0) return result;
    result.valid = true;
    result.firstBall = ballIndex;
    result.featureId = featureId;
    result.timeOfImpactSeconds = timeOfImpactSeconds;
    result.normal = normal;
    result.pocketEvent = event;
    result.kind = event == PocketBoundaryEventKind::StraightRail
        ? ContinuousContactKind::StraightRail
        : (event == PocketBoundaryEventKind::LeftJaw ||
                  event == PocketBoundaryEventKind::RightJaw
              ? ContinuousContactKind::Jaw
              : (event == PocketBoundaryEventKind::Throat
                    ? ContinuousContactKind::Throat
                    : ContinuousContactKind::Capture));
    return result;
}

bool continuousContactLess(
    const ContinuousContactCandidate& first,
    const ContinuousContactCandidate& second)
{
    return std::make_tuple(first.timeOfImpactSeconds, kindOrder(first.kind),
               first.firstBall, first.secondBall, first.featureId) <
        std::make_tuple(second.timeOfImpactSeconds, kindOrder(second.kind),
               second.firstBall, second.secondBall, second.featureId);
}

}  // namespace billiardgl
