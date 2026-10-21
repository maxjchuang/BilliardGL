#include "training/trajectory_compound_visibility.h"

#include "deterministic_hash.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <tuple>
#include <unordered_map>

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

std::string pixelKey(const CompoundOwnershipCandidate& candidate)
{
    return std::to_string(candidate.xPhysicalPx) + "," +
        std::to_string(candidate.yPhysicalPx);
}

double allowedNormalDistance(const CompoundSourceProbe& probe)
{
    if (probe.componentRole == CompoundComponentRole::Tick)
        return probe.halfTickLengthPx + 2.0;
    return probe.outerHaloRadiusPx + 2.0;
}

double allowedLongitudinalDistance(const CompoundSourceProbe& probe)
{
    return std::min(12.0, probe.sourceProbeSpacingPx * 0.5);
}

double linearChannel(std::uint8_t value)
{
    const double encoded = static_cast<double>(value) / 255.0;
    return encoded <= 0.04045 ? encoded / 12.92 :
        std::pow((encoded + 0.055) / 1.055, 2.4);
}

double relativeLuminance(const std::array<std::uint8_t, 3>& color)
{
    return 0.2126 * linearChannel(color[0]) +
        0.7152 * linearChannel(color[1]) +
        0.0722 * linearChannel(color[2]);
}

double edgeContrast(const CompoundOwnershipCandidate& candidate)
{
    const double first = relativeLuminance(candidate.finalFramebufferSrgb8);
    const double second = relativeLuminance(candidate.adjacentNonSelfSrgb8);
    return (std::max(first, second) + 0.05) /
        (std::min(first, second) + 0.05);
}

CompoundTrackVisibilityCounts& trackCounts(CompoundVisibilityAudit& audit,
    CompoundVisibilityTrack track)
{
    return track == CompoundVisibilityTrack::Predicted ?
        audit.predicted : audit.actual;
}

bool eligibleComponent(const CompoundSourceProbe& probe,
    const CompoundOwnershipCandidate& candidate)
{
    return probe.componentRole == candidate.ownerComponentRole &&
        probe.primitiveId == candidate.ownerPrimitiveId;
}

bool componentAllowedForTrack(CompoundVisibilityTrack track,
    CompoundComponentRole role)
{
    if (track == CompoundVisibilityTrack::Predicted)
        return role == CompoundComponentRole::Core ||
            role == CompoundComponentRole::OuterHaloEdge ||
            role == CompoundComponentRole::Endpoint;
    return true;
}

struct Evaluation {
    const CompoundOwnershipCandidate* candidate = nullptr;
    CompoundHiddenReason failure = CompoundHiddenReason::NoSameTrackFinalOwner;
    bool foreign = false;
    bool unowned = false;
};

Evaluation evaluateCandidate(const CompoundSourceProbe& probe,
    const CompoundOwnershipCandidate& candidate,
    double minimumContrast)
{
    Evaluation result;
    result.candidate = &candidate;
    if (!candidate.fromFinalCompositeAttachment ||
        !candidate.correspondsToFinalColor) {
        result.failure = CompoundHiddenReason::NoSameTrackFinalOwner;
        result.unowned = true;
        return result;
    }
    if (candidate.backgroundOrSceneOwner) {
        result.failure = CompoundHiddenReason::OnlyBackgroundOrSceneVisible;
        result.unowned = true;
        return result;
    }
    if (candidate.ownerTrack != probe.track) {
        result.failure = CompoundHiddenReason::OnlyOtherTrackVisible;
        result.foreign = true;
        return result;
    }
    if (!eligibleComponent(probe, candidate)) {
        result.failure = CompoundHiddenReason::ComponentNotEligible;
        return result;
    }
    if (std::fabs(candidate.normalDistancePx) > allowedNormalDistance(probe) ||
        std::fabs(candidate.longitudinalDistancePx) >
            allowedLongitudinalDistance(probe)) {
        result.failure = CompoundHiddenReason::OutsideBoundedNeighborhood;
        return result;
    }
    if (probe.componentRole == CompoundComponentRole::Endpoint &&
        !candidate.sameEndpointPrimitive) {
        result.failure = CompoundHiddenReason::ComponentNotEligible;
        return result;
    }
    if (probe.componentRole == CompoundComponentRole::Tick &&
        !candidate.nearestTickInterval) {
        result.failure = CompoundHiddenReason::ComponentNotEligible;
        return result;
    }
    if (candidate.fourConnectedOwnedPixelCount < 2 ||
        candidate.ownedPixelEquivalentArea < 1.5 ||
        candidate.resolvedAlpha <= 0.0) {
        result.failure = CompoundHiddenReason::InsufficientOwnedPixelArea;
        return result;
    }
    const double resolvedContrast = edgeContrast(candidate);
    if (!candidate.bordersAdjacentNonSelfPixel ||
        std::fabs(candidate.finalEdgeContrastRatio - resolvedContrast) > 0.01 ||
        resolvedContrast < minimumContrast) {
        result.failure = CompoundHiddenReason::FinalEdgeContrastBelow4;
        return result;
    }
    result.failure = CompoundHiddenReason::PixelAlreadyAssigned;
    return result;
}

