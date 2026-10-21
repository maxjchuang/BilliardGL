#include "training/phase4_final_ownership_instrumentation.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <set>
#include <sys/stat.h>
#include <unistd.h>

namespace billiardgl::training {
namespace {

json::Value rgb(const std::array<std::uint8_t, 3>& value)
{
    json::Value result = json::Value::array();
    for (std::uint8_t channel : value)
        result.asArray().push_back(json::Value(static_cast<int>(channel)));
    return result;
}

json::Value counts(const CompoundTrackVisibilityCounts& value)
{
    json::Value result = json::Value::object();
    result["source"] = json::Value(static_cast<int>(value.source));
    result["visible"] = json::Value(static_cast<int>(value.visible));
    result["hidden"] = json::Value(static_cast<int>(value.hidden));
    result["coverage"] = json::Value(value.coverage);
    return result;
}

json::Value stringArray(const std::vector<std::string>& values)
{
    json::Value result = json::Value::array();
    for (const std::string& value : values)
        result.asArray().push_back(json::Value(value));
    return result;
}

json::Value countMap(const std::map<std::string, std::size_t>& values)
{
    json::Value result = json::Value::object();
    for (const auto& value : values)
        result[value.first] = json::Value(static_cast<int>(value.second));
    return result;
}

void validateInput(const Phase4CaptureTelemetryInput& input)
{
    if (input.captureId.empty() || input.planOrdinalOneBased == 0 ||
        input.ppmPath.empty() || input.ppmSha256.size() != 64)
        throw std::runtime_error("phase4_telemetry_identity_missing");
    if (input.audit.sourceProbeCount == 0 ||
        input.audit.orderedSourceProbeIds.size() != input.audit.sourceProbeCount ||
        input.audit.orderedVisibleProbeIds.size() !=
            input.audit.visibleProbeCount ||
        input.audit.visibleProbeCount + input.audit.hiddenProbeCount !=
            input.audit.sourceProbeCount ||
        input.audit.orderedSourceProbeDigest.size() != 64 ||
        input.audit.orderedVisibleProbeDigest.size() != 64)
        throw std::runtime_error("phase4_telemetry_audit_incomplete");
    if (input.audit.hiddenProbes.size() != input.audit.hiddenProbeCount)
        throw std::runtime_error("phase4_telemetry_hidden_records_incomplete");
    if (input.ownershipCandidates.empty() || input.arbitrationRecords.empty())
        throw std::runtime_error("phase4_telemetry_ownership_records_missing");
    if (input.sourceAndBinaryBindings.empty())
        throw std::runtime_error("phase4_telemetry_bindings_missing");
    if (!input.machinePass && input.machineErrorCode.empty())
        throw std::runtime_error("phase4_telemetry_machine_error_missing");
    if (input.machinePredicateLedgerRequired) {
        if (input.machinePredicateLedger.empty())
            throw std::runtime_error(
                "phase4_telemetry_machine_predicate_ledger_missing");
        std::set<std::string> predicateNames;
        bool allPredicatesPass = true;
        for (const Phase4MachinePredicateTelemetry& predicate :
                input.machinePredicateLedger) {
            if (predicate.name.empty() || predicate.measured.empty() ||
                predicate.expected.empty() ||
                !predicateNames.insert(predicate.name).second)
                throw std::runtime_error(
                    "phase4_telemetry_machine_predicate_ledger_invalid");
            allPredicatesPass = allPredicatesPass && predicate.result;
        }
        if (input.machinePass != allPredicatesPass)
            throw std::runtime_error(
                "phase4_telemetry_machine_predicate_decision_mismatch");
    }
    if (!input.legacyPpmDiagnosticOnly ||
        input.legacyPpmVisibleProbeCount + input.legacyPpmHiddenProbeCount !=
            input.audit.sourceProbeCount ||
        input.legacyPpmVisibleProbeCoverage < 0.0 ||
        input.legacyPpmVisibleProbeCoverage > 1.0)
        throw std::runtime_error(
            "phase4_telemetry_legacy_diagnostic_boundary_invalid");
    std::set<std::string> sourceIds(input.audit.orderedSourceProbeIds.begin(),
        input.audit.orderedSourceProbeIds.end());
    if (sourceIds.size() != input.audit.sourceProbeCount)
        throw std::runtime_error("phase4_telemetry_source_ids_invalid");
    for (const CompoundHiddenProbeRecord& hidden : input.audit.hiddenProbes)
        if (sourceIds.count(hidden.sourceProbeId) == 0)
            throw std::runtime_error("phase4_telemetry_hidden_id_unknown");
    std::set<std::string> candidateSourceIds;
    for (const FinalOwnershipCandidateTelemetry& candidate :
            input.ownershipCandidates) {
        if (sourceIds.count(candidate.sourceProbeId) == 0 ||
            candidate.eligibilityOrRejectionReason.empty())
            throw std::runtime_error(
                "phase4_telemetry_candidate_record_incomplete");
        candidateSourceIds.insert(candidate.sourceProbeId);
    }
    if (candidateSourceIds != sourceIds)
        throw std::runtime_error(
            "phase4_telemetry_candidate_inventory_incomplete");
    std::map<std::string, std::size_t> ownedCandidateCounts;
    for (const FinalOwnershipCandidateTelemetry& candidate :
            input.ownershipCandidates)
        if (candidate.resolvedSampleCount > 0)
            ownedCandidateCounts[candidate.sourceProbeId] = 0;
    std::map<std::string, std::size_t> selectedBoundaryCounts;
    for (const FinalOwnershipBoundaryTelemetry& boundary :
            input.componentAwareBoundaryRecords) {
        if (sourceIds.count(boundary.sourceProbeId) == 0 ||
            boundary.boundaryClass.empty() ||
            boundary.deterministicSortKey.empty() ||
            boundary.associationMode.empty() ||
            boundary.associationSeedPrimitiveStem.empty() ||
            boundary.associatedOwnedIslandId.empty() ||
            boundary.boundaryAreaOwnerSourceProbeId.empty() ||
            boundary.sampleIndex < 0)
            throw std::runtime_error(
                "phase4_telemetry_boundary_record_incomplete");
        if (ownedCandidateCounts.count(boundary.sourceProbeId) == 0)
            throw std::runtime_error(
                "phase4_telemetry_boundary_without_owned_candidate");
        ++ownedCandidateCounts[boundary.sourceProbeId];
        if (boundary.selected) {
            if (!boundary.eligibleForRole)
                throw std::runtime_error(
                    "phase4_telemetry_selected_boundary_ineligible");
            ++selectedBoundaryCounts[boundary.sourceProbeId];
        }
    }
    for (const auto& item : ownedCandidateCounts) {
        if (item.second == 0)
            throw std::runtime_error(
                "phase4_telemetry_boundary_inventory_incomplete");
        if (selectedBoundaryCounts[item.first] > 1)
            throw std::runtime_error(
                "phase4_telemetry_multiple_selected_boundaries");
    }
    std::set<std::string> arbitratedSourceIds;
    for (const FinalOwnershipArbitrationTelemetry& arbitration :
            input.arbitrationRecords) {
        if (arbitration.physicalSampleOrPixelId.empty() ||
            arbitration.claimingSourceProbeIds.empty() ||
            arbitration.deterministicSortKey.empty() ||
            (arbitration.winnerSourceProbeId.empty() &&
                arbitration.loserReason.empty()))
            throw std::runtime_error(
                "phase4_telemetry_arbitration_record_incomplete");
        for (const std::string& id : arbitration.claimingSourceProbeIds) {
            if (sourceIds.count(id) == 0)
                throw std::runtime_error(
                    "phase4_telemetry_arbitration_id_unknown");
            arbitratedSourceIds.insert(id);
        }
    }
    if (arbitratedSourceIds != sourceIds)
        throw std::runtime_error(
            "phase4_telemetry_arbitration_inventory_incomplete");
}

}  // namespace

json::Value buildPhase4CaptureTelemetryRow(
    const Phase4CaptureTelemetryInput& input)
{
    validateInput(input);
    const CompoundVisibilityAudit& audit = input.audit;
    json::Value row = json::Value::object();
    row["schema"] = json::Value(
        "billiardgl.phase4.final_ownership_capture_telemetry.v1");
    row["instrumentation_schema_version"] = json::Value(1);
    row["capture_id"] = json::Value(input.captureId);
    row["plan_ordinal_one_based"] = json::Value(
        static_cast<int>(input.planOrdinalOneBased));
    row["machine_status"] = json::Value(input.machinePass ? "PASS" : "FAILED");
    row["machine_error_code"] = json::Value(input.machineErrorCode);
    row["machine_predicate_ledger_required"] =
        json::Value(input.machinePredicateLedgerRequired);
    row["machine_predicate_ledger"] = json::Value::array();
    row["failed_machine_predicate_names"] = json::Value::array();
    for (const Phase4MachinePredicateTelemetry& predicate :
            input.machinePredicateLedger) {
        json::Value value = json::Value::object();
        value["name"] = json::Value(predicate.name);
        value["result"] = json::Value(predicate.result);
        value["measured"] = json::Value(predicate.measured);
        value["expected"] = json::Value(predicate.expected);
        row["machine_predicate_ledger"].asArray().push_back(value);
        if (!predicate.result)
            row["failed_machine_predicate_names"].asArray().push_back(
                json::Value(predicate.name));
    }
    row["ppm_path"] = json::Value(input.ppmPath);
    row["ppm_sha256"] = json::Value(input.ppmSha256);
    row["source_probe_count"] = json::Value(static_cast<int>(audit.sourceProbeCount));
    row["visible_probe_count"] = json::Value(static_cast<int>(audit.visibleProbeCount));
    row["hidden_probe_count"] = json::Value(static_cast<int>(audit.hiddenProbeCount));
    row["aggregate_visible_probe_coverage"] = json::Value(audit.visibleProbeCoverage);
    row["predicted_source_visible_hidden_and_coverage"] = counts(audit.predicted);
    row["actual_source_visible_hidden_and_coverage"] = counts(audit.actual);
    row["component_source_counts"] = countMap(audit.componentSourceCounts);
    row["component_visible_counts"] = countMap(audit.componentVisibleCounts);
    row["component_hidden_reason_counts"] =
        countMap(audit.componentHiddenReasonCounts);
    row["ordered_source_probe_ids"] = stringArray(audit.orderedSourceProbeIds);
    row["ordered_source_probe_digest"] = json::Value(audit.orderedSourceProbeDigest);
    row["ordered_visible_probe_ids"] = stringArray(audit.orderedVisibleProbeIds);
    row["ordered_visible_probe_digest"] = json::Value(audit.orderedVisibleProbeDigest);
    row["hidden_probe_id_track_role_reason_coordinates"] = json::Value::array();
    for (const CompoundHiddenProbeRecord& hidden : audit.hiddenProbes) {
        json::Value value = json::Value::object();
        value["source_probe_id"] = json::Value(hidden.sourceProbeId);
        value["track"] = json::Value(compoundTrackName(hidden.track));
        value["component_role"] = json::Value(
            compoundComponentRoleName(hidden.componentRole));
        value["reason"] = json::Value(compoundHiddenReasonName(hidden.reason));
        value["x_physical_px"] = json::Value(hidden.xPhysicalPx);
        value["y_physical_px"] = json::Value(hidden.yPhysicalPx);
        row["hidden_probe_id_track_role_reason_coordinates"].asArray().push_back(value);
    }
    row["ownership_candidate_records"] = json::Value::array();
    for (const FinalOwnershipCandidateTelemetry& candidate : input.ownershipCandidates) {
        json::Value value = json::Value::object();
        value["source_probe_id"] = json::Value(candidate.sourceProbeId);
        value["physical_pixel"] = json::Value::array();
        value["physical_pixel"].asArray().push_back(json::Value(candidate.xPhysicalPx));
        value["physical_pixel"].asArray().push_back(json::Value(candidate.yPhysicalPx));
        value["resolved_sample_count"] = json::Value(
            static_cast<int>(candidate.resolvedSampleCount));
        value["resolved_pixel_equivalent_area"] =
            json::Value(candidate.resolvedPixelEquivalentArea);
        value["exclusive_owned_pixel_count"] = json::Value(
            static_cast<int>(candidate.exclusiveOwnedPixelCount));
        value["exclusive_largest_four_connected_pixel_count"] = json::Value(
            static_cast<int>(
                candidate.exclusiveLargestFourConnectedPixelCount));
        value["pre_arbitration_support_pixel_count"] = json::Value(
            static_cast<int>(candidate.preArbitrationSupportPixelCount));
        value["pre_arbitration_largest_eight_connected_pixel_count"] =
            json::Value(static_cast<int>(
                candidate.preArbitrationLargestEightConnectedPixelCount));
        value["owner_track"] = json::Value(candidate.ownerTrack);
        value["owner_component"] = json::Value(candidate.ownerComponent);
        value["owner_primitive_id"] = json::Value(candidate.ownerPrimitiveId);
        value["owner_visual_sublayer"] =
            json::Value(candidate.ownerVisualSublayer);
        value["probe_local_tangent_distance_px"] =
            json::Value(candidate.tangentDistancePx);
        value["probe_local_normal_distance_px"] =
            json::Value(candidate.normalDistancePx);
        value["final_rgb_srgb8"] = rgb(candidate.finalSrgb8);
        value["adjacent_non_self_rgb_srgb8"] = rgb(candidate.adjacentNonSelfSrgb8);
        value["edge_contrast_ratio"] = json::Value(candidate.edgeContrastRatio);
        value["eligibility_or_rejection_reason"] =
            json::Value(candidate.eligibilityOrRejectionReason);
        row["ownership_candidate_records"].asArray().push_back(value);
    }
    row["component_aware_boundary_records"] = json::Value::array();
    for (const FinalOwnershipBoundaryTelemetry& boundary :
            input.componentAwareBoundaryRecords) {
        json::Value value = json::Value::object();
        value["source_probe_id"] = json::Value(boundary.sourceProbeId);
        value["owner_physical_pixel"] = json::Value::array();
        value["owner_physical_pixel"].asArray().push_back(
            json::Value(boundary.ownerXPhysicalPx));
        value["owner_physical_pixel"].asArray().push_back(
            json::Value(boundary.ownerYPhysicalPx));
        value["adjacent_physical_pixel"] = json::Value::array();
        value["adjacent_physical_pixel"].asArray().push_back(
            json::Value(boundary.adjacentXPhysicalPx));
        value["adjacent_physical_pixel"].asArray().push_back(
            json::Value(boundary.adjacentYPhysicalPx));
        value["sample_index"] = json::Value(boundary.sampleIndex);
        value["owner_track"] = json::Value(boundary.ownerTrack);
        value["owner_component"] = json::Value(boundary.ownerComponent);
        value["owner_primitive_id"] = json::Value(boundary.ownerPrimitiveId);
        value["owner_visual_sublayer"] =
            json::Value(boundary.ownerVisualSublayer);
        value["adjacent_track"] = json::Value(boundary.adjacentTrack);
        value["adjacent_component"] = json::Value(boundary.adjacentComponent);
        value["adjacent_primitive_id"] =
            json::Value(boundary.adjacentPrimitiveId);
        value["adjacent_visual_sublayer"] =
            json::Value(boundary.adjacentVisualSublayer);
        value["adjacent_later_ui_owned"] =
            json::Value(boundary.adjacentLaterUiOwned);
        value["owner_final_rgb_srgb8"] = rgb(boundary.ownerFinalSrgb8);
        value["adjacent_final_rgb_srgb8"] =
            rgb(boundary.adjacentFinalSrgb8);
        value["edge_contrast_ratio"] =
            json::Value(boundary.edgeContrastRatio);
        value["boundary_class"] = json::Value(boundary.boundaryClass);
        value["deterministic_sort_key"] =
            json::Value(boundary.deterministicSortKey);
        value["association_mode"] = json::Value(boundary.associationMode);
        value["association_seed_primitive_stem"] =
            json::Value(boundary.associationSeedPrimitiveStem);
        value["associated_owned_island_id"] =
            json::Value(boundary.associatedOwnedIslandId);
        value["boundary_area_owner_source_probe_id"] =
            json::Value(boundary.boundaryAreaOwnerSourceProbeId);
        value["eligible_for_role"] = json::Value(boundary.eligibleForRole);
        value["selected"] = json::Value(boundary.selected);
        row["component_aware_boundary_records"].asArray().push_back(value);
    }
    row["exclusive_arbitration_records"] = json::Value::array();
    for (const FinalOwnershipArbitrationTelemetry& arbitration :
            input.arbitrationRecords) {
        json::Value value = json::Value::object();
        value["physical_sample_or_pixel_id"] =
            json::Value(arbitration.physicalSampleOrPixelId);
        value["claiming_source_probe_ids"] =
            stringArray(arbitration.claimingSourceProbeIds);
        value["winner_source_probe_id"] =
            json::Value(arbitration.winnerSourceProbeId);
        value["sort_key_same_track_distance_primitive_probe_ordinal"] =
            json::Value(arbitration.deterministicSortKey);
        value["loser_reason"] = json::Value(arbitration.loserReason);
        row["exclusive_arbitration_records"].asArray().push_back(value);
    }
    row["double_assignment_count"] =
        json::Value(static_cast<int>(audit.doubleAssignmentCount));
    row["foreign_track_evidence_count"] =
        json::Value(static_cast<int>(audit.foreignTrackEvidenceCount));
    row["unowned_evidence_count"] =
        json::Value(static_cast<int>(audit.unownedEvidenceCount));
    row["integrated_final_ownership"] = json::Value(input.integratedFinalOwnership);
    row["authoritative_visibility_gate"] =
        json::Value("corrected_integrated_final_semantic_ownership");
    row["legacy_ppm_diagnostic_only"] =
        json::Value(input.legacyPpmDiagnosticOnly);
    row["legacy_ppm_contrast_ok"] = json::Value(input.legacyPpmContrastOk);
    row["legacy_ppm_visible_probe_count"] = json::Value(
        static_cast<int>(input.legacyPpmVisibleProbeCount));
    row["legacy_ppm_hidden_probe_count"] = json::Value(
        static_cast<int>(input.legacyPpmHiddenProbeCount));
    row["legacy_ppm_visible_probe_coverage"] =
        json::Value(input.legacyPpmVisibleProbeCoverage);
    row["legacy_integrated_decision_discrepancy"] =
        json::Value(input.legacyIntegratedDecisionDiscrepancy);
    row["source_and_binary_bindings"] = json::Value::object();
    for (const auto& binding : input.sourceAndBinaryBindings)
        row["source_and_binary_bindings"][binding.first] = json::Value(binding.second);
    return row;
}

Phase4TelemetryWriteResult appendPhase4CaptureTelemetryRowDurable(
    const std::string& path, const Phase4CaptureTelemetryInput& input)
{
    Phase4TelemetryWriteResult result;
    try {
        result.canonicalRow = json::stringify(buildPhase4CaptureTelemetryRow(input));
        result.canonicalRow.push_back('\n');
        if (input.forceDurableFlushFailureForTest)
            throw std::runtime_error("phase4_telemetry_durable_flush_failed");
        const int descriptor = open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND,
            S_IRUSR | S_IWUSR);
        if (descriptor < 0)
            throw std::runtime_error("phase4_telemetry_append_open_failed");
        std::size_t written = 0;
        while (written < result.canonicalRow.size()) {
            const ssize_t count = write(descriptor,
                result.canonicalRow.data() + written,
                result.canonicalRow.size() - written);
            if (count <= 0) {
                close(descriptor);
                throw std::runtime_error("phase4_telemetry_append_write_failed");
            }
            written += static_cast<std::size_t>(count);
        }
        if (fsync(descriptor) != 0) {
            close(descriptor);
            throw std::runtime_error("phase4_telemetry_durable_flush_failed");
        }
        if (close(descriptor) != 0)
            throw std::runtime_error("phase4_telemetry_append_close_failed");
        result.ok = true;
    } catch (const std::exception& error) {
        result.errorCode = error.what();
    }
    return result;
}

void preservePhase4TelemetryThenEnforceMachineGate(
    const std::string& path, const Phase4CaptureTelemetryInput& input)
{
    const Phase4TelemetryWriteResult write =
        appendPhase4CaptureTelemetryRowDurable(path, input);
    if (!write.ok)
        throw std::runtime_error(write.errorCode.empty() ?
            "phase4_telemetry_preservation_failed" : write.errorCode);
    if (!input.machinePass)
        throw std::runtime_error("phase4_overlay_machine_gate_failed:" +
            input.captureId);
}

}  // namespace billiardgl::training
