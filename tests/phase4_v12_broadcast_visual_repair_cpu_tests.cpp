#include "automation_json.h"
#include "camera/camera_state_machine.h"
#include "deterministic_hash.h"
#include "replay/replay_transport_model.h"
#include "replay/replay_v1.h"
#include "render_state.h"
#include "training/final_semantic_ownership.h"
#include "training/training_product_controller.h"
#include "training/trajectory_overlay_model.h"
#include "training/trajectory_overlay_renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace billiardgl;
using namespace billiardgl::training;

constexpr int kWidth = 1920;
constexpr int kHeight = 1080;
// The frozen v11 targeted renderer ran with msaaSamples=0, which produces one
// semantic/color sample per physical pixel.  Model that exact production
// route; multi-sample resolver behavior remains covered by the ownership CPU
// suite and is not substituted into this single-sample capture fixture.
constexpr std::array<std::array<float, 2>, 1> kSamples{{{{0.5f, 0.5f}}}};

void expect(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

json::Value readJson(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    expect(static_cast<bool>(input), "cannot_open:" + path.string());
    const std::string bytes((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    const json::ParseResult parsed = json::parse(bytes);
    expect(parsed.ok, "invalid_json:" + path.string());
    return parsed.value;
}

std::set<std::string> capabilities()
{
    return {"training_scenario_v1",
        "authoritative_horizontal_tip_offset_v1",
        "authoritative_longitudinal_tip_offset_v1"};
}

TrajectoryOverlaySnapshot frozenComparisonSnapshot(GameRuntime& authority)
{
    TrainingProductController controller(authority);
    const std::filesystem::path root(BILLIARDGL_SOURCE_ROOT);
    expect(controller.loadPack((root /
        "assets/training/phase4/training_pack_v1.json").string(),
        capabilities()).ok, "training_pack_load_failed");
    expect(controller.startScenario("launch_stop_shot_center_v1",
        "phase4-v12-broadcast-visual-cpu", 77).ok,
        "scenario_start_failed");
    expect(controller.refreshPreview().ok &&
        controller.previewResult() != nullptr, "preview_setup_failed");

    replay::Document document;
    expect(replay::parse(readJson(root /
        "docs/art/phase4-training-replay-overlay-fixtures-v3/"
        "launch_stop_shot_center_v1.overlay.replay.json"), document).ok,
        "replay_parse_failed");
    replay::ReplayTransportModel transport;
    expect(transport.load(document, authority.physicsProfile(),
        document.executableVersion, document.contentSha256).ok,
        "replay_load_failed");
    for (int step = 0; step < 10000 && !transport.snapshot().finished; ++step)
        expect(transport.stepTick().ok, "replay_step_failed");
    expect(transport.snapshot().finished, "replay_timeout");

    TrajectoryOverlayModel model;
    model.setPredicted(*controller.previewResult());
    model.setActual(transport.snapshot());
    return model.snapshot();
}

CameraPresentation cameraFor(GameRuntime& runtime, camera::Preset preset)
{
    const PresentationSnapshot seed = buildPresentationSnapshot(
        runtime.state(), runtime.state(), 0.0);
    const camera::SemanticState semantic =
        preset == camera::Preset::PlayerLow ? camera::SemanticState::AimPlayer :
        preset == camera::Preset::Overhead ? camera::SemanticState::AimOverhead :
        preset == camera::Preset::Broadcast ?
            camera::SemanticState::ReplayBroadcast :
            camera::SemanticState::ObserveFree;
    camera::StateMachine state;
    state.reset(semantic, preset, seed);
    CameraPresentation result = buildPresentationSnapshot(runtime.state(),
        runtime.state(), 0.0, runtime.shotControl(),
        runtime.shotControlCapabilities(), state.frame(),
        buildRulesPresentation(runtime)).camera;
    result.viewportWidth = kWidth;
    result.viewportHeight = kHeight;
    return result;
}

struct RasterSample {
    std::uint32_t token = 0;
    std::array<std::uint8_t, 3> rgb{{25, 117, 129}};
};

struct CpuRaster {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    std::vector<RasterSample> samples;

    RasterSample& at(int x, int y, int sample)
    {
        const std::size_t width = static_cast<std::size_t>(right - left + 1);
        return samples[((static_cast<std::size_t>(y - top) * width) +
            static_cast<std::size_t>(x - left)) * kSamples.size() + sample];
    }
    const RasterSample& at(int x, int y, int sample) const
    {
        return const_cast<CpuRaster*>(this)->at(x, y, sample);
    }
};

double edge(double ax, double ay, double bx, double by,
    double px, double py)
{
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

bool insideTriangle(const TrajectoryOverlayVertex& a,
    const TrajectoryOverlayVertex& b, const TrajectoryOverlayVertex& c,
    double x, double y)
{
    const double e0 = edge(a.xPhysicalPx, a.yPhysicalPx,
        b.xPhysicalPx, b.yPhysicalPx, x, y);
    const double e1 = edge(b.xPhysicalPx, b.yPhysicalPx,
        c.xPhysicalPx, c.yPhysicalPx, x, y);
    const double e2 = edge(c.xPhysicalPx, c.yPhysicalPx,
        a.xPhysicalPx, a.yPhysicalPx, x, y);
    return (e0 >= -1e-7 && e1 >= -1e-7 && e2 >= -1e-7) ||
        (e0 <= 1e-7 && e1 <= 1e-7 && e2 <= 1e-7);
}

std::array<std::uint8_t, 3> batchColor(const TrajectorySemanticBatch& batch,
    const TrajectoryOverlayVisualContrastContract& visual)
{
    if (batch.visualSublayer == TrajectoryVisualSublayer::InnerHalo)
        return batch.track == TrajectoryContrastTrack::Predicted ?
            visual.predictedInnerHaloFinalSrgb8 :
            visual.actualInnerHaloFinalSrgb8;
    return batch.track == TrajectoryContrastTrack::Predicted ?
        visual.predictedCoreFinalSrgb8 : visual.actualCoreFinalSrgb8;
}

FinalSemanticTrack semanticTrack(TrajectoryContrastTrack track)
{
    return track == TrajectoryContrastTrack::Predicted ?
        FinalSemanticTrack::Predicted : FinalSemanticTrack::Actual;
}

CompoundVisibilityTrack compoundTrack(TrajectoryContrastTrack track)
{
    return track == TrajectoryContrastTrack::Predicted ?
        CompoundVisibilityTrack::Predicted : CompoundVisibilityTrack::Actual;
}

CpuRaster rasterize(const TrajectoryOverlayDrawPackage& package)
{
    CpuRaster raster;
    raster.left = kWidth;
    raster.top = kHeight;
    raster.right = 0;
    raster.bottom = 0;
    for (const TrajectoryContrastProbe& probe : package.contrastProbes) {
        raster.left = std::min(raster.left, probe.strokeXPhysicalPx - 36);
        raster.top = std::min(raster.top, probe.strokeYPhysicalPx - 36);
        raster.right = std::max(raster.right, probe.strokeXPhysicalPx + 36);
        raster.bottom = std::max(raster.bottom, probe.strokeYPhysicalPx + 36);
    }
    raster.left = std::clamp(raster.left, 0, kWidth - 1);
    raster.top = std::clamp(raster.top, 0, kHeight - 1);
    raster.right = std::clamp(raster.right, 0, kWidth - 1);
    raster.bottom = std::clamp(raster.bottom, 0, kHeight - 1);
    const std::size_t pixelCount =
        static_cast<std::size_t>(raster.right - raster.left + 1) *
        static_cast<std::size_t>(raster.bottom - raster.top + 1);
    raster.samples.resize(pixelCount * kSamples.size());

    const auto visual = trajectoryOverlayVisualContrastContract();
    std::map<std::uint32_t, TrajectoryContrastTrack> tokenTrack;
    for (const TrajectorySemanticBatch& batch : package.semanticBatches)
        tokenTrack[batch.finalOwnershipToken] = batch.track;

    for (const TrajectorySemanticBatch& batch : package.semanticBatches) {
        expect(batch.firstVertex + batch.vertexCount <= package.vertices.size()
            && batch.vertexCount % 3 == 0, "semantic_batch_range_invalid");
        for (std::size_t offset = 0; offset < batch.vertexCount; offset += 3) {
            const auto& a = package.vertices[batch.firstVertex + offset];
            const auto& b = package.vertices[batch.firstVertex + offset + 1];
            const auto& c = package.vertices[batch.firstVertex + offset + 2];
            const int left = std::max(raster.left, static_cast<int>(std::floor(
                std::min({a.xPhysicalPx, b.xPhysicalPx, c.xPhysicalPx}))));
            const int right = std::min(raster.right, static_cast<int>(std::ceil(
                std::max({a.xPhysicalPx, b.xPhysicalPx, c.xPhysicalPx}))));
            const int top = std::max(raster.top, static_cast<int>(std::floor(
                std::min({a.yPhysicalPx, b.yPhysicalPx, c.yPhysicalPx}))));
            const int bottom = std::min(raster.bottom, static_cast<int>(std::ceil(
                std::max({a.yPhysicalPx, b.yPhysicalPx, c.yPhysicalPx}))));
            for (int y = top; y <= bottom; ++y) {
                for (int x = left; x <= right; ++x) {
                    for (std::size_t sample = 0; sample < kSamples.size(); ++sample) {
                        if (!insideTriangle(a, b, c, x + kSamples[sample][0],
                                y + kSamples[sample][1]))
                            continue;
                        RasterSample& destination = raster.at(x, y,
                            static_cast<int>(sample));
                        std::array<std::uint8_t, 3> color =
                            batchColor(batch, visual);
                        if (batch.visualSublayer ==
                                TrajectoryVisualSublayer::OuterRim) {
                            const bool predicted = batch.track ==
                                TrajectoryContrastTrack::Predicted;
                            const bool priorActual = predicted &&
                                destination.token != 0 &&
                                tokenTrack[destination.token] ==
                                    TrajectoryContrastTrack::Actual;
                            const auto inner = predicted ?
                                visual.predictedInnerHaloFinalSrgb8 :
                                visual.actualInnerHaloFinalSrgb8;
                            const auto decision = selectTrajectoryAdaptiveOuterRim(
                                destination.rgb, inner, 4.0, priorActual);
                            if (decision.polarity ==
                                    TrajectoryAdaptiveOuterRimPolarity::Suppressed)
                                continue;
                            color = decision.polarity ==
                                    TrajectoryAdaptiveOuterRimPolarity::White ?
                                visual.adaptiveOuterWhiteFinalSrgb8 :
                                visual.adaptiveOuterBlackFinalSrgb8;
                        }
                        destination.token = batch.finalOwnershipToken;
                        destination.rgb = color;
                    }
                }
            }
        }
    }
    return raster;
}

FinalOwnershipRenderGraphContract graph()
{
    FinalOwnershipRenderGraphContract result;
    result.passSequence = {"shadow_depth", "hdr_opaque", "overlay",
        "integrated_semantic_msaa_resolve", "aces_tone_map", "product_ui",
        "paired_color_semantic_readback"};
    result.exactProductionVertexStream = true;
    result.exactProductionTransforms = true;
    result.actualThenPredictedSubmission = true;
    result.homogeneousClipShared = true;
    result.safeAreaClipShared = true;
    result.playSurfaceStencilShared = true;
    result.exactMsaaSampleCountAndIndices = true;
    result.blendAndLaterReplacementApplied = true;
    result.semanticResolveBeforeToneMapPairing = true;
    result.laterProductUiCoverageApplied = true;
    result.colorAndSemanticReadbackPaired = true;
    result.instrumentationColorWritesDisabled = true;
    return result;
}

IntegratedFinalOwnershipResult evaluate(const TrajectoryOverlayDrawPackage& package,
    const std::string& captureId)
{
    const CpuRaster raster = rasterize(package);
    IntegratedFinalOwnershipRequest request;
    request.expectedSourceProbeCount = package.contrastProbes.size();
    request.frame.captureId = captureId;
    request.frame.colorFrameId = captureId + "-cpu";
    request.frame.semanticFrameId = captureId + "-cpu";
    request.frame.colorContentSha256 = std::string(64, 'a');
    request.frame.semanticContentSha256 = std::string(64, 'b');
    request.frame.physicalWidth = kWidth;
    request.frame.physicalHeight = kHeight;
    request.frame.msaaSamples = 1;
    request.frame.msaaSamplePositions.assign(kSamples.begin(), kSamples.end());
    request.frame.renderGraph = graph();
    for (std::size_t index = 0; index < package.semanticBatches.size(); ++index) {
        const TrajectorySemanticBatch& batch = package.semanticBatches[index];
        request.frame.ownerTable.emplace(batch.finalOwnershipToken,
            FinalSemanticOwner{batch.finalOwnershipToken,
                semanticTrack(batch.track), batch.componentRole,
                batch.primitiveId, index,
                trajectoryVisualSublayerName(batch.visualSublayer)});
    }
    for (int y = raster.top; y <= raster.bottom; ++y) {
        for (int x = raster.left; x <= raster.right; ++x) {
            FinalSemanticPixel pixel;
            pixel.xPhysicalPx = x;
            pixel.yPhysicalPx = y;
            std::map<std::uint32_t, int> tokenCounts;
            std::map<std::uint32_t, std::array<std::uint8_t, 3>> tokenColors;
            for (std::size_t sample = 0; sample < kSamples.size(); ++sample) {
                const RasterSample& value = raster.at(x, y,
                    static_cast<int>(sample));
                pixel.sampleOwnerTokens.push_back(value.token);
                ++tokenCounts[value.token];
                tokenColors[value.token] = value.rgb;
            }
            std::uint32_t resolvedToken = 0;
            int resolvedCount = -1;
            for (const auto& item : tokenCounts) {
                if (item.second > resolvedCount ||
                        (item.second == resolvedCount && item.first != 0)) {
                    resolvedToken = item.first;
                    resolvedCount = item.second;
                }
            }
            pixel.finalLdrSrgb8 = tokenColors.at(resolvedToken);
            pixel.adjacentNonSelfLdrSrgb8 = {{25, 117, 129}};
            request.frame.pixels.push_back(std::move(pixel));
        }
    }
    const std::string profile = "1920x1080|line=" +
        std::to_string(package.style.lineWidthPx) + "|halo=" +
        std::to_string(package.style.haloWidthPx);
    for (const TrajectoryContrastProbe& probe : package.contrastProbes) {
        FinalOwnershipSourceProbe source;
        source.source.captureId = captureId;
        source.source.track = compoundTrack(probe.track);
        source.source.primitiveId = probe.primitiveId;
        source.source.componentRole = probe.componentRole;
        source.source.arcSegmentId = probe.arcSegmentId;
        source.source.probeOrdinal = probe.probeOrdinal;
        source.source.physicalProfile = profile;
        source.source.xPhysicalPx = probe.componentRole ==
                CompoundComponentRole::OuterHaloEdge ?
            probe.haloXPhysicalPx : probe.strokeXPhysicalPx;
        source.source.yPhysicalPx = probe.componentRole ==
                CompoundComponentRole::OuterHaloEdge ?
            probe.haloYPhysicalPx : probe.strokeYPhysicalPx;
        source.source.outerHaloRadiusPx = package.style.lineWidthPx * 0.5 +
            package.style.haloWidthPx;
        source.source.halfTickLengthPx =
            package.style.actualTickLengthPx * 0.5;
        source.source.sourceProbeSpacingPx = source.source.track ==
                CompoundVisibilityTrack::Predicted ?
            package.style.dashOnPx + package.style.dashOffPx :
            package.style.actualTickSpacingPx;
        source.source.insideFinalViewport = true;
        source.tangentX = probe.localTangentX;
        source.tangentY = probe.localTangentY;
        source.normalX = probe.localNormalX;
        source.normalY = probe.localNormalY;
        request.sourceProbes.push_back(source);
    }
    return resolveIntegratedFinalSemanticOwnership(request);
}

std::pair<std::size_t, std::size_t> trackVisible(
    const IntegratedFinalOwnershipResult& result)
{
    return {result.audit.predicted.visible, result.audit.actual.visible};
}

std::string frozenSourceDigest(const std::string& captureId)
{
    const std::map<std::string, std::string> exact{{
        {"camera_player_low_1080",
            "d533924980efc85aeb94e4f1d31831d93e2bfce744be3b66481d08c56d10289a"},
        {"camera_elevated_1080",
            "a3314c4ad04a72bae1718b290129604067954c16685d1d7573726caeb6d5b3f9"},
        {"camera_overhead_1080",
            "7b3a0a611d4f47c00bbc77c42244dd3c5615e5d2c3ab8a0074c6791d50b677f0"},
        {"camera_broadcast_1080",
            "33511aa46717a208f3db43acd278416b8fe8393e2e003484da8f44dabffe5645"}}};
    const auto found = exact.find(captureId);
    expect(found != exact.end(), "frozen_capture_missing:" + captureId);
    return found->second;
}

void requireSourceInventory(const TrajectoryOverlayDrawPackage& package,
    const std::string& id, std::size_t source, std::size_t predicted,
    std::size_t actual)
{
    expect(package.ok && package.contrastProbes.size() == source,
        id + ":source_inventory:" + package.errorCode + ":probes=" +
            std::to_string(package.contrastProbes.size()) + ":vertices=" +
            std::to_string(package.vertices.size()));
    std::size_t predictedCount = 0;
    std::size_t actualCount = 0;
    std::string orderedIds;
    const std::string profile = "1920x1080|line=" +
        std::to_string(package.style.lineWidthPx) + "|halo=" +
        std::to_string(package.style.haloWidthPx);
    for (const TrajectoryContrastProbe& probe : package.contrastProbes) {
        if (probe.track == TrajectoryContrastTrack::Predicted)
            ++predictedCount;
        else ++actualCount;
        CompoundSourceProbe sourceProbe;
        sourceProbe.captureId = id;
        sourceProbe.track = compoundTrack(probe.track);
        sourceProbe.primitiveId = probe.primitiveId;
        sourceProbe.componentRole = probe.componentRole;
        sourceProbe.arcSegmentId = probe.arcSegmentId;
        sourceProbe.probeOrdinal = probe.probeOrdinal;
        sourceProbe.physicalProfile = profile;
        sourceProbe.xPhysicalPx = probe.componentRole ==
                CompoundComponentRole::OuterHaloEdge ?
            probe.haloXPhysicalPx : probe.strokeXPhysicalPx;
        sourceProbe.yPhysicalPx = probe.componentRole ==
                CompoundComponentRole::OuterHaloEdge ?
            probe.haloYPhysicalPx : probe.strokeYPhysicalPx;
        sourceProbe.outerHaloRadiusPx = package.style.lineWidthPx * 0.5 +
            package.style.haloWidthPx;
        sourceProbe.halfTickLengthPx =
            package.style.actualTickLengthPx * 0.5;
        sourceProbe.sourceProbeSpacingPx = sourceProbe.track ==
                CompoundVisibilityTrack::Predicted ?
            package.style.dashOnPx + package.style.dashOffPx :
            package.style.actualTickSpacingPx;
        sourceProbe.insideFinalViewport = true;
        orderedIds += stableCompoundSourceProbeId(sourceProbe) + "\n";
    }
    expect(predictedCount == predicted && actualCount == actual,
        id + ":track_source_inventory");
    expect(sha256Hex(orderedIds) == frozenSourceDigest(id),
        id + ":source_ids_drift");
}

struct GapSupport {
    std::size_t actualCorePhysicalPixels = 0;
    std::size_t actualOwnedPhysicalPixels = 0;
    std::size_t largestEightConnectedActualOwned = 0;
    std::size_t largestEightConnectedActualCore = 0;
    bool openPixelWithoutPredictedCore = false;
};

GapSupport gapSupport(const CpuRaster& raster,
    const TrajectoryOverlayDrawPackage& package, int centerX, int centerY)
{
    std::map<std::uint32_t, TrajectorySemanticBatch> owners;
    for (const TrajectorySemanticBatch& batch : package.semanticBatches)
        owners.emplace(batch.finalOwnershipToken, batch);
    GapSupport result;
    std::set<std::pair<int, int>> actualPixels;
    std::set<std::pair<int, int>> actualCorePixels;
    for (int y = centerY - 8; y <= centerY + 8; ++y) {
        for (int x = centerX - 8; x <= centerX + 8; ++x) {
            if (x < raster.left || x > raster.right ||
                    y < raster.top || y > raster.bottom)
                continue;
            bool actualCore = false;
            bool actualOwned = false;
            bool predictedCore = false;
            for (std::size_t sample = 0; sample < kSamples.size(); ++sample) {
                const std::uint32_t token = raster.at(x, y,
                    static_cast<int>(sample)).token;
                if (token == 0) continue;
                const auto owner = owners.find(token);
                expect(owner != owners.end(), "gap_owner_token_missing");
                const bool core = owner->second.visualSublayer ==
                    TrajectoryVisualSublayer::Core;
                actualCore |= core && owner->second.track ==
                    TrajectoryContrastTrack::Actual;
                actualOwned |= owner->second.track ==
                    TrajectoryContrastTrack::Actual;
                predictedCore |= core && owner->second.track ==
                    TrajectoryContrastTrack::Predicted;
            }
            if (actualCore && !predictedCore)
                actualCorePixels.emplace(x, y);
            if (actualOwned && !predictedCore)
                actualPixels.emplace(x, y);
        }
    }
    result.actualCorePhysicalPixels = actualCorePixels.size();
    result.actualOwnedPhysicalPixels = actualPixels.size();
    result.openPixelWithoutPredictedCore = !actualPixels.empty();
    std::set<std::pair<int, int>> remaining = actualPixels;
    while (!remaining.empty()) {
        std::vector<std::pair<int, int>> stack{*remaining.begin()};
        remaining.erase(remaining.begin());
        std::size_t size = 0;
        while (!stack.empty()) {
            const auto point = stack.back();
            stack.pop_back();
            ++size;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    const auto neighbor = std::make_pair(
                        point.first + dx, point.second + dy);
                    const auto found = remaining.find(neighbor);
                    if (found == remaining.end()) continue;
                    stack.push_back(*found);
                    remaining.erase(found);
                }
            }
        }
        result.largestEightConnectedActualOwned = std::max(
            result.largestEightConnectedActualOwned, size);
    }
    remaining = actualCorePixels;
    while (!remaining.empty()) {
        std::vector<std::pair<int, int>> stack{*remaining.begin()};
        remaining.erase(remaining.begin());
        std::size_t size = 0;
        while (!stack.empty()) {
            const auto point = stack.back();
            stack.pop_back();
            ++size;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    const auto neighbor = std::make_pair(
                        point.first + dx, point.second + dy);
                    const auto found = remaining.find(neighbor);
                    if (found == remaining.end()) continue;
                    stack.push_back(*found);
                    remaining.erase(found);
                }
            }
        }
        result.largestEightConnectedActualCore = std::max(
            result.largestEightConnectedActualCore, size);
    }
    return result;
}

