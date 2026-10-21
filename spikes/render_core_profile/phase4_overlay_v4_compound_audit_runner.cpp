#include "automation_json.h"
#include "training/trajectory_compound_visibility.h"

#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    using namespace billiardgl;
    using namespace billiardgl::training;
    if (argc != 2 || std::string(argv[1]) != "--contract-self-test") {
        std::cerr << "v4 compound runner is telemetry-only; "
            "actual GPU capture is not authorized\n";
        return 2;
    }
    CompoundVisibilityAuditRequest request;
    request.minimumSourceProbeCount = 159;
    request.expectedSourceProbeCount = 159;
    request.predictedActualLumaDeltaSrgb8 = 40.0;
    for (int index = 0; index < 159; ++index) {
        CompoundSourceProbe probe;
        probe.captureId = "v4_contract_self_test_not_gpu_evidence";
        probe.track = index % 2 == 0 ? CompoundVisibilityTrack::Predicted :
            CompoundVisibilityTrack::Actual;
        probe.primitiveId = "p-" + std::to_string(index);
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
        probe.arcSegmentId = "a-" + std::to_string(index / 8);
        probe.probeOrdinal = static_cast<std::uint64_t>(index);
        probe.physicalProfile = "contract-self-test";
        probe.outerHaloRadiusPx = 5.0;
        probe.halfTickLengthPx = 8.0;
        probe.sourceProbeSpacingPx = 20.0;
        request.sourceProbes.push_back(probe);
        CompoundOwnershipCandidate candidate;
        candidate.sourceProbeId = stableCompoundSourceProbeId(probe);
        candidate.xPhysicalPx = index * 3;
        candidate.yPhysicalPx = 10 + index % 7;
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
    const CompoundVisibilityAudit audit =
        auditCompoundFinalVisibility(request);
    json::Value output = json::Value::object();
    output["schema"] = json::Value(
        "billiardgl.phase4.overlay_v4_compound_visibility_runner.v1");
    output["status"] = json::Value(audit.ok ?
        "OFFLINE_CONTRACT_SELF_TEST_PASS" : "OFFLINE_CONTRACT_SELF_TEST_FAIL");
    output["actual_gpu_capture_count"] = json::Value(0);
    output["gpu_capture_authorized"] = json::Value(false);
    output["formal68_authorized"] = json::Value(false);
    output["source_probe_count"] = json::Value(
        static_cast<double>(audit.sourceProbeCount));
    output["visible_probe_count"] = json::Value(
        static_cast<double>(audit.visibleProbeCount));
    output["source_probe_id_digest"] = json::Value(
        audit.orderedSourceProbeDigest);
    output["visible_probe_id_digest"] = json::Value(
        audit.orderedVisibleProbeDigest);
    output["hidden_probe_count"] = json::Value(
        static_cast<double>(audit.hiddenProbeCount));
    output["aggregate_visible_probe_coverage"] = json::Value(
        audit.visibleProbeCoverage);
    output["predicted_source_probe_count"] = json::Value(
        static_cast<double>(audit.predicted.source));
    output["predicted_visible_probe_count"] = json::Value(
        static_cast<double>(audit.predicted.visible));
    output["predicted_hidden_probe_count"] = json::Value(
        static_cast<double>(audit.predicted.hidden));
    output["predicted_visible_probe_coverage"] = json::Value(
        audit.predicted.coverage);
    output["actual_source_probe_count"] = json::Value(
        static_cast<double>(audit.actual.source));
    output["actual_visible_probe_count"] = json::Value(
        static_cast<double>(audit.actual.visible));
    output["actual_hidden_probe_count"] = json::Value(
        static_cast<double>(audit.actual.hidden));
    output["actual_visible_probe_coverage"] = json::Value(
        audit.actual.coverage);
    output["minimum_visible_edge_contrast_ratio"] = json::Value(
        audit.minimumVisibleEdgeContrastRatio);
    output["contrast_4_5_target_coverage"] = json::Value(
        audit.designTargetContrastCoverage);
    output["double_assignment_count"] = json::Value(
        static_cast<double>(audit.doubleAssignmentCount));
    output["foreign_track_evidence_count"] = json::Value(
        static_cast<double>(audit.foreignTrackEvidenceCount));
    output["unowned_evidence_count"] = json::Value(
        static_cast<double>(audit.unownedEvidenceCount));
    json::Value componentSource = json::Value::object();
    for (const auto& value : audit.componentSourceCounts)
        componentSource[value.first] = json::Value(
            static_cast<double>(value.second));
    output["component_source_counts"] = componentSource;
    json::Value componentVisible = json::Value::object();
    for (const auto& value : audit.componentVisibleCounts)
        componentVisible[value.first] = json::Value(
            static_cast<double>(value.second));
    output["component_visible_counts"] = componentVisible;
    output["component_hidden_reason_counts"] = json::Value::object();
    output["error_code"] = json::Value(audit.errorCode);
    std::cout << json::stringify(output) << '\n';
    return audit.ok ? 0 : 1;
}
