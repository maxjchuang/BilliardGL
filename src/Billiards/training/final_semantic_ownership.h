#pragma once

#include "training/phase4_final_ownership_instrumentation.h"
#include "training/trajectory_overlay_renderer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace billiardgl::training {

enum class FinalSemanticTrack { None, Predicted, Actual };

struct FinalSemanticOwner {
    std::uint32_t token = 0;
    FinalSemanticTrack track = FinalSemanticTrack::None;
    CompoundComponentRole componentRole = CompoundComponentRole::Core;
    std::string primitiveId;
    std::uint64_t productionDrawOrdinal = 0;
    // Visual-only provenance beneath the immutable semantic owner identity.
    std::string visualSublayer = "core";
};

struct FinalSemanticPixel {
    int xPhysicalPx = 0;
    int yPhysicalPx = 0;
    // One token for every production MSAA sample. Zero means scene/background.
    std::vector<std::uint32_t> sampleOwnerTokens;
    double laterUiCoverage = 0.0;
    std::array<std::uint8_t, 3> finalLdrSrgb8{{0, 0, 0}};
    std::array<std::uint8_t, 3> adjacentNonSelfLdrSrgb8{{0, 0, 0}};
    // Native producer supplies a separate chrome-only signal. Older CPU
    // fixtures conservatively treat all later UI as chrome when absent.
    std::optional<double> interactiveChromeCoverage;
};

struct FinalOwnershipRenderGraphContract {
    std::vector<std::string> passSequence;
    bool exactProductionVertexStream = false;
    bool exactProductionTransforms = false;
    bool actualThenPredictedSubmission = false;
    bool homogeneousClipShared = false;
    bool safeAreaClipShared = false;
    bool playSurfaceStencilShared = false;
    bool exactMsaaSampleCountAndIndices = false;
    bool blendAndLaterReplacementApplied = false;
    bool semanticResolveBeforeToneMapPairing = false;
    bool laterProductUiCoverageApplied = false;
    bool colorAndSemanticReadbackPaired = false;
    bool instrumentationColorWritesDisabled = false;
};

struct IntegratedFinalOwnershipFrame {
    std::string captureId;
    std::string colorFrameId;
    std::string semanticFrameId;
    std::string colorContentSha256;
    std::string semanticContentSha256;
    int physicalWidth = 0;
    int physicalHeight = 0;
    int msaaSamples = 0;
    std::vector<std::array<float, 2>> msaaSamplePositions;
    std::map<std::uint32_t, FinalSemanticOwner> ownerTable;
    std::vector<FinalSemanticPixel> pixels;
    FinalOwnershipRenderGraphContract renderGraph;
};

struct FinalOwnershipSourceProbe {
    CompoundSourceProbe source;
    // Geometry audits may retain the exact authored center separately from
    // the immutable integer contrast-sampling location.
    std::optional<std::array<double, 2>> geometryCenter;
    // Orthonormal, unshifted projected path frame.
    double tangentX = 1.0;
    double tangentY = 0.0;
    double normalX = 0.0;
    double normalY = 1.0;
};

struct IntegratedFinalOwnershipRequest {
    IntegratedFinalOwnershipFrame frame;
    std::vector<FinalOwnershipSourceProbe> sourceProbes;
    std::size_t expectedSourceProbeCount = 95;
    double minimumVisibleCoverage = 0.90;
    double minimumEdgeContrastRatio = 4.0;
    double minimumResolvedPixelEquivalentArea = 1.5;
    std::size_t minimumRasterConnectedPhysicalPixels = 2;
};

struct IntegratedFinalOwnershipResult {
    bool ok = false;
    std::string errorCode;
    CompoundVisibilityAudit audit;
    std::vector<FinalOwnershipCandidateTelemetry> candidateTelemetry;
    std::vector<FinalOwnershipArbitrationTelemetry> arbitrationTelemetry;
    std::vector<FinalOwnershipBoundaryTelemetry> boundaryTelemetry;
};

const char* finalSemanticTrackName(FinalSemanticTrack track);

IntegratedFinalOwnershipResult resolveIntegratedFinalSemanticOwnership(
    const IntegratedFinalOwnershipRequest& request);

// Geometry is measured from the exact per-sample final owner attachment, not
// from the authored vertices.  This complements visibility/contrast: it proves
// the surviving compound stroke width, endpoint size, actual-tick wings, and
// absence of partially transparent HUD leakage in the final composite.
struct FinalTrajectoryGeometryProbeAudit {
    std::string sourceProbeId;
    CompoundComponentRole componentRole = CompoundComponentRole::Core;
    double measuredNormalWidthPx = 0.0;
    double expectedNormalWidthPx = 0.0;
    double measuredTangentSpanPx = 0.0;
    double expectedTangentSpanPx = 0.0;
    double measuredGuaranteedAreaPx2 = 0.0;
    std::string centerOwnerTrack;
    std::string centerOwnerComponent;
    std::string centerOwnerPrimitiveId;
    bool required = false;
    bool guaranteedConnected = false;
    bool normalWidthPassed = false;
    bool tangentSpanPassed = false;
    bool shapePassed = false;
    bool passed = false;
};

struct FinalTrajectoryGeometryAudit {
    bool ok = false;
    std::string errorCode;
    std::size_t partialUiOverlapPixelCount = 0;
    std::size_t fullyUiCoveredTrajectoryPixelCount = 0;
    std::vector<FinalTrajectoryGeometryProbeAudit> probes;
};

FinalTrajectoryGeometryAudit auditIntegratedFinalTrajectoryGeometry(
    const IntegratedFinalOwnershipFrame& frame,
    const std::vector<FinalOwnershipSourceProbe>& sourceProbes,
    const TrajectoryOverlayStyle& style, double tolerancePx = 1.0);

}  // namespace billiardgl::training
