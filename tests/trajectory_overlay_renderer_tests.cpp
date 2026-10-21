#include "render/modern/camera_projection.h"
#include "training/trajectory_overlay_renderer.h"

#include <cmath>
#include <array>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

void expect(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error(message);
}

billiardgl::CameraPresentation overheadCamera()
{
    billiardgl::CameraPresentation camera;
    camera.eye = {0.0f, 520.0f, 0.0f};
    camera.target = {0.0f, 86.5f, 0.0f};
    camera.fieldOfViewDegrees = 48.0f;
    return camera;
}

billiardgl::CameraPresentation orbitCamera(
    float yaw, float pitchDegrees, float distance, float fov)
{
    constexpr float radians = 0.01745329251994329577f;
    billiardgl::CameraPresentation camera;
    camera.target = {0.0f, 86.5f, 0.0f};
    const float pitch = pitchDegrees * radians;
    const float horizontal = std::cos(pitch) * distance;
    camera.eye = {std::cos(yaw) * horizontal,
        camera.target.y + std::sin(pitch) * distance,
        std::sin(yaw) * horizontal};
    camera.fieldOfViewDegrees = fov;
    return camera;
}

billiardgl::training::TrajectoryOverlaySnapshot overlayFixture()
{
    using namespace billiardgl;
    using namespace billiardgl::training;
    TrajectoryOverlayModel model;
    PreviewResult preview;
    preview.ok = true;
    preview.railCollisionEvents = 2;
    for (std::uint64_t tick = 1; tick <= 20; ++tick) {
        PreviewSample sample;
        sample.tick = tick;
        sample.cueBallVisible = true;
        sample.cueBallPosition = {
            -55.0f + static_cast<float>(tick) * 5.0f, 86.5f, -25.0f};
        preview.samples.push_back(sample);
    }
    model.setPredicted(preview);
    replay::ReplayTransportSnapshot replay;
    replay.loaded = true;
    replay.railCollisionEvents = 1;
    for (std::uint64_t tick = 1; tick <= 20; ++tick)
        replay.cueTrajectory.push_back({tick,
            {-45.0f + static_cast<float>(tick) * 4.0f, 86.5f, 25.0f}});
    replay.cueEndpointVisible = true;
    replay.cueEndpoint = replay.cueTrajectory.back().position;
    model.setActual(replay);
    return model.snapshot();
}

billiardgl::training::TrajectoryOverlaySnapshot directionalActualFixture(
    float stepX, float stepZ)
{
    using namespace billiardgl;
    using namespace billiardgl::training;
    TrajectoryOverlaySnapshot snapshot = overlayFixture();
    snapshot.predicted.visible = false;
    snapshot.predicted.endpointVisible = false;
    for (std::size_t index = 0; index < snapshot.actual.samples.size(); ++index)
        snapshot.actual.samples[index].position = {
            -20.0f + stepX * static_cast<float>(index), 86.5f,
            -40.0f + stepZ * static_cast<float>(index)};
    snapshot.actual.endpoint = snapshot.actual.samples.back().position;
    return snapshot;
}

