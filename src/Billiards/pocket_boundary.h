#pragma once

#include "game_state.h"
#include "physics_profile.h"
#include "table_specs.h"

#include <array>

namespace billiardgl {

enum class PocketBoundaryRegion {
    Outside,
    Approaching,
    Throat,
    Capture,
    Solid
};

enum class PocketBoundaryEventKind {
    None,
    StraightRail,
    LeftJaw,
    RightJaw,
    Throat,
    Capture,
    Ambiguous
};

struct PocketBoundaryFrame {
    int pocketId = -1;
    PocketKind kind = PocketKind::Corner;
    Point3 mouthCenter;
    Point3 inward;
    Point3 tangent;
    float mouthWidthCm = 0.0f;
    float throatWidthCm = 0.0f;
    float jawRadiusCm = 0.0f;
    float throatDepthCm = 0.0f;
    float captureDepthCm = 0.0f;
};

struct PocketLocalPoint {
    double depthCm = 0.0;
    double offsetCm = 0.0;
};

struct PocketBoundaryQuery {
    PocketBoundaryRegion region = PocketBoundaryRegion::Outside;
    PocketLocalPoint local;
    bool passable = false;
    double availableWidthCm = 0.0;
    double throatSignedDistanceCm = 0.0;
    double captureSignedDistanceCm = 0.0;
};

struct PocketBoundaryEvent {
    PocketBoundaryEventKind kind = PocketBoundaryEventKind::None;
    int pocketId = -1;
    double fraction = 1.0;
    Point3 position;
    Point3 inwardNormal;
    PocketLocalPoint local;
    bool passable = false;
};

struct PocketTransitionResult {
    PocketInteractionPhase previous = PocketInteractionPhase::Outside;
    PocketInteractionPhase current = PocketInteractionPhase::Outside;
    bool changed = false;
    bool captureEmitted = false;
};

std::array<PocketBoundaryFrame, 6> buildPocketBoundaryFrames(
    const TableBoundaryProperties& boundary);
PocketLocalPoint pocketLocalPoint(
    const PocketBoundaryFrame& frame, const Point3& position);
PocketBoundaryQuery classifyPocketPoint(
    const PocketBoundaryFrame& frame, const Point3& position,
    double ballRadiusCm);
PocketBoundaryEvent sweepPocketBoundary(
    const PocketBoundaryFrame& frame, const Point3& start,
    const Point3& end, double ballRadiusCm);
PocketTransitionResult advancePocketInteraction(
    PocketInteractionState& state, int pocketId,
    PocketBoundaryEventKind event, PocketBoundaryRegion region,
    unsigned long long captureSequence);
const char* pocketBoundaryEventKindName(PocketBoundaryEventKind kind);
const char* pocketBoundaryRegionName(PocketBoundaryRegion region);
const char* pocketInteractionPhaseName(PocketInteractionPhase phase);

}  // namespace billiardgl
