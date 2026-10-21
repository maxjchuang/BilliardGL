#include "automation_json.h"
#include "training/phase4_final_ownership_instrumentation.h"

#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace {

using namespace billiardgl;
using namespace billiardgl::training;

void expect(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

std::string temporaryPath()
{
    std::string pattern = "/tmp/billiardgl-phase4-telemetry-XXXXXX";
    const int descriptor = mkstemp(pattern.data());
    if (descriptor < 0) throw std::runtime_error("mkstemp_failed");
    close(descriptor);
    unlink(pattern.c_str());
    return pattern;
}

std::string readFile(const std::string& path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>());
}

Phase4CaptureTelemetryInput completeInput(bool machinePass)
{
    Phase4CaptureTelemetryInput input;
    input.captureId = "camera_player_low_1080";
    input.planOrdinalOneBased = 1;
    input.machinePass = machinePass;
    input.machineErrorCode = machinePass ? "" :
        "compound_coverage_below_contract";
    input.machinePredicateLedgerRequired = true;
    input.machinePredicateLedger = {
        {"ownership_ok", machinePass, machinePass ? "true" : "false", "true"},
        {"trajectory_draw_submissions_exact_2", true, "2", "2"},
    };
    input.ppmPath = "read-only/frozen-ordinal1.ppm";
    input.ppmSha256 = std::string(64, 'a');
    input.integratedFinalOwnership = false;
    input.legacyPpmDiagnosticOnly = true;
    input.legacyPpmContrastOk = false;
    input.legacyPpmVisibleProbeCount = 46;
    input.legacyPpmHiddenProbeCount = 49;
    input.legacyPpmVisibleProbeCoverage = 46.0 / 95.0;
    input.legacyIntegratedDecisionDiscrepancy = machinePass;
    input.sourceAndBinaryBindings = {
        {"design_gate_sha256", std::string(64, 'b')},
        {"runner_sha256", std::string(64, 'c')},
    };

    CompoundVisibilityAudit& audit = input.audit;
    audit.ok = machinePass;
    audit.errorCode = input.machineErrorCode;
    audit.sourceProbeCount = 95;
    audit.visibleProbeCount = 46;
    audit.hiddenProbeCount = 49;
    audit.visibleProbeCoverage = 46.0 / 95.0;
    audit.predicted = {48, 24, 24, 0.5};
    audit.actual = {47, 22, 25, 22.0 / 47.0};
    audit.componentSourceCounts = {{"core", 95}};
    audit.componentVisibleCounts = {{"core", 46}};
    audit.componentHiddenReasonCounts =
        {{"core:no_same_track_final_owner", 49}};
    for (int index = 0; index < 95; ++index) {
        const std::string id = "probe-" + std::to_string(index);
        audit.orderedSourceProbeIds.push_back(id);
        if (index < 46) {
            audit.orderedVisibleProbeIds.push_back(id);
        } else {
            CompoundHiddenProbeRecord hidden;
            hidden.sourceProbeId = id;
            hidden.track = index % 2 == 0 ? CompoundVisibilityTrack::Predicted :
                CompoundVisibilityTrack::Actual;
            hidden.componentRole = CompoundComponentRole::Core;
            hidden.reason = CompoundHiddenReason::NoSameTrackFinalOwner;
            hidden.xPhysicalPx = 100 + index;
            hidden.yPhysicalPx = 200 + index;
            audit.hiddenProbes.push_back(hidden);
        }
    }
    audit.orderedSourceProbeDigest = std::string(64, 'd');
    audit.orderedVisibleProbeDigest = std::string(64, 'e');

    for (int index = 0; index < 95; ++index) {
        FinalOwnershipCandidateTelemetry candidate;
        candidate.sourceProbeId = "probe-" + std::to_string(index);
        candidate.xPhysicalPx = 100 + index;
        candidate.yPhysicalPx = 200 + index;
        candidate.resolvedSampleCount = 8;
        candidate.resolvedPixelEquivalentArea = 2.0;
        candidate.exclusiveOwnedPixelCount = 2;
        candidate.exclusiveLargestFourConnectedPixelCount = 1;
        candidate.preArbitrationSupportPixelCount = 4;
        candidate.preArbitrationLargestEightConnectedPixelCount = 4;
        candidate.ownerTrack = index % 2 == 0 ? "predicted" : "actual";
        candidate.ownerComponent = "core";
        candidate.ownerPrimitiveId = "primitive-" + std::to_string(index);
        candidate.ownerVisualSublayer = "core";
        candidate.finalSrgb8 = {{240, 240, 240}};
        candidate.adjacentNonSelfSrgb8 = {{10, 10, 10}};
        candidate.edgeContrastRatio = 16.0;
        candidate.eligibilityOrRejectionReason = index < 46 ?
            "eligible" : "no_same_track_final_owner";
        input.ownershipCandidates.push_back(candidate);

        FinalOwnershipBoundaryTelemetry boundary;
        boundary.sourceProbeId = candidate.sourceProbeId;
        boundary.ownerXPhysicalPx = candidate.xPhysicalPx;
        boundary.ownerYPhysicalPx = candidate.yPhysicalPx;
        boundary.adjacentXPhysicalPx = candidate.xPhysicalPx + 1;
        boundary.adjacentYPhysicalPx = candidate.yPhysicalPx;
        boundary.ownerTrack = candidate.ownerTrack;
        boundary.ownerComponent = candidate.ownerComponent;
        boundary.ownerPrimitiveId = candidate.ownerPrimitiveId;
        boundary.ownerVisualSublayer = "outer_rim";
        boundary.adjacentTrack = "none";
        boundary.adjacentComponent = "none";
        boundary.adjacentVisualSublayer = "scene";
        boundary.ownerFinalSrgb8 = candidate.finalSrgb8;
        boundary.adjacentFinalSrgb8 = candidate.adjacentNonSelfSrgb8;
        boundary.edgeContrastRatio = candidate.edgeContrastRatio;
        boundary.boundaryClass = "compound_outer_silhouette";
        boundary.deterministicSortKey = "outer|0|0|right";
        boundary.associationMode = "exclusive_area_owner";
        boundary.associationSeedPrimitiveStem = candidate.ownerPrimitiveId;
        boundary.associatedOwnedIslandId = "fixture-island-0";
        boundary.boundaryAreaOwnerSourceProbeId = candidate.sourceProbeId;
        boundary.eligibleForRole = true;
        boundary.selected = true;
        input.componentAwareBoundaryRecords.push_back(boundary);

        FinalOwnershipArbitrationTelemetry arbitration;
        arbitration.physicalSampleOrPixelId = std::to_string(100 + index) +
            "," + std::to_string(200 + index) + ":resolved";
        arbitration.claimingSourceProbeIds = {candidate.sourceProbeId};
        arbitration.winnerSourceProbeId = candidate.sourceProbeId;
        arbitration.deterministicSortKey = candidate.ownerTrack + "|0|" +
            candidate.ownerPrimitiveId + "|" + std::to_string(index);
        input.arbitrationRecords.push_back(arbitration);
    }
    return input;
}

