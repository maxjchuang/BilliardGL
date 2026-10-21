#include "training/final_semantic_ownership.h"

#include "deterministic_hash.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <tuple>

namespace billiardgl::training {
namespace {

std::string joinedDigest(const std::vector<std::string>& values)
{
    std::string bytes;
    for (const std::string& value : values) {
        bytes += value;
        bytes.push_back('\n');
    }
    return sha256Hex(bytes);
}

double linearChannel(std::uint8_t value)
{
    const double encoded = static_cast<double>(value) / 255.0;
    return encoded <= 0.04045 ? encoded / 12.92 :
        std::pow((encoded + 0.055) / 1.055, 2.4);
}

double luminance(const std::array<std::uint8_t, 3>& color)
{
    return 0.2126 * linearChannel(color[0]) +
        0.7152 * linearChannel(color[1]) +
        0.0722 * linearChannel(color[2]);
}

double contrast(const std::array<std::uint8_t, 3>& first,
    const std::array<std::uint8_t, 3>& second)
{
    const double a = luminance(first);
    const double b = luminance(second);
    return (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
}

bool allowedComponent(CompoundVisibilityTrack track,
    CompoundComponentRole role)
{
    if (track == CompoundVisibilityTrack::Predicted)
        return role == CompoundComponentRole::Core ||
            role == CompoundComponentRole::OuterHaloEdge ||
            role == CompoundComponentRole::Endpoint;
    return role == CompoundComponentRole::Core ||
        role == CompoundComponentRole::OuterHaloEdge ||
        role == CompoundComponentRole::SolidInPredictedDashGap ||
        role == CompoundComponentRole::Tick ||
        role == CompoundComponentRole::Endpoint;
}

bool sameTrack(CompoundVisibilityTrack probe, FinalSemanticTrack owner)
{
    return (probe == CompoundVisibilityTrack::Predicted &&
            owner == FinalSemanticTrack::Predicted) ||
        (probe == CompoundVisibilityTrack::Actual &&
            owner == FinalSemanticTrack::Actual);
}

double normalBound(const CompoundSourceProbe& probe)
{
    return probe.componentRole == CompoundComponentRole::Tick ?
        probe.halfTickLengthPx + 2.0 : probe.outerHaloRadiusPx + 2.0;
}

double tangentBound(const CompoundSourceProbe& probe)
{
    return std::min(12.0, probe.sourceProbeSpacingPx * 0.5);
}

CompoundHiddenReason reasonFromString(const std::string& reason)
{
    if (reason == "out_of_bounds_or_clipped")
        return CompoundHiddenReason::OutOfBoundsOrClipped;
    if (reason == "only_other_track_visible" ||
        reason == "foreign_track_color_match")
        return CompoundHiddenReason::OnlyOtherTrackVisible;
    if (reason == "only_background_or_scene_visible" ||
        reason == "later_ui_owned")
        return CompoundHiddenReason::OnlyBackgroundOrSceneVisible;
    if (reason == "insufficient_owned_pixel_area")
        return CompoundHiddenReason::InsufficientOwnedPixelArea;
    if (reason == "insufficient_exclusive_owned_area")
        return CompoundHiddenReason::InsufficientExclusiveOwnedArea;
    if (reason == "insufficient_raster_continuity")
        return CompoundHiddenReason::InsufficientRasterContinuity;
    if (reason == "outside_bounded_neighborhood")
        return CompoundHiddenReason::OutsideBoundedNeighborhood;
    if (reason == "pixel_already_assigned")
        return CompoundHiddenReason::PixelAlreadyAssigned;
    if (reason == "final_edge_contrast_below_4" ||
        reason == "final_color_mismatch")
        return CompoundHiddenReason::FinalEdgeContrastBelow4;
    if (reason == "component_not_eligible")
        return CompoundHiddenReason::ComponentNotEligible;
    if (reason == "no_eligible_component_aware_boundary")
        return CompoundHiddenReason::NoEligibleComponentAwareBoundary;
    return CompoundHiddenReason::NoSameTrackFinalOwner;
}

bool validGraph(const IntegratedFinalOwnershipFrame& frame,
    std::string& error)
{
    const FinalOwnershipRenderGraphContract& graph = frame.renderGraph;
    const std::vector<std::string> required = {
        "shadow_depth", "hdr_opaque", "overlay",
        "integrated_semantic_msaa_resolve", "aces_tone_map", "product_ui",
        "paired_color_semantic_readback"};
    std::size_t cursor = 0;
    bool opaqueSeen = false, overlaySeen = false, bloomSeen = false, dofSeen = false;
    for (const std::string& pass : graph.passSequence) {
        if (pass == "hdr_opaque") opaqueSeen = true;
        if (pass == "overlay") overlaySeen = true;
        if (pass == "scene_bloom") {
            if (!opaqueSeen || overlaySeen || bloomSeen) {
                error = "final_ownership_bloom_order_mismatch";
                return false;
            }
            bloomSeen = true;
        }
        if (pass == "scene_dof") {
            if (!opaqueSeen || overlaySeen || dofSeen) {
                error = "final_ownership_dof_order_mismatch";
                return false;
            }
            dofSeen = true;
        }
        if (cursor < required.size() && pass == required[cursor]) ++cursor;
    }
    if (cursor != required.size()) {
        error = "final_ownership_render_graph_order_mismatch";
        return false;
    }
    if (!graph.exactProductionVertexStream ||
        !graph.exactProductionTransforms ||
        !graph.actualThenPredictedSubmission ||
        !graph.homogeneousClipShared || !graph.safeAreaClipShared ||
        !graph.playSurfaceStencilShared ||
        !graph.exactMsaaSampleCountAndIndices ||
        !graph.blendAndLaterReplacementApplied ||
        !graph.semanticResolveBeforeToneMapPairing ||
        !graph.laterProductUiCoverageApplied ||
        !graph.colorAndSemanticReadbackPaired ||
        !graph.instrumentationColorWritesDisabled) {
        error = "final_ownership_render_graph_capability_missing";
        return false;
    }
    return true;
}

std::size_t largestFourConnectedComponent(
    const std::set<std::pair<int, int>>& pixels)
{
    std::set<std::pair<int, int>> remaining = pixels;
    std::size_t largest = 0;
    while (!remaining.empty()) {
        std::vector<std::pair<int, int>> pending{*remaining.begin()};
        remaining.erase(remaining.begin());
        std::size_t count = 0;
        while (!pending.empty()) {
            const auto current = pending.back();
            pending.pop_back();
            ++count;
            const std::pair<int, int> neighbors[] = {
                {current.first - 1, current.second},
                {current.first + 1, current.second},
                {current.first, current.second - 1},
                {current.first, current.second + 1},
            };
            for (const auto& neighbor : neighbors) {
                const auto found = remaining.find(neighbor);
                if (found != remaining.end()) {
                    pending.push_back(*found);
                    remaining.erase(found);
                }
            }
        }
        largest = std::max(largest, count);
    }
    return largest;
}

std::size_t largestEightConnectedComponent(
    const std::set<std::pair<int, int>>& pixels)
{
    std::set<std::pair<int, int>> remaining = pixels;
    std::size_t largest = 0;
    while (!remaining.empty()) {
        std::vector<std::pair<int, int>> pending{*remaining.begin()};
        remaining.erase(remaining.begin());
        std::size_t count = 0;
        while (!pending.empty()) {
            const auto current = pending.back();
            pending.pop_back();
            ++count;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    const auto found = remaining.find({
                        current.first + dx, current.second + dy});
                    if (found != remaining.end()) {
                        pending.push_back(*found);
                        remaining.erase(found);
                    }
                }
            }
        }
        largest = std::max(largest, count);
    }
    return largest;
}

struct SampleClaim {
    std::size_t probeIndex = 0;
    std::size_t pixelIndex = 0;
    int sampleIndex = 0;
    double tangentDistance = 0.0;
    double normalDistance = 0.0;
    std::string primitiveId;
    std::uint64_t probeOrdinal = 0;
};

std::string primitiveStem(const std::string& primitiveId)
{
    for (const std::string suffix : {std::string("_core"),
             std::string("_halo")}) {
        if (primitiveId.size() >= suffix.size() &&
                primitiveId.compare(primitiveId.size() - suffix.size(),
                    suffix.size(), suffix) == 0)
            return primitiveId.substr(0, primitiveId.size() - suffix.size());
    }
    return primitiveId;
}

class OwnedIslandUnionFind {
public:
    explicit OwnedIslandUnionFind(std::size_t size) : parent_(size), rank_(size)
    {
        for (std::size_t index = 0; index < size; ++index)
            parent_[index] = index;
    }

