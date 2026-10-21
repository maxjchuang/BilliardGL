#!/usr/bin/env python3
"""Deterministically derive the v4 capture runner from the frozen v3 shell.

The v3 source is read-only input.  The generated translation unit receives a
new authorization boundary, semantic ownership attachment, compound audit and
v4 telemetry.  This script never executes the generated runner.
"""

from pathlib import Path
import argparse
import hashlib

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "spikes/render_core_profile/phase4_overlay_gpu_capture_runner.mm"
SUPPORT = ROOT / "spikes/render_core_profile/phase4_overlay_v4_gpu_capture_support.inc"

FINAL_OWNERSHIP_SOURCES = [
    "src/Billiards/render/modern/modern_renderer.cpp",
    "src/Billiards/render/modern/modern_renderer.h",
    "src/Billiards/render/ui/product_ui_gpu_renderer.cpp",
    "src/Billiards/training/trajectory_overlay_renderer.cpp",
    "src/Billiards/training/trajectory_overlay_renderer.h",
    "src/Billiards/training/final_semantic_ownership.cpp",
    "src/Billiards/training/final_semantic_ownership.h",
    "src/Billiards/training/phase4_final_ownership_instrumentation.cpp",
    "src/Billiards/training/phase4_final_ownership_instrumentation.h",
    "src/Billiards/training/trajectory_compound_visibility.cpp",
    "src/Billiards/training/trajectory_compound_visibility.h",
]


def exactly_once(text: str, old: str, new: str) -> str:
    if text.count(old) != 1:
        raise RuntimeError(f"expected exactly one generator anchor: {old[:80]!r}")
    return text.replace(old, new)