void assertCompleteRow(const json::Value& row, const std::string& status)
{
    const char* required[] = {
        "capture_id", "plan_ordinal_one_based", "machine_status",
        "machine_error_code", "ppm_path", "ppm_sha256",
        "machine_predicate_ledger_required", "machine_predicate_ledger",
        "failed_machine_predicate_names",
        "source_probe_count", "visible_probe_count", "hidden_probe_count",
        "aggregate_visible_probe_coverage",
        "predicted_source_visible_hidden_and_coverage",
        "actual_source_visible_hidden_and_coverage", "component_source_counts",
        "component_visible_counts", "component_hidden_reason_counts",
        "ordered_source_probe_ids", "ordered_source_probe_digest",
        "ordered_visible_probe_ids", "ordered_visible_probe_digest",
        "hidden_probe_id_track_role_reason_coordinates",
        "ownership_candidate_records", "exclusive_arbitration_records",
        "component_aware_boundary_records",
        "double_assignment_count", "foreign_track_evidence_count",
        "unowned_evidence_count", "instrumentation_schema_version",
        "source_and_binary_bindings",
        "authoritative_visibility_gate", "legacy_ppm_diagnostic_only",
        "legacy_ppm_contrast_ok", "legacy_ppm_visible_probe_count",
        "legacy_ppm_hidden_probe_count", "legacy_ppm_visible_probe_coverage",
        "legacy_integrated_decision_discrepancy",
    };
    for (const char* field : required)
        expect(row.has(field), std::string("missing required row field: ") + field);
    expect(row.at("machine_status").asString() == status,
        "machine status changed while preserving telemetry");
    const auto& candidate = row.at("ownership_candidate_records").asArray().front();
    expect(candidate.at("exclusive_owned_pixel_count").asInt() == 2 &&
        candidate.at("exclusive_largest_four_connected_pixel_count").asInt() == 1 &&
        candidate.at("pre_arbitration_support_pixel_count").asInt() == 4 &&
        candidate.at("pre_arbitration_largest_eight_connected_pixel_count").asInt() == 4,
        "area/topology provenance must be serialized before machine require");
    expect(row.at("machine_predicate_ledger_required").asBool() &&
        row.at("machine_predicate_ledger").asArray().size() == 2 &&
        row.at("machine_predicate_ledger").asArray()[1].at("name").asString() ==
            "trajectory_draw_submissions_exact_2" &&
        row.at("machine_predicate_ledger").asArray()[1].at("measured").asString() ==
            "2" &&
        row.at("machine_predicate_ledger").asArray()[1].at("expected").asString() ==
            "2" &&
        row.at("failed_machine_predicate_names").asArray().size() ==
            (status == "PASS" ? 0 : 1),
        "complete named machine predicate ledger is serialized before require");
    expect(row.at("source_probe_count").asInt() == 95 &&
        row.at("ordered_source_probe_ids").asArray().size() == 95 &&
        row.at("hidden_probe_id_track_role_reason_coordinates").asArray().size() == 49,
        "complete immutable inventory is serialized");
    expect(row.at("ownership_candidate_records").asArray().size() == 95 &&
        row.at("exclusive_arbitration_records").asArray().size() == 95,
        "candidate and arbitration records are serialized");
    expect(row.at("component_aware_boundary_records").asArray().size() == 95,
        "complete component-aware boundary alternatives are serialized");
    const auto& firstBoundary =
        row.at("component_aware_boundary_records").asArray().front();
    expect(firstBoundary.at("association_mode").asString() ==
            "exclusive_area_owner" &&
        firstBoundary.at("association_seed_primitive_stem").asString() ==
            "primitive-0" &&
        firstBoundary.at("associated_owned_island_id").asString() ==
            "fixture-island-0" &&
        !firstBoundary.at("boundary_area_owner_source_probe_id").asString().empty(),
        "boundary association provenance is serialized independently of area");
    expect(firstBoundary.at("owner_visual_sublayer").asString() ==
            "outer_rim" &&
        firstBoundary.at("adjacent_visual_sublayer").asString() == "scene",
        "visual sublayer provenance is serialized beneath immutable ownership identity");
    expect(row.at("authoritative_visibility_gate").asString() ==
            "corrected_integrated_final_semantic_ownership" &&
        row.at("legacy_ppm_diagnostic_only").asBool(),
        "legacy PPM remains serialized but cannot be authoritative");
}