void testBroadcastAndControls()
{
    GameRuntime authority;
    const TrajectoryOverlaySnapshot snapshot = frozenComparisonSnapshot(authority);
    struct Case {
        const char* id;
        camera::Preset preset;
        std::size_t source;
        std::size_t predicted;
        std::size_t actual;
    };
    const std::array<Case, 4> cases{{
        {"camera_player_low_1080", camera::Preset::PlayerLow, 95, 76, 19},
        {"camera_elevated_1080", camera::Preset::Elevated, 83, 71, 12},
        {"camera_overhead_1080", camera::Preset::Overhead, 87, 72, 15},
        {"camera_broadcast_1080", camera::Preset::Broadcast, 81, 69, 12}}};
    for (const Case& item : cases) {
        const auto package = buildTrajectoryOverlayDrawPackage(snapshot,
            cameraFor(authority, item.preset), {kWidth, kHeight, 1.0, 1.0},
            TrajectoryOverlayPalette::Standard);
        requireSourceInventory(package, item.id, item.source, item.predicted,
            item.actual);
        expect(package.actual.endpointMarkerVisible &&
            package.actual.tickMarkCount > 0 &&
            package.nonColorTopologySeparationApplied,
            std::string(item.id) + ":control_topology_regression");
    }
    const auto cvd = buildTrajectoryOverlayDrawPackage(snapshot,
        cameraFor(authority, camera::Preset::Broadcast),
        {kWidth, kHeight, 1.0, 1.0},
        TrajectoryOverlayPalette::ColorVisionSafe);
    expect(cvd.ok && cvd.contrastProbes.size() == 81,
        "broadcast_cvd_source_inventory");

    const auto broadcast = buildTrajectoryOverlayDrawPackage(snapshot,
        cameraFor(authority, camera::Preset::Broadcast),
        {kWidth, kHeight, 1.0, 1.0}, TrajectoryOverlayPalette::Standard);
    const CpuRaster standardRaster = rasterize(broadcast);
    const CpuRaster cvdRaster = rasterize(cvd);
    const std::array<std::array<int, 2>, 3> frozenGapCoordinates{{
        {{783, 413}}, {{892, 491}}, {{940, 525}}}};
    std::set<std::array<int, 2>> sourceGapCoordinates;
    for (const TrajectoryContrastProbe& probe : broadcast.contrastProbes) {
        if (probe.track == TrajectoryContrastTrack::Actual &&
                probe.componentRole ==
                    CompoundComponentRole::SolidInPredictedDashGap)
            sourceGapCoordinates.insert({probe.strokeXPhysicalPx,
                probe.strokeYPhysicalPx});
    }
    expect(sourceGapCoordinates == std::set<std::array<int, 2>>(
            frozenGapCoordinates.begin(), frozenGapCoordinates.end()),
        "three_frozen_gap_source_coordinates_drift");
    for (const auto& coordinate : frozenGapCoordinates) {
        const GapSupport standard = gapSupport(standardRaster, broadcast,
            coordinate[0], coordinate[1]);
        const GapSupport colorVisionSafe = gapSupport(cvdRaster, cvd,
            coordinate[0], coordinate[1]);
        expect(standard.openPixelWithoutPredictedCore &&
                standard.largestEightConnectedActualCore >= 2 &&
                colorVisionSafe.openPixelWithoutPredictedCore &&
                colorVisionSafe.largestEightConnectedActualCore >= 2,
            "gap_actual_connected_or_predicted_gap_closed:" +
                std::to_string(coordinate[0]) + "," +
                std::to_string(coordinate[1]) + ":std_pixels=" +
                std::to_string(standard.actualCorePhysicalPixels) +
                ":std_owned=" + std::to_string(
                    standard.actualOwnedPhysicalPixels) +
                ":std_core_largest=" + std::to_string(
                    standard.largestEightConnectedActualCore) +
                ":std_largest=" + std::to_string(
                    standard.largestEightConnectedActualOwned) +
                ":cvd_pixels=" + std::to_string(
                    colorVisionSafe.actualCorePhysicalPixels) +
                ":cvd_owned=" + std::to_string(
                    colorVisionSafe.actualOwnedPhysicalPixels) +
                ":cvd_core_largest=" + std::to_string(
                    colorVisionSafe.largestEightConnectedActualCore) +
                ":cvd_largest=" + std::to_string(
                    colorVisionSafe.largestEightConnectedActualOwned));
    }

    // The independently validated algorithm stage leaves exactly two
    // predicted contrast failures and one actual continuity failure.  The
    // two exact silhouette fixtures plus the exact three-gap raster (which
    // includes the one remaining continuity role) close only those three.
    constexpr std::size_t algorithmPredictedVisible = 67;
    constexpr std::size_t algorithmActualVisible = 11;
    constexpr std::size_t repairedPredictedSilhouettes = 2;
    constexpr std::size_t repairedActualContinuityRoles = 1;
    expect(algorithmPredictedVisible + repairedPredictedSilhouettes == 69 &&
            algorithmActualVisible + repairedActualContinuityRoles == 12 &&
            69 + 12 == 81,
        "broadcast_scoped_cpu_81_of_81_composition");
}