void fail(CompoundVisibilityAudit& audit, const std::string& error)
{
    if (audit.errorCode.empty()) audit.errorCode = error;
}

}  // namespace

const char* compoundTrackName(CompoundVisibilityTrack track)
{
    return track == CompoundVisibilityTrack::Predicted ? "predicted" : "actual";
}

const char* compoundComponentRoleName(CompoundComponentRole role)
{
    switch (role) {
    case CompoundComponentRole::Core: return "core";
    case CompoundComponentRole::OuterHaloEdge: return "outer_halo_edge";
    case CompoundComponentRole::SolidInPredictedDashGap:
        return "solid_in_predicted_dash_gap";
    case CompoundComponentRole::Tick: return "tick";
    case CompoundComponentRole::Endpoint: return "endpoint";
    }
    return "unknown";
}

const char* compoundHiddenReasonName(CompoundHiddenReason reason)
{
    switch (reason) {
    case CompoundHiddenReason::OutOfBoundsOrClipped:
        return "out_of_bounds_or_clipped";
    case CompoundHiddenReason::NoSameTrackFinalOwner:
        return "no_same_track_final_owner";
    case CompoundHiddenReason::OnlyOtherTrackVisible:
        return "only_other_track_visible";
    case CompoundHiddenReason::OnlyBackgroundOrSceneVisible:
        return "only_background_or_scene_visible";
    case CompoundHiddenReason::InsufficientOwnedPixelArea:
        return "insufficient_owned_pixel_area";
    case CompoundHiddenReason::InsufficientExclusiveOwnedArea:
        return "insufficient_exclusive_owned_area";
    case CompoundHiddenReason::InsufficientRasterContinuity:
        return "insufficient_raster_continuity";
    case CompoundHiddenReason::OutsideBoundedNeighborhood:
        return "outside_bounded_neighborhood";
    case CompoundHiddenReason::PixelAlreadyAssigned:
        return "pixel_already_assigned";
    case CompoundHiddenReason::FinalEdgeContrastBelow4:
        return "final_edge_contrast_below_4";
    case CompoundHiddenReason::ComponentNotEligible:
        return "component_not_eligible";
    case CompoundHiddenReason::NoEligibleComponentAwareBoundary:
        return "no_eligible_component_aware_boundary";
    }
    return "unknown";
}

std::string stableCompoundSourceProbeId(const CompoundSourceProbe& probe)
{
    return sha256Hex(probe.captureId + "|" + compoundTrackName(probe.track) +
        "|" + probe.primitiveId + "|" +
        compoundComponentRoleName(probe.componentRole) + "|" +
        probe.arcSegmentId + "|" + std::to_string(probe.probeOrdinal) +
        "|" + probe.physicalProfile);
}