void testFailedDecisionIsDurableBeforeThrow()
{
    const std::string path = temporaryPath();
    bool caught = false;
    try {
        preservePhase4TelemetryThenEnforceMachineGate(path, completeInput(false));
    } catch (const std::runtime_error& error) {
        caught = std::string(error.what()).find(
            "phase4_overlay_machine_gate_failed") == 0;
    }
    expect(caught, "unchanged failed machine decision must throw");
    const std::string bytes = readFile(path);
    expect(!bytes.empty() && bytes.back() == '\n' &&
        bytes.find('\n') == bytes.size() - 1,
        "exactly one durable JSONL row precedes the machine throw");
    const json::ParseResult parsed = json::parse(bytes);
    expect(parsed.ok, "preserved failure row parses");
    assertCompleteRow(parsed.value, "FAILED");
    unlink(path.c_str());
}

void testPassDecisionWritesSameSchemaExactlyOnce()
{
    const std::string path = temporaryPath();
    preservePhase4TelemetryThenEnforceMachineGate(path, completeInput(true));
    const std::string bytes = readFile(path);
    expect(!bytes.empty() && bytes.find('\n') == bytes.size() - 1,
        "pass decision appends exactly one row");
    const json::ParseResult parsed = json::parse(bytes);
    expect(parsed.ok, "pass row parses");
    assertCompleteRow(parsed.value, "PASS");
    unlink(path.c_str());
}

