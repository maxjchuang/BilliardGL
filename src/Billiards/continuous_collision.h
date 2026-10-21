#pragma once

#include "game_state.h"
#include "pocket_boundary.h"

#include <tuple>
#include <vector>

namespace billiardgl {

enum class ContinuousContactKind {
    BallBall,
    SupportPlane,
    StraightRail,
    Jaw,
    Mouth,
    Throat,
    Capture
};

// A supported ball striking a cushion uses the sealed elevated-nose arm from
// cushion_collision_v1. An airborne ball reaches the same vertical boundary
// through the 3D table-departure query, but its instantaneous contact normal
// is the sphere/plane normal: the arm is radial and parallel to that normal.
// Keeping this explicit prevents spin at an unrelated elevated supported-ball
// arm from turning an approaching airborne impact into a separating contact.
enum class BoundaryContactGeometry {
    SupportedCushion,
    AirborneCushionNose,
    // Sphere against the rounded upper inner edge, with an upward component.
    RoundedUpperRailEdge,
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
    double supportPlaneCenterYCm = 0.0;
    Point3 normal;
    BoundaryContactGeometry boundaryGeometry =
        BoundaryContactGeometry::SupportedCushion;
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