std::string probeIdentityDigest(
    const std::vector<billiardgl::training::TrajectoryContrastProbe>& probes)
{
    std::uint64_t hash = 1469598103934665603ull;
    auto append = [&](const std::string& text) {
        for (const unsigned char value : text) {
            hash ^= value;
            hash *= 1099511628211ull;
        }
        hash ^= 0xffu;
        hash *= 1099511628211ull;
    };
    for (const auto& probe : probes) {
        append(probe.primitiveId);
        append(probe.arcSegmentId);
        append(std::to_string(static_cast<int>(probe.componentRole)));
        append(std::to_string(probe.probeOrdinal));
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

std::string vertexByteDigest(
    const std::vector<billiardgl::training::TrajectoryOverlayVertex>& vertices)
{
    static_assert(sizeof(billiardgl::training::TrajectoryOverlayVertex) == 36,
        "GPU trajectory vertex byte layout changed");
    std::uint64_t hash = 1469598103934665603ull;
    for (const auto& vertex : vertices) {
        const auto* bytes = reinterpret_cast<const unsigned char*>(&vertex);
        for (std::size_t offset = 0; offset < sizeof(vertex); ++offset) {
            hash ^= bytes[offset];
            hash *= 1099511628211ull;
        }
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

void testStylePhysicalScaling()
{
    using namespace billiardgl::training;
    const TrajectoryOverlayStyle hd = resolveTrajectoryOverlayStyle(
        {1280, 720, 1.0, 1.0});
    const TrajectoryOverlayStyle full = resolveTrajectoryOverlayStyle(
        {1920, 1080, 1.0, 1.0});
    const TrajectoryOverlayStyle retina = resolveTrajectoryOverlayStyle(
        {2560, 1440, 2.0, 1.35});
    const TrajectoryOverlayStyle compact = resolveTrajectoryOverlayStyle(
        {1280, 720, 1.0, 1.35});
    const TrajectoryOverlayStyle retina1080 = resolveTrajectoryOverlayStyle(
        {1920, 1080, 1.5, 1.35});
    expect(std::fabs(hd.lineWidthPx - 3.0) < 1e-9,
        "generic 720p physical line width remains scale-bound");
    expect(std::fabs(full.lineWidthPx - 4.5) < 1e-9 &&
        std::fabs(full.dashOnPx - 18.0) < 1e-9 &&
        std::fabs(full.dashOffPx - 12.0) < 1e-9 &&
        std::fabs(full.haloWidthPx - 2.0) < 1e-9 &&
        std::fabs(full.outerContrastRimWidthPx - 2.0) < 1e-9 &&
        std::fabs(full.actualInternalJoinOverlapPx - 0.75) < 1e-9,
        "1080p UI-100 profile resolves the exact v3 physical stroke contract");
    expect(std::fabs(compact.lineWidthPx - 4.5) < 1e-9 &&
        std::fabs(compact.actualTickSpacingPx - 42.0) < 1e-9 &&
        std::fabs(compact.endpointOuterDiameterPx - 24.0) < 1e-9 &&
        std::fabs(compact.actualEndpointDotDiameterPx - 11.0) < 1e-9,
        "720p UI-135 profile resolves the exact CVD geometry contract");
    expect(std::fabs(retina1080.lineWidthPx - 6.5) < 1e-9 &&
        std::fabs(retina1080.dashOnPx - 24.0) < 1e-9 &&
        std::fabs(retina1080.actualTickLengthPx - 25.6) < 1e-9 &&
        std::fabs(retina1080.haloWidthPx - 3.0) < 1e-9,
        "Retina UI-135 profile resolves the exact enlarged physical contract");
    expect(retina.endpointOuterDiameterPx > full.endpointOuterDiameterPx,
        "retina plus UI scale enlarges the endpoint marker");
    for (const auto& style : {hd, full, retina, compact, retina1080})
        expect((style.actualTickLengthPx - style.lineWidthPx -
                    2.0 * (style.haloWidthPx + style.outerContrastRimWidthPx)) * 0.5 >= 2.0 - 1e-9,
            "actual marker retains at least two physical pixels per wing at 1x and 2x");
    expect(resolveTrajectoryOverlayStyle({1920, 1080, 1.0, 1.36})
            .lineWidthPx == 0.0,
        "out-of-contract UI scale fails closed");
}

void testProjectionAndEncodingContract()
{
    using namespace billiardgl;
    using namespace billiardgl::training;
    const CameraPresentation camera = overheadCamera();
    const TrajectoryOverlaySnapshot snapshot = overlayFixture();
    const TrajectoryOverlayDrawPackage package =
        buildTrajectoryOverlayDrawPackage(snapshot, camera,
            {1920, 1080, 1.0, 1.0});
    expect(package.ok, "valid overlay package builds");
    expect(package.usedSharedCameraProjection &&
        package.homogeneousClipApplied && package.safeAreaClipApplied &&
        package.playSurfaceStencilApplied,
        "all projection and clipping contracts are recorded");
    expect(package.predicted.visibleStrokeCount >= 3 &&
        package.predicted.endpointMarkerVisible,
        "predicted trajectory has dashed strokes and ring endpoint");
    expect(package.actual.visibleStrokeCount > 0 &&
        package.actual.tickMarkCount > 0 &&
        package.actual.endpointMarkerVisible,
        "actual trajectory has solid strokes, perpendicular ticks and dot");
    expect(package.predicted.vertexCount <= 8192 &&
        package.actual.vertexCount <= 8192,
        "each track stays inside the vertex budget");
    expect(package.nonColorTopologySeparationApplied &&
        package.predicted.lateralOffsetPx == 0.0 &&
        package.actual.lateralOffsetPx == 0.0,
        "coincident centerlines remain authoritative while dash/tick/endpoint topology separates tracks without color");
    expect(package.additionalDrawCalls == 2 &&
        package.newTextureCount == 0 &&
        package.textureResidencyDeltaBytes == 0,
        "actual and predicted retain z-order as two backdrop-aware ranges with no package-owned textures");
    std::size_t semanticVertexCount = 0;
    for (const auto& batch : package.semanticBatches) {
        expect(!batch.primitiveId.empty() && !batch.arcSegmentId.empty() &&
            batch.firstVertex + batch.vertexCount <= package.vertices.size(),
            "capture-only semantic batch binds an exact production range");
        semanticVertexCount += batch.vertexCount;
    }
    expect(semanticVertexCount == package.vertices.size() &&
        package.contrastProbes.size() > 0,
        "semantic attachment metadata covers the exact draw stream without changing it");
    for (std::size_t index = 0; index < package.contrastProbes.size(); ++index) {
        const auto& probe = package.contrastProbes[index];
        expect(!probe.primitiveId.empty() && !probe.arcSegmentId.empty() &&
            probe.probeOrdinal == index,
            "source probes bind stable primitive, arc, component and ordinal identity");
    }
    std::set<std::string> actualBodyCoreArcs;
    std::size_t actualTickProbeCount = 0;
    for (const auto& probe : package.contrastProbes) {
        if (probe.track == TrajectoryContrastTrack::Actual &&
            probe.componentRole == CompoundComponentRole::Core &&
            probe.primitiveId.rfind("actual_segment_", 0) == 0)
            actualBodyCoreArcs.insert(probe.arcSegmentId);
        if (probe.track == TrajectoryContrastTrack::Actual &&
            probe.componentRole == CompoundComponentRole::Tick) {
            ++actualTickProbeCount;
            expect(probe.strokeXPhysicalPx >= 0 &&
                probe.strokeXPhysicalPx < 1920 &&
                probe.strokeYPhysicalPx >= 0 &&
                probe.strokeYPhysicalPx < 1080 &&
                probe.haloXPhysicalPx >= 0 &&
                probe.haloXPhysicalPx < 1920 &&
                probe.haloYPhysicalPx >= 0 &&
                probe.haloYPhysicalPx < 1080 &&
                probe.backgroundXPhysicalPx >= 0 &&
                probe.backgroundXPhysicalPx < 1920 &&
                probe.backgroundYPhysicalPx >= 0 &&
                probe.backgroundYPhysicalPx < 1080,
                "every visible tick probe owns in-bounds core, halo and background samples");
        }
    }
    const std::string probeDigest = probeIdentityDigest(
        package.contrastProbes);
    const std::string vertexDigest = vertexByteDigest(package.vertices);
    expect(actualBodyCoreArcs.size() == package.actual.projectedSegmentCount,
        "every visible actual segment contributes an exact body-core contrast probe independent of sparse tick-role rotation: probes=" +
            std::to_string(actualBodyCoreArcs.size()) + " segments=" +
            std::to_string(package.actual.projectedSegmentCount));
    expect(actualTickProbeCount == package.actual.tickMarkCount,
        "every visible actual tick contributes exactly one Tick probe");
    expect(package.contrastProbes.size() == 46 &&
        probeDigest == "016c58846f9446c5",
        "visual styling preserves the frozen CPU source-probe inventory and identity digest: count=" +
            std::to_string(package.contrastProbes.size()) + " digest=" +
            probeDigest);
    // 2026-09-29: approved longer time-marker wings change real vertex
    // positions; retain an exact regression binding to that production shape.
    expect(vertexDigest == "1b1ccdcbada2d8e5",
        "readable time-marker geometry retains its exact GPU vertex bytes: " +
            vertexDigest);

    const render::SharedCameraProjection projection =
        render::buildSharedCameraProjection(camera, 1920, 1080);
    const render::HomogeneousPoint clip = render::transformWorldToClip(
        projection, snapshot.predicted.endpoint);
    render::PhysicalScreenPoint physical;
    expect(render::clipToPhysicalScreen(projection, clip, physical),
        "shared camera projects endpoint to physical viewport");
    const double roundTripNdcX = physical.x / 1920.0 * 2.0 - 1.0;
    const double roundTripNdcY = 1.0 - physical.y / 1080.0 * 2.0;
    expect(std::fabs(roundTripNdcX - clip.x / clip.w) < 0.5 / 1920.0 &&
        std::fabs(roundTripNdcY - clip.y / clip.w) < 0.5 / 1080.0,
        "physical viewport mapping is within half a pixel");
}

void testFinalLdrAwareVisualStyleFixtures()
{
    using namespace billiardgl::training;
    const auto visual = trajectoryOverlayVisualContrastContract();
    const std::array<std::array<std::uint8_t, 3>, 4> backgrounds{{
        {{9, 9, 11}}, {{25, 117, 127}},
        {{223, 221, 212}}, {{246, 246, 242}}}};
    bool selectedBlack = false;
    bool selectedWhite = false;
    bool suppressedRedundantRim = false;
    for (const auto& background : backgrounds) {
        for (const auto& inner : {visual.predictedInnerHaloFinalSrgb8,
                 visual.actualInnerHaloFinalSrgb8}) {
            const auto decision = selectTrajectoryAdaptiveOuterRim(
                background, inner);
            expect(decision.selectedOuterContrastRatio >= 4.0,
                "dark, teal, pale, and near-white backgrounds retain a final-LDR >=4 boundary");
            selectedBlack = selectedBlack || decision.polarity ==
                TrajectoryAdaptiveOuterRimPolarity::Black;
            selectedWhite = selectedWhite || decision.polarity ==
                TrajectoryAdaptiveOuterRimPolarity::White;
            suppressedRedundantRim = suppressedRedundantRim ||
                decision.polarity ==
                    TrajectoryAdaptiveOuterRimPolarity::Suppressed;
        }
    }
    expect(selectedBlack && selectedWhite && suppressedRedundantRim,
        "adaptive polarity rejects fixed track colors and suppresses a redundant continuous third line");

    const std::array<std::array<std::uint8_t, 3>, 3> actualTrackColors{{
        visual.actualCoreFinalSrgb8,
        visual.actualInnerHaloFinalSrgb8,
        visual.adaptiveOuterBlackFinalSrgb8}};
    for (const auto& actualColor : actualTrackColors) {
        const auto predicted = selectTrajectoryAdaptiveOuterRim(actualColor,
            visual.predictedInnerHaloFinalSrgb8, 4.0, true);
        expect(predicted.polarity !=
                TrajectoryAdaptiveOuterRimPolarity::Suppressed &&
            predicted.selectedOuterContrastRatio >= 4.0,
            "predicted outer rim uses the actual-composited backdrop under coaxial cross-track pressure");
    }
    const auto actualOnScene = selectTrajectoryAdaptiveOuterRim(
        {{223, 221, 212}}, visual.actualInnerHaloFinalSrgb8);
    expect(actualOnScene.polarity ==
            TrajectoryAdaptiveOuterRimPolarity::Black &&
        actualOnScene.selectedOuterContrastRatio >= 4.0,
        "actual rim uses the frozen scene backdrop and selects black on pale cloth or ball pixels");
    expect(trajectoryOverlayFinalSrgbContrastRatio(
            visual.predictedCoreFinalSrgb8,
            visual.predictedInnerHaloFinalSrgb8) >= 4.0 &&
        trajectoryOverlayFinalSrgbContrastRatio(
            visual.actualCoreFinalSrgb8,
            visual.actualInnerHaloFinalSrgb8) >= 4.0 &&
        visual.actualCoreFinalSrgb8 ==
            std::array<std::uint8_t, 3>{{2, 91, 141}},
        "both track cores retain at least 4:1 contrast against their inner halo");

    // A single white predicted halo repairs the outer silhouette but erases
    // the core/inner boundary; a single black actual halo has the converse
    // failure. Both tempting one-color fixes must stay rejected.
    expect(trajectoryOverlayFinalSrgbContrastRatio(
            visual.predictedCoreFinalSrgb8,
            visual.predictedOuterRimFinalSrgb8) < 4.0 &&
        trajectoryOverlayFinalSrgbContrastRatio(
            visual.actualCoreFinalSrgb8,
            visual.actualOuterRimFinalSrgb8) < 4.0,
        "single-color halo counterexamples fail the core-inner contrast gate");

    const TrajectoryOverlayStyle style = resolveTrajectoryOverlayStyle(
        {1920, 1080, 1.0, 1.0});
    const double outerAddedCrossSectionPx =
        2.0 * style.outerContrastRimWidthPx;
    const double coreAddedGeometricAreaPx =
        2.0 * style.predictedCoreCapExtensionPx * style.lineWidthPx;
    const double predictedInnerCapExtension = std::max(style.haloWidthPx,
        style.predictedCoreCapExtensionPx + 2.0);
    expect(outerAddedCrossSectionPx == 4.0 &&
        coreAddedGeometricAreaPx >= 13.5 &&
        predictedInnerCapExtension -
            style.predictedCoreCapExtensionPx == 2.0 &&
        style.haloWidthPx >= 2.0 &&
        style.actualInternalJoinOverlapPx == 0.75,
        "cap-area recovery and side-only rim stay intact while the 1080p inner annulus reaches 2px and actual joins overlap without widening");

    const auto package = buildTrajectoryOverlayDrawPackage(
        overlayFixture(), overheadCamera(), {1920, 1080, 1.0, 1.0},
        TrajectoryOverlayPalette::ColorVisionSafe);
    expect(package.ok && package.additionalDrawCalls == 2 &&
        package.newTextureCount == 0,
        "visual correction keeps exact actual-then-predicted ranges without package-owned textures");
    bool sawPredictedOuter = false;
    bool sawPredictedInner = false;
    bool sawActualOuter = false;
    bool sawActualInner = false;
    std::map<std::string, std::set<TrajectoryVisualSublayer>> haloIdentity;
    int previousTrack = 0;
    int previousSublayer = -1;
    for (const auto& batch : package.semanticBatches) {
        const int track = batch.track == TrajectoryContrastTrack::Actual ?
            0 : 1;
        const int sublayer = static_cast<int>(batch.visualSublayer);
        expect(track >= previousTrack,
            "immutable actual-then-predicted track z-order is preserved");
        if (track != previousTrack) previousSublayer = -1;
        expect(sublayer >= previousSublayer,
            "each track is globally ordered outer then inner then core");
        previousTrack = track;
        previousSublayer = sublayer;
        if (batch.componentRole != CompoundComponentRole::OuterHaloEdge ||
                batch.primitiveId.find("endpoint") != std::string::npos)
            continue;
        const std::string semanticIdentity = std::to_string(track) + "|" +
            batch.primitiveId + "|" + batch.arcSegmentId;
        haloIdentity[semanticIdentity].insert(batch.visualSublayer);
        for (std::size_t offset = 0; offset < batch.vertexCount; ++offset)
            expect(package.vertices[batch.firstVertex + offset].finalOwnershipToken ==
                    batch.finalOwnershipToken,
                "each visual range retains exact semantic ownership provenance");
        if (batch.track == TrajectoryContrastTrack::Predicted) {
            if (batch.visualSublayer == TrajectoryVisualSublayer::OuterRim) {
                expect(batch.vertexCount == 12,
                    "predicted cap emits side-only outer bands so the 2px inner annulus owns the unchanged cap envelope");
                sawPredictedOuter = true;
            } else if (batch.visualSublayer ==
                    TrajectoryVisualSublayer::InnerHalo) {
                expect(batch.vertexCount == 6,
                    "predicted inner halo is a distinct later visual range");
                sawPredictedInner = true;
            }
        } else {
            if (batch.visualSublayer == TrajectoryVisualSublayer::OuterRim) {
                expect(batch.vertexCount == 24,
                    "actual outer rim is a narrow four-sided band, never a full fill");
                sawActualOuter = true;
            } else if (batch.visualSublayer ==
                    TrajectoryVisualSublayer::InnerHalo) {
                expect(batch.vertexCount == 6,
                    "actual inner halo is a distinct later visual range");
                sawActualInner = true;
            }
        }
    }
    expect(sawPredictedOuter && sawPredictedInner && sawActualOuter &&
        sawActualInner,
        "both tracks preserve one semantic primitive identity beneath explicit visual sublayers");
    for (const auto& item : haloIdentity)
        expect(item.second == std::set<TrajectoryVisualSublayer>{
                TrajectoryVisualSublayer::OuterRim,
                TrajectoryVisualSublayer::InnerHalo},
            "outer and inner provenance remain subordinate to one immutable track/component/primitive/arc identity");
}

void testFourGateCameraPresets()
{
    using namespace billiardgl::training;
    const std::array<billiardgl::CameraPresentation, 4> cameras{{
        orbitCamera(-1.5707963f, 20.0f, 145.0f, 42.0f),
        orbitCamera(-1.5707963f, 42.0f, 220.0f, 48.0f),
        orbitCamera(-1.5707963f, 88.0f, 380.0f, 40.0f),
        orbitCamera(-0.96f, 30.0f, 255.0f, 45.0f)}};
    for (std::size_t index = 0; index < cameras.size(); ++index) {
        const TrajectoryOverlayDrawPackage package =
            buildTrajectoryOverlayDrawPackage(overlayFixture(), cameras[index],
                {1920, 1080, 1.0, 1.0});
        expect(package.ok, "gate camera preset " + std::to_string(index) +
            " builds with screen-space play-surface clipping");
        expect(package.additionalDrawCalls <= 2 &&
            package.predicted.vertexCount <= 8192 &&
            package.actual.vertexCount <= 8192,
            "gate camera preset stays in draw and vertex budgets");
    }
}

void testActualTickProbeGeometryAcrossDirectionsAndDpi()
{
    using namespace billiardgl::training;
    const std::array<TrajectoryOverlaySnapshot, 3> snapshots{{
        directionalActualFixture(2.0f, 0.0f),
        directionalActualFixture(0.0f, 2.0f),
        directionalActualFixture(1.5f, 1.5f)}};
    const std::array<billiardgl::CameraPresentation, 2> cameras{{
        overheadCamera(), orbitCamera(-0.96f, 30.0f, 255.0f, 45.0f)}};
    const std::array<TrajectoryOverlayViewport, 2> viewports{{
        {1280, 720, 1.0, 1.0}, {2560, 1440, 2.0, 1.0}}};
    bool sawHorizontal = false;
    bool sawVertical = false;
    bool sawDiagonal = false;
    std::array<double, 2> maximumHaloOffsets{{0.0, 0.0}};
    for (std::size_t viewportIndex = 0;
         viewportIndex < viewports.size(); ++viewportIndex) {
        for (const auto& camera : cameras) {
            for (const auto& snapshot : snapshots) {
                const auto package = buildTrajectoryOverlayDrawPackage(
                    snapshot, camera, viewports[viewportIndex]);
                expect(package.ok, "directional actual overlay builds");
                for (const auto& probe : package.contrastProbes) {
                    if (probe.track != TrajectoryContrastTrack::Actual ||
                            probe.componentRole != CompoundComponentRole::Tick)
                        continue;
                    expect(probe.primitiveId.rfind("actual_tick_", 0) == 0 &&
                        probe.primitiveId.find("_core") != std::string::npos,
                        "tick probe binds the emitted Tick semantic batch");
                    const double tangentLength = std::hypot(
                        probe.localTangentX, probe.localTangentY);
                    const double normalLength = std::hypot(
                        probe.localNormalX, probe.localNormalY);
                    const double dot = probe.localTangentX *
                        probe.localNormalX + probe.localTangentY *
                        probe.localNormalY;
                    expect(std::fabs(tangentLength - 1.0) < 1e-6 &&
                        std::fabs(normalLength - 1.0) < 1e-6 &&
                        std::fabs(dot) < 1e-6,
                        "tick probe tangent and normal are orthonormal");
                    const double haloDx = probe.haloXPhysicalPx -
                        probe.strokeXPhysicalPx;
                    const double haloDy = probe.haloYPhysicalPx -
                        probe.strokeYPhysicalPx;
                    expect(std::fabs(haloDx * probe.localTangentX +
                            haloDy * probe.localTangentY) <= 1.0 &&
                        haloDx * probe.localNormalX +
                            haloDy * probe.localNormalY > 0.5,
                        "tick halo sample advances along the rasterized tick normal");
                    maximumHaloOffsets[viewportIndex] = std::max(
                        maximumHaloOffsets[viewportIndex],
                        std::hypot(haloDx, haloDy));
                    sawHorizontal = sawHorizontal ||
                        std::fabs(probe.localTangentY) < 0.1;
                    sawVertical = sawVertical ||
                        std::fabs(probe.localTangentX) < 0.1;
                    sawDiagonal = sawDiagonal ||
                        (std::fabs(probe.localTangentX) > 0.2 &&
                            std::fabs(probe.localTangentY) > 0.2);
                }
            }
        }
    }
    expect(sawHorizontal && sawVertical && sawDiagonal,
        "tick sampling covers horizontal, vertical, diagonal and rotated projections");
    expect(maximumHaloOffsets[1] > maximumHaloOffsets[0] * 1.5,
        "Retina tick halo sampling scales in physical pixels");
}

void testDiscontinuityAndAdvancedIsolation()
{
    using namespace billiardgl;
    using namespace billiardgl::training;
    TrajectoryOverlaySnapshot snapshot = overlayFixture();
    snapshot.predicted.samples.insert(snapshot.predicted.samples.begin() + 5,
        {3, {std::numeric_limits<float>::quiet_NaN(), 86.5f, 0.0f}, true});
    snapshot.advancedExpanded = true;
    const TrajectoryOverlayDrawPackage package =
        buildTrajectoryOverlayDrawPackage(snapshot, overheadCamera(),
            {1280, 720, 1.0, 0.85},
            TrajectoryOverlayPalette::ColorVisionSafe);
    expect(package.ok, "invalid source sample creates a break, not a failure");
    expect(package.predicted.projectedSegmentCount <
        snapshot.predicted.samples.size() - 1,
        "nonfinite and non-increasing ticks break connectivity");
    expect(!package.advancedGeometryEmitted,
        "expanded diagnostics never mutate overlay geometry");
}

void testPreviewOrbitDashProgress()
{
    using namespace billiardgl::training;
    // Fractional projected lengths repeatedly cross dash boundaries as the
    // camera moves; no boundary may leave the tessellation cursor unchanged.
    const auto snapshot = overlayFixture();
    for (int index = 0; index < 120; ++index) {
        const auto camera = orbitCamera(0.58f + index * 0.001f,
            18.3f, 145.0f, 42.0f);
        const auto package = buildTrajectoryOverlayDrawPackage(snapshot,
            camera, {2048, 1536, 1.0, 1.0});
        expect(package.ok && package.predicted.vertexCount <= 8192,
            "fractional dash boundaries terminate within the vertex budget");
    }
}

void testContrastTelemetryAndNegativeFixtures()
{
    using namespace billiardgl::training;
    const std::array<std::uint8_t, 3> amber{{255, 115, 5}};
    const std::array<std::uint8_t, 3> cyan{{13, 255, 255}};
    const std::array<std::uint8_t, 3> black{{0, 0, 0}};
    std::vector<TrajectoryContrastSample> passing;
    for (int x = 0; x < 100; ++x) {
        passing.push_back({TrajectoryContrastTrack::Predicted,
            x, 10, amber, black});
        passing.push_back({TrajectoryContrastTrack::Actual,
            x, 20, cyan, black});
    }
    const auto pass = auditTrajectoryContrastSamples(passing, amber, cyan,
        TrajectoryOverlayPalette::ColorVisionSafe);
    expect(pass.ok && pass.minimumLocalContrastRatio >= 4.0 &&
        pass.designTargetCoverage >= 0.99 &&
        pass.predictedActualLumaDeltaSrgb8 >= 36.0 &&
        pass.failureCoordinates.empty(),
        "contrast telemetry reports hard minimum, p01, target coverage, luma delta, and no failures");

    auto haloBacked = passing;
    for (auto& sample : haloBacked) {
        sample.strokeSrgb8 = {{255, 255, 255}};
        sample.localBackgroundSrgb8 = {{245, 245, 245}};
        sample.haloSrgb8 = {{0, 0, 0}};
        sample.haloSampleValid = true;
    }
    expect(auditTrajectoryContrastSamples(haloBacked, amber, cyan,
            TrajectoryOverlayPalette::ColorVisionSafe).ok,
        "compound semantic stroke plus mandated halo preserves a 4:1 silhouette over a matching background");

    auto lowContrast = passing;
    lowContrast.front().strokeSrgb8 = {{40, 40, 40}};
    lowContrast.front().localBackgroundSrgb8 = {{35, 35, 35}};
    const auto contrastFailure = auditTrajectoryContrastSamples(
        lowContrast, amber, cyan, TrajectoryOverlayPalette::ColorVisionSafe);
    expect(!contrastFailure.ok && contrastFailure.errorCode ==
            "trajectory_local_contrast_below_4" &&
        !contrastFailure.failureCoordinates.empty() &&
        contrastFailure.failureCoordinates.front()[0] == 0,
        "one below-4.0 sample fails closed with its physical coordinate");

    const std::array<std::uint8_t, 3> nearRole{{254, 114, 5}};
    const auto lumaFailure = auditTrajectoryContrastSamples(
        passing, amber, nearRole, TrajectoryOverlayPalette::ColorVisionSafe);
    expect(!lumaFailure.ok && lumaFailure.errorCode ==
            "trajectory_role_luma_delta_below_profile",
        "CVD predicted/actual luma delta below 36 fails closed");
    passing.erase(std::remove_if(passing.begin(), passing.end(),
        [](const auto& value) {
            return value.track == TrajectoryContrastTrack::Actual;
        }), passing.end());
    expect(auditTrajectoryContrastSamples(passing, amber, cyan,
            TrajectoryOverlayPalette::Standard).errorCode ==
            "trajectory_contrast_track_inventory_missing",
        "contrast audit rejects a missing semantic track inventory");
}

}  // namespace

int main()
{
    try {
        testStylePhysicalScaling();
        testProjectionAndEncodingContract();
        testFinalLdrAwareVisualStyleFixtures();
        testFourGateCameraPresets();
        testActualTickProbeGeometryAcrossDirectionsAndDpi();
        testDiscontinuityAndAdvancedIsolation();
        testPreviewOrbitDashProgress();
        testContrastTelemetryAndNegativeFixtures();
        std::cout << "trajectory overlay renderer tests passed\n";
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