def generate() -> str:
    text = BASE.read_text(encoding="utf-8")
    text = exactly_once(text, '#include "deterministic_hash.h"\n',
        '#include "deterministic_hash.h"\n'
        '#include "training/trajectory_compound_visibility.h"\n'
        '#include <fcntl.h>\n#include <sys/stat.h>\n#include <unistd.h>\n')
    text = exactly_once(text,
        "training::TrajectoryContrastAudit contrastAudit(\n",
        "training::TrajectoryContrastAudit contrastAuditV3(\n")
    marker = "\nbool longReplayFailsClosed(const CameraPresentation& camera)\n"
    text = exactly_once(text, marker,
        "\n" + SUPPORT.read_text(encoding="utf-8") + marker)
    text = exactly_once(text,
        "contrastAudit(readPpm(ppm), cpuPackage, overlayPalette,\n"
        "                        predictedRequired, actualRequired);",
        "contrastAudit(readPpm(ppm), cpuPackage, overlayPalette,\n"
        "                        predictedRequired, actualRequired, capture.id);")
    text = exactly_once(text,
        "const bool railsTextContract = actualRequired &&\n"
        "                    textRunCount(frame, actualRails) ==",
        "const bool railsTextContract = !actualRequired ||\n"
        "                    textRunCount(frame, actualRails) ==")
    text = exactly_once(text,
        "require(captures.size() == 7 || captures.size() == 68,\n"
        "                \"phase4_overlay_plan_requires_exact_7_or_68\");",
        "require(captures.size() == 11 || captures.size() == 68,\n"
        "                \"phase4_v4_plan_requires_exact_11_or_68\");")
    text = exactly_once(text,
        "const char* machineStatus = formal ?\n"
        "                \"GPU_MACHINE_PASS_PENDING_TA\" : \"PASS_PREFLIGHT_ONLY\";",
        "const char* machineStatus = formal ?\n"
        "                \"V4_GPU_MACHINE_PASS_PENDING_TA\" :\n"
        "                \"V4_PASS_PREFLIGHT_ONLY_PENDING_TA\";")
    text = exactly_once(text,
        "            [NSApplication sharedApplication];",
        "            validateAndConsumeV4Authorization(argv[0], argv[2]);\n"
        "            [NSApplication sharedApplication];")
    text = exactly_once(text,
        "            std::filesystem::create_directories(argv[2]);\n\n"
        "            validateAndConsumeV4Authorization(argv[0], argv[2]);",
        "            validateAndConsumeV4Authorization(argv[0], argv[2]);\n"
        "            std::filesystem::create_directories(argv[2]);")
    telemetry_anchor = (
        "                row[\"contrast_failure_coordinates\"] = json::Value::array();\n"
        "                for (const auto& coordinate : contrast.failureCoordinates) {\n"
        "                    json::Value point = json::Value::array();\n"
        "                    point.asArray().push_back(json::Value(coordinate[0]));\n"
        "                    point.asArray().push_back(json::Value(coordinate[1]));\n"
        "                    row[\"contrast_failure_coordinates\"].asArray().push_back(point);\n"
        "                }\n")
    telemetry = telemetry_anchor + r'''                row["compound_source_probe_count"] = json::Value(
                    static_cast<int>(latestCompoundAudit.sourceProbeCount));
                row["compound_visible_probe_count"] = json::Value(
                    static_cast<int>(latestCompoundAudit.visibleProbeCount));
                row["compound_hidden_probe_count"] = json::Value(
                    static_cast<int>(latestCompoundAudit.hiddenProbeCount));
                row["compound_visible_probe_coverage"] = json::Value(
                    latestCompoundAudit.visibleProbeCoverage);
                row["predicted_compound_source_probe_count"] = json::Value(
                    static_cast<int>(latestCompoundAudit.predicted.source));
                row["predicted_compound_visible_probe_count"] = json::Value(
                    static_cast<int>(latestCompoundAudit.predicted.visible));
                row["predicted_compound_hidden_probe_count"] = json::Value(
                    static_cast<int>(latestCompoundAudit.predicted.hidden));
                row["predicted_compound_visible_probe_coverage"] = json::Value(
                    latestCompoundAudit.predicted.coverage);
                row["actual_compound_source_probe_count"] = json::Value(
                    static_cast<int>(latestCompoundAudit.actual.source));
                row["actual_compound_visible_probe_count"] = json::Value(
                    static_cast<int>(latestCompoundAudit.actual.visible));
                row["actual_compound_hidden_probe_count"] = json::Value(
                    static_cast<int>(latestCompoundAudit.actual.hidden));
                row["actual_compound_visible_probe_coverage"] = json::Value(
                    latestCompoundAudit.actual.coverage);
                row["compound_source_probe_id_digest"] = json::Value(
                    latestCompoundAudit.orderedSourceProbeDigest);
                row["compound_visible_probe_id_digest"] = json::Value(
                    latestCompoundAudit.orderedVisibleProbeDigest);
                row["compound_double_assignment_count"] = json::Value(
                    static_cast<int>(latestCompoundAudit.doubleAssignmentCount));
                row["compound_foreign_track_evidence_count"] = json::Value(
                    static_cast<int>(latestCompoundAudit.foreignTrackEvidenceCount));
                row["compound_unowned_evidence_count"] = json::Value(
                    static_cast<int>(latestCompoundAudit.unownedEvidenceCount));
                row["compound_source_probe_ids"] = json::Value::array();
                for (const auto& id : latestCompoundAudit.orderedSourceProbeIds)
                    row["compound_source_probe_ids"].asArray().push_back(
                        json::Value(id));
                row["compound_visible_probe_ids"] = json::Value::array();
                for (const auto& id : latestCompoundAudit.orderedVisibleProbeIds)
                    row["compound_visible_probe_ids"].asArray().push_back(
                        json::Value(id));
                row["compound_component_source_counts"] = json::Value::object();
                for (const auto& value : latestCompoundAudit.componentSourceCounts)
                    row["compound_component_source_counts"][value.first] =
                        json::Value(static_cast<int>(value.second));
                row["compound_component_visible_counts"] = json::Value::object();
                for (const auto& value : latestCompoundAudit.componentVisibleCounts)
                    row["compound_component_visible_counts"][value.first] =
                        json::Value(static_cast<int>(value.second));
                row["compound_component_hidden_reason_counts"] =
                    json::Value::object();
                for (const auto& value :
                        latestCompoundAudit.componentHiddenReasonCounts)
                    row["compound_component_hidden_reason_counts"][value.first] =
                        json::Value(static_cast<int>(value.second));
                row["compound_hidden_probes"] = json::Value::array();
                for (const auto& hidden : latestCompoundAudit.hiddenProbes) {
                    json::Value item = json::Value::object();
                    item["source_probe_id"] = json::Value(hidden.sourceProbeId);
                    item["track"] = json::Value(
                        training::compoundTrackName(hidden.track));
                    item["component_role"] = json::Value(
                        training::compoundComponentRoleName(hidden.componentRole));
                    item["reason"] = json::Value(
                        training::compoundHiddenReasonName(hidden.reason));
                    item["x_physical_px"] = json::Value(hidden.xPhysicalPx);
                    item["y_physical_px"] = json::Value(hidden.yPhysicalPx);
                    row["compound_hidden_probes"].asArray().push_back(item);
                }
                row["authorization_receipt_consumed"] = json::Value(true);
                row["gpu_capture_authorized_for_this_invocation"] =
                    json::Value(true);
'''
    text = exactly_once(text, telemetry_anchor, telemetry)
    text = text.replace("phase4-overlay-gpu-capture:",
                        "phase4-overlay-v4-gpu-capture:")
    text = text.replace("phase4-overlay-gpu-preflight:",
                        "phase4-overlay-v4-gpu-preflight:")
    return text


