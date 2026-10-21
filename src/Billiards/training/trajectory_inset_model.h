#pragma once

#include "training/trajectory_overlay_model.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace billiardgl::training {

enum class TrajectoryInsetPalette { Standard, ColorVisionSafe };
enum class TrajectoryInsetTrack { Predicted, Actual };
enum class TrajectoryInsetRole {
    ActualLeftHalo,
    ActualLeftCore,
    ActualRightHalo,
    ActualRightCore,
    ActualEndpointCapsule,
    ActualDirectionNotch,
    PredictedOuterRim,
    PredictedInnerHalo,
    PredictedDashCore,
    PredictedEndpointDiamond,
    PredictedEventVisitMarker,
    PredictedEventVisitMarkerHalo,
    PredictedEventVisitLeader,
};

struct TrajectoryInsetRect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct TrajectoryInsetLayout {
    std::string profileId;
    int physicalWidth = 0;
    int physicalHeight = 0;
    float uiScale = 1.0f;
    TrajectoryInsetRect panelRect;
    TrajectoryInsetRect plotRect;
};

struct TrajectoryInsetVertex {
    float xPhysicalPx = 0.0f;
    float yPhysicalPx = 0.0f;
    std::array<float, 4> linearRgba{{1.0f, 1.0f, 1.0f, 1.0f}};
    std::uint32_t insetOwnershipToken = 0;
};

struct TrajectoryInsetSemanticBatch {
    TrajectoryInsetTrack track = TrajectoryInsetTrack::Predicted;
    TrajectoryInsetRole role = TrajectoryInsetRole::PredictedDashCore;
    std::string parentSourceKey;
    std::string primitiveId;
    std::string arcSegmentId;
    std::string insetMemberKey;
    std::size_t firstVertex = 0;
    std::size_t vertexCount = 0;
    std::uint32_t insetOwnershipToken = 0;
};

struct TrajectoryInsetSourceMappingRow {
    std::string parentSourceKey;
    std::vector<std::string> orderedWorldMemberKeys;
    std::string worldNotEmittedReason;
    std::vector<std::string> orderedInsetMemberKeys;
    struct PredictedRouteInterval {
        std::string arcSegmentId;
        std::size_t routeOrderOrdinal = 0;
        std::size_t routeSpanOrdinal = 0;
        double sourceLocalTStart = 0.0;
        double sourceLocalTEnd = 0.0;
        double routeDistanceStartPx = 0.0;
        double routeDistanceEndPx = 0.0;
        std::string dashEventKey;
        std::string nonRenderedReason;
    };
    std::vector<PredictedRouteInterval> orderedPredictedRouteIntervals;
};

struct TrajectoryInsetDashEventContributor {
    std::string parentSourceKey;
    std::string arcSegmentId;
    std::size_t routeOrderOrdinal = 0;
    double sourceLocalTStart = 0.0;
    double sourceLocalTEnd = 0.0;
    double eventRouteDistanceStartPx = 0.0;
    double eventRouteDistanceEndPx = 0.0;
};

struct TrajectoryInsetDashEvent {
    std::string eventKey;
    std::size_t routeWideDashEventOrdinal = 0;
    std::size_t routeSpanOrdinal = 0;
    double routeDistanceStartPx = 0.0;
    double routeDistanceEndPx = 0.0;
    std::vector<TrajectoryInsetDashEventContributor> orderedContributors;
    std::array<std::string, 3> orderedRoleMemberKeys;
    std::array<std::uint32_t, 3> orderedRoleTokens{{0, 0, 0}};
};

