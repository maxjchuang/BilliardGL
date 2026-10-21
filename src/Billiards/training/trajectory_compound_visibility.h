#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <map>
#include <string>
#include <vector>

namespace billiardgl::training {

enum class CompoundVisibilityTrack { Predicted, Actual };

enum class CompoundComponentRole {
    Core,
    OuterHaloEdge,
    SolidInPredictedDashGap,
    Tick,
    Endpoint,
};

enum class CompoundHiddenReason {
    OutOfBoundsOrClipped,
    NoSameTrackFinalOwner,
    OnlyOtherTrackVisible,
    OnlyBackgroundOrSceneVisible,
    InsufficientOwnedPixelArea,
    InsufficientExclusiveOwnedArea,
    InsufficientRasterContinuity,
    OutsideBoundedNeighborhood,
    PixelAlreadyAssigned,
    FinalEdgeContrastBelow4,
    ComponentNotEligible,
    NoEligibleComponentAwareBoundary,
};

const char* compoundTrackName(CompoundVisibilityTrack track);
const char* compoundComponentRoleName(CompoundComponentRole role);
const char* compoundHiddenReasonName(CompoundHiddenReason reason);

struct CompoundSourceProbe {
    std::string captureId;
    CompoundVisibilityTrack track = CompoundVisibilityTrack::Predicted;
    std::string primitiveId;
    CompoundComponentRole componentRole = CompoundComponentRole::Core;
    std::string arcSegmentId;
    std::uint64_t probeOrdinal = 0;
    std::string physicalProfile;
    int xPhysicalPx = 0;
    int yPhysicalPx = 0;
    double outerHaloRadiusPx = 0.0;
    double halfTickLengthPx = 0.0;
    double sourceProbeSpacingPx = 0.0;
    bool insideFinalViewport = true;
};

// A candidate is emitted only by the capture-only final-composite ownership
// attachment.  It describes a resolved, physical-pixel ownership island; it
// is not an analytic/pre-composite geometry hit.
struct CompoundOwnershipCandidate {
    std::string sourceProbeId;
    int xPhysicalPx = 0;
    int yPhysicalPx = 0;
    CompoundVisibilityTrack ownerTrack = CompoundVisibilityTrack::Predicted;
    CompoundComponentRole ownerComponentRole = CompoundComponentRole::Core;
    std::string ownerPrimitiveId;
    double resolvedAlpha = 0.0;
    bool fromFinalCompositeAttachment = false;
    bool correspondsToFinalColor = false;
    bool backgroundOrSceneOwner = false;
    double normalDistancePx = 0.0;
    double longitudinalDistancePx = 0.0;
    bool sameEndpointPrimitive = true;
    bool nearestTickInterval = true;
    std::size_t fourConnectedOwnedPixelCount = 0;
    double ownedPixelEquivalentArea = 0.0;
    std::array<std::uint8_t, 3> finalFramebufferSrgb8{{0, 0, 0}};
    std::array<std::uint8_t, 3> adjacentNonSelfSrgb8{{0, 0, 0}};
    bool bordersAdjacentNonSelfPixel = false;
    double finalEdgeContrastRatio = 0.0;
};

struct CompoundVisibilityInvariantContract {
    double predictedLateralOffsetPx = 0.0;
    double actualLateralOffsetPx = 0.0;
    std::string compositingOrder = "actual_then_predicted";
    bool routeInputsUnchanged = true;
    bool worldSamplesUnchanged = true;
    bool endpointsUnchanged = true;
};

struct CompoundVisibilityAuditRequest {
    std::vector<CompoundSourceProbe> sourceProbes;
    std::vector<CompoundOwnershipCandidate> ownershipCandidates;
    CompoundVisibilityInvariantContract invariants;
    std::size_t minimumSourceProbeCount = 159;
    std::size_t expectedSourceProbeCount = 0;
    double minimumVisibleCoverage = 0.90;
    double minimumEdgeContrastRatio = 4.0;
    double designTargetContrastRatio = 4.5;
    double designTargetCoverage = 0.99;
    double predictedActualLumaDeltaSrgb8 = 0.0;
    double requiredPredictedActualLumaDeltaSrgb8 = 32.0;
    // Capture modes may intentionally contain only one semantic track.  These
    // flags make that absence explicit instead of weakening comparison gates.
    bool predictedRequired = true;
    bool actualRequired = true;
};

struct CompoundTrackVisibilityCounts {
    std::size_t source = 0;
    std::size_t visible = 0;
    std::size_t hidden = 0;
    double coverage = 0.0;
};

struct CompoundHiddenProbeRecord {
    std::string sourceProbeId;
    CompoundVisibilityTrack track = CompoundVisibilityTrack::Predicted;
    CompoundComponentRole componentRole = CompoundComponentRole::Core;
    CompoundHiddenReason reason = CompoundHiddenReason::NoSameTrackFinalOwner;
    int xPhysicalPx = 0;
    int yPhysicalPx = 0;
};

struct CompoundVisibilityAudit {
    bool ok = false;
    std::string errorCode;
    std::vector<std::string> orderedSourceProbeIds;
    std::string orderedSourceProbeDigest;
    std::vector<std::string> orderedVisibleProbeIds;
    std::string orderedVisibleProbeDigest;
    std::size_t sourceProbeCount = 0;
    std::size_t visibleProbeCount = 0;
    std::size_t hiddenProbeCount = 0;
    double visibleProbeCoverage = 0.0;
    CompoundTrackVisibilityCounts predicted;
    CompoundTrackVisibilityCounts actual;
    std::map<std::string, std::size_t> componentSourceCounts;
    std::map<std::string, std::size_t> componentVisibleCounts;
    std::map<std::string, std::size_t> hiddenReasonCounts;
    std::map<std::string, std::size_t> componentHiddenReasonCounts;
    std::vector<CompoundHiddenProbeRecord> hiddenProbes;
    std::size_t doubleAssignmentCount = 0;
    std::size_t foreignTrackEvidenceCount = 0;
    std::size_t unownedEvidenceCount = 0;
    double minimumVisibleEdgeContrastRatio = 0.0;
    double designTargetContrastCoverage = 0.0;
};

std::string stableCompoundSourceProbeId(const CompoundSourceProbe& probe);

CompoundVisibilityAudit auditCompoundFinalVisibility(
    const CompoundVisibilityAuditRequest& request);

}  // namespace billiardgl::training