    std::size_t find(std::size_t value)
    {
        if (parent_[value] != value) parent_[value] = find(parent_[value]);
        return parent_[value];
    }

    void unite(std::size_t first, std::size_t second)
    {
        first = find(first);
        second = find(second);
        if (first == second) return;
        if (rank_[first] < rank_[second]) std::swap(first, second);
        parent_[second] = first;
        if (rank_[first] == rank_[second]) ++rank_[first];
    }

private:
    std::vector<std::size_t> parent_;
    std::vector<unsigned char> rank_;
};

std::string boundarySortKey(const FinalOwnershipBoundaryTelemetry& boundary,
    CompoundComponentRole sourceRole, CompoundComponentRole ownerRole,
    const SampleClaim& claim, std::size_t directionOrdinal)
{
    // Eligibility is role-aware before ranking.  Keep the role match first,
    // then the already-contractual probe-local distance, physical coordinate,
    // and fixed four-neighbor direction.  The fixed-width representation makes
    // the serialized provenance key identical to the lexical ordering key.
    const int roleRank = sourceRole == ownerRole ? 0 : 1;
    std::ostringstream stream;
    stream << "role_rank=" << roleRank << '|'
           << "normal_distance=" << std::fixed << std::setprecision(12)
           << std::setw(18) << std::setfill('0') << claim.normalDistance << '|'
           << "tangent_distance=" << std::setw(18) << std::setfill('0')
           << claim.tangentDistance << '|'
           << "owner_y=" << std::setw(8) << std::setfill('0')
           << boundary.ownerYPhysicalPx << '|'
           << "owner_x=" << std::setw(8) << std::setfill('0')
           << boundary.ownerXPhysicalPx << '|'
           << "direction=" << directionOrdinal << '|'
           << "sample=" << std::setw(4) << std::setfill('0')
           << boundary.sampleIndex << '|'
           << "class=" << boundary.boundaryClass;
    return stream.str();
}

bool roleAllowsBoundary(CompoundComponentRole sourceRole,
    CompoundComponentRole ownerRole,
    const std::string& boundaryClass,
    CompoundComponentRole adjacentRole)
{
    if (sourceRole == CompoundComponentRole::OuterHaloEdge ||
            ownerRole == CompoundComponentRole::OuterHaloEdge)
        return boundaryClass == "compound_outer_silhouette";
    if (boundaryClass == "compound_outer_silhouette") return true;
    return boundaryClass == "same_track_compound_inner_edge" &&
        adjacentRole == CompoundComponentRole::OuterHaloEdge;
}

}  // namespace

const char* finalSemanticTrackName(FinalSemanticTrack track)
{
    switch (track) {
    case FinalSemanticTrack::Predicted: return "predicted";
    case FinalSemanticTrack::Actual: return "actual";
    case FinalSemanticTrack::None: return "none";
    }
    return "none";
}