void testIncompleteRowFailsClosedWithoutClaim()
{
    const std::string path = temporaryPath();
    Phase4CaptureTelemetryInput input = completeInput(true);
    input.ownershipCandidates.clear();
    const Phase4TelemetryWriteResult result =
        appendPhase4CaptureTelemetryRowDurable(path, input);
    expect(!result.ok && result.errorCode ==
            "phase4_telemetry_ownership_records_missing" &&
        readFile(path).empty(),
        "incomplete telemetry fails closed before claiming a result");
    unlink(path.c_str());
}

void testForcedFlushFailureIsDistinctAndCannotPass()
{
    const std::string path = temporaryPath();
    Phase4CaptureTelemetryInput input = completeInput(true);
    input.forceDurableFlushFailureForTest = true;
    bool caught = false;
    try {
        preservePhase4TelemetryThenEnforceMachineGate(path, input);
    } catch (const std::runtime_error& error) {
        caught = std::string(error.what()) ==
            "phase4_telemetry_durable_flush_failed";
    }
    expect(caught && readFile(path).empty(),
        "flush failure has a distinct error and no PASS claim");
    unlink(path.c_str());
}

void testMissingBoundarySetFailsClosed()
{
    const std::string path = temporaryPath();
    Phase4CaptureTelemetryInput input = completeInput(true);
    input.componentAwareBoundaryRecords.clear();
    const Phase4TelemetryWriteResult result =
        appendPhase4CaptureTelemetryRowDurable(path, input);
    expect(!result.ok && result.errorCode ==
            "phase4_telemetry_boundary_inventory_incomplete" &&
        readFile(path).empty(),
        "owned candidates require a complete boundary set before any claim");
    unlink(path.c_str());
}

void testRequiredMachinePredicateLedgerMissingFailsClosed()
{
    const std::string path = temporaryPath();
    Phase4CaptureTelemetryInput input = completeInput(true);
    input.machinePredicateLedger.clear();
    const Phase4TelemetryWriteResult result =
        appendPhase4CaptureTelemetryRowDurable(path, input);
    expect(!result.ok && result.errorCode ==
            "phase4_telemetry_machine_predicate_ledger_missing" &&
        readFile(path).empty(),
        "capture runner cannot claim without a complete predicate ledger");
    unlink(path.c_str());
}

void testDuplicateMachinePredicateNameFailsClosed()
{
    const std::string path = temporaryPath();
    Phase4CaptureTelemetryInput input = completeInput(true);
    input.machinePredicateLedger[1].name = input.machinePredicateLedger[0].name;
    const Phase4TelemetryWriteResult result =
        appendPhase4CaptureTelemetryRowDurable(path, input);
    expect(!result.ok && result.errorCode ==
            "phase4_telemetry_machine_predicate_ledger_invalid" &&
        readFile(path).empty(), "duplicate predicate names fail closed");
    unlink(path.c_str());
}

void testMachineDecisionMustEqualPredicateConjunction()
{
    const std::string path = temporaryPath();
    Phase4CaptureTelemetryInput input = completeInput(true);
    input.machinePredicateLedger[0].result = false;
    const Phase4TelemetryWriteResult result =
        appendPhase4CaptureTelemetryRowDurable(path, input);
    expect(!result.ok && result.errorCode ==
            "phase4_telemetry_machine_predicate_decision_mismatch" &&
        readFile(path).empty(),
        "machine decision cannot diverge from the serialized predicate conjunction");
    unlink(path.c_str());
}

}  // namespace

int main()
{
    try {
        testFailedDecisionIsDurableBeforeThrow();
        testPassDecisionWritesSameSchemaExactlyOnce();
        testIncompleteRowFailsClosedWithoutClaim();
        testForcedFlushFailureIsDistinctAndCannotPass();
        testMissingBoundarySetFailsClosed();
        testRequiredMachinePredicateLedgerMissingFailsClosed();
        testDuplicateMachinePredicateNameFailsClosed();
        testMachineDecisionMustEqualPredicateConjunction();
        std::cout << "phase4 final ownership telemetry tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "phase4 final ownership telemetry tests failed: "
                  << error.what() << '\n';
        return 1;
    }
}
