#pragma once

#include "game_state.h"
#include "pocket_boundary.h"

#include <vector>

namespace billiardgl {

enum class ContinuousContactKind {
    BallBall,
    StraightRail,
    Jaw,
    Throat,
    Capture
};

struct ContinuousContactCandidate {
    bool valid = false;
    ContinuousContactKind kind = ContinuousContactKind::BallBall;
    int firstBall = -1;
    int secondBall = -1;
    int featureId = -1;
    double timeOfImpactSeconds = 0.0;
    double penetrationCm = 0.0;
    Point3 normal;
    PocketBoundaryEventKind pocketEvent = PocketBoundaryEventKind::None;
};

ContinuousContactCandidate sweptBallBallCandidate(
    const BallState& first, int firstIndex,
    const BallState& second, int secondIndex,
    double maximumTimeSeconds, double combinedRadiusCm,
    double toleranceSeconds = 1e-7);
std::vector<ContinuousContactCandidate> generateBallBallCandidates(
    const GameState& state, double maximumTimeSeconds,
    double ballRadiusCm, double toleranceSeconds = 1e-7);
ContinuousContactCandidate boundaryContactCandidate(
    int ballIndex, int featureId, double timeOfImpactSeconds,
    const Point3& normal, PocketBoundaryEventKind event);
bool continuousContactLess(
    const ContinuousContactCandidate& first,
    const ContinuousContactCandidate& second);

}  // namespace billiardgl
