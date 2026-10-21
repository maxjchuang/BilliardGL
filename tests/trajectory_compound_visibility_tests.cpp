#include "training/trajectory_compound_visibility.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace billiardgl::training;

void expect(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error(message);
}

CompoundVisibilityAuditRequest passingRequest()
{
    CompoundVisibilityAuditRequest request;
    request.expectedSourceProbeCount = 160;
    request.predictedActualLumaDeltaSrgb8 = 40.0;
    for (int index = 0; index < 160; ++index) {
        CompoundSourceProbe probe;
        probe.captureId = "core__launch_right_english_one_rail_v1__broadcast";
        probe.track = index % 2 == 0 ? CompoundVisibilityTrack::Predicted :
            CompoundVisibilityTrack::Actual;
        const CompoundComponentRole predictedRoles[] = {
            CompoundComponentRole::Core,
            CompoundComponentRole::OuterHaloEdge,
            CompoundComponentRole::Endpoint};
        const CompoundComponentRole actualRoles[] = {
            CompoundComponentRole::Core,
            CompoundComponentRole::OuterHaloEdge,
            CompoundComponentRole::SolidInPredictedDashGap,
            CompoundComponentRole::Tick,
            CompoundComponentRole::Endpoint};
        probe.componentRole = probe.track == CompoundVisibilityTrack::Predicted ?
            predictedRoles[(index / 2) % 3] : actualRoles[(index / 2) % 5];
        probe.primitiveId = "primitive-" + std::to_string(index);
        probe.arcSegmentId = "arc-" + std::to_string(index / 8);
        probe.probeOrdinal = static_cast<std::uint64_t>(index);
        probe.physicalProfile = "1920x1080@1.0-ui1.0-standard";
        probe.xPhysicalPx = 20 + index * 3;
        probe.yPhysicalPx = 120 + index % 11;
        probe.outerHaloRadiusPx = 5.0;
        probe.halfTickLengthPx = 8.0;
        probe.sourceProbeSpacingPx = 20.0;
        request.sourceProbes.push_back(probe);

        CompoundOwnershipCandidate candidate;
        candidate.sourceProbeId = stableCompoundSourceProbeId(probe);
        candidate.xPhysicalPx = probe.xPhysicalPx;
        candidate.yPhysicalPx = probe.yPhysicalPx;
        candidate.ownerTrack = probe.track;
        candidate.ownerComponentRole = probe.componentRole;
        candidate.ownerPrimitiveId = probe.primitiveId;
        candidate.resolvedAlpha = 1.0;
        candidate.fromFinalCompositeAttachment = true;
        candidate.correspondsToFinalColor = true;
        candidate.fourConnectedOwnedPixelCount = 2;
        candidate.ownedPixelEquivalentArea = 2.0;
        candidate.finalFramebufferSrgb8 = {{255, 255, 255}};
        candidate.adjacentNonSelfSrgb8 = {{0, 0, 0}};
        candidate.bordersAdjacentNonSelfPixel = true;
        candidate.finalEdgeContrastRatio = 21.0;
        request.ownershipCandidates.push_back(candidate);
    }
    return request;
}

void testStableIdentityAndPassingAudit()
{
    const auto request = passingRequest();
    const std::string first = stableCompoundSourceProbeId(
        request.sourceProbes.front());
    expect(first.size() == 64 && first == request.ownershipCandidates.front()
            .sourceProbeId,
        "source probe identity is stable SHA-256 over the frozen tuple");
    auto changed = request.sourceProbes.front();
    ++changed.probeOrdinal;
    expect(stableCompoundSourceProbeId(changed) != first,
        "probe ordinal is part of stable identity");
    const auto audit = auditCompoundFinalVisibility(request);
    expect(audit.ok && audit.sourceProbeCount == 160 &&
        audit.visibleProbeCount == 160 && audit.hiddenProbeCount == 0 &&
        audit.predicted.coverage == 1.0 && audit.actual.coverage == 1.0 &&
        audit.doubleAssignmentCount == 0 &&
        audit.foreignTrackEvidenceCount == 0 &&
        audit.orderedSourceProbeDigest.size() == 64 &&
        audit.orderedVisibleProbeDigest.size() == 64,
        "final-composite same-track ownership passes with stable inventories");
}