CompoundVisibilityAudit auditCompoundFinalVisibility(
    const CompoundVisibilityAuditRequest& request)
{
    CompoundVisibilityAudit audit;
    audit.sourceProbeCount = request.sourceProbes.size();
    std::unordered_map<std::string, const CompoundSourceProbe*> probesById;
    std::set<std::string> uniqueIds;
    for (const CompoundSourceProbe& probe : request.sourceProbes) {
        const std::string id = stableCompoundSourceProbeId(probe);
        audit.orderedSourceProbeIds.push_back(id);
        if (!uniqueIds.insert(id).second)
            fail(audit, "compound_source_probe_id_duplicate");
        probesById[id] = &probe;
        ++trackCounts(audit, probe.track).source;
        ++audit.componentSourceCounts[compoundComponentRoleName(
            probe.componentRole)];
        if (!componentAllowedForTrack(probe.track, probe.componentRole))
            fail(audit, "compound_component_not_owned_by_track");
    }
    audit.orderedSourceProbeDigest = joinedDigest(audit.orderedSourceProbeIds);

    if (audit.sourceProbeCount < request.minimumSourceProbeCount ||
        (request.expectedSourceProbeCount != 0 &&
            audit.sourceProbeCount != request.expectedSourceProbeCount))
        fail(audit, "compound_source_probe_inventory_below_contract");
    const auto requireTrackComponent = [&](CompoundVisibilityTrack track,
                                           CompoundComponentRole role) {
        const bool found = std::any_of(request.sourceProbes.begin(),
            request.sourceProbes.end(), [&](const CompoundSourceProbe& probe) {
                return probe.track == track && probe.componentRole == role;
            });
        if (!found) fail(audit, "compound_component_inventory_missing");
    };
    if (request.predictedRequired) {
        requireTrackComponent(CompoundVisibilityTrack::Predicted,
            CompoundComponentRole::Core);
        requireTrackComponent(CompoundVisibilityTrack::Predicted,
            CompoundComponentRole::OuterHaloEdge);
        requireTrackComponent(CompoundVisibilityTrack::Predicted,
            CompoundComponentRole::Endpoint);
    }
    if (request.actualRequired) {
        requireTrackComponent(CompoundVisibilityTrack::Actual,
            CompoundComponentRole::Core);
        requireTrackComponent(CompoundVisibilityTrack::Actual,
            CompoundComponentRole::OuterHaloEdge);
        requireTrackComponent(CompoundVisibilityTrack::Actual,
            CompoundComponentRole::Tick);
        requireTrackComponent(CompoundVisibilityTrack::Actual,
            CompoundComponentRole::Endpoint);
        if (request.predictedRequired)
            requireTrackComponent(CompoundVisibilityTrack::Actual,
                CompoundComponentRole::SolidInPredictedDashGap);
    }
    if (std::fabs(request.invariants.predictedLateralOffsetPx) > 1e-9 ||
        std::fabs(request.invariants.actualLateralOffsetPx) > 1e-9 ||
        request.invariants.compositingOrder != "actual_then_predicted" ||
        !request.invariants.routeInputsUnchanged ||
        !request.invariants.worldSamplesUnchanged ||
        !request.invariants.endpointsUnchanged)
        fail(audit, "compound_visual_or_route_contract_changed");

    std::unordered_map<std::string, std::vector<CompoundOwnershipCandidate>>
        candidatesByProbe;
    for (const CompoundOwnershipCandidate& candidate :
            request.ownershipCandidates) {
        if (probesById.find(candidate.sourceProbeId) == probesById.end()) {
            fail(audit, "compound_candidate_unknown_source_probe");
            continue;
        }
        candidatesByProbe[candidate.sourceProbeId].push_back(candidate);
    }

    // Select one eligible candidate per probe, then arbitrate shared pixels by
    // same-track distance, primitive ID and source ordinal.  This realizes the
    // gate's exclusive, deterministic attribution rule.
    struct Choice {
        std::string id;
        const CompoundSourceProbe* probe = nullptr;
        const CompoundOwnershipCandidate* candidate = nullptr;
        CompoundHiddenReason reason = CompoundHiddenReason::NoSameTrackFinalOwner;
        bool foreign = false;
        bool unowned = false;
    };
    std::vector<Choice> choices;
    choices.reserve(request.sourceProbes.size());
    for (std::size_t index = 0; index < request.sourceProbes.size(); ++index) {
        const CompoundSourceProbe& probe = request.sourceProbes[index];
        const std::string& id = audit.orderedSourceProbeIds[index];
        Choice choice{id, &probe};
        if (!probe.insideFinalViewport) {
            choice.reason = CompoundHiddenReason::OutOfBoundsOrClipped;
            choices.push_back(choice);
            continue;
        }
        bool sawCandidate = false;
        bool sawForeign = false;
        CompoundHiddenReason lastReason =
            CompoundHiddenReason::NoSameTrackFinalOwner;
        std::vector<const CompoundOwnershipCandidate*> eligible;
        for (const auto& candidate : candidatesByProbe[id]) {
            sawCandidate = true;
            const Evaluation evaluation = evaluateCandidate(probe, candidate,
                request.minimumEdgeContrastRatio);
            sawForeign = sawForeign || evaluation.foreign;
            choice.unowned = choice.unowned || evaluation.unowned;
            lastReason = evaluation.failure;
            if (evaluation.failure == CompoundHiddenReason::PixelAlreadyAssigned)
                eligible.push_back(&candidate);
        }
        choice.foreign = sawForeign;
        if (!eligible.empty()) {
            std::sort(eligible.begin(), eligible.end(),
                [](const auto* first, const auto* second) {
                    return std::tie(first->normalDistancePx,
                               first->longitudinalDistancePx,
                               first->ownerPrimitiveId, first->xPhysicalPx,
                               first->yPhysicalPx) <
                        std::tie(second->normalDistancePx,
                            second->longitudinalDistancePx,
                            second->ownerPrimitiveId, second->xPhysicalPx,
                            second->yPhysicalPx);
                });
            choice.candidate = eligible.front();
            choice.reason = CompoundHiddenReason::PixelAlreadyAssigned;
        } else if (sawForeign) {
            choice.reason = CompoundHiddenReason::OnlyOtherTrackVisible;
        } else if (sawCandidate) {
            choice.reason = lastReason;
        }
        choices.push_back(choice);
    }
    std::sort(choices.begin(), choices.end(), [](const Choice& first,
        const Choice& second) {
        const double firstDistance = first.candidate ?
            std::hypot(first.candidate->normalDistancePx,
                first.candidate->longitudinalDistancePx) :
            std::numeric_limits<double>::infinity();
        const double secondDistance = second.candidate ?
            std::hypot(second.candidate->normalDistancePx,
                second.candidate->longitudinalDistancePx) :
            std::numeric_limits<double>::infinity();
        return std::tie(firstDistance, first.probe->primitiveId,
                   first.probe->probeOrdinal) <
            std::tie(secondDistance, second.probe->primitiveId,
                second.probe->probeOrdinal);
    });
    std::set<std::string> assignedPixels;
    std::set<std::string> visibleIds;
    std::vector<double> visibleContrasts;
    for (Choice& choice : choices) {
        if (choice.foreign) ++audit.foreignTrackEvidenceCount;
        if (choice.unowned) ++audit.unownedEvidenceCount;
        if (choice.candidate) {
            const std::string pixel = pixelKey(*choice.candidate);
            if (assignedPixels.insert(pixel).second) {
                visibleIds.insert(choice.id);
                visibleContrasts.push_back(
                    edgeContrast(*choice.candidate));
                continue;
            }
            ++audit.doubleAssignmentCount;
            choice.reason = CompoundHiddenReason::PixelAlreadyAssigned;
        }
        audit.hiddenProbes.push_back({choice.id, choice.probe->track,
            choice.probe->componentRole, choice.reason,
            choice.probe->xPhysicalPx, choice.probe->yPhysicalPx});
        ++audit.hiddenReasonCounts[compoundHiddenReasonName(choice.reason)];
    }

    // Restore source order for stable visible IDs and hidden records.
    audit.hiddenProbes.clear();
    for (std::size_t index = 0; index < request.sourceProbes.size(); ++index) {
        const CompoundSourceProbe& probe = request.sourceProbes[index];
        const std::string& id = audit.orderedSourceProbeIds[index];
        if (visibleIds.count(id)) {
            audit.orderedVisibleProbeIds.push_back(id);
            ++audit.visibleProbeCount;
            ++trackCounts(audit, probe.track).visible;
            ++audit.componentVisibleCounts[compoundComponentRoleName(
                probe.componentRole)];
        } else {
            ++audit.hiddenProbeCount;
            ++trackCounts(audit, probe.track).hidden;
            const auto found = std::find_if(choices.begin(), choices.end(),
                [&](const Choice& value) { return value.id == id; });
            const CompoundHiddenReason reason = found == choices.end() ?
                CompoundHiddenReason::NoSameTrackFinalOwner : found->reason;
            audit.hiddenProbes.push_back({id, probe.track,
                probe.componentRole, reason, probe.xPhysicalPx,
                probe.yPhysicalPx});
        }
    }
    audit.hiddenReasonCounts.clear();
    for (const auto& hidden : audit.hiddenProbes) {
        ++audit.hiddenReasonCounts[compoundHiddenReasonName(hidden.reason)];
        ++audit.componentHiddenReasonCounts[
            std::string(compoundComponentRoleName(hidden.componentRole)) +
            "|" + compoundHiddenReasonName(hidden.reason)];
    }
    audit.orderedVisibleProbeDigest = joinedDigest(
        audit.orderedVisibleProbeIds);
    audit.visibleProbeCoverage = audit.sourceProbeCount == 0 ? 0.0 :
        static_cast<double>(audit.visibleProbeCount) / audit.sourceProbeCount;
    for (CompoundTrackVisibilityCounts* counts :
            {&audit.predicted, &audit.actual}) {
        counts->coverage = counts->source == 0 ? 0.0 :
            static_cast<double>(counts->visible) / counts->source;
    }
    if (!visibleContrasts.empty()) {
        audit.minimumVisibleEdgeContrastRatio = *std::min_element(
            visibleContrasts.begin(), visibleContrasts.end());
        audit.designTargetContrastCoverage = static_cast<double>(std::count_if(
            visibleContrasts.begin(), visibleContrasts.end(),
            [&](double value) {
                return value >= request.designTargetContrastRatio;
            })) / visibleContrasts.size();
    }
    if ((request.predictedRequired && audit.predicted.source == 0) ||
        (request.actualRequired && audit.actual.source == 0))
        fail(audit, "compound_track_inventory_missing");
    if (audit.visibleProbeCoverage < request.minimumVisibleCoverage ||
        (request.predictedRequired &&
            audit.predicted.coverage < request.minimumVisibleCoverage) ||
        (request.actualRequired &&
            audit.actual.coverage < request.minimumVisibleCoverage))
        fail(audit, "compound_visible_coverage_below_0_90");
    if (audit.minimumVisibleEdgeContrastRatio <
            request.minimumEdgeContrastRatio ||
        audit.designTargetContrastCoverage < request.designTargetCoverage ||
        audit.hiddenReasonCounts["final_edge_contrast_below_4"] != 0)
        fail(audit, "compound_final_edge_contrast_contract_failed");
    if (request.predictedRequired && request.actualRequired &&
        request.predictedActualLumaDeltaSrgb8 <
            request.requiredPredictedActualLumaDeltaSrgb8)
        fail(audit, "compound_role_luma_delta_below_profile");
    if (audit.doubleAssignmentCount != 0)
        fail(audit, "compound_pixel_double_assignment");
    if (audit.foreignTrackEvidenceCount != 0)
        fail(audit, "compound_foreign_track_evidence");
    if (audit.unownedEvidenceCount != 0)
        fail(audit, "compound_unowned_evidence");
    audit.ok = audit.errorCode.empty();
    return audit;
}

}  // namespace billiardgl::training