def source_aggregate_sha256() -> str:
    digest = hashlib.sha256()
    for relative in FINAL_OWNERSHIP_SOURCES:
        data = (ROOT / relative).read_bytes()
        digest.update(relative.encode())
        digest.update(b"\0")
        digest.update(hashlib.sha256(data).hexdigest().encode())
        digest.update(b"\n")
    return digest.hexdigest()


def add_final_ownership_instrumentation(text: str) -> str:
    aggregate = source_aggregate_sha256()
    text = exactly_once(text,
        '#include "training/trajectory_compound_visibility.h"\n',
        '#include "training/trajectory_compound_visibility.h"\n'
        '#include "training/phase4_final_ownership_instrumentation.h"\n')
    text = exactly_once(text,
        "    options.msaaSamples = 0; options.quality = RenderQuality::Medium;",
        "    options.msaaSamples = 0; options.quality = RenderQuality::Medium;\n"
        "    options.enableFinalOwnershipInstrumentation = true;")

    validator_start = text.index("void validateAndConsumeV4Authorization(")
    validator_end = text.index("\nbool longReplayFailsClosed(", validator_start)
    validator = f'''void validateAndConsumeV4Authorization(const char* runnerPath,
    const char* outputRoot)
{{
    const char* receiptEnvironment = std::getenv(
        "BILLIARDGL_PHASE4_FINAL_OWNERSHIP_GPU_AUTH_RECEIPT");
    require(receiptEnvironment != nullptr && receiptEnvironment[0] != '\\0',
        "phase4_final_ownership_gpu_authorization_receipt_required");
    const std::filesystem::path receiptPath(receiptEnvironment);
    const json::ParseResult parsed = json::parse(v4ReadFile(receiptPath));
    require(parsed.ok && parsed.value.isObject(),
        "phase4_final_ownership_gpu_authorization_receipt_invalid");
    const json::Value& receipt = parsed.value;
    require(receipt.at("schema").asString() ==
            "billiardgl.phase4.final_ownership_gpu_authorization.v1" &&
        receipt.at("authorized").asBool() &&
        receipt.at("single_use").asBool() &&
        receipt.at("targeted_case_count").asInt() == 11 &&
        receipt.at("design_gate_sha256").asString() ==
            "c3b6f032deaac6bd39d32755d4ab423915e8b70b08b6da18626707b8d7134056" &&
        receipt.at("implementation_source_aggregate_sha256").asString() ==
            "{aggregate}" &&
        receipt.at("runner_sha256").asString() ==
            sha256Hex(v4ReadFile(std::filesystem::path(runnerPath))) &&
        receipt.at("billiards_executable_sha256").asString().size() == 64 &&
        receipt.at("cpu_fixture_receipt_sha256").asString().size() == 64 &&
        receipt.at("frozen_replay_receipt_sha256").asString().size() == 64 &&
        receipt.at("ready_manifest_sha256").asString().size() == 64,
        "phase4_final_ownership_gpu_authorization_binding_failed");
    require(std::filesystem::path(receipt.at("output_root").asString()) ==
            std::filesystem::absolute(outputRoot).lexically_normal(),
        "phase4_final_ownership_gpu_authorization_output_root_mismatch");
    const std::filesystem::path consumed = receiptPath.string() + ".consumed";
    const int descriptor = open(consumed.c_str(), O_WRONLY | O_CREAT | O_EXCL,
        S_IRUSR | S_IWUSR);
    require(descriptor >= 0,
        "phase4_final_ownership_gpu_authorization_already_consumed");
    const std::string marker = receipt.at("authorization_nonce").asString()+"\\n";
    require(marker.size() > 1 && write(descriptor,marker.data(),marker.size()) ==
        static_cast<ssize_t>(marker.size()),
        "phase4_final_ownership_gpu_authorization_consume_write_failed");
    require(fsync(descriptor) == 0 && close(descriptor) == 0,
        "phase4_final_ownership_gpu_authorization_consume_flush_failed");
}}
'''
    text = text[:validator_start] + validator + text[validator_end:]

    helper_anchor = "\nbool longReplayFailsClosed(const CameraPresentation& camera)\n"
    helper = r'''
training::IntegratedFinalOwnershipRequest finalOwnershipRequest(
    training::IntegratedFinalOwnershipFrame frame,
    const training::TrajectoryOverlayDrawPackage& package,
    const std::string& captureId)
{
    training::IntegratedFinalOwnershipRequest request;
    request.frame = std::move(frame);
    request.expectedSourceProbeCount = package.contrastProbes.size();
    const std::string profile = std::to_string(request.frame.physicalWidth) +
        "x" + std::to_string(request.frame.physicalHeight) + "|line=" +
        std::to_string(package.style.lineWidthPx) + "|halo=" +
        std::to_string(package.style.haloWidthPx);
    for (const training::TrajectoryContrastProbe& probe :
            package.contrastProbes) {
        training::FinalOwnershipSourceProbe source;
        source.source.captureId = captureId;
        source.source.track = probe.track ==
            training::TrajectoryContrastTrack::Predicted ?
                training::CompoundVisibilityTrack::Predicted :
                training::CompoundVisibilityTrack::Actual;
        source.source.primitiveId = probe.primitiveId;
        source.source.componentRole = probe.componentRole;
        source.source.arcSegmentId = probe.arcSegmentId;
        source.source.probeOrdinal = probe.probeOrdinal;
        source.source.physicalProfile = profile;
        source.source.xPhysicalPx = probe.componentRole ==
            training::CompoundComponentRole::OuterHaloEdge ?
                probe.haloXPhysicalPx : probe.strokeXPhysicalPx;
        source.source.yPhysicalPx = probe.componentRole ==
            training::CompoundComponentRole::OuterHaloEdge ?
                probe.haloYPhysicalPx : probe.strokeYPhysicalPx;
        source.source.outerHaloRadiusPx = package.style.lineWidthPx * 0.5 +
            package.style.haloWidthPx;
        source.source.halfTickLengthPx =
            package.style.actualTickLengthPx * 0.5;
        source.source.sourceProbeSpacingPx = source.source.track ==
            training::CompoundVisibilityTrack::Predicted ?
                package.style.dashOnPx + package.style.dashOffPx :
                package.style.actualTickSpacingPx;
        source.source.insideFinalViewport = source.source.xPhysicalPx >= 0 &&
            source.source.yPhysicalPx >= 0 &&
            source.source.xPhysicalPx < request.frame.physicalWidth &&
            source.source.yPhysicalPx < request.frame.physicalHeight;
        source.tangentX = probe.localTangentX;
        source.tangentY = probe.localTangentY;
        source.normalX = probe.localNormalX;
        source.normalY = probe.localNormalY;
        request.sourceProbes.push_back(source);
    }
    return request;
}
'''
    text = exactly_once(text, helper_anchor, helper + helper_anchor)

    capture_anchor = (
        "                const training::TrajectoryContrastAudit contrast =\n"
        "                    contrastAudit(readPpm(ppm), cpuPackage, overlayPalette,\n"
        "                        predictedRequired, actualRequired, capture.id);\n")
    capture_replacement = capture_anchor + r'''                training::IntegratedFinalOwnershipFrame ownershipFrame;
                const bool ownershipCaptured =
                    renderer.captureIntegratedFinalOwnership(
                        capture.id, ownershipFrame, error);
                training::IntegratedFinalOwnershipResult ownership;
                if (ownershipCaptured) {
                    ownership = training::resolveIntegratedFinalSemanticOwnership(
                        finalOwnershipRequest(std::move(ownershipFrame),
                            cpuPackage, capture.id));
                } else {
                    ownership.errorCode = error;
                }
'''
    text = exactly_once(text, capture_anchor, capture_replacement)
    text = exactly_once(text,
        "                const bool machinePass = cpuPackage.ok && endpointContract &&",
        "                const bool machinePass = ownershipCaptured && ownership.ok &&\n"
        "                    (index != 0 || cpuPackage.contrastProbes.size() == 95) &&\n"
        "                    cpuPackage.ok && endpointContract &&")
    text = exactly_once(text,
        "                    contrast.ok && contrast.minimumLocalContrastRatio >= 4.0 &&\n"
        "                    contrast.designTargetCoverage >= 0.99 &&\n",
        "")

    machine_start = (
        "                const bool machinePass = ownershipCaptured && ownership.ok &&\n")
    machine_end = "                    fixture.authorityUnchanged();\n"
    start = text.find(machine_start)
    end = text.find(machine_end, start)
    if start < 0 or end < 0 or text.find(machine_start, start + 1) >= 0:
        raise RuntimeError("machine predicate block anchor drift")
    end += len(machine_end)
    machine_ledger = r'''                std::vector<training::Phase4MachinePredicateTelemetry>
                    machinePredicateLedger;
                const auto boolText = [](bool value) {
                    return value ? std::string("true") : std::string("false");
                };
                const auto addMachinePredicate = [&](const std::string& name,
                        bool result, const std::string& measured,
                        const std::string& expected) {
                    machinePredicateLedger.push_back(
                        {name, result, measured, expected});
                };
                addMachinePredicate("ownership_captured", ownershipCaptured,
                    boolText(ownershipCaptured), "true");
                addMachinePredicate("ownership_ok", ownership.ok,
                    boolText(ownership.ok), "true");
                addMachinePredicate("ordinal1_source_probe_count_exact_95",
                    index != 0 || cpuPackage.contrastProbes.size() == 95,
                    std::to_string(cpuPackage.contrastProbes.size()),
                    index == 0 ? "95" : "not_applicable");
                addMachinePredicate("cpu_package_ok", cpuPackage.ok,
                    boolText(cpuPackage.ok), "true");
                addMachinePredicate("endpoint_contract", endpointContract,
                    boolText(endpointContract), "true");
                addMachinePredicate("track_contract", trackContract,
                    boolText(trackContract), "true");
                addMachinePredicate("topology_contract", topologyContract,
                    boolText(topologyContract), "true");
                addMachinePredicate("physical_style_contract", physicalStyleContract,
                    boolText(physicalStyleContract), "true");
                addMachinePredicate("rails_text_contract", railsTextContract,
                    boolText(railsTextContract), "true");
                addMachinePredicate("actual_only_legend_contract",
                    actualOnlyLegendContract, boolText(actualOnlyLegendContract),
                    "true");
                addMachinePredicate("advanced_text_contract", advancedTextContract,
                    boolText(advancedTextContract), "true");
                addMachinePredicate("content_audit_ok", contentAudit.ok,
                    boolText(contentAudit.ok), "true");
                addMachinePredicate("trajectory_draw_submissions_exact_2",
                    metrics.trajectoryOverlayDrawCalls == 2,
                    std::to_string(metrics.trajectoryOverlayDrawCalls), "2");
                addMachinePredicate("trajectory_actual_draw_submitted",
                    metrics.trajectoryOverlayActualDrawSubmitted,
                    boolText(metrics.trajectoryOverlayActualDrawSubmitted), "true");
                addMachinePredicate("trajectory_actual_backdrop_frozen_midpoint",
                    metrics.trajectoryOverlayActualBackdropFrozen,
                    boolText(metrics.trajectoryOverlayActualBackdropFrozen), "true");
                addMachinePredicate("trajectory_predicted_draw_submitted",
                    metrics.trajectoryOverlayPredictedDrawSubmitted,
                    boolText(metrics.trajectoryOverlayPredictedDrawSubmitted), "true");
                addMachinePredicate("trajectory_submission_order_actual_freeze_predicted",
                    metrics.trajectoryOverlaySubmissionOrderValid,
                    boolText(metrics.trajectoryOverlaySubmissionOrderValid), "true");
                addMachinePredicate("trajectory_texture_count_zero",
                    metrics.trajectoryOverlayTextureCount == 0,
                    std::to_string(metrics.trajectoryOverlayTextureCount), "0");
                addMachinePredicate("trajectory_shared_projection",
                    metrics.trajectoryOverlayUsedSharedProjection,
                    boolText(metrics.trajectoryOverlayUsedSharedProjection), "true");
                addMachinePredicate("trajectory_homogeneous_clip",
                    metrics.trajectoryOverlayHomogeneousClip,
                    boolText(metrics.trajectoryOverlayHomogeneousClip), "true");
                addMachinePredicate("trajectory_safe_area_clip",
                    metrics.trajectoryOverlaySafeAreaClip,
                    boolText(metrics.trajectoryOverlaySafeAreaClip), "true");
                addMachinePredicate("trajectory_play_surface_clip",
                    metrics.trajectoryOverlayPlaySurfaceClip,
                    boolText(metrics.trajectoryOverlayPlaySurfaceClip), "true");
                addMachinePredicate("trajectory_error_empty",
                    metrics.trajectoryOverlayErrorCode.empty(),
                    metrics.trajectoryOverlayErrorCode.empty() ? "empty" :
                        metrics.trajectoryOverlayErrorCode, "empty");
                addMachinePredicate("complete_draw_calls_at_most_100",
                    completeDrawCalls <= 100, std::to_string(completeDrawCalls),
                    "<=100");
                addMachinePredicate("texture_resident_bytes_at_most_256_mib",
                    textureResidentBytes <= 256u * 1024u * 1024u,
                    std::to_string(textureResidentBytes), "<=268435456");
                addMachinePredicate("predicted_vertices_at_most_8192",
                    cpuPackage.predicted.vertexCount <= 8192,
                    std::to_string(cpuPackage.predicted.vertexCount), "<=8192");
                addMachinePredicate("actual_vertices_at_most_8192",
                    cpuPackage.actual.vertexCount <= 8192,
                    std::to_string(cpuPackage.actual.vertexCount), "<=8192");
                addMachinePredicate("trajectory_texture_residency_delta_zero",
                    cpuPackage.textureResidencyDeltaBytes == 0,
                    std::to_string(cpuPackage.textureResidencyDeltaBytes), "0");
                addMachinePredicate("ui_safe_zero", uiAudit.safe == 0,
                    std::to_string(uiAudit.safe), "0");
                addMachinePredicate("ui_truncation_zero", uiAudit.truncation == 0,
                    std::to_string(uiAudit.truncation), "0");
                addMachinePredicate("ui_overlap_zero", uiAudit.overlap == 0,
                    std::to_string(uiAudit.overlap), "0");
                addMachinePredicate("ui_bounded_inspector", uiAudit.boundedInspector,
                    boolText(uiAudit.boundedInspector), "true");
                addMachinePredicate("ui_gameplay_corridor", uiAudit.gameplayCorridor,
                    boolText(uiAudit.gameplayCorridor), "true");
                addMachinePredicate("ui_inspector_backdrop_bounded",
                    uiAudit.inspectorBackdropBounded,
                    boolText(uiAudit.inspectorBackdropBounded), "true");
                addMachinePredicate("ui_geometry_gate", uiAudit.geometryGate,
                    boolText(uiAudit.geometryGate), "true");
                addMachinePredicate("ui_protected_intersection_area_zero",
                    uiAudit.protectedIntersectionArea == 0.0,
                    std::to_string(uiAudit.protectedIntersectionArea), "0");
                addMachinePredicate("ui_debug_nodes_zero", uiAudit.debugNodes == 0,
                    std::to_string(uiAudit.debugNodes), "0");
                addMachinePredicate("ui_unexpanded_icons_zero",
                    uiAudit.unexpandedIcons == 0,
                    std::to_string(uiAudit.unexpandedIcons), "0");
                addMachinePredicate("ui_opaque_icons_zero",
                    uiAudit.opaqueIcons == 0,
                    std::to_string(uiAudit.opaqueIcons), "0");
                addMachinePredicate("ui_panel_alpha_violations_zero",
                    uiAudit.panelAlphaViolations == 0,
                    std::to_string(uiAudit.panelAlphaViolations), "0");
                addMachinePredicate("fixture_authority_unchanged",
                    fixture.authorityUnchanged(),
                    boolText(fixture.authorityUnchanged()), "true");
                bool machinePass = true;
                for (const auto& predicate : machinePredicateLedger)
                    machinePass = machinePass && predicate.result;
'''
    text = text[:start] + machine_ledger + text[end:]

    require_anchor = (
        '                require(machinePass, "phase4_overlay_machine_gate_failed:" + capture.id);\n')
    telemetry = f'''                training::Phase4CaptureTelemetryInput telemetry;
                telemetry.captureId = capture.id;
                telemetry.planOrdinalOneBased = index + 1;
                telemetry.machinePass = machinePass;
                telemetry.machineErrorCode = machinePass ? "" :
                    (ownership.errorCode.empty() ?
                        "phase4_overlay_machine_gate_failed" :
                        ownership.errorCode);
                telemetry.machinePredicateLedgerRequired = true;
                telemetry.machinePredicateLedger = machinePredicateLedger;
                telemetry.ppmPath = ppm.string();
                telemetry.ppmSha256 = fileSha256(ppm.string());
                telemetry.audit = ownership.audit;
                telemetry.ownershipCandidates = ownership.candidateTelemetry;
                telemetry.arbitrationRecords = ownership.arbitrationTelemetry;
                telemetry.componentAwareBoundaryRecords = ownership.boundaryTelemetry;
                telemetry.integratedFinalOwnership = ownershipCaptured;
                telemetry.legacyPpmDiagnosticOnly = true;
                telemetry.legacyPpmContrastOk = contrast.ok;
                telemetry.legacyPpmVisibleProbeCount = contrast.visibleProbeCount;
                telemetry.legacyPpmHiddenProbeCount = contrast.hiddenProbeCount;
                telemetry.legacyPpmVisibleProbeCoverage =
                    contrast.visibleProbeCoverage;
                telemetry.legacyIntegratedDecisionDiscrepancy =
                    contrast.ok != ownership.ok;
                telemetry.sourceAndBinaryBindings = {{{{
                    "design_gate_sha256",
                    "c3b6f032deaac6bd39d32755d4ab423915e8b70b08b6da18626707b8d7134056"}},
                    {{"diagnostic_manifest_sha256",
                    "fb041c6e96c83f30df78742ead8a59251e3425f8b62999dd16390b349bdf7349"}},
                    {{"root_report_sha256",
                    "54d0bef6af28085f6e87456645802d4eb3e2537c6ca1869a57fece27141ab50b"}},
                    {{"ta_gate_sha256",
                    "ad95db70125384a1de289792002aba05c3557d587a70ea849d5d168f415e57fa"}},
                    {{"frozen_failure_sha256",
                    "2dbd499249df1e44631669165a891f94d232f4fa436c9fd0910716de35d2717f"}},
                    {{"implementation_source_aggregate_sha256","{aggregate}"}},
                    {{"runner_binary_sha256",fileSha256(argv[0])}}}};
                training::preservePhase4TelemetryThenEnforceMachineGate(
                    std::string(argv[3]) + ".final-ownership.jsonl", telemetry);
''' + require_anchor
    text = exactly_once(text, require_anchor, telemetry)
    text = text.replace("phase4-overlay-v4-gpu-capture:",
        "phase4-final-ownership-gpu-capture:")
    return text


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--final-ownership", action="store_true")
    args = parser.parse_args()
    output = generate()
    if args.final_ownership:
        output = add_final_ownership_instrumentation(output)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(output, encoding="utf-8")
    print(args.output)


if __name__ == "__main__":
    main()