void testOtherTrackAndBackgroundCannotCoverProbe()
{
    auto foreign = passingRequest();
    foreign.minimumVisibleCoverage = 1.0;
    foreign.ownershipCandidates[0].ownerTrack = CompoundVisibilityTrack::Actual;
    const auto foreignAudit = auditCompoundFinalVisibility(foreign);
    expect(!foreignAudit.ok && foreignAudit.foreignTrackEvidenceCount == 1 &&
        foreignAudit.hiddenReasonCounts.at("only_other_track_visible") == 1,
        "other-track color/halo coverage cannot prove source visibility");

    auto background = passingRequest();
    background.minimumVisibleCoverage = 1.0;
    background.ownershipCandidates[0].backgroundOrSceneOwner = true;
    const auto backgroundAudit = auditCompoundFinalVisibility(background);
    expect(!backgroundAudit.ok &&
        backgroundAudit.hiddenReasonCounts.at(
            "only_background_or_scene_visible") == 1 &&
        backgroundAudit.unownedEvidenceCount == 1 &&
        backgroundAudit.hiddenProbes.front().xPhysicalPx == 20,
        "matching framebuffer RGB without semantic ownership is rejected");
}

void testPreCompositeAndBoundsCannotCoverProbe()
{
    auto precomposite = passingRequest();
    precomposite.minimumVisibleCoverage = 1.0;
    precomposite.ownershipCandidates[0].fromFinalCompositeAttachment = false;
    expect(!auditCompoundFinalVisibility(precomposite).ok,
        "analytic or pre-composite coverage evidence is rejected");

    auto bounds = passingRequest();
    bounds.minimumVisibleCoverage = 1.0;
    bounds.sourceProbes[0].insideFinalViewport = false;
    const auto boundsAudit = auditCompoundFinalVisibility(bounds);
    expect(!boundsAudit.ok && boundsAudit.hiddenReasonCounts.at(
            "out_of_bounds_or_clipped") == 1,
        "out-of-bounds source probes stay in the denominator and are hidden");
}

void testExclusiveAttributionAndBoundedNeighborhood()
{
    auto duplicatePixel = passingRequest();
    duplicatePixel.minimumVisibleCoverage = 1.0;
    duplicatePixel.ownershipCandidates[1].xPhysicalPx =
        duplicatePixel.ownershipCandidates[0].xPhysicalPx;
    duplicatePixel.ownershipCandidates[1].yPhysicalPx =
        duplicatePixel.ownershipCandidates[0].yPhysicalPx;
    const auto duplicateAudit = auditCompoundFinalVisibility(duplicatePixel);
    expect(!duplicateAudit.ok && duplicateAudit.doubleAssignmentCount == 1 &&
        duplicateAudit.hiddenReasonCounts.at("pixel_already_assigned") == 1,
        "one resolved pixel cannot be attributed to two source probes");

    auto distant = passingRequest();
    distant.minimumVisibleCoverage = 1.0;
    distant.ownershipCandidates[0].longitudinalDistancePx = 10.1;
    const auto distantAudit = auditCompoundFinalVisibility(distant);
    expect(!distantAudit.ok && distantAudit.hiddenReasonCounts.at(
            "outside_bounded_neighborhood") == 1,
        "longitudinal assignment is capped by half spacing");

    auto endpoint = passingRequest();
    endpoint.minimumVisibleCoverage = 1.0;
    int endpointIndex = 0;
    while (endpoint.sourceProbes[endpointIndex].componentRole !=
            CompoundComponentRole::Endpoint)
        ++endpointIndex;
    endpoint.ownershipCandidates[endpointIndex].sameEndpointPrimitive = false;
    const auto endpointAudit = auditCompoundFinalVisibility(endpoint);
    expect(!endpointAudit.ok && endpointAudit.hiddenReasonCounts.at(
            "component_not_eligible") == 1,
        "endpoint evidence cannot migrate across endpoint primitives");
}

void testInventoryAndPerTrackCoverageFailClosed()
{
    auto shortInventory = passingRequest();
    shortInventory.sourceProbes.pop_back();
    shortInventory.ownershipCandidates.pop_back();
    const auto shortAudit = auditCompoundFinalVisibility(shortInventory);
    expect(!shortAudit.ok && shortAudit.errorCode ==
            "compound_source_probe_inventory_below_contract",
        "inventory below the frozen 159-source floor is rejected");

    auto duplicateId = passingRequest();
    duplicateId.sourceProbes.back() = duplicateId.sourceProbes.front();
    const auto duplicateIdAudit = auditCompoundFinalVisibility(duplicateId);
    expect(!duplicateIdAudit.ok && duplicateIdAudit.errorCode ==
            "compound_source_probe_id_duplicate",
        "probe-ID gaming through duplicate identity is rejected");

    auto perTrack = passingRequest();
    int removed = 0;
    for (auto& candidate : perTrack.ownershipCandidates) {
        if (candidate.ownerTrack == CompoundVisibilityTrack::Actual &&
            removed++ < 9)
            candidate.fromFinalCompositeAttachment = false;
    }
    const auto perTrackAudit = auditCompoundFinalVisibility(perTrack);
    expect(!perTrackAudit.ok && perTrackAudit.visibleProbeCoverage > 0.90 &&
        perTrackAudit.actual.coverage < 0.90 &&
        perTrackAudit.predicted.coverage == 1.0,
        "aggregate coverage cannot mask a failing semantic track");
}

