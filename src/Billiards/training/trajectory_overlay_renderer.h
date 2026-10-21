#pragma once

#include "presentation_snapshot.h"
#include "training/trajectory_compound_visibility.h"
#include "training/trajectory_overlay_model.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace billiardgl::training {

enum class TrajectoryOverlayPalette {
    Standard,
    ColorVisionSafe,
};

struct TrajectoryOverlayViewport {
    int physicalWidth = 0;
    int physicalHeight = 0;
    double deviceScale = 1.0;
    double uiScale = 1.0;
};

struct TrajectoryOverlayStyle {
    double lineWidthPx = 0.0;
    double dashOnPx = 0.0;
    double dashOffPx = 0.0;
    double actualTickLengthPx = 0.0;
    double actualTickSpacingPx = 0.0;
    double endpointOuterDiameterPx = 0.0;
    double endpointRingStrokePx = 0.0;
    double actualEndpointDotDiameterPx = 0.0;
    double haloWidthPx = 1.0;
    // Visual-only exterior band. It is emitted inside the existing halo
    // semantic batch, so source probe and ownership identities do not change.
    double outerContrastRimWidthPx = 0.0;
    // Visual-only cap growth for predicted dash cores. This adds unique
    // covered samples without moving the authoritative route centerline.
    double predictedCoreCapExtensionPx = 0.0;
    // Reserved for profile experiments; the shipping correction gate keeps
    // both tracks on the authoritative centerline and separates them by
    // dash/tick/endpoint topology plus deterministic compositing order.
    double predictedLateralOffsetPx = 0.0;
    double actualLateralOffsetPx = 0.0;
    // Visual-only overlap at internal actual-track joins. It extends neither
    // route endpoint and never changes the core's normal envelope.
    double actualInternalJoinOverlapPx = 0.0;
};

struct TrajectoryOverlayVisualContrastContract {
    std::array<float, 4> predictedOuterRimLinearRgba{{8.0f, 8.0f, 8.0f, 1.0f}};
    std::array<float, 4> predictedInnerHaloLinearRgba{{0.0f, 0.0f, 0.0f, 1.0f}};
    std::array<float, 4> actualOuterRimLinearRgba{{0.0f, 0.0f, 0.0f, 1.0f}};
    std::array<float, 4> actualInnerHaloLinearRgba{{8.0f, 8.0f, 8.0f, 1.0f}};
    // Calibrated production resolve/blend/ACES/sRGB outputs. These values are
    // also present in the frozen v8 integrated final-ownership telemetry.
    std::array<std::uint8_t, 3> predictedOuterRimFinalSrgb8{{255, 255, 255}};
    std::array<std::uint8_t, 3> predictedCoreFinalSrgb8{{255, 253, 112}};
    std::array<std::uint8_t, 3> predictedInnerHaloFinalSrgb8{{0, 0, 0}};
    std::array<std::uint8_t, 3> actualOuterRimFinalSrgb8{{0, 0, 0}};
    std::array<std::uint8_t, 3> actualCoreFinalSrgb8{{2, 91, 141}};
    std::array<std::uint8_t, 3> actualInnerHaloFinalSrgb8{{255, 255, 255}};
    std::array<std::uint8_t, 3> adaptiveOuterBlackFinalSrgb8{{0, 0, 0}};
    std::array<std::uint8_t, 3> adaptiveOuterWhiteFinalSrgb8{{255, 255, 255}};
};

enum class TrajectoryVisualSublayer : std::uint32_t {
    OuterRim = 0,
    InnerHalo = 1,
    Core = 2,
};

enum class TrajectoryAdaptiveOuterRimPolarity {
    Suppressed,
    Black,
    White,
};

struct TrajectoryAdaptiveOuterRimDecision {
    TrajectoryAdaptiveOuterRimPolarity polarity =
        TrajectoryAdaptiveOuterRimPolarity::Suppressed;
    double innerHaloContrastRatio = 0.0;
    double selectedOuterContrastRatio = 0.0;
};

struct TrajectoryOverlayVertex {
    float xPhysicalPx = 0.0f;
    float yPhysicalPx = 0.0f;
    std::array<float, 4> linearRgba{{1.0f, 1.0f, 1.0f, 1.0f}};
    // Capture-only semantic value. The production color shader never reads it;
    // the optional ownership MRT consumes the same single draw submission.
    std::uint32_t finalOwnershipToken = 0;
    // Visual provenance is deliberately subordinate to the immutable
    // track/component/primitive ownership identity.
    std::uint32_t visualSublayer =
        static_cast<std::uint32_t>(TrajectoryVisualSublayer::Core);
    std::uint32_t adaptiveInnerHaloWhite = 0;
};

struct TrajectoryOverlayTrackDiagnostics {
    std::size_t sourceSampleCount = 0;
    std::size_t projectedSegmentCount = 0;
    std::size_t visibleStrokeCount = 0;
    std::size_t tickMarkCount = 0;
    std::size_t vertexCount = 0;
    bool endpointMarkerVisible = false;
    bool endpointOffscreen = false;
    double projectedArcLengthPx = 0.0;
    double lateralOffsetPx = 0.0;
};