IntegratedFinalOwnershipResult resolveIntegratedFinalSemanticOwnership(
    const IntegratedFinalOwnershipRequest& request)
{
    IntegratedFinalOwnershipResult result;
    const IntegratedFinalOwnershipFrame& frame = request.frame;
    auto fail = [&](const std::string& error) {
        if (result.errorCode.empty()) result.errorCode = error;
    };
    if (frame.captureId.empty() || frame.colorFrameId.empty() ||
        frame.semanticFrameId.empty() ||
        frame.colorFrameId != frame.semanticFrameId ||
        frame.colorContentSha256.size() != 64 ||
        frame.semanticContentSha256.size() != 64)
        fail("final_ownership_frame_or_digest_mismatch");
    if (frame.physicalWidth <= 0 || frame.physicalHeight <= 0 ||
        (frame.msaaSamples != 1 && frame.msaaSamples != 2 &&
            frame.msaaSamples != 4) || frame.msaaSamplePositions.size() !=
            static_cast<std::size_t>(frame.msaaSamples))
        fail("final_ownership_unsupported_dimensions_or_msaa");
    std::string graphError;
    if (!validGraph(frame, graphError)) fail(graphError);
    if (request.sourceProbes.size() != request.expectedSourceProbeCount)
        fail("final_ownership_probe_denominator_drift");
    std::set<std::string> uniqueProbeIds;
    for (const FinalOwnershipSourceProbe& probe : request.sourceProbes) {
        const double tangentLength = std::hypot(probe.tangentX, probe.tangentY);
        const double normalLength = std::hypot(probe.normalX, probe.normalY);
        const double dot = probe.tangentX * probe.normalX +
            probe.tangentY * probe.normalY;
        if (std::fabs(tangentLength - 1.0) > 1e-6 ||
            std::fabs(normalLength - 1.0) > 1e-6 || std::fabs(dot) > 1e-6)
            fail("final_ownership_probe_coordinate_frame_invalid");
        const std::string id = stableCompoundSourceProbeId(probe.source);
        if (!uniqueProbeIds.insert(id).second)
            fail("final_ownership_probe_id_duplicate");
    }
    std::map<std::pair<int, int>, std::size_t> pixelIndexByCoordinate;
    for (std::size_t pixelIndex = 0; pixelIndex < frame.pixels.size();
            ++pixelIndex) {
        const FinalSemanticPixel& pixel = frame.pixels[pixelIndex];
        if (pixel.xPhysicalPx < 0 || pixel.yPhysicalPx < 0 ||
            pixel.xPhysicalPx >= frame.physicalWidth ||
            pixel.yPhysicalPx >= frame.physicalHeight ||
            pixel.sampleOwnerTokens.size() !=
                static_cast<std::size_t>(frame.msaaSamples) ||
            pixel.laterUiCoverage < 0.0 || pixel.laterUiCoverage > 1.0)
            fail("final_ownership_sample_payload_incomplete");
        for (std::uint32_t token : pixel.sampleOwnerTokens) {
            if (token != 0 && frame.ownerTable.find(token) ==
                    frame.ownerTable.end())
                fail("final_ownership_owner_token_unresolved");
        }
        if (!pixelIndexByCoordinate.emplace(
                std::make_pair(pixel.xPhysicalPx, pixel.yPhysicalPx),
                pixelIndex).second)
            fail("final_ownership_pixel_payload_duplicate");
    }
    if (!result.errorCode.empty()) return result;

    // Build final same-track, four-connected semantic ownership islands before
    // per-source exclusive area arbitration.  Island identity is derived from
    // the minimum physical sample coordinate so it is stable under payload
    // iteration order.
    const std::size_t semanticNodeCount = frame.pixels.size() *
        static_cast<std::size_t>(frame.msaaSamples);
    OwnedIslandUnionFind islands(semanticNodeCount);
    auto semanticNodeIndex = [&](std::size_t pixelIndex, int sample) {
        return pixelIndex * static_cast<std::size_t>(frame.msaaSamples) +
            static_cast<std::size_t>(sample);
    };
    auto validOwnedNode = [&](std::size_t pixelIndex, int sample) {
        return frame.pixels[pixelIndex].laterUiCoverage <= 0.0 &&
            frame.pixels[pixelIndex].sampleOwnerTokens[
                static_cast<std::size_t>(sample)] != 0;
    };
    const std::array<std::array<int, 2>, 2> islandDirections{{
        {{1, 0}}, {{0, 1}}}};
    for (std::size_t pixelIndex = 0; pixelIndex < frame.pixels.size();
            ++pixelIndex) {
        const FinalSemanticPixel& pixel = frame.pixels[pixelIndex];
        for (int sample = 0; sample < frame.msaaSamples; ++sample) {
            if (!validOwnedNode(pixelIndex, sample)) continue;
            const FinalSemanticOwner& owner = frame.ownerTable.at(
                pixel.sampleOwnerTokens[static_cast<std::size_t>(sample)]);
            for (const auto& direction : islandDirections) {
                const auto found = pixelIndexByCoordinate.find({
                    pixel.xPhysicalPx + direction[0],
                    pixel.yPhysicalPx + direction[1]});
                if (found == pixelIndexByCoordinate.end() ||
                        !validOwnedNode(found->second, sample))
                    continue;
                const FinalSemanticPixel& adjacent =
                    frame.pixels[found->second];
                const FinalSemanticOwner& adjacentOwner = frame.ownerTable.at(
                    adjacent.sampleOwnerTokens[
                        static_cast<std::size_t>(sample)]);
                if (adjacentOwner.track == owner.track)
                    islands.unite(semanticNodeIndex(pixelIndex, sample),
                        semanticNodeIndex(found->second, sample));
            }
        }
    }
    std::map<std::size_t, std::string> minimumIslandNodeLabel;
    for (std::size_t pixelIndex = 0; pixelIndex < frame.pixels.size();
            ++pixelIndex) {
        const FinalSemanticPixel& pixel = frame.pixels[pixelIndex];
        for (int sample = 0; sample < frame.msaaSamples; ++sample) {
            if (!validOwnedNode(pixelIndex, sample)) continue;
            const std::size_t root = islands.find(
                semanticNodeIndex(pixelIndex, sample));
            const FinalSemanticOwner& owner = frame.ownerTable.at(
                pixel.sampleOwnerTokens[static_cast<std::size_t>(sample)]);
            std::ostringstream label;
            label << finalSemanticTrackName(owner.track) << ':'
                  << std::setw(8) << std::setfill('0') << pixel.yPhysicalPx
                  << ':' << std::setw(8) << std::setfill('0')
                  << pixel.xPhysicalPx << ':' << std::setw(4)
                  << std::setfill('0') << sample;
            const auto found = minimumIslandNodeLabel.find(root);
            if (found == minimumIslandNodeLabel.end() ||
                    label.str() < found->second)
                minimumIslandNodeLabel[root] = label.str();
        }
    }
    std::vector<std::string> ownedIslandIds(semanticNodeCount);
    for (std::size_t pixelIndex = 0; pixelIndex < frame.pixels.size();
            ++pixelIndex) {
        for (int sample = 0; sample < frame.msaaSamples; ++sample) {
            if (!validOwnedNode(pixelIndex, sample)) continue;
            const std::size_t node = semanticNodeIndex(pixelIndex, sample);
            ownedIslandIds[node] = "owned-island:" +
                minimumIslandNodeLabel.at(islands.find(node));
        }
    }

    std::vector<std::string> observedReason(request.sourceProbes.size(),
        "no_same_track_final_owner");
    std::map<std::string, std::vector<SampleClaim>> claimsBySample;
    for (std::size_t probeIndex = 0;
            probeIndex < request.sourceProbes.size(); ++probeIndex) {
        const FinalOwnershipSourceProbe& probe = request.sourceProbes[probeIndex];
        if (!probe.source.insideFinalViewport) {
            observedReason[probeIndex] = "out_of_bounds_or_clipped";
            continue;
        }
        bool sawUi = false;
        bool sawForeign = false;
        bool sawSameTrackOutside = false;
        bool sawBadComponent = false;
        const double scanRadius = tangentBound(probe.source) +
            normalBound(probe.source) + 2.0;
        const int minX = std::max(0, static_cast<int>(std::floor(
            probe.source.xPhysicalPx - scanRadius)));
        const int maxX = std::min(frame.physicalWidth - 1,
            static_cast<int>(std::ceil(
                probe.source.xPhysicalPx + scanRadius)));
        const int minY = std::max(0, static_cast<int>(std::floor(
            probe.source.yPhysicalPx - scanRadius)));
        const int maxY = std::min(frame.physicalHeight - 1,
            static_cast<int>(std::ceil(
                probe.source.yPhysicalPx + scanRadius)));
        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                const auto foundPixel = pixelIndexByCoordinate.find({x, y});
                if (foundPixel == pixelIndexByCoordinate.end()) continue;
                const std::size_t pixelIndex = foundPixel->second;
                const FinalSemanticPixel& pixel = frame.pixels[pixelIndex];
                const double dx = pixel.xPhysicalPx -
                    probe.source.xPhysicalPx;
                const double dy = pixel.yPhysicalPx -
                    probe.source.yPhysicalPx;
                const double tangent = dx * probe.tangentX +
                    dy * probe.tangentY;
                const double normal = dx * probe.normalX +
                    dy * probe.normalY;
                if (std::fabs(normal) > normalBound(probe.source) + 2.0 ||
                    std::fabs(tangent) > tangentBound(probe.source) + 2.0)
                    continue;
                if (pixel.laterUiCoverage > 0.0) {
                    sawUi = true;
                    continue;
                }
            for (int sample = 0; sample < frame.msaaSamples; ++sample) {
                const std::uint32_t token =
                    pixel.sampleOwnerTokens[static_cast<std::size_t>(sample)];
                if (token == 0) continue;
                const FinalSemanticOwner& owner = frame.ownerTable.at(token);
                if (!sameTrack(probe.source.track, owner.track)) {
                    sawForeign = true;
                    continue;
                }
                if (!allowedComponent(probe.source.track,
                        owner.componentRole)) {
                    sawBadComponent = true;
                    continue;
                }
                if (std::fabs(normal) > normalBound(probe.source) ||
                    std::fabs(tangent) > tangentBound(probe.source)) {
                    sawSameTrackOutside = true;
                    continue;
                }
                // Endpoint and tick substitutions stay tied to the same
                // endpoint/nearest primitive interval.
                if ((owner.componentRole == CompoundComponentRole::Endpoint ||
                        owner.componentRole == CompoundComponentRole::Tick) &&
                    owner.primitiveId != probe.source.primitiveId) {
                    sawBadComponent = true;
                    continue;
                }
                const std::string key = std::to_string(pixel.xPhysicalPx) + "," +
                    std::to_string(pixel.yPhysicalPx) + ":" +
                    std::to_string(sample);
                claimsBySample[key].push_back({probeIndex, pixelIndex, sample,
                    std::fabs(tangent), std::fabs(normal),
                    probe.source.primitiveId, probe.source.probeOrdinal});
            }
            }
        }
        if (sawSameTrackOutside)
            observedReason[probeIndex] = "outside_bounded_neighborhood";
        else if (sawBadComponent)
            observedReason[probeIndex] = "component_not_eligible";
        else if (sawForeign)
            observedReason[probeIndex] = "only_other_track_visible";
        else if (sawUi)
            observedReason[probeIndex] = "later_ui_owned";
        else
            observedReason[probeIndex] = "only_background_or_scene_visible";
    }

    // Scalar area remains exclusively assigned. Raster topology is evaluated
    // independently over the bounded same-track/allowed-component support
    // that existed before per-probe arbitration split a thin path into pieces.
    std::vector<std::set<std::pair<int, int>>> preArbitrationSupportPixels(
        request.sourceProbes.size());
    for (const auto& entry : claimsBySample) {
        for (const SampleClaim& claim : entry.second) {
            const FinalSemanticPixel& pixel = frame.pixels[claim.pixelIndex];
            preArbitrationSupportPixels[claim.probeIndex].insert({
                pixel.xPhysicalPx, pixel.yPhysicalPx});
        }
    }
    std::vector<std::vector<SampleClaim>> won(request.sourceProbes.size());
    std::vector<std::string> exclusiveAreaOwnerSourceIds(semanticNodeCount,
        "unclaimed");
    for (auto& entry : claimsBySample) {
        std::vector<SampleClaim>& claims = entry.second;
        std::sort(claims.begin(), claims.end(),
            [](const SampleClaim& a, const SampleClaim& b) {
                return std::tie(a.normalDistance, a.tangentDistance,
                           a.primitiveId, a.probeOrdinal) <
                    std::tie(b.normalDistance, b.tangentDistance,
                        b.primitiveId, b.probeOrdinal);
            });
        won[claims.front().probeIndex].push_back(claims.front());
        exclusiveAreaOwnerSourceIds[semanticNodeIndex(
            claims.front().pixelIndex, claims.front().sampleIndex)] =
            stableCompoundSourceProbeId(
                request.sourceProbes[claims.front().probeIndex].source);
        FinalOwnershipArbitrationTelemetry arbitration;
        arbitration.physicalSampleOrPixelId = entry.first;
        for (const SampleClaim& claim : claims)
            arbitration.claimingSourceProbeIds.push_back(
                stableCompoundSourceProbeId(
                    request.sourceProbes[claim.probeIndex].source));
        arbitration.winnerSourceProbeId =
            arbitration.claimingSourceProbeIds.front();
        arbitration.deterministicSortKey =
            std::to_string(claims.front().normalDistance) + "|" +
            std::to_string(claims.front().tangentDistance) + "|" +
            claims.front().primitiveId + "|" +
            std::to_string(claims.front().probeOrdinal);
        arbitration.loserReason = claims.size() > 1 ?
            "pixel_already_assigned" : "";
        result.arbitrationTelemetry.push_back(arbitration);
        for (std::size_t index = 1; index < claims.size(); ++index)
            observedReason[claims[index].probeIndex] =
                "pixel_already_assigned";
    }
    std::set<std::string> probesWithArbitration;
    for (const FinalOwnershipArbitrationTelemetry& arbitration :
            result.arbitrationTelemetry)
        probesWithArbitration.insert(arbitration.claimingSourceProbeIds.begin(),
            arbitration.claimingSourceProbeIds.end());
    for (const FinalOwnershipSourceProbe& probe : request.sourceProbes) {
        const std::string id = stableCompoundSourceProbeId(probe.source);
        if (probesWithArbitration.count(id) != 0) continue;
        FinalOwnershipArbitrationTelemetry noClaim;
        noClaim.physicalSampleOrPixelId = "unowned:" + id;
        noClaim.claimingSourceProbeIds = {id};
        noClaim.deterministicSortKey = "no_eligible_final_sample";
        noClaim.loserReason = "no_same_track_final_owner";
        result.arbitrationTelemetry.push_back(noClaim);
    }

    CompoundVisibilityAudit& audit = result.audit;
    audit.sourceProbeCount = request.sourceProbes.size();
    std::vector<double> visibleContrasts;
    for (std::size_t index = 0; index < request.sourceProbes.size(); ++index) {
        const FinalOwnershipSourceProbe& probe = request.sourceProbes[index];
        const std::string sourceId = stableCompoundSourceProbeId(probe.source);
        audit.orderedSourceProbeIds.push_back(sourceId);
        CompoundTrackVisibilityCounts& track =
            probe.source.track == CompoundVisibilityTrack::Predicted ?
                audit.predicted : audit.actual;
        ++track.source;
        ++audit.componentSourceCounts[
            compoundComponentRoleName(probe.source.componentRole)];

        FinalOwnershipCandidateTelemetry telemetry;
        telemetry.sourceProbeId = sourceId;
        telemetry.xPhysicalPx = probe.source.xPhysicalPx;
        telemetry.yPhysicalPx = probe.source.yPhysicalPx;
        telemetry.ownerTrack = compoundTrackName(probe.source.track);
        telemetry.ownerComponent = compoundComponentRoleName(
            probe.source.componentRole);
        telemetry.ownerPrimitiveId = probe.source.primitiveId;
        telemetry.eligibilityOrRejectionReason = observedReason[index];

        std::set<std::pair<int, int>> ownedPixels;
        std::set<std::size_t> exclusiveOwnedNodes;
        std::set<std::string> seedOwnedIslandIds;
        std::size_t sampleCount = 0;
        double closestDistance = std::numeric_limits<double>::infinity();
        std::size_t closestPixel = 0;
        for (const SampleClaim& claim : won[index]) {
            ++sampleCount;
            const FinalSemanticPixel& pixel = frame.pixels[claim.pixelIndex];
            const std::size_t node = semanticNodeIndex(claim.pixelIndex,
                claim.sampleIndex);
            exclusiveOwnedNodes.insert(node);
            seedOwnedIslandIds.insert(ownedIslandIds[node]);
            ownedPixels.insert({pixel.xPhysicalPx, pixel.yPhysicalPx});
            const double distance = std::hypot(claim.normalDistance,
                claim.tangentDistance);
            if (distance < closestDistance) {
                closestDistance = distance;
                closestPixel = claim.pixelIndex;
                telemetry.tangentDistancePx = claim.tangentDistance;
                telemetry.normalDistancePx = claim.normalDistance;
                const FinalSemanticOwner& owner = frame.ownerTable.at(
                    pixel.sampleOwnerTokens[
                        static_cast<std::size_t>(claim.sampleIndex)]);
                telemetry.ownerTrack = finalSemanticTrackName(owner.track);
                telemetry.ownerComponent = compoundComponentRoleName(
                    owner.componentRole);
                telemetry.ownerPrimitiveId = owner.primitiveId;
                telemetry.ownerVisualSublayer = owner.visualSublayer;
            }
        }
        telemetry.resolvedSampleCount = sampleCount;
        telemetry.resolvedPixelEquivalentArea =
            static_cast<double>(sampleCount) / frame.msaaSamples;
        telemetry.exclusiveOwnedPixelCount = ownedPixels.size();
        telemetry.exclusiveLargestFourConnectedPixelCount =
            largestFourConnectedComponent(ownedPixels);
        telemetry.preArbitrationSupportPixelCount =
            preArbitrationSupportPixels[index].size();
        telemetry.preArbitrationLargestEightConnectedPixelCount =
            largestEightConnectedComponent(
                preArbitrationSupportPixels[index]);
        std::vector<std::size_t> probeBoundaryIndices;
        const std::array<std::array<int, 2>, 4> directions{{
            {{-1, 0}}, {{1, 0}}, {{0, -1}}, {{0, 1}}}};
        const std::string sourcePrimitiveStem = primitiveStem(
            probe.source.primitiveId);
        std::size_t associatedOwnedNodeCount = 0;
        for (std::size_t ownerPixelIndex = 0;
                sampleCount > 0 && ownerPixelIndex < frame.pixels.size();
                ++ownerPixelIndex) {
            const FinalSemanticPixel& ownerPixel =
                frame.pixels[ownerPixelIndex];
            if (ownerPixel.laterUiCoverage > 0.0) continue;
            const double dx = ownerPixel.xPhysicalPx -
                probe.source.xPhysicalPx;
            const double dy = ownerPixel.yPhysicalPx -
                probe.source.yPhysicalPx;
            const double tangent = dx * probe.tangentX +
                dy * probe.tangentY;
            const double normal = dx * probe.normalX +
                dy * probe.normalY;
            if (std::fabs(normal) > normalBound(probe.source) ||
                    std::fabs(tangent) > tangentBound(probe.source))
                continue;
            for (int sample = 0; sample < frame.msaaSamples; ++sample) {
                const std::uint32_t ownerToken = ownerPixel.sampleOwnerTokens[
                    static_cast<std::size_t>(sample)];
                if (ownerToken == 0) continue;
                const FinalSemanticOwner& owner = frame.ownerTable.at(ownerToken);
                if (!sameTrack(probe.source.track, owner.track) ||
                        !allowedComponent(probe.source.track,
                            owner.componentRole))
                    continue;
                const std::size_t ownerNode = semanticNodeIndex(
                    ownerPixelIndex, sample);
                const std::string& islandId = ownedIslandIds[ownerNode];
                std::string associationMode;
                if (exclusiveOwnedNodes.count(ownerNode) != 0)
                    associationMode = "exclusive_area_owner";
                else if (primitiveStem(owner.primitiveId) ==
                        sourcePrimitiveStem)
                    associationMode = "exact_primitive_stem";
                else if (seedOwnedIslandIds.count(islandId) != 0)
                    associationMode = "owned_island_component";
                else
                    continue;
                ++associatedOwnedNodeCount;
            for (std::size_t directionOrdinal = 0;
                    directionOrdinal < directions.size();
                    ++directionOrdinal) {
                const auto& direction = directions[directionOrdinal];
                const auto adjacentFound = pixelIndexByCoordinate.find({
                    ownerPixel.xPhysicalPx + direction[0],
                    ownerPixel.yPhysicalPx + direction[1]});
                if (adjacentFound == pixelIndexByCoordinate.end()) continue;
                const FinalSemanticPixel& adjacentPixel =
                    frame.pixels[adjacentFound->second];
                const std::uint32_t adjacentToken =
                    adjacentPixel.sampleOwnerTokens[
                        static_cast<std::size_t>(sample)];
                if (adjacentToken == ownerToken &&
                        adjacentPixel.laterUiCoverage <= 0.0)
                    continue;
                FinalOwnershipBoundaryTelemetry boundary;
                boundary.sourceProbeId = sourceId;
                boundary.ownerXPhysicalPx = ownerPixel.xPhysicalPx;
                boundary.ownerYPhysicalPx = ownerPixel.yPhysicalPx;
                boundary.adjacentXPhysicalPx = adjacentPixel.xPhysicalPx;
                boundary.adjacentYPhysicalPx = adjacentPixel.yPhysicalPx;
                boundary.sampleIndex = sample;
                boundary.ownerTrack = finalSemanticTrackName(owner.track);
                boundary.ownerComponent = compoundComponentRoleName(
                    owner.componentRole);
                boundary.ownerPrimitiveId = owner.primitiveId;
                boundary.ownerVisualSublayer = owner.visualSublayer;
                boundary.ownerFinalSrgb8 = ownerPixel.finalLdrSrgb8;
                boundary.adjacentFinalSrgb8 = adjacentPixel.finalLdrSrgb8;
                boundary.edgeContrastRatio = contrast(
                    boundary.ownerFinalSrgb8, boundary.adjacentFinalSrgb8);
                CompoundComponentRole adjacentRole =
                    CompoundComponentRole::Core;
                if (adjacentPixel.laterUiCoverage > 0.0) {
                    boundary.adjacentLaterUiOwned = true;
                    boundary.adjacentTrack = "none";
                    boundary.adjacentComponent = "ui";
                    boundary.adjacentVisualSublayer = "ui";
                    boundary.boundaryClass = "later_ui_edge";
                } else if (adjacentToken == 0) {
                    boundary.adjacentTrack = "none";
                    boundary.adjacentComponent = "scene_or_background";
                    boundary.adjacentVisualSublayer = "scene_or_background";
                    boundary.boundaryClass = "compound_outer_silhouette";
                } else {
                    const FinalSemanticOwner& adjacentOwner =
                        frame.ownerTable.at(adjacentToken);
                    adjacentRole = adjacentOwner.componentRole;
                    boundary.adjacentTrack =
                        finalSemanticTrackName(adjacentOwner.track);
                    boundary.adjacentComponent = compoundComponentRoleName(
                        adjacentOwner.componentRole);
                    boundary.adjacentPrimitiveId = adjacentOwner.primitiveId;
                    boundary.adjacentVisualSublayer =
                        adjacentOwner.visualSublayer;
                    if (adjacentOwner.track != owner.track) {
                        boundary.boundaryClass = "foreign_track_edge";
                    } else if (adjacentOwner.componentRole ==
                            owner.componentRole) {
                        boundary.boundaryClass =
                            "same_component_token_seam";
                    } else {
                        boundary.boundaryClass =
                            "same_track_compound_inner_edge";
                    }
                }
                boundary.eligibleForRole = roleAllowsBoundary(
                    probe.source.componentRole, owner.componentRole,
                    boundary.boundaryClass, adjacentRole);
                boundary.associationMode = associationMode;
                boundary.associationSeedPrimitiveStem = sourcePrimitiveStem;
                boundary.associatedOwnedIslandId = islandId;
                boundary.boundaryAreaOwnerSourceProbeId =
                    exclusiveAreaOwnerSourceIds[ownerNode];
                SampleClaim associatedClaim;
                associatedClaim.normalDistance = std::fabs(normal);
                associatedClaim.tangentDistance = std::fabs(tangent);
                boundary.deterministicSortKey = boundarySortKey(boundary,
                    probe.source.componentRole, owner.componentRole,
                    associatedClaim, directionOrdinal);
                probeBoundaryIndices.push_back(result.boundaryTelemetry.size());
                result.boundaryTelemetry.push_back(boundary);
            }
            }
        }
        std::vector<std::size_t> eligibleBoundaryIndices;
        for (std::size_t boundaryIndex : probeBoundaryIndices)
            if (result.boundaryTelemetry[boundaryIndex].eligibleForRole)
                eligibleBoundaryIndices.push_back(boundaryIndex);
        std::sort(eligibleBoundaryIndices.begin(), eligibleBoundaryIndices.end(),
            [&](std::size_t first, std::size_t second) {
                return result.boundaryTelemetry[first].deterministicSortKey <
                    result.boundaryTelemetry[second].deterministicSortKey;
            });
        if (!eligibleBoundaryIndices.empty()) {
            FinalOwnershipBoundaryTelemetry& selected =
                result.boundaryTelemetry[eligibleBoundaryIndices.front()];
            selected.selected = true;
            telemetry.xPhysicalPx = selected.ownerXPhysicalPx;
            telemetry.yPhysicalPx = selected.ownerYPhysicalPx;
            telemetry.finalSrgb8 = selected.ownerFinalSrgb8;
            telemetry.adjacentNonSelfSrgb8 = selected.adjacentFinalSrgb8;
            telemetry.edgeContrastRatio = selected.edgeContrastRatio;
        } else if (sampleCount > 0) {
            telemetry.eligibilityOrRejectionReason =
                probeBoundaryIndices.empty() || associatedOwnedNodeCount == 0 ?
                "component_aware_boundary_payload_missing" :
                "no_eligible_component_aware_boundary";
            if (probeBoundaryIndices.empty() || associatedOwnedNodeCount == 0)
                fail("final_ownership_boundary_payload_incomplete");
        }
        bool visible = sampleCount > 0 && !eligibleBoundaryIndices.empty();
        if (visible && telemetry.resolvedPixelEquivalentArea <
                request.minimumResolvedPixelEquivalentArea) {
            visible = false;
            telemetry.eligibilityOrRejectionReason =
                "insufficient_exclusive_owned_area";
        }
        if (visible &&
                telemetry.preArbitrationLargestEightConnectedPixelCount <
                    request.minimumRasterConnectedPhysicalPixels) {
            visible = false;
            telemetry.eligibilityOrRejectionReason =
                "insufficient_raster_continuity";
        }
        if (visible && telemetry.edgeContrastRatio <
                request.minimumEdgeContrastRatio) {
            visible = false;
            telemetry.eligibilityOrRejectionReason =
                "final_edge_contrast_below_4";
        }
        if (visible) {
            telemetry.eligibilityOrRejectionReason = "eligible";
            audit.orderedVisibleProbeIds.push_back(sourceId);
            ++audit.visibleProbeCount;
            ++track.visible;
            ++audit.componentVisibleCounts[
                compoundComponentRoleName(probe.source.componentRole)];
            visibleContrasts.push_back(telemetry.edgeContrastRatio);
        } else {
            ++audit.hiddenProbeCount;
            ++track.hidden;
            const CompoundHiddenReason hiddenReason = reasonFromString(
                telemetry.eligibilityOrRejectionReason);
            audit.hiddenProbes.push_back({sourceId, probe.source.track,
                probe.source.componentRole, hiddenReason,
                probe.source.xPhysicalPx, probe.source.yPhysicalPx});
            ++audit.hiddenReasonCounts[compoundHiddenReasonName(hiddenReason)];
            ++audit.componentHiddenReasonCounts[
                std::string(compoundComponentRoleName(
                    probe.source.componentRole)) + "|" +
                compoundHiddenReasonName(hiddenReason)];
            if (hiddenReason == CompoundHiddenReason::OnlyOtherTrackVisible)
                ++audit.foreignTrackEvidenceCount;
            if (hiddenReason ==
                    CompoundHiddenReason::OnlyBackgroundOrSceneVisible)
                ++audit.unownedEvidenceCount;
        }
        result.candidateTelemetry.push_back(telemetry);
    }
    audit.orderedSourceProbeDigest = joinedDigest(audit.orderedSourceProbeIds);
    audit.orderedVisibleProbeDigest = joinedDigest(audit.orderedVisibleProbeIds);
    audit.visibleProbeCoverage = audit.sourceProbeCount == 0 ? 0.0 :
        static_cast<double>(audit.visibleProbeCount) / audit.sourceProbeCount;
    audit.predicted.coverage = audit.predicted.source == 0 ? 0.0 :
        static_cast<double>(audit.predicted.visible) / audit.predicted.source;
    audit.actual.coverage = audit.actual.source == 0 ? 0.0 :
        static_cast<double>(audit.actual.visible) / audit.actual.source;
    if (!visibleContrasts.empty())
        audit.minimumVisibleEdgeContrastRatio = *std::min_element(
            visibleContrasts.begin(), visibleContrasts.end());
    audit.ok = result.errorCode.empty() &&
        audit.visibleProbeCoverage >= request.minimumVisibleCoverage &&
        (audit.predicted.source == 0 ||
            audit.predicted.coverage >= request.minimumVisibleCoverage) &&
        (audit.actual.source == 0 ||
            audit.actual.coverage >= request.minimumVisibleCoverage);
    if (!audit.ok && audit.errorCode.empty()) audit.errorCode =
        result.errorCode.empty() ? "compound_visibility_below_contract" :
        result.errorCode;
    result.ok = audit.ok;
    result.errorCode = audit.errorCode;
    return result;
}