struct InsetTrajectoryProbe {
    std::string probeId;
    std::string parentSourceKey;
    std::string insetMemberKey;
    std::string companionInsetMemberKey;
    std::uint32_t insetOwnershipToken = 0;
    int strokeXPhysicalPx = 0;
    int strokeYPhysicalPx = 0;
    int companionXPhysicalPx = 0;
    int companionYPhysicalPx = 0;
    int backgroundXPhysicalPx = 0;
    int backgroundYPhysicalPx = 0;
    // Event identity remains the original core even when its visible evidence
    // is an explicit visit marker. Hidden core is never relabelled visible.
    std::string sourceCoreMemberKey;
    bool sourceCoreVisible = true;
    std::string evidenceRole;
    std::string visitLabel;
    std::uint64_t sourceFirstTick = 0;
    std::uint64_t sourceLastTick = 0;
    std::array<float, 2> sourceAnchor{{0,0}};
    TrajectoryInsetRect visitMarkerRect;
};

// Optional suffix for the existing ordered-probe ledger. Empty for legacy
// core-only probes, so independent consumers share one exact encoding.
std::string trajectoryInsetProbeSourceEvidenceLedger(const InsetTrajectoryProbe& probe);

struct TrajectoryInsetRailMidpointWitness {
    std::string parentSourceKey;
    std::array<float, 2> canonicalFirst{{0.0f, 0.0f}};
    std::array<float, 2> canonicalSecond{{0.0f, 0.0f}};
    std::array<float, 2> leftFirst{{0.0f, 0.0f}};
    std::array<float, 2> leftSecond{{0.0f, 0.0f}};
    std::array<float, 2> rightFirst{{0.0f, 0.0f}};
    std::array<float, 2> rightSecond{{0.0f, 0.0f}};
};

struct TrajectoryInsetBuildRequest {
    std::string generationId;
    TrajectoryOverlaySnapshot snapshot;
    TrajectoryInsetLayout layout;
    TrajectoryInsetPalette palette = TrajectoryInsetPalette::Standard;
    // V19 defaults the main-world overlay off. A later diagnostic world pass
    // may provide exact members, but it may never affect inset eligibility.
    std::string worldNotEmittedReason = "world_overlay_default_off";
    struct WorldMappingInput {
        std::string parentSourceKey;
        std::vector<std::string> orderedWorldMemberKeys;
        std::string worldNotEmittedReason;
    };
    // Empty means the default world overlay is off. A diagnostic world pass
    // supplies an exact row for every parent; partial/foreign rows fail closed.
    std::vector<WorldMappingInput> worldMappings;
    std::vector<std::uint32_t> forbiddenWorldOwnershipTokens;
};

struct TrajectoryInsetDrawPackage {
    bool ok = false;
    std::string errorCode;
    std::string probeResolutionFailureMember;
    std::string generationId;
    TrajectoryInsetLayout layout;
    TrajectoryInsetRect canonicalTableRect;
    TrajectoryInsetPalette palette = TrajectoryInsetPalette::Standard;
    std::vector<TrajectoryInsetVertex> vertices;
    std::vector<TrajectoryInsetSemanticBatch> semanticBatches;
    std::vector<TrajectoryInsetSourceMappingRow> mappingRows;
    std::vector<TrajectoryInsetDashEvent> predictedDashEvents;
    std::vector<InsetTrajectoryProbe> probes;
    std::vector<TrajectoryInsetRailMidpointWitness> actualMidpointWitnesses;
    std::vector<std::string> orderedParentSourceKeys;
    std::string orderedParentSourceDigest;
    std::vector<std::string> orderedProbeIds;
    std::string orderedProbeDigest;
    std::string orderedDashEventDigest;
    std::string orderedDashContributorDigest;
    std::string orderedSourceRouteIntervalDigest;
    std::size_t predictedRouteSpanCount = 0;
    std::size_t sourceGroupDenominator = 0;
    std::size_t semanticMemberDenominator = 0;
    std::size_t probeDenominator = 0;
    bool cameraIndependent = false;
    bool fixedPanelBackground = false;
    bool worldVisibilityIgnored = false;
    std::array<std::string, 2> orientationLabels{{
        "HEAD / 开球端", "FOOT / 底端"}};
    bool orientationLabelsExcludedFromDenominator = false;
};

const char* trajectoryInsetRoleName(TrajectoryInsetRole role);
const char* trajectoryInsetTrackName(TrajectoryInsetTrack track);

}  // namespace billiardgl::training
