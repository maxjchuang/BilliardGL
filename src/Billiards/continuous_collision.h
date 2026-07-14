#pragma once

#include "game_state.h"
#include "pocket_boundary.h"

#include <tuple>
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
    int pocketId = -1;
    double timeOfImpactSeconds = 0.0;
    double penetrationCm = 0.0;
    Point3 normal;
    PocketBoundaryEventKind pocketEvent = PocketBoundaryEventKind::None;
};

ContinuousContactCandidate sweptBallBallCandidate(
    const BallState& first, int firstIndex,
    const BallState& second, int secondIndex,
    double maximumTimeSeconds, double combinedRadiusCm,
    double toleranceSeconds = 1e-7,
    bool includeRestingContacts = false,
    double approachSpeedEpsilonCmS = 0.001);
std::vector<ContinuousContactCandidate> generateBallBallCandidates(
    const GameState& state, double maximumTimeSeconds,
    double ballRadiusCm, double toleranceSeconds = 1e-7);
std::vector<ContinuousContactCandidate> generateBallBallCandidates(
    const GameState& state, double maximumTimeSeconds,
    double ballRadiusCm, double toleranceSeconds,
    bool includeRestingContacts,
    double approachSpeedEpsilonCmS = 0.001);
ContinuousContactCandidate boundaryContactCandidate(
    int ballIndex, int featureId, double timeOfImpactSeconds,
    const Point3& normal, PocketBoundaryEventKind event, int pocketId = -1);
bool continuousContactLess(
    const ContinuousContactCandidate& first,
    const ContinuousContactCandidate& second);
std::tuple<int, int, int, int> continuousContactStableKey(
    const ContinuousContactCandidate& candidate);

}  // namespace billiardgl