FinalTrajectoryGeometryAudit auditIntegratedFinalTrajectoryGeometry(
    const IntegratedFinalOwnershipFrame& frame,
    const std::vector<FinalOwnershipSourceProbe>& sourceProbes,
    const TrajectoryOverlayStyle& style, double tolerancePx)
{
    FinalTrajectoryGeometryAudit result;
    const auto fail = [&](const std::string& error) {
        if (result.errorCode.empty()) result.errorCode = error;
    };
    if (frame.physicalWidth <= 0 || frame.physicalHeight <= 0 ||
        frame.msaaSamples <= 0 || frame.msaaSamplePositions.size() !=
            static_cast<std::size_t>(frame.msaaSamples) ||
        !std::isfinite(tolerancePx) || tolerancePx < 0.0 ||
        !std::isfinite(style.lineWidthPx) || style.lineWidthPx <= 0.0 ||
        !std::isfinite(style.haloWidthPx) || style.haloWidthPx < 0.0 ||
        !std::isfinite(style.outerContrastRimWidthPx) ||
        style.outerContrastRimWidthPx < 0.0 ||
        !std::isfinite(style.actualTickLengthPx) ||
        style.actualTickLengthPx <= 0.0 ||
        !std::isfinite(style.actualTickSpacingPx) ||
        style.actualTickSpacingPx <= 0.0 ||
        !std::isfinite(style.endpointOuterDiameterPx) ||
        style.endpointOuterDiameterPx <= 0.0 ||
        !std::isfinite(style.actualEndpointDotDiameterPx) ||
        style.actualEndpointDotDiameterPx <= 0.0)
        fail("final_trajectory_geometry_input_invalid");
    for (const auto& sample : frame.msaaSamplePositions)
        if (!std::isfinite(sample[0]) || !std::isfinite(sample[1]) ||
                sample[0] < 0.0f || sample[0] > 1.0f ||
                sample[1] < 0.0f || sample[1] > 1.0f)
            fail("final_trajectory_geometry_input_invalid");
    std::set<std::pair<int, int>> framePixelCoordinates;
    for (const FinalSemanticPixel& pixel : frame.pixels) {
        if (pixel.xPhysicalPx < 0 || pixel.yPhysicalPx < 0 ||
                pixel.xPhysicalPx >= frame.physicalWidth ||
                pixel.yPhysicalPx >= frame.physicalHeight ||
                pixel.sampleOwnerTokens.size() !=
                    static_cast<std::size_t>(frame.msaaSamples) ||
                !std::isfinite(pixel.laterUiCoverage) ||
                pixel.laterUiCoverage < 0.0 ||
                pixel.laterUiCoverage > 1.0) {
            fail("final_trajectory_geometry_pixel_payload_invalid");
            continue;
        }
        if (!framePixelCoordinates.emplace(pixel.xPhysicalPx,
                pixel.yPhysicalPx).second) {
            fail("final_trajectory_geometry_pixel_payload_invalid");
            continue;
        }
        for (std::uint32_t token : pixel.sampleOwnerTokens)
            if (token != 0 && frame.ownerTable.find(token) ==
                    frame.ownerTable.end())
                fail("final_trajectory_geometry_owner_unresolved");
        const bool hasTrajectory = std::any_of(
            pixel.sampleOwnerTokens.begin(), pixel.sampleOwnerTokens.end(),
            [](std::uint32_t token) { return token != 0; });
        if (!hasTrajectory) continue;
        const double chrome = pixel.interactiveChromeCoverage.value_or(
            pixel.laterUiCoverage);
        if (!std::isfinite(chrome) || chrome < 0.0 || chrome > 1.0) {
            fail("final_trajectory_geometry_pixel_payload_invalid");
            continue;
        }
        if (chrome > 0.0 && chrome < 1.0)
            ++result.partialUiOverlapPixelCount;
        else if (chrome >= 1.0)
            ++result.fullyUiCoveredTrajectoryPixelCount;
    }
    if (result.partialUiOverlapPixelCount != 0)
        fail("final_trajectory_partially_visible_under_ui");
    if (!result.errorCode.empty() &&
            result.errorCode != "final_trajectory_partially_visible_under_ui")
        return result;

    const double sampleCellWidth = 1.0 /
        std::sqrt(static_cast<double>(frame.msaaSamples));
    // The core + inner halo is the guaranteed compound silhouette.  The
    // adaptive outer rim is deliberately allowed to discard fragments after
    // comparing against the already composited backdrop, so it is an upper
    // envelope, never a mandatory width contribution.
    const double guaranteedCompoundWidth = style.lineWidthPx +
        2.0 * style.haloWidthPx;
    const double maximumCompoundWidth = guaranteedCompoundWidth +
        2.0 * style.outerContrastRimWidthPx;
    const auto expectedTrack = [](CompoundVisibilityTrack track) {
        return track == CompoundVisibilityTrack::Predicted ?
            FinalSemanticTrack::Predicted : FinalSemanticTrack::Actual;
    };
    for (const FinalOwnershipSourceProbe& source : sourceProbes) {
        FinalTrajectoryGeometryProbeAudit audit;
        audit.sourceProbeId = stableCompoundSourceProbeId(source.source);
        audit.componentRole = source.source.componentRole;
        const bool tick = source.source.componentRole ==
            CompoundComponentRole::Tick;
        const bool endpoint = source.source.componentRole ==
            CompoundComponentRole::Endpoint;
        const bool body = source.source.componentRole ==
                CompoundComponentRole::Core ||
            source.source.componentRole ==
                CompoundComponentRole::SolidInPredictedDashGap;
        audit.required = tick || endpoint || body;
        const double geometryX = source.geometryCenter ?
            (*source.geometryCenter)[0] : source.source.xPhysicalPx;
        const double geometryY = source.geometryCenter ?
            (*source.geometryCenter)[1] : source.source.yPhysicalPx;
        const double tangentLength = std::hypot(source.tangentX,
            source.tangentY);
        const double normalLength = std::hypot(source.normalX,
            source.normalY);
        const double tangentNormalDot = source.tangentX * source.normalX +
            source.tangentY * source.normalY;
        if (!std::isfinite(geometryX) || !std::isfinite(geometryY) ||
                !std::isfinite(tangentLength) ||
                !std::isfinite(normalLength) ||
                !std::isfinite(tangentNormalDot) ||
                std::fabs(tangentLength - 1.0) > 1e-6 ||
                std::fabs(normalLength - 1.0) > 1e-6 ||
                std::fabs(tangentNormalDot) > 1e-6 ||
                !std::isfinite(source.source.sourceProbeSpacingPx) ||
                source.source.sourceProbeSpacingPx <= 0.0 ||
                source.source.xPhysicalPx < 0 ||
                source.source.yPhysicalPx < 0 ||
                source.source.xPhysicalPx >= frame.physicalWidth ||
                source.source.yPhysicalPx >= frame.physicalHeight) {
            fail("final_trajectory_geometry_source_probe_invalid");
            audit.passed = false;
            result.probes.push_back(std::move(audit));
            continue;
        }
        audit.expectedNormalWidthPx = endpoint ?
            (source.source.track == CompoundVisibilityTrack::Predicted ?
                style.endpointOuterDiameterPx +
                    2.0 * style.haloWidthPx :
                style.actualEndpointDotDiameterPx +
                    2.0 * style.haloWidthPx) :
            guaranteedCompoundWidth;
        audit.expectedTangentSpanPx = tick ?
            style.actualTickLengthPx : (endpoint ?
                audit.expectedNormalWidthPx : 0.0);

        const std::string sourceStem = primitiveStem(
            source.source.primitiveId);
        std::map<int, std::set<int>> normalBinsByTangentBin;
        std::map<int, std::set<int>> guaranteedBinsByTangentBin;
        std::set<std::pair<int, int>> ownedPixels;
        std::set<std::pair<int, int>> guaranteedOwnedPixels;
        double minimumTangent = std::numeric_limits<double>::infinity();
        double maximumTangent = -std::numeric_limits<double>::infinity();
        double minimumAxisX = std::numeric_limits<double>::infinity();
        double maximumAxisX = -std::numeric_limits<double>::infinity();
        double minimumAxisY = std::numeric_limits<double>::infinity();
        double maximumAxisY = -std::numeric_limits<double>::infinity();
        double nearestCenterDistance = std::numeric_limits<double>::infinity();
        for (const FinalSemanticPixel& pixel : frame.pixels) {
            for (int sampleIndex = 0; sampleIndex < frame.msaaSamples;
                    ++sampleIndex) {
                const std::uint32_t token = pixel.sampleOwnerTokens[
                    static_cast<std::size_t>(sampleIndex)];
                if (token == 0) continue;
                const auto ownerFound = frame.ownerTable.find(token);
                if (ownerFound == frame.ownerTable.end()) continue;
                const FinalSemanticOwner& owner = ownerFound->second;
                const auto& sample = frame.msaaSamplePositions[
                    static_cast<std::size_t>(sampleIndex)];
                const double x = pixel.xPhysicalPx + sample[0];
                const double y = pixel.yPhysicalPx + (1.0 - sample[1]);
                const double dx = x - geometryX;
                const double dy = y - geometryY;
                const double tangent = dx * source.tangentX +
                    dy * source.tangentY;
                const double normal = dx * source.normalX +
                    dy * source.normalY;
                const double centerDistance = std::hypot(dx, dy);
                if (centerDistance < nearestCenterDistance &&
                        pixel.laterUiCoverage < 1.0) {
                    nearestCenterDistance = centerDistance;
                    audit.centerOwnerTrack = finalSemanticTrackName(
                        owner.track);
                    audit.centerOwnerComponent = compoundComponentRoleName(
                        owner.componentRole);
                    audit.centerOwnerPrimitiveId = owner.primitiveId;
                }
                if (pixel.laterUiCoverage >= 1.0 ||
                        owner.track != expectedTrack(source.source.track) ||
                        primitiveStem(owner.primitiveId) != sourceStem)
                    continue;
                const double requiredBodySpan = std::min(3.0,
                    source.source.sourceProbeSpacingPx * 0.25);
                const double tangentBound = tick ?
                        style.actualTickLengthPx * 0.5 +
                            style.haloWidthPx +
                            style.outerContrastRimWidthPx + tolerancePx + 1.0 :
                    (endpoint ? audit.expectedNormalWidthPx * 0.5 +
                            style.outerContrastRimWidthPx + tolerancePx + 1.0 :
                        requiredBodySpan * 0.5 + tolerancePx);
                const double normalBound =
                    audit.expectedNormalWidthPx * 0.5 + tolerancePx + 1.0;
                if (std::fabs(tangent) > tangentBound ||
                        std::fabs(normal) > normalBound)
                    continue;
                const int tangentBin = static_cast<int>(
                    std::floor(tangent / sampleCellWidth));
                const int normalBin = static_cast<int>(
                    std::floor(normal / sampleCellWidth));
                normalBinsByTangentBin[tangentBin].insert(normalBin);
                ownedPixels.emplace(pixel.xPhysicalPx, pixel.yPhysicalPx);
                const bool guaranteedSublayer =
                    owner.visualSublayer != "outer_rim";
                if (guaranteedSublayer) {
                    guaranteedBinsByTangentBin[tangentBin].insert(normalBin);
                    guaranteedOwnedPixels.emplace(pixel.xPhysicalPx,
                        pixel.yPhysicalPx);
                }
                minimumTangent = std::min(minimumTangent, tangent);
                maximumTangent = std::max(maximumTangent, tangent);
                minimumAxisX = std::min(minimumAxisX, dx);
                maximumAxisX = std::max(maximumAxisX, dx);
                minimumAxisY = std::min(minimumAxisY, dy);
                maximumAxisY = std::max(maximumAxisY, dy);
            }
        }
        std::vector<int> validTangentBins;
        std::size_t occupiedSampleBinCount = 0;
        for (const auto& bin : normalBinsByTangentBin) {
            if (bin.second.empty()) continue;
            const int minimumNormalBin = *bin.second.begin();
            const int maximumNormalBin = *bin.second.rbegin();
            const double sectionWidth =
                (maximumNormalBin - minimumNormalBin) * sampleCellWidth +
                    sampleCellWidth;
            audit.measuredNormalWidthPx = std::max(
                audit.measuredNormalWidthPx,
                sectionWidth);
        }
        for (const auto& bin : guaranteedBinsByTangentBin) {
            occupiedSampleBinCount += bin.second.size();
            if (bin.second.empty()) continue;
            const int minimumNormalBin = *bin.second.begin();
            const int maximumNormalBin = *bin.second.rbegin();
            const bool normalContiguous = bin.second.size() ==
                static_cast<std::size_t>(maximumNormalBin -
                    minimumNormalBin + 1);
            const double sectionWidth =
                (maximumNormalBin - minimumNormalBin) * sampleCellWidth +
                    sampleCellWidth;
            if (normalContiguous && sectionWidth + tolerancePx >=
                    audit.expectedNormalWidthPx)
                validTangentBins.push_back(bin.first);
        }
        if (minimumTangent <= maximumTangent)
            audit.measuredTangentSpanPx = maximumTangent - minimumTangent +
                sampleCellWidth;
        if (endpoint && minimumAxisX <= maximumAxisX &&
                minimumAxisY <= maximumAxisY) {
            audit.measuredNormalWidthPx = std::min(
                maximumAxisX - minimumAxisX + sampleCellWidth,
                maximumAxisY - minimumAxisY + sampleCellWidth);
            audit.measuredTangentSpanPx = std::max(
                maximumAxisX - minimumAxisX + sampleCellWidth,
                maximumAxisY - minimumAxisY + sampleCellWidth);
        }
        const bool connected = guaranteedOwnedPixels.empty() ? false :
            largestEightConnectedComponent(guaranteedOwnedPixels) ==
                guaranteedOwnedPixels.size();
        audit.guaranteedConnected = connected;
        bool shapeOk = connected;
        if (body) {
            const double requiredBodySpan = std::min(3.0,
                source.source.sourceProbeSpacingPx * 0.25);
            bool tangentCoverageContinuous =
                !guaranteedBinsByTangentBin.empty();
            int previousTangentBin = 0;
            bool firstTangentBin = true;
            for (const auto& bin : guaranteedBinsByTangentBin) {
                if (!firstTangentBin &&
                        (bin.first - previousTangentBin) *
                            sampleCellWidth > 1.5)
                    tangentCoverageContinuous = false;
                firstTangentBin = false;
                previousTangentBin = bin.first;
            }
            const double occupiedArea = occupiedSampleBinCount *
                sampleCellWidth * sampleCellWidth;
            audit.measuredGuaranteedAreaPx2 = occupiedArea;
            const double minimumOccupiedArea = requiredBodySpan *
                style.lineWidthPx * 0.55;
            // A rotated MSAA line need not occupy every quantized normal bin
            // in every tangent slice.  Connectivity, tangent coverage and
            // area density reject a thin bridge or isolated extrema without
            // imposing an axis-aligned raster pattern.
            shapeOk = tangentCoverageContinuous &&
                audit.measuredTangentSpanPx + tolerancePx >=
                    requiredBodySpan &&
                occupiedArea + tolerancePx >= minimumOccupiedArea;
        } else if (tick) {
            // Test the actual GPU sample lattice in two fixed authored wing
            // rectangles. Empty cells in a synthetic half-pixel grid are not
            // missing samples. Every real sample inside either wing must be
            // owned by this tick's guaranteed core/inner halo: a missing row,
            // thin bridge, other primitive or endpoint cannot rescue it.
            std::map<std::pair<int, int>, const FinalSemanticPixel*> pixels;
            const double halfLength = style.actualTickLengthPx * 0.5;
            const double halfWidth = guaranteedCompoundWidth * 0.5;
            const double centerExclusion = maximumCompoundWidth * 0.5;
            const double radius = halfLength + halfWidth + 1.0;
            for (const auto& pixel : frame.pixels)
                if (std::fabs(pixel.xPhysicalPx - geometryX) <= radius + 1.0 &&
                    std::fabs(pixel.yPhysicalPx - geometryY) <= radius + 1.0)
                    pixels[{pixel.xPhysicalPx, pixel.yPhysicalPx}] = &pixel;
            std::array<std::size_t, 2> expected{{0, 0}};
            bool allOwned = true;
            for (int y = std::max(0, static_cast<int>(std::floor(geometryY-radius)));
                 y < std::min(frame.physicalHeight, static_cast<int>(std::ceil(geometryY+radius))); ++y)
                for (int x = std::max(0, static_cast<int>(std::floor(geometryX-radius)));
                     x < std::min(frame.physicalWidth, static_cast<int>(std::ceil(geometryX+radius))); ++x)
                    for (int i = 0; i < frame.msaaSamples; ++i) {
                        const auto& offset = frame.msaaSamplePositions[i];
                        const double dx = x + offset[0] - geometryX;
                        const double dy = y + 1.0 - offset[1] - geometryY;
                        const double along = dx*source.tangentX + dy*source.tangentY;
                        const double normal = dx*source.normalX + dy*source.normalY;
                        if (std::fabs(along) <= centerExclusion + 1e-5 ||
                            std::fabs(along) >= halfLength - 1e-5 ||
                            std::fabs(normal) >= halfWidth - 1e-5) continue;
                        ++expected[along < 0.0 ? 0 : 1];
                        const auto found = pixels.find({x, y});
                        if (found == pixels.end() || found->second->laterUiCoverage > 0.0) {
                            allOwned = false;
                            continue;
                        }
                        const auto owner = frame.ownerTable.find(found->second->sampleOwnerTokens[i]);
                        if (owner == frame.ownerTable.end() ||
                            owner->second.track != expectedTrack(source.source.track) ||
                            primitiveStem(owner->second.primitiveId) != sourceStem ||
                            owner->second.visualSublayer == "outer_rim")
                            allOwned = false;
                    }
            shapeOk = allOwned && expected[0] > 0 && expected[1] > 0;
        } else if (endpoint) {
            const double radius = audit.expectedNormalWidthPx * 0.5;
            const bool predictedRing = source.source.track ==
                CompoundVisibilityTrack::Predicted;
            const double innerRadius = predictedRing ? std::max(0.0,
                (style.endpointOuterDiameterPx -
                    style.endpointRingStrokePx) * 0.5) : 0.0;
            const double minimumFilledArea = 3.14159265358979323846 *
                (radius * radius - innerRadius * innerRadius) * 0.65;
            const bool centerOwned = std::any_of(ownedPixels.begin(),
                ownedPixels.end(), [&](const auto& coordinate) {
                    const double dx = coordinate.first + 0.5 -
                        source.source.xPhysicalPx;
                    const double dy = coordinate.second + 0.5 -
                        source.source.yPhysicalPx;
                    return std::hypot(dx, dy) <= sampleCellWidth;
                });
            std::set<int> angularSectors;
            if (predictedRing) {
                for (const auto& coordinate : ownedPixels) {
                    const double dx = coordinate.first + 0.5 -
                        source.source.xPhysicalPx;
                    const double dy = coordinate.second + 0.5 -
                        source.source.yPhysicalPx;
                    double angle = std::atan2(dy, dx);
                    if (angle < 0.0) angle +=
                        2.0 * 3.14159265358979323846;
                    angularSectors.insert(static_cast<int>(std::floor(
                        angle * 8.0 /
                        (2.0 * 3.14159265358979323846))) % 8);
                }
            }
            shapeOk = connected &&
                (predictedRing ? angularSectors.size() == 8 : centerOwned) &&
                ownedPixels.size() >=
                static_cast<std::size_t>(std::ceil(minimumFilledArea));
        }
        const double maximumExpectedNormalWidth = endpoint ?
            audit.expectedNormalWidthPx +
                2.0 * style.outerContrastRimWidthPx :
            maximumCompoundWidth;
        const bool normalOk = !audit.required ||
            (audit.measuredNormalWidthPx + tolerancePx >=
                    audit.expectedNormalWidthPx &&
                audit.measuredNormalWidthPx <=
                    maximumExpectedNormalWidth + tolerancePx);
        const double maximumTickSpan = style.actualTickLengthPx +
            2.0 * (style.haloWidthPx +
                style.outerContrastRimWidthPx);
        const bool tangentOk = (!tick && !endpoint) ||
            (audit.measuredTangentSpanPx + tolerancePx >=
                    audit.expectedTangentSpanPx &&
                audit.measuredTangentSpanPx <=
                    (tick ? maximumTickSpan :
                        maximumExpectedNormalWidth) + tolerancePx);
        // A body source fully replaced at its center by another authored
        // primitive is not a second geometry obligation.  The independent
        // visibility resolver accounts for it, while the final visible
        // primitive is audited by its own canonical source.  Ticks are the
        // deliberate exception: predicted may own their center, so both
        // fixed wings remain required.
        if (!tick && (ownedPixels.empty() ||
                primitiveStem(audit.centerOwnerPrimitiveId) != sourceStem))
            audit.required = false;
        audit.normalWidthPassed = normalOk;
        audit.tangentSpanPassed = tangentOk;
        audit.shapePassed = shapeOk;
        audit.passed = !audit.required ||
            (normalOk && tangentOk && shapeOk);
        if (audit.required && !audit.passed)
            fail(tick ? "final_trajectory_tick_geometry_mismatch" :
                (endpoint ? "final_trajectory_endpoint_geometry_mismatch" :
                    "final_trajectory_body_width_mismatch"));
        result.probes.push_back(std::move(audit));
    }
    result.ok = result.errorCode.empty();
    return result;
}

}  // namespace billiardgl::training