enum class TrajectoryContrastTrack { Predicted, Actual };

struct TrajectoryContrastProbe {
    TrajectoryContrastTrack track;
    int strokeXPhysicalPx = 0;
    int strokeYPhysicalPx = 0;
    int backgroundXPhysicalPx = 0;
    int backgroundYPhysicalPx = 0;
    int haloXPhysicalPx = 0;
    int haloYPhysicalPx = 0;
    std::string primitiveId;
    std::string arcSegmentId;
    CompoundComponentRole componentRole = CompoundComponentRole::Core;
    std::uint64_t probeOrdinal = 0;
    double localTangentX = 1.0;
    double localTangentY = 0.0;
    double localNormalX = 0.0;
    double localNormalY = 1.0;
};

struct TrajectorySemanticBatch {
    TrajectoryContrastTrack track = TrajectoryContrastTrack::Predicted;
    CompoundComponentRole componentRole = CompoundComponentRole::Core;
    std::string primitiveId;
    std::string arcSegmentId;
    std::size_t firstVertex = 0;
    std::size_t vertexCount = 0;
    std::uint32_t finalOwnershipToken = 0;
    TrajectoryVisualSublayer visualSublayer =
        TrajectoryVisualSublayer::Core;
};

struct TrajectoryOverlayDrawPackage {
    bool ok = false;
    std::string errorCode;
    TrajectoryOverlayStyle style;
    std::vector<TrajectoryOverlayVertex> vertices;
    TrajectoryOverlayTrackDiagnostics predicted;
    TrajectoryOverlayTrackDiagnostics actual;
    std::size_t additionalDrawCalls = 0;
    std::size_t newTextureCount = 0;
    std::size_t textureResidencyDeltaBytes = 0;
    bool usedSharedCameraProjection = false;
    bool homogeneousClipApplied = false;
    bool safeAreaClipApplied = false;
    bool playSurfaceStencilApplied = false;
    bool advancedGeometryEmitted = false;
    bool nonColorTopologySeparationApplied = false;
    std::vector<TrajectoryContrastProbe> contrastProbes;
    // Capture-only semantic ranges. They reference the exact production
    // triangle stream and never alter vertices, colors, order, or draw count.
    std::vector<TrajectorySemanticBatch> semanticBatches;
};

struct TrajectoryContrastSample {
    TrajectoryContrastTrack track = TrajectoryContrastTrack::Predicted;
    int xPhysicalPx = 0;
    int yPhysicalPx = 0;
    std::array<std::uint8_t, 3> strokeSrgb8{{0, 0, 0}};
    std::array<std::uint8_t, 3> localBackgroundSrgb8{{0, 0, 0}};
    std::array<std::uint8_t, 3> haloSrgb8{{0, 0, 0}};
    bool haloSampleValid = false;
};

struct TrajectoryContrastAudit {
    bool ok = false;
    std::string errorCode;
    double minimumLocalContrastRatio = 0.0;
    double p01LocalContrastRatio = 0.0;
    double designTargetCoverage = 0.0;
    double predictedActualLumaDeltaSrgb8 = 0.0;
    std::size_t sourceProbeCount = 0;
    std::size_t visibleProbeCount = 0;
    std::size_t hiddenProbeCount = 0;
    double visibleProbeCoverage = 0.0;
    std::vector<std::array<int, 2>> failureCoordinates;
};

TrajectoryContrastAudit auditTrajectoryContrastSamples(
    const std::vector<TrajectoryContrastSample>& samples,
    const std::array<std::uint8_t, 3>& predictedRoleSrgb8,
    const std::array<std::uint8_t, 3>& actualRoleSrgb8,
    TrajectoryOverlayPalette palette,
    bool predictedRequired = true,
    bool actualRequired = true);

TrajectoryOverlayStyle resolveTrajectoryOverlayStyle(
    const TrajectoryOverlayViewport& viewport);

TrajectoryOverlayVisualContrastContract trajectoryOverlayVisualContrastContract();

double trajectoryOverlayFinalSrgbContrastRatio(
    const std::array<std::uint8_t, 3>& first,
    const std::array<std::uint8_t, 3>& second);

const char* trajectoryVisualSublayerName(TrajectoryVisualSublayer sublayer);

TrajectoryAdaptiveOuterRimDecision selectTrajectoryAdaptiveOuterRim(
    const std::array<std::uint8_t, 3>& backdropFinalSrgb8,
    const std::array<std::uint8_t, 3>& innerHaloFinalSrgb8,
    double minimumContrastRatio = 4.0,
    bool priorTrackCoverage = false);

TrajectoryOverlayDrawPackage buildTrajectoryOverlayDrawPackage(
    const TrajectoryOverlaySnapshot& snapshot,
    const CameraPresentation& camera,
    const TrajectoryOverlayViewport& viewport,
    TrajectoryOverlayPalette palette = TrajectoryOverlayPalette::Standard);

}  // namespace billiardgl::training