void keepOnlyTrack(CompoundVisibilityAuditRequest& request,
    CompoundVisibilityTrack track)
{
    request.sourceProbes.erase(std::remove_if(request.sourceProbes.begin(),
        request.sourceProbes.end(), [&](const CompoundSourceProbe& probe) {
            return probe.track != track;
        }), request.sourceProbes.end());
    request.ownershipCandidates.erase(std::remove_if(
        request.ownershipCandidates.begin(), request.ownershipCandidates.end(),
        [&](const CompoundOwnershipCandidate& candidate) {
            return candidate.ownerTrack != track;
        }), request.ownershipCandidates.end());
    request.minimumSourceProbeCount = 1;
    request.expectedSourceProbeCount = request.sourceProbes.size();
}

void testExplicitSingleTrackCaptureModes()
{
    auto predictedOnly = passingRequest();
    keepOnlyTrack(predictedOnly, CompoundVisibilityTrack::Predicted);
    predictedOnly.actualRequired = false;
    expect(auditCompoundFinalVisibility(predictedOnly).ok,
        "predicted-only capture does not invent an actual-track requirement");

    auto actualOnly = passingRequest();
    keepOnlyTrack(actualOnly, CompoundVisibilityTrack::Actual);
    actualOnly.predictedRequired = false;
    expect(auditCompoundFinalVisibility(actualOnly).ok,
        "actual-only capture does not invent a predicted-track requirement");

    predictedOnly.actualRequired = true;
    expect(!auditCompoundFinalVisibility(predictedOnly).ok,
        "a track declared required still fails closed when absent");
}

void testContrastAndVisualMutationFailClosed()
{
    auto hardContrast = passingRequest();
    hardContrast.ownershipCandidates[0].finalFramebufferSrgb8 = {{100, 100, 100}};
    hardContrast.ownershipCandidates[0].adjacentNonSelfSrgb8 = {{90, 90, 90}};
    hardContrast.ownershipCandidates[0].finalEdgeContrastRatio = 1.2;
    expect(!auditCompoundFinalVisibility(hardContrast).ok,
        "final compound edge contrast below 4.0 is rejected");

    auto targetContrast = passingRequest();
    targetContrast.ownershipCandidates[0].finalFramebufferSrgb8 = {{0, 0, 0}};
    targetContrast.ownershipCandidates[0].adjacentNonSelfSrgb8 = {{116, 116, 116}};
    targetContrast.ownershipCandidates[0].finalEdgeContrastRatio = 4.49;
    targetContrast.ownershipCandidates[1].finalFramebufferSrgb8 = {{0, 0, 0}};
    targetContrast.ownershipCandidates[1].adjacentNonSelfSrgb8 = {{116, 116, 116}};
    targetContrast.ownershipCandidates[1].finalEdgeContrastRatio = 4.49;
    const auto targetAudit = auditCompoundFinalVisibility(targetContrast);
    expect(!targetAudit.ok && targetAudit.designTargetContrastCoverage < 0.99,
        "less than 99 percent at the 4.5 design target is rejected");

    auto routeMutation = passingRequest();
    routeMutation.invariants.predictedLateralOffsetPx = 0.25;
    expect(auditCompoundFinalVisibility(routeMutation).errorCode ==
            "compound_visual_or_route_contract_changed",
        "screen offset, route, endpoint or z-order changes are forbidden");
}

}  // namespace

int main()
{
    try {
        testStableIdentityAndPassingAudit();
        testOtherTrackAndBackgroundCannotCoverProbe();
        testPreCompositeAndBoundsCannotCoverProbe();
        testExclusiveAttributionAndBoundedNeighborhood();
        testInventoryAndPerTrackCoverageFailClosed();
        testExplicitSingleTrackCaptureModes();
        testContrastAndVisualMutationFailClosed();
        std::cout << "trajectory compound visibility tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
