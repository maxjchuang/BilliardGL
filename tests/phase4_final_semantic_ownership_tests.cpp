#include "training/final_semantic_ownership.h"

#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

namespace {

using namespace billiardgl::training;

int passedFixtures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

void pass(const char* id, bool condition, const std::string& message)
{
    expect(condition, std::string(id) + ": " + message);
    ++passedFixtures;
    std::cout << "PASS " << id << '\n';
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

FinalOwnershipSourceProbe probe(std::string primitive = "primitive-0",
    std::uint64_t ordinal = 0)
{
    FinalOwnershipSourceProbe result;
    result.source.captureId = "capture-1";
    result.source.track = CompoundVisibilityTrack::Predicted;
    result.source.primitiveId = std::move(primitive);
    result.source.componentRole = CompoundComponentRole::Core;
    result.source.arcSegmentId = "arc-0";
    result.source.probeOrdinal = ordinal;
    result.source.physicalProfile = "fixture-64x64@4x";
    result.source.xPhysicalPx = 20;
    result.source.yPhysicalPx = 20;
    result.source.outerHaloRadiusPx = 5.0;
    result.source.sourceProbeSpacingPx = 10.0;
    result.source.insideFinalViewport = true;
    return result;
}

FinalSemanticPixel pixel(int x, int y, std::vector<std::uint32_t> samples)
{
    FinalSemanticPixel result;
    result.xPhysicalPx = x;
    result.yPhysicalPx = y;
    result.sampleOwnerTokens = std::move(samples);
    result.finalLdrSrgb8 = {{245, 245, 245}};
    result.adjacentNonSelfLdrSrgb8 = {{8, 8, 8}};
    return result;
}

IntegratedFinalOwnershipRequest requestWith(
    CompoundComponentRole ownerRole = CompoundComponentRole::Core,
    FinalSemanticTrack ownerTrack = FinalSemanticTrack::Predicted)
{
    IntegratedFinalOwnershipRequest request;
    request.expectedSourceProbeCount = 1;
    request.minimumVisibleCoverage = 1.0;
    request.frame.captureId = "capture-1";
    request.frame.colorFrameId = "frame-1";
    request.frame.semanticFrameId = "frame-1";
    request.frame.colorContentSha256 = std::string(64, 'a');
    request.frame.semanticContentSha256 = std::string(64, 'b');
    request.frame.physicalWidth = 64;
    request.frame.physicalHeight = 64;
    request.frame.msaaSamples = 4;
    request.frame.msaaSamplePositions = {
        {{0.375f, 0.125f}}, {{0.875f, 0.375f}},
        {{0.125f, 0.625f}}, {{0.625f, 0.875f}}};
    request.frame.renderGraph = graph();
    request.frame.ownerTable.emplace(1, FinalSemanticOwner{1, ownerTrack,
        ownerRole, "primitive-0", 10});
    request.frame.pixels.push_back(pixel(20, 20, {1, 1, 1, 1}));
    request.frame.pixels.push_back(pixel(21, 20, {1, 1, 1, 1}));
    auto leftBackground = pixel(19, 20, {0, 0, 0, 0});
    auto rightBackground = pixel(22, 20, {0, 0, 0, 0});
    leftBackground.finalLdrSrgb8 = {{8, 8, 8}};
    rightBackground.finalLdrSrgb8 = {{8, 8, 8}};
    request.frame.pixels.push_back(leftBackground);
    request.frame.pixels.push_back(rightBackground);
    request.sourceProbes.push_back(probe());
    return request;
}

void testPanelOcclusion()
{
    auto request = requestWith();
    for (auto& sample : request.frame.pixels) sample.laterUiCoverage = 1.0;
    const auto result = resolveIntegratedFinalSemanticOwnership(request);
    pass("N1_TRUE_PANEL_OCCLUSION", !result.ok &&
        result.candidateTelemetry.front().eligibilityOrRejectionReason ==
            "later_ui_owned" && result.audit.unownedEvidenceCount == 1,
        "later UI coverage must force track=none");
    for (auto& sample : request.frame.pixels)
        sample.interactiveChromeCoverage = 0.0;
    const auto worldResult = resolveIntegratedFinalSemanticOwnership(request);
    pass("N1_WORLD_ANNOTATION_CANNOT_RECLAIM_TRAJECTORY", !worldResult.ok &&
        worldResult.candidateTelemetry.front().eligibilityOrRejectionReason ==
            "later_ui_owned" && worldResult.audit.unownedEvidenceCount == 1,
        "world classification removes no total-overlay ownership exclusion");
}

void testBloomRenderGraphOrder()
{
    auto correct = requestWith();
    correct.frame.renderGraph.passSequence.insert(
        correct.frame.renderGraph.passSequence.begin() + 2, "scene_bloom");
    auto late = requestWith();
    late.frame.renderGraph.passSequence.insert(
        late.frame.renderGraph.passSequence.begin() + 3, "scene_bloom");
    auto early = requestWith();
    early.frame.renderGraph.passSequence.insert(
        early.frame.renderGraph.passSequence.begin(), "scene_bloom");
    auto duplicate = correct;
    duplicate.frame.renderGraph.passSequence.insert(
        duplicate.frame.renderGraph.passSequence.begin() + 2, "scene_bloom");
    expect(resolveIntegratedFinalSemanticOwnership(correct).ok,
        "scene-first bloom permits final-pixel ownership evaluation");
    for (const auto* invalid : {&late, &early, &duplicate}) {
        const auto result = resolveIntegratedFinalSemanticOwnership(*invalid);
        expect(!result.ok && result.errorCode == "final_ownership_bloom_order_mismatch",
            "invalid bloom order cannot pass final-pixel ownership evidence");
    }
    pass("BLOOM_RENDER_GRAPH_ORDER", true,
        "only one opaque-before-overlay bloom pass is eligible");
    auto dof = requestWith();
    dof.frame.renderGraph.passSequence.insert(
        dof.frame.renderGraph.passSequence.begin() + 2, "scene_dof");
    expect(resolveIntegratedFinalSemanticOwnership(dof).ok,
        "scene-first DOF permits final-pixel ownership evaluation");
    for (int index : {0, 3, 7}) {
        auto invalid = requestWith();
        invalid.frame.renderGraph.passSequence.insert(
            invalid.frame.renderGraph.passSequence.begin() + index, "scene_dof");
        const auto result = resolveIntegratedFinalSemanticOwnership(invalid);
        expect(!result.ok && result.errorCode == "final_ownership_dof_order_mismatch",
            "DOF cannot precede opaque scene or blur already drawn trajectories");
    }
    dof.frame.renderGraph.passSequence.insert(
        dof.frame.renderGraph.passSequence.begin() + 2, "scene_dof");
    expect(resolveIntegratedFinalSemanticOwnership(dof).errorCode ==
        "final_ownership_dof_order_mismatch", "duplicate DOF rejected");
    pass("DOF_RENDER_GRAPH_ORDER", true, "DOF only before sharp overlays");
}

void testCrossTrackNoRescue()
{
    const auto result = resolveIntegratedFinalSemanticOwnership(requestWith(
        CompoundComponentRole::Core, FinalSemanticTrack::Actual));
    pass("N2_TRUE_CROSS_TRACK_COVERAGE_NO_RESCUE", !result.ok &&
        result.audit.foreignTrackEvidenceCount == 1,
        "foreign final owner cannot cover predicted probe");
}

void testCrossComponentSameTrackRescue()
{
    const auto result = resolveIntegratedFinalSemanticOwnership(requestWith(
        CompoundComponentRole::OuterHaloEdge));
    pass("P1_CROSS_COMPONENT_SAME_TRACK_RESCUE", result.ok &&
        result.audit.visibleProbeCount == 1 &&
        result.candidateTelemetry.front().ownerComponent == "outer_halo_edge",
        "same-track final halo can rescue one core probe");
}

void testFinalColorMismatch()
{
    auto request = requestWith();
    for (auto& sample : request.frame.pixels) {
        sample.finalLdrSrgb8 = {{100, 100, 100}};
        sample.adjacentNonSelfLdrSrgb8 = {{96, 96, 96}};
    }
    const auto result = resolveIntegratedFinalSemanticOwnership(request);
    pass("N3_FINAL_COLOR_MISMATCH", !result.ok &&
        result.candidateTelemetry.front().eligibilityOrRejectionReason ==
            "final_edge_contrast_below_4",
        "paired final LDR contrast below 4.0 is rejected");
}

void testSharedPixelArbitration()
{
    auto request = requestWith();
    request.expectedSourceProbeCount = 2;
    request.sourceProbes.push_back(probe("primitive-1", 1));
    const auto result = resolveIntegratedFinalSemanticOwnership(request);
    bool loserRecorded = false;
    for (const auto& record : result.arbitrationTelemetry)
        loserRecorded = loserRecorded || record.loserReason ==
            "pixel_already_assigned";
    pass("N4_SHARED_PIXEL_ARBITRATION", !result.ok &&
        result.audit.visibleProbeCount == 1 &&
        result.audit.doubleAssignmentCount == 0 && loserRecorded,
        "each sample has one deterministic winner and explicit loser");
}

void testTangentNormalBounds()
{
    auto request = requestWith();
    request.frame.pixels.clear();
    request.frame.pixels.push_back(pixel(26, 20, {1, 1, 1, 1}));
    request.frame.pixels.push_back(pixel(27, 20, {1, 1, 1, 1}));
    const auto result = resolveIntegratedFinalSemanticOwnership(request);
    pass("N5_TANGENT_NORMAL_BOUNDS", !result.ok &&
        result.candidateTelemetry.front().eligibilityOrRejectionReason ==
            "outside_bounded_neighborhood",
        "true tangent bound rejects an otherwise nearby point");
}

void testForeignTrackColorMatch()
{
    const auto result = resolveIntegratedFinalSemanticOwnership(requestWith(
        CompoundComponentRole::Core, FinalSemanticTrack::Actual));
    pass("N6_FOREIGN_TRACK_COLOR_MATCH", !result.ok &&
        result.audit.foreignTrackEvidenceCount == 1,
        "palette-equivalent foreign owner remains foreign");
}

void testSceneBackgroundColorMatch()
{
    auto request = requestWith();
    for (auto& sample : request.frame.pixels)
        sample.sampleOwnerTokens.assign(4, 0);
    const auto result = resolveIntegratedFinalSemanticOwnership(request);
    pass("N7_SCENE_OR_BACKGROUND_COLOR_MATCH", !result.ok &&
        result.audit.unownedEvidenceCount == 1,
        "color-only scene/background pixels cannot count");
}

void testMsaaBelowArea()
{
    auto request = requestWith();
    request.frame.pixels[1].sampleOwnerTokens = {1, 0, 0, 0};
    const auto result = resolveIntegratedFinalSemanticOwnership(request);
    pass("N8_MSAA_PARTIAL_COVERAGE_BELOW_AREA", !result.ok &&
        result.candidateTelemetry.front().resolvedPixelEquivalentArea == 1.25,
        "measured area below 1.5 remains hidden");
}

void testMsaaAboveArea()
{
    auto request = requestWith();
    request.frame.pixels[1].sampleOwnerTokens = {1, 1, 0, 0};
    const auto result = resolveIntegratedFinalSemanticOwnership(request);
    pass("P2_MSAA_PARTIAL_COVERAGE_ABOVE_AREA", result.ok &&
        result.candidateTelemetry.front().resolvedPixelEquivalentArea == 1.5 &&
        result.audit.visibleProbeCount == 1,
        "qualifying partial MSAA area is credited exactly once");
}

void testClippedSample()
{
    auto request = requestWith();
    request.sourceProbes.front().source.insideFinalViewport = false;
    const auto result = resolveIntegratedFinalSemanticOwnership(request);
    pass("N9_CLIPPED_OR_DISCARDED_SAMPLE", !result.ok &&
        result.candidateTelemetry.front().eligibilityOrRejectionReason ==
            "out_of_bounds_or_clipped",
        "pre-clip hit absent after production clip remains hidden");
}

void testFailureFieldOrFlushMissing()
{
    Phase4CaptureTelemetryInput input;
    const Phase4TelemetryWriteResult result =
        appendPhase4CaptureTelemetryRowDurable(
            "/tmp/phase4-must-not-write-incomplete.jsonl", input);
    pass("N10_FAILURE_ROW_FIELD_OR_FLUSH_MISSING", !result.ok &&
        result.errorCode == "phase4_telemetry_identity_missing",
        "incomplete telemetry terminates before a PASS claim");
}

void testDifferentTokenSameComponentSeamRejected()
{
    auto request = requestWith();
    request.frame.ownerTable.emplace(2, FinalSemanticOwner{2,
        FinalSemanticTrack::Predicted, CompoundComponentRole::Core,
        "primitive-token-seam", 11});
    request.frame.pixels.clear();
    request.frame.pixels.push_back(pixel(20, 20, {1, 1, 1, 1}));
    request.frame.pixels.push_back(pixel(21, 20, {1, 1, 1, 1}));
    request.frame.pixels.push_back(pixel(19, 20, {2, 2, 2, 2}));
    request.frame.pixels.push_back(pixel(22, 20, {2, 2, 2, 2}));
    for (auto& item : request.frame.pixels)
        item.finalLdrSrgb8 = {{245, 245, 245}};
    const auto result = resolveIntegratedFinalSemanticOwnership(request);
    bool seam = false;
    for (const auto& boundary : result.boundaryTelemetry)
        seam = seam || boundary.boundaryClass ==
            "same_component_token_seam";
    pass("PAIR_N1_DIFFERENT_TOKEN_SEAM_REJECTED", !result.ok && seam &&
        result.audit.visibleProbeCount == 0,
        "a raw different-token seam is not final contrast evidence");
}

void testOuterHaloCannotUseInnerCoreEdge()
{
    auto request = requestWith(CompoundComponentRole::OuterHaloEdge);
    request.frame.ownerTable.emplace(2, FinalSemanticOwner{2,
        FinalSemanticTrack::Predicted, CompoundComponentRole::Core,
        "inner-core", 11});
    request.frame.pixels.clear();
    request.frame.pixels.push_back(pixel(24, 20, {1, 1, 1, 1}));
    request.frame.pixels.push_back(pixel(25, 20, {1, 1, 1, 1}));
    auto core = pixel(26, 20, {2, 2, 2, 2});
    core.finalLdrSrgb8 = {{8, 8, 8}};
    request.frame.pixels.push_back(core);
    const auto result = resolveIntegratedFinalSemanticOwnership(request);
    bool rejectedInner = false;
    for (const auto& boundary : result.boundaryTelemetry)
        rejectedInner = rejectedInner || (boundary.ownerComponent ==
            "outer_halo_edge" && boundary.boundaryClass ==
            "same_track_compound_inner_edge" && !boundary.eligibleForRole);
    pass("PAIR_N2_HALO_INNER_CORE_EDGE_REJECTED", !result.ok && rejectedInner,
        "outer halo evidence may use only the true outer silhouette");
}

void testCoreMayUseApprovedHaloInnerEdge()
{
    auto request = requestWith();
    request.frame.ownerTable.emplace(2, FinalSemanticOwner{2,
        FinalSemanticTrack::Predicted, CompoundComponentRole::OuterHaloEdge,
        "primitive-0-halo", 9});
    request.frame.pixels.clear();
    request.frame.pixels.push_back(pixel(20, 20, {1, 1, 1, 1}));
    request.frame.pixels.push_back(pixel(21, 20, {1, 1, 1, 1}));
    auto halo = pixel(22, 20, {2, 2, 2, 2});
    halo.finalLdrSrgb8 = {{8, 8, 8}};
    request.frame.pixels.push_back(halo);
    const auto result = resolveIntegratedFinalSemanticOwnership(request);
    bool selectedInner = false;
    for (const auto& boundary : result.boundaryTelemetry)
        selectedInner = selectedInner || (boundary.selected &&
            boundary.boundaryClass == "same_track_compound_inner_edge" &&
            boundary.ownerComponent == "core" &&
            boundary.adjacentComponent == "outer_halo_edge");
    pass("PAIR_P1_CORE_APPROVED_HALO_INNER_EDGE", result.ok && selectedInner,
        "core evidence may use an exact final-LDR edge against its halo");
}

void testTrueOuterSilhouetteEligibleForHalo()
{
    const auto result = resolveIntegratedFinalSemanticOwnership(requestWith(
        CompoundComponentRole::OuterHaloEdge));
    bool selectedOuter = false;
    for (const auto& boundary : result.boundaryTelemetry)
        selectedOuter = selectedOuter || (boundary.selected &&
            boundary.boundaryClass == "compound_outer_silhouette" &&
            boundary.ownerComponent == "outer_halo_edge");
    pass("PAIR_P2_TRUE_OUTER_SILHOUETTE", result.ok && selectedOuter,
        "outer halo is measured only against a true compound silhouette");
}

void testPixelIterationOrderInvariant()
{
    auto first = requestWith();
    auto second = first;
    std::reverse(second.frame.pixels.begin(), second.frame.pixels.end());
    const auto a = resolveIntegratedFinalSemanticOwnership(first);
    const auto b = resolveIntegratedFinalSemanticOwnership(second);
    const auto& ca = a.candidateTelemetry.front();
    const auto& cb = b.candidateTelemetry.front();
    pass("PAIR_N3_PIXEL_ORDER_INVARIANT", a.ok && b.ok &&
        ca.finalSrgb8 == cb.finalSrgb8 &&
        ca.adjacentNonSelfSrgb8 == cb.adjacentNonSelfSrgb8 &&
        ca.edgeContrastRatio == cb.edgeContrastRatio,
        "container and neighbor iteration order cannot change selection");
}

void testForeignAndUiBoundariesIneligible()
{
    auto request = requestWith();
    request.frame.ownerTable.emplace(2, FinalSemanticOwner{2,
        FinalSemanticTrack::Actual, CompoundComponentRole::OuterHaloEdge,
        "foreign-halo", 11});
    request.frame.pixels.clear();
    request.frame.pixels.push_back(pixel(20, 20, {1, 1, 1, 1}));
    request.frame.pixels.push_back(pixel(21, 20, {1, 1, 1, 1}));
    request.frame.pixels.push_back(pixel(19, 20, {2, 2, 2, 2}));
    auto ui = pixel(22, 20, {0, 0, 0, 0});
    ui.laterUiCoverage = 1.0;
    request.frame.pixels.push_back(ui);
    const auto result = resolveIntegratedFinalSemanticOwnership(request);
    bool foreign = false, laterUi = false, selected = false;
    for (const auto& boundary : result.boundaryTelemetry) {
        foreign = foreign || boundary.boundaryClass == "foreign_track_edge";
        laterUi = laterUi || boundary.boundaryClass == "later_ui_edge";
        selected = selected || boundary.selected;
    }
    pass("PAIR_N4_FOREIGN_AND_UI_INELIGIBLE", !result.ok && foreign &&
        laterUi && !selected,
        "foreign-track and later-UI boundaries remain ineligible");
}

void testMissingBoundaryPayloadFailsClosed()
{
    auto request = requestWith();
    request.frame.pixels.resize(2);
    const auto result = resolveIntegratedFinalSemanticOwnership(request);
    pass("PAIR_N5_MISSING_BOUNDARY_PAYLOAD_FAILS_CLOSED", !result.ok &&
        result.errorCode == "final_ownership_boundary_payload_incomplete",
        "owned islands without complete boundary alternatives fail closed");
}

IntegratedFinalOwnershipRequest crossProbeAssociationRequest(
    CompoundComponentRole sourceRole)
{
    auto request = requestWith(sourceRole);
    request.expectedSourceProbeCount = 2;
    request.minimumVisibleCoverage = 0.5;
    const std::string primitive = sourceRole ==
        CompoundComponentRole::OuterHaloEdge ?
        "association_dash_halo" : "association_dash_core";
    request.frame.ownerTable.at(1).primitiveId = primitive;
    request.sourceProbes.front().source.primitiveId = primitive;
    request.sourceProbes.front().source.componentRole = sourceRole;
    auto second = request.sourceProbes.front();
    second.source.probeOrdinal = 1;
    second.source.xPhysicalPx = 22;
    request.sourceProbes.push_back(second);
    request.frame.pixels.clear();
    request.frame.pixels.push_back(pixel(20, 20, {1, 1, 1, 1}));
    request.frame.pixels.push_back(pixel(21, 20, {1, 1, 1, 1}));
    request.frame.pixels.push_back(pixel(22, 20, {1, 1, 1, 1}));
    if (sourceRole == CompoundComponentRole::Core) {
        request.frame.ownerTable.emplace(2, FinalSemanticOwner{2,
            FinalSemanticTrack::Predicted,
            CompoundComponentRole::OuterHaloEdge,
            "association_dash_halo", 9});
        auto halo = pixel(23, 20, {2, 2, 2, 2});
        halo.finalLdrSrgb8 = {{0, 0, 0}};
        request.frame.pixels.push_back(halo);
        auto background = pixel(24, 20, {0, 0, 0, 0});
        background.finalLdrSrgb8 = {{8, 8, 8}};
        request.frame.pixels.push_back(background);
    } else {
        for (auto& item : request.frame.pixels)
            item.finalLdrSrgb8 = {{0, 0, 0}};
        auto background = pixel(23, 20, {0, 0, 0, 0});
        background.finalLdrSrgb8 = {{245, 245, 245}};
        request.frame.pixels.push_back(background);
    }
    return request;
}

const FinalOwnershipBoundaryTelemetry* selectedFor(
    const IntegratedFinalOwnershipResult& result, const std::string& sourceId)
{
    for (const auto& boundary : result.boundaryTelemetry)
        if (boundary.sourceProbeId == sourceId && boundary.selected)
            return &boundary;
    return nullptr;
}

void testAssociationCoreCanUseOtherAreaOwnersBoundary()
{
    const auto request = crossProbeAssociationRequest(
        CompoundComponentRole::Core);
    const std::string firstId = stableCompoundSourceProbeId(
        request.sourceProbes.front().source);
    const std::string secondId = stableCompoundSourceProbeId(
        request.sourceProbes.back().source);
    const auto result = resolveIntegratedFinalSemanticOwnership(request);
    const auto* selected = selectedFor(result, firstId);
    pass("ASSOC_P1_CORE_CROSS_PARTITION_INNER_EDGE", result.ok && selected &&
        selected->boundaryClass == "same_track_compound_inner_edge" &&
        selected->associationMode != "exclusive_area_owner" &&
        selected->boundaryAreaOwnerSourceProbeId == secondId &&
        result.audit.doubleAssignmentCount == 0,
        "boundary provenance may cross a same-island area partition without "
        "transferring exclusive area credit");
}

void testAssociationHaloCanUseOtherAreaOwnersSilhouette()
{
    const auto request = crossProbeAssociationRequest(
        CompoundComponentRole::OuterHaloEdge);
    const std::string firstId = stableCompoundSourceProbeId(
        request.sourceProbes.front().source);
    const std::string secondId = stableCompoundSourceProbeId(
        request.sourceProbes.back().source);
    const auto result = resolveIntegratedFinalSemanticOwnership(request);
    const auto* selected = selectedFor(result, firstId);
    pass("ASSOC_P2_HALO_CROSS_PARTITION_SILHOUETTE", result.ok && selected &&
        selected->boundaryClass == "compound_outer_silhouette" &&
        selected->associationMode == "exact_primitive_stem" &&
        selected->boundaryAreaOwnerSourceProbeId == secondId &&
        result.audit.doubleAssignmentCount == 0,
        "outer halo may associate another area owner's true silhouette only");
}

void testAssociationSameComponentSeamStillRejected()
{
    auto request = requestWith();
    request.frame.ownerTable.emplace(2, FinalSemanticOwner{2,
        FinalSemanticTrack::Predicted, CompoundComponentRole::Core,
        "other_core", 11});
    request.frame.pixels[2].sampleOwnerTokens.assign(4, 2);
    request.frame.pixels[3].sampleOwnerTokens.assign(4, 2);
    const auto result = resolveIntegratedFinalSemanticOwnership(request);
    pass("ASSOC_N1_SAME_COMPONENT_SEAM_REJECTED", !result.ok &&
        !result.audit.hiddenProbes.empty() &&
        result.audit.hiddenProbes.front().reason ==
            CompoundHiddenReason::NoEligibleComponentAwareBoundary &&
        std::none_of(result.boundaryTelemetry.begin(),
            result.boundaryTelemetry.end(), [](const auto& boundary) {
                return boundary.boundaryClass ==
                    "same_component_token_seam" &&
                    boundary.eligibleForRole;
            }), "island association cannot promote a raw token seam");
}

void testAssociationForeignAndUiStillRejected()
{
    auto request = requestWith();
    request.frame.ownerTable.emplace(2, FinalSemanticOwner{2,
        FinalSemanticTrack::Actual, CompoundComponentRole::OuterHaloEdge,
        "foreign_halo", 11});
    request.frame.pixels[2].sampleOwnerTokens.assign(4, 2);
    request.frame.pixels[3].laterUiCoverage = 1.0;
    const auto result = resolveIntegratedFinalSemanticOwnership(request);
    pass("ASSOC_N2_FOREIGN_UI_REJECTED", !result.ok &&
        std::none_of(result.boundaryTelemetry.begin(),
            result.boundaryTelemetry.end(), [](const auto& boundary) {
                return (boundary.boundaryClass == "foreign_track_edge" ||
                    boundary.boundaryClass == "later_ui_edge") &&
                    boundary.eligibleForRole;
            }), "island traversal cannot cross foreign-track or later UI");
}

void testAssociationOutsideBoundsRejected()
{
    auto request = requestWith();
    request.frame.ownerTable.emplace(2, FinalSemanticOwner{2,
        FinalSemanticTrack::Predicted,
        CompoundComponentRole::OuterHaloEdge, "primitive-0_halo", 11});
    request.frame.pixels.resize(2);
    request.frame.pixels.push_back(pixel(26, 20, {1, 1, 1, 1}));
    auto outsideHalo = pixel(27, 20, {2, 2, 2, 2});
    outsideHalo.finalLdrSrgb8 = {{8, 8, 8}};
    request.frame.pixels.push_back(outsideHalo);
    const auto result = resolveIntegratedFinalSemanticOwnership(request);
    pass("ASSOC_N3_OUTSIDE_BOUNDS_REJECTED", !result.ok &&
        result.errorCode == "final_ownership_boundary_payload_incomplete",
        "exact primitive association remains inside unchanged local bounds");
}

void testAssociationMissingIslandAlternativesFailsClosed()
{
    auto request = requestWith();
    request.frame.pixels.resize(2);
    const auto result = resolveIntegratedFinalSemanticOwnership(request);
    pass("ASSOC_N4_TRUNCATED_ISLAND_FAILS_CLOSED", !result.ok &&
        result.errorCode == "final_ownership_boundary_payload_incomplete",
        "missing associated-island alternatives cannot be a machine pass");
}

void testAssociationIterationOrderInvariant()
{
    auto first = crossProbeAssociationRequest(CompoundComponentRole::Core);
    auto second = first;
    const std::string sourceId = stableCompoundSourceProbeId(
        first.sourceProbes.front().source);
    std::reverse(second.frame.pixels.begin(), second.frame.pixels.end());
    std::reverse(second.sourceProbes.begin(), second.sourceProbes.end());
    const auto a = resolveIntegratedFinalSemanticOwnership(first);
    const auto b = resolveIntegratedFinalSemanticOwnership(second);
    const auto* firstSelected = selectedFor(a, sourceId);
    const auto* secondSelected = selectedFor(b, sourceId);
    pass("ASSOC_N5_ITERATION_ORDER_INVARIANT", a.ok && b.ok &&
        firstSelected && secondSelected &&
        firstSelected->ownerXPhysicalPx == secondSelected->ownerXPhysicalPx &&
        firstSelected->ownerYPhysicalPx == secondSelected->ownerYPhysicalPx &&
        firstSelected->adjacentXPhysicalPx ==
            secondSelected->adjacentXPhysicalPx &&
        firstSelected->adjacentYPhysicalPx ==
            secondSelected->adjacentYPhysicalPx &&
        firstSelected->deterministicSortKey ==
            secondSelected->deterministicSortKey,
        "pixel and source container order cannot change association selection");
}

IntegratedFinalOwnershipRequest rasterSupportRequest(
    const std::vector<std::pair<int, int>>& ownerPixels)
{
    auto request = requestWith();
    request.frame.msaaSamples = 1;
    request.frame.msaaSamplePositions = {{{0.5f, 0.5f}}};
    request.frame.pixels.clear();
    std::set<std::pair<int, int>> owners(ownerPixels.begin(), ownerPixels.end());
    std::set<std::pair<int, int>> backgrounds;
    for (const auto& coordinate : owners) {
        request.frame.pixels.push_back(pixel(
            coordinate.first, coordinate.second, {1}));
        for (const auto& offset : {std::pair<int, int>{-1, 0},
                 {1, 0}, {0, -1}, {0, 1}}) {
            const std::pair<int, int> adjacent{
                coordinate.first + offset.first,
                coordinate.second + offset.second};
            if (owners.count(adjacent) == 0) backgrounds.insert(adjacent);
        }
    }
    for (const auto& coordinate : backgrounds) {
        auto background = pixel(coordinate.first, coordinate.second, {0});
        background.finalLdrSrgb8 = {{8, 8, 8}};
        request.frame.pixels.push_back(background);
    }
    return request;
}

void testRasterSupportDiagonalContinuity()
{
    const auto result = resolveIntegratedFinalSemanticOwnership(
        rasterSupportRequest({{20, 20}, {21, 21}}));
    const auto& candidate = result.candidateTelemetry.front();
    pass("TOPO_P1_DIAGONAL_TWO_PIXEL_SUPPORT", result.ok &&
        candidate.resolvedPixelEquivalentArea == 2.0 &&
        candidate.exclusiveLargestFourConnectedPixelCount == 1 &&
        candidate.preArbitrationLargestEightConnectedPixelCount == 2,
        "a visually contiguous diagonal raster satisfies topology without "
        "weakening exclusive scalar area");
}

void testRasterSupportOrthogonalContinuity()
{
    const auto horizontal = resolveIntegratedFinalSemanticOwnership(
        rasterSupportRequest({{20, 20}, {21, 20}}));
    const auto vertical = resolveIntegratedFinalSemanticOwnership(
        rasterSupportRequest({{20, 20}, {20, 21}}));
    pass("TOPO_P2_ORTHOGONAL_TWO_PIXEL_SUPPORT", horizontal.ok && vertical.ok &&
        horizontal.candidateTelemetry.front().
            preArbitrationLargestEightConnectedPixelCount == 2 &&
        vertical.candidateTelemetry.front().
            preArbitrationLargestEightConnectedPixelCount == 2,
        "horizontal and vertical raster continuity remain accepted");
}

void testRasterSupportIsolatedPixelsRejected()
{
    const auto result = resolveIntegratedFinalSemanticOwnership(
        rasterSupportRequest({{20, 20}, {22, 22}}));
    const auto& candidate = result.candidateTelemetry.front();
    pass("TOPO_N1_ISOLATED_SCATTER_REJECTED", !result.ok &&
        candidate.resolvedPixelEquivalentArea == 2.0 &&
        candidate.preArbitrationLargestEightConnectedPixelCount == 1 &&
        candidate.eligibilityOrRejectionReason ==
            "insufficient_raster_continuity",
        "sufficient scattered scalar area is not visual continuity");
}

void testRasterSupportAreaStillRequired()
{
    auto request = requestWith();
    request.frame.pixels[1].sampleOwnerTokens = {1, 0, 0, 0};
    const auto result = resolveIntegratedFinalSemanticOwnership(request);
    const auto& candidate = result.candidateTelemetry.front();
    pass("TOPO_N2_EXCLUSIVE_AREA_STILL_REQUIRED", !result.ok &&
        candidate.resolvedPixelEquivalentArea == 1.25 &&
        candidate.preArbitrationLargestEightConnectedPixelCount == 2 &&
        candidate.eligibilityOrRejectionReason ==
            "insufficient_exclusive_owned_area",
        "connected support cannot rescue exclusive scalar area below 1.5");
}

void testRasterSupportIneligiblePixelsExcluded()
{
    auto foreign = requestWith(CompoundComponentRole::Core,
        FinalSemanticTrack::Actual);
    auto scene = requestWith();
    for (auto& item : scene.frame.pixels)
        item.sampleOwnerTokens.assign(4, 0);
    auto ui = requestWith();
    for (auto& item : ui.frame.pixels) item.laterUiCoverage = 1.0;
    auto clipped = requestWith();
    clipped.sourceProbes.front().source.insideFinalViewport = false;
    auto outside = requestWith();
    outside.frame.pixels.clear();
    outside.frame.pixels.push_back(pixel(30, 20, {1, 1, 1, 1}));
    outside.frame.pixels.push_back(pixel(31, 20, {1, 1, 1, 1}));
    const auto a = resolveIntegratedFinalSemanticOwnership(foreign);
    const auto b = resolveIntegratedFinalSemanticOwnership(scene);
    const auto c = resolveIntegratedFinalSemanticOwnership(ui);
    const auto d = resolveIntegratedFinalSemanticOwnership(clipped);
    const auto e = resolveIntegratedFinalSemanticOwnership(outside);
    pass("TOPO_N3_INELIGIBLE_SUPPORT_EXCLUDED", !a.ok && !b.ok && !c.ok &&
        !d.ok && !e.ok &&
        a.candidateTelemetry.front().preArbitrationSupportPixelCount == 0 &&
        b.candidateTelemetry.front().preArbitrationSupportPixelCount == 0 &&
        c.candidateTelemetry.front().preArbitrationSupportPixelCount == 0 &&
        d.candidateTelemetry.front().preArbitrationSupportPixelCount == 0 &&
        e.candidateTelemetry.front().preArbitrationSupportPixelCount == 0,
        "foreign, scene, later UI, clipped, and out-of-bounds pixels cannot "
        "enter topology support");
}

void testRasterSupportIterationOrderInvariant()
{
    auto first = rasterSupportRequest({{20, 20}, {21, 21}});
    auto second = first;
    std::reverse(second.frame.pixels.begin(), second.frame.pixels.end());
    const auto a = resolveIntegratedFinalSemanticOwnership(first);
    const auto b = resolveIntegratedFinalSemanticOwnership(second);
    pass("TOPO_N4_ITERATION_ORDER_INVARIANT", a.ok && b.ok &&
        a.candidateTelemetry.front().preArbitrationSupportPixelCount ==
            b.candidateTelemetry.front().preArbitrationSupportPixelCount &&
        a.candidateTelemetry.front().
            preArbitrationLargestEightConnectedPixelCount ==
            b.candidateTelemetry.front().
                preArbitrationLargestEightConnectedPixelCount &&
        a.audit.orderedVisibleProbeDigest == b.audit.orderedVisibleProbeDigest,
        "pixel iteration order cannot change raster topology");
}

void testRasterSupportDoesNotTransferExclusiveArea()
{
    auto first = requestWith();
    first.expectedSourceProbeCount = 2;
    first.sourceProbes.push_back(probe("primitive-1", 1));
    auto second = first;
    std::reverse(second.sourceProbes.begin(), second.sourceProbes.end());
    const auto a = resolveIntegratedFinalSemanticOwnership(first);
    const auto b = resolveIntegratedFinalSemanticOwnership(second);
    std::size_t areaA = 0, areaB = 0;
    for (const auto& candidate : a.candidateTelemetry)
        areaA += candidate.resolvedSampleCount;
    for (const auto& candidate : b.candidateTelemetry)
        areaB += candidate.resolvedSampleCount;
    pass("TOPO_N5_EXCLUSIVE_AREA_UNCHANGED", !a.ok && !b.ok &&
        a.audit.doubleAssignmentCount == 0 &&
        b.audit.doubleAssignmentCount == 0 && areaA == 8 && areaB == 8 &&
        a.arbitrationTelemetry.size() == b.arbitrationTelemetry.size(),
        "topology support does not transfer exclusive scalar area credit");
}

void testRasterSupportReasonSplit()
{
    auto belowArea = requestWith();
    belowArea.frame.pixels[1].sampleOwnerTokens = {1, 0, 0, 0};
    const auto area = resolveIntegratedFinalSemanticOwnership(belowArea);
    const auto continuity = resolveIntegratedFinalSemanticOwnership(
        rasterSupportRequest({{20, 20}, {22, 22}}));
    pass("TOPO_N6_REASON_SPLIT", !area.ok && !continuity.ok &&
        area.audit.hiddenProbes.front().reason ==
            CompoundHiddenReason::InsufficientExclusiveOwnedArea &&
        continuity.audit.hiddenProbes.front().reason ==
            CompoundHiddenReason::InsufficientRasterContinuity,
        "scalar area and raster continuity failures serialize separately");
}

TrajectoryOverlayStyle geometryStyle()
{
    TrajectoryOverlayStyle style;
    style.lineWidthPx = 3.0;
    style.haloWidthPx = 1.0;
    style.outerContrastRimWidthPx = 1.0;
    style.actualTickLengthPx = 9.0;
    style.actualTickSpacingPx = 32.0;
    style.endpointOuterDiameterPx = 3.0;
    style.actualEndpointDotDiameterPx = 3.0;
    return style;
}

IntegratedFinalOwnershipFrame geometryFrame()
{
    IntegratedFinalOwnershipFrame frame;
    frame.physicalWidth = 64;
    frame.physicalHeight = 64;
    frame.msaaSamples = 1;
    frame.msaaSamplePositions = {{{0.5f, 0.5f}}};
    return frame;
}

FinalOwnershipSourceProbe geometryProbe(CompoundVisibilityTrack track,
    CompoundComponentRole role, const std::string& primitive)
{
    FinalOwnershipSourceProbe source;
    source.source.captureId = "geometry";
    source.source.track = track;
    source.source.componentRole = role;
    source.source.primitiveId = primitive;
    source.source.arcSegmentId = "arc";
    source.source.physicalProfile = "64x64";
    source.source.xPhysicalPx = 20;
    source.source.yPhysicalPx = 20;
    source.source.sourceProbeSpacingPx = 20.0;
    source.tangentX = 1.0;
    source.normalY = 1.0;
    return source;
}

void addOwnedRectangle(IntegratedFinalOwnershipFrame& frame,
    int x0, int x1, int y0, int y1, std::uint32_t token)
{
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            frame.pixels.push_back(pixel(x, y, {token}));
}

void testFinalGeometryBodyWidthAndThinMutation()
{
    auto frame = geometryFrame();
    frame.ownerTable.emplace(1, FinalSemanticOwner{1,
        FinalSemanticTrack::Predicted, CompoundComponentRole::Core,
        "predicted_segment_0_core", 0});
    addOwnedRectangle(frame, 18, 22, 17, 23, 1);
    const auto source = geometryProbe(CompoundVisibilityTrack::Predicted,
        CompoundComponentRole::Core, "predicted_segment_0_core");
    const auto good = auditIntegratedFinalTrajectoryGeometry(
        frame, {source}, geometryStyle());
    auto guaranteedOnly = geometryFrame();
    guaranteedOnly.ownerTable = frame.ownerTable;
    addOwnedRectangle(guaranteedOnly, 18, 22, 18, 22, 1);
    const auto guaranteedOnlyAudit = auditIntegratedFinalTrajectoryGeometry(
        guaranteedOnly, {source}, geometryStyle());
    auto excessive = geometryFrame();
    excessive.ownerTable = frame.ownerTable;
    addOwnedRectangle(excessive, 18, 22, 13, 27, 1);
    const auto excessiveAudit = auditIntegratedFinalTrajectoryGeometry(
        excessive, {source}, geometryStyle());
    auto thin = frame;
    thin.pixels.erase(std::remove_if(thin.pixels.begin(), thin.pixels.end(),
        [](const auto& value) { return value.yPhysicalPx <= 20; }),
        thin.pixels.end());
    const auto bad = auditIntegratedFinalTrajectoryGeometry(
        thin, {source}, geometryStyle());
    auto brokenMiddle = frame;
    brokenMiddle.pixels.erase(std::remove_if(
        brokenMiddle.pixels.begin(), brokenMiddle.pixels.end(),
        [](const auto& value) { return value.xPhysicalPx == 20; }),
        brokenMiddle.pixels.end());
    const auto broken = auditIntegratedFinalTrajectoryGeometry(
        brokenMiddle, {source}, geometryStyle());
    pass("GEOMETRY_P1_BODY_WIDTH_AND_THIN_NEGATIVE", good.ok &&
        guaranteedOnlyAudit.ok && !excessiveAudit.ok &&
        std::fabs(good.probes.front().measuredNormalWidthPx - 7.0) <= 1.0 &&
        !bad.ok && !broken.ok &&
        bad.errorCode == "final_trajectory_body_width_mismatch",
        "the guaranteed core-plus-inner-halo width passes without an adaptive rim while thin, excessive, and internally broken masks fail");
}

void testFinalGeometryTickWingsSurvivePredictedCenter()
{
    auto style = geometryStyle();
    style.actualTickLengthPx = 13.0;
    auto frame = geometryFrame();
    frame.ownerTable.emplace(1, FinalSemanticOwner{1,
        FinalSemanticTrack::Actual, CompoundComponentRole::Tick,
        "actual_tick_0_core", 0});
    frame.ownerTable.emplace(2, FinalSemanticOwner{2,
        FinalSemanticTrack::Predicted, CompoundComponentRole::Core,
        "predicted_segment_0_core", 1});
    for (int y = 17; y <= 23; ++y)
        for (int x = 14; x <= 26; ++x)
            frame.pixels.push_back(pixel(x, y,
                {x >= 19 && x <= 21 ? 2u : 1u}));
    const auto source = geometryProbe(CompoundVisibilityTrack::Actual,
        CompoundComponentRole::Tick, "actual_tick_0_core");
    const auto audit = auditIntegratedFinalTrajectoryGeometry(
        frame, {source}, style);
    auto brokenWing = frame;
    brokenWing.pixels.erase(std::remove_if(brokenWing.pixels.begin(),
        brokenWing.pixels.end(), [](const auto& value) {
            return value.xPhysicalPx == 15;
        }), brokenWing.pixels.end());
    const auto broken = auditIntegratedFinalTrajectoryGeometry(
        brokenWing, {source}, style);
    pass("GEOMETRY_P2_TICK_WINGS_NOT_CENTER", audit.ok &&
        audit.probes.front().measuredTangentSpanPx == 13.0 &&
        audit.probes.front().centerOwnerTrack == "predicted" &&
        !broken.ok,
        "both full tick wings remain continuous even when predicted compositing owns center");
}

void testTickActualMsaaLattice()
{
    bool allCases = true;
    for (double angle : {0.0, 1.5707963267948966, 0.41}) {
        auto style = geometryStyle();
        style.haloWidthPx = style.outerContrastRimWidthPx = 4.0/3.0;
        style.actualTickLengthPx = 12.333333333333334;
        auto frame = geometryFrame();
        frame.msaaSamples = 4;
        frame.msaaSamplePositions = {{{.375f,.125f}},{{.875f,.375f}},
            {{.125f,.625f}},{{.625f,.875f}}};
        frame.ownerTable.emplace(1, FinalSemanticOwner{1,
            FinalSemanticTrack::Actual, CompoundComponentRole::Tick,
            "actual_tick_0_core", 0});
        auto source = geometryProbe(CompoundVisibilityTrack::Actual,
            CompoundComponentRole::Tick, "actual_tick_0_core");
        source.geometryCenter = std::array<double, 2>{{20.2,20.2}};
        source.tangentX = std::cos(angle); source.tangentY = std::sin(angle);
        source.normalX = -std::sin(angle); source.normalY = std::cos(angle);
        for (int y=10;y<31;++y) for (int x=10;x<31;++x) {
            auto p = pixel(x,y,{0,0,0,0});
            for (int i=0;i<4;++i) {
                const double dx=x+frame.msaaSamplePositions[i][0]-20.2;
                const double dy=y+1.0-frame.msaaSamplePositions[i][1]-20.2;
                const double t=dx*source.tangentX+dy*source.tangentY;
                const double n=dx*source.normalX+dy*source.normalY;
                if (std::fabs(t)<style.actualTickLengthPx*.5 &&
                    std::fabs(n)<(style.lineWidthPx+2*style.haloWidthPx)*.5)
                    p.sampleOwnerTokens[i]=1;
            }
            frame.pixels.push_back(p);
        }
        allCases &= auditIntegratedFinalTrajectoryGeometry(frame,{source},style).ok;
        for (int mutation=0;mutation<3;++mutation) {
            auto broken=frame;
            for (auto& p : broken.pixels) for (int i=0;i<4;++i) {
                const double dx=p.xPhysicalPx+frame.msaaSamplePositions[i][0]-20.2;
                const double dy=p.yPhysicalPx+1.0-frame.msaaSamplePositions[i][1]-20.2;
                const double t=dx*source.tangentX+dy*source.tangentY;
                const double n=dx*source.normalX+dy*source.normalY;
                if ((mutation==0 && t>4.2) ||
                    (mutation==1 && std::fabs(n)>0.5) ||
                    (mutation==2 && t>5.0 && t<5.75))
                    p.sampleOwnerTokens[i]=0;
            }
            allCases &= !auditIntegratedFinalTrajectoryGeometry(broken,{source},style).ok;
        }
    }
    pass("GEOMETRY_MSAA_AUTHORED_WINGS", allCases,
        "real 4x lattice passes axis-aligned/rotated subpixel ticks; missing, thin and interrupted wings fail");
}

void testFinalGeometryEndpointAndMissingEdge()
{
    auto frame = geometryFrame();
    frame.ownerTable.emplace(1, FinalSemanticOwner{1,
        FinalSemanticTrack::Predicted, CompoundComponentRole::Endpoint,
        "predicted_endpoint_core", 0});
    for (int y = 17; y <= 23; ++y)
        for (int x = 17; x <= 23; ++x)
            if (x <= 18 || x >= 22 || y <= 18 || y >= 22)
                frame.pixels.push_back(pixel(x, y, {1}));
    const auto source = geometryProbe(CompoundVisibilityTrack::Predicted,
        CompoundComponentRole::Endpoint, "predicted_endpoint_core");
    const auto good = auditIntegratedFinalTrajectoryGeometry(
        frame, {source}, geometryStyle());
    auto missing = frame;
    missing.pixels.erase(std::remove_if(missing.pixels.begin(),
        missing.pixels.end(), [](const auto& value) {
            return value.xPhysicalPx <= 18;
        }), missing.pixels.end());
    const auto bad = auditIntegratedFinalTrajectoryGeometry(
        missing, {source}, geometryStyle());
    auto brokenRing = frame;
    brokenRing.pixels.erase(std::remove_if(brokenRing.pixels.begin(),
        brokenRing.pixels.end(), [](const auto& value) {
            return value.yPhysicalPx <= 18;
        }), brokenRing.pixels.end());
    const auto brokenRingAudit = auditIntegratedFinalTrajectoryGeometry(
        brokenRing, {source}, geometryStyle());
    pass("GEOMETRY_P3_ENDPOINT_SIZE_AND_MISSING_EDGE", good.ok && !bad.ok &&
        !brokenRingAudit.ok &&
        bad.errorCode == "final_trajectory_endpoint_geometry_mismatch",
        "predicted endpoint annulus keeps its hole but rejects a missing edge or disconnected angular gap");

    auto actual = geometryFrame();
    actual.ownerTable.emplace(1, FinalSemanticOwner{1,
        FinalSemanticTrack::Actual, CompoundComponentRole::Endpoint,
        "actual_endpoint_core", 0});
    addOwnedRectangle(actual, 17, 23, 17, 23, 1);
    const auto actualSource = geometryProbe(
        CompoundVisibilityTrack::Actual, CompoundComponentRole::Endpoint,
        "actual_endpoint_core");
    auto hollowActual = actual;
    hollowActual.pixels.erase(std::remove_if(hollowActual.pixels.begin(),
        hollowActual.pixels.end(), [](const auto& value) {
            return value.xPhysicalPx > 17 && value.xPhysicalPx < 23 &&
                value.yPhysicalPx > 17 && value.yPhysicalPx < 23;
        }), hollowActual.pixels.end());
    const auto actualAudit = auditIntegratedFinalTrajectoryGeometry(
        actual, {actualSource}, geometryStyle());
    const auto hollowActualAudit = auditIntegratedFinalTrajectoryGeometry(
        hollowActual, {actualSource}, geometryStyle());
    pass("GEOMETRY_P4_ACTUAL_ENDPOINT_FILLED", actualAudit.ok &&
        !hollowActualAudit.ok,
        "actual endpoint remains a filled dot and cannot be replaced by the predicted annulus shape");
}

void testFinalGeometryPartialUiRejectedOpaqueReplacementAllowed()
{
    auto base = geometryFrame();
    base.ownerTable.emplace(1, FinalSemanticOwner{1,
        FinalSemanticTrack::Predicted, CompoundComponentRole::Core,
        "predicted_segment_0_core", 0});
    addOwnedRectangle(base, 18, 22, 17, 23, 1);
    const auto source = geometryProbe(CompoundVisibilityTrack::Predicted,
        CompoundComponentRole::Core, "predicted_segment_0_core");
    auto partial = base;
    partial.pixels.front().laterUiCoverage = 0.5;
    const auto rejected = auditIntegratedFinalTrajectoryGeometry(
        partial, {source}, geometryStyle());
    auto world = partial;
    world.pixels.front().interactiveChromeCoverage = 0.0;
    const auto worldAudit = auditIntegratedFinalTrajectoryGeometry(
        world, {source}, geometryStyle());
    pass("WORLD_ANNOTATION_NOT_INTERACTIVE_CHROME",
        worldAudit.partialUiOverlapPixelCount == 0 &&
        world.pixels.front().laterUiCoverage == 0.5,
        "teaching classification preserves total later overlay coverage");
    auto opaque = base;
    auto covered = pixel(40, 40, {1});
    covered.laterUiCoverage = 1.0;
    opaque.pixels.push_back(covered);
    const auto accepted = auditIntegratedFinalTrajectoryGeometry(
        opaque, {source}, geometryStyle());
    pass("GEOMETRY_N1_PARTIAL_UI_VS_OPAQUE_REPLACEMENT", !rejected.ok &&
        rejected.errorCode == "final_trajectory_partially_visible_under_ui" &&
        accepted.ok && accepted.fullyUiCoveredTrajectoryPixelCount == 1,
        "translucent HUD leakage fails while a fully replaced hidden trajectory does not");
}

void testFinalGeometryMalformedPayloadFailsClosed()
{
    auto frame = geometryFrame();
    frame.ownerTable.emplace(1, FinalSemanticOwner{1,
        FinalSemanticTrack::Predicted, CompoundComponentRole::Core,
        "predicted_segment_0_core", 0});
    addOwnedRectangle(frame, 18, 22, 17, 23, 1);
    const auto source = geometryProbe(CompoundVisibilityTrack::Predicted,
        CompoundComponentRole::Core, "predicted_segment_0_core");

    auto truncatedSamples = frame;
    truncatedSamples.pixels.front().sampleOwnerTokens.clear();
    auto duplicatePixel = frame;
    duplicatePixel.pixels.push_back(duplicatePixel.pixels.front());
    auto badSamplePosition = frame;
    badSamplePosition.msaaSamplePositions.front()[0] =
        std::numeric_limits<float>::quiet_NaN();
    auto badSource = source;
    badSource.tangentX = std::numeric_limits<double>::quiet_NaN();
    auto badStyle = geometryStyle();
    badStyle.actualTickSpacingPx = 0.0;

    const auto truncated = auditIntegratedFinalTrajectoryGeometry(
        truncatedSamples, {source}, geometryStyle());
    const auto duplicate = auditIntegratedFinalTrajectoryGeometry(
        duplicatePixel, {source}, geometryStyle());
    const auto samplePosition = auditIntegratedFinalTrajectoryGeometry(
        badSamplePosition, {source}, geometryStyle());
    const auto sourceAudit = auditIntegratedFinalTrajectoryGeometry(
        frame, {badSource}, geometryStyle());
    const auto styleAudit = auditIntegratedFinalTrajectoryGeometry(
        frame, {source}, badStyle);
    pass("GEOMETRY_N2_MALFORMED_PAYLOAD_FAILS_CLOSED",
        !truncated.ok && !duplicate.ok && !samplePosition.ok &&
        !sourceAudit.ok && !styleAudit.ok,
        "truncated MSAA owners, duplicate pixels, NaNs, and invalid style inputs fail without indexing undefined data");
}

}  // namespace

int main()
{
    try {
        testPanelOcclusion();
        testBloomRenderGraphOrder();
        testCrossTrackNoRescue();
        testCrossComponentSameTrackRescue();
        testFinalColorMismatch();
        testSharedPixelArbitration();
        testTangentNormalBounds();
        testForeignTrackColorMatch();
        testSceneBackgroundColorMatch();
        testMsaaBelowArea();
        testMsaaAboveArea();
        testClippedSample();
        testFailureFieldOrFlushMissing();
        testDifferentTokenSameComponentSeamRejected();
        testOuterHaloCannotUseInnerCoreEdge();
        testCoreMayUseApprovedHaloInnerEdge();
        testTrueOuterSilhouetteEligibleForHalo();
        testPixelIterationOrderInvariant();
        testForeignAndUiBoundariesIneligible();
        testMissingBoundaryPayloadFailsClosed();
        testAssociationCoreCanUseOtherAreaOwnersBoundary();
        testAssociationHaloCanUseOtherAreaOwnersSilhouette();
        testAssociationSameComponentSeamStillRejected();
        testAssociationForeignAndUiStillRejected();
        testAssociationOutsideBoundsRejected();
        testAssociationMissingIslandAlternativesFailsClosed();
        testAssociationIterationOrderInvariant();
        testRasterSupportDiagonalContinuity();
        testRasterSupportOrthogonalContinuity();
        testRasterSupportIsolatedPixelsRejected();
        testRasterSupportAreaStillRequired();
        testRasterSupportIneligiblePixelsExcluded();
        testRasterSupportIterationOrderInvariant();
        testRasterSupportDoesNotTransferExclusiveArea();
        testRasterSupportReasonSplit();
        testFinalGeometryBodyWidthAndThinMutation();
        testFinalGeometryTickWingsSurvivePredictedCenter();
        testTickActualMsaaLattice();
        testFinalGeometryEndpointAndMissingEdge();
        testFinalGeometryPartialUiRejectedOpaqueReplacementAllowed();
        testFinalGeometryMalformedPayloadFailsClosed();
        expect(passedFixtures == 45, "exactly 45 gate fixtures must pass");
        std::cout << "phase4 final semantic ownership fixtures: 45/45 PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "phase4 final semantic ownership tests failed: "
                  << error.what() << '\n';
        return 1;
    }
}