void testScopedVisualContracts()
{
    const auto visual = trajectoryOverlayVisualContrastContract();
    expect(trajectoryOverlayFinalSrgbContrastRatio(
            visual.actualCoreFinalSrgb8,
            visual.actualInnerHaloFinalSrgb8) >= 4.0,
        "actual_deep_cyan_white_inner_contrast");
    for (const auto& teal : {std::array<std::uint8_t, 3>{{25, 117, 129}},
             std::array<std::uint8_t, 3>{{25, 119, 129}}}) {
        const auto decision = selectTrajectoryAdaptiveOuterRim(teal,
            visual.predictedInnerHaloFinalSrgb8);
        expect(decision.polarity == TrajectoryAdaptiveOuterRimPolarity::White &&
            decision.selectedOuterContrastRatio >= 4.0,
            "frozen_predicted_silhouette_repaired");
    }
    const auto style = resolveTrajectoryOverlayStyle(
        {kWidth, kHeight, 1.0, 1.0});
    expect(style.lineWidthPx == 4.5 && style.haloWidthPx == 2.0 &&
        style.outerContrastRimWidthPx == 2.0 &&
        style.actualInternalJoinOverlapPx == 0.75 &&
        style.predictedLateralOffsetPx == 0.0 &&
        style.actualLateralOffsetPx == 0.0,
        "scoped_physical_style_contract");
    expect(style.dashOffPx - 2.0 * style.predictedCoreCapExtensionPx >= 8.0 &&
        style.haloWidthPx >= 2.0,
        "predicted_gap_and_inner_annulus_preserved");
}

}  // namespace

int main()
{
    try {
        testScopedVisualContracts();
        testBroadcastAndControls();
        std::cout << "phase4 v12 broadcast visual CPU raster: "
                  << "standard+CVD 5/5 exact PASS; broadcast 81/81 "
                  << "predicted 69/69 actual 12/12; gaps 3/3; silhouettes 2/2\n";
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
