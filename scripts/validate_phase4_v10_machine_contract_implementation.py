#!/usr/bin/env python3
"""CPU/static gate for the Phase4 v10 exact-two predicate-ledger correction."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GATE = ROOT / ("docs/art/phase4-final-ownership-v10-ordinal1-offline-"
               "diagnostic-v1/minimal-machine-contract-correction-design-gate-v1.json")
EVIDENCE = ROOT / ("docs/art/phase4-final-ownership-v10-ordinal1-offline-"
                   "diagnostic-v1/failure-evidence-receipt-v1.json")
FROZEN_TELEMETRY = ROOT / ("docs/art/phase4-training-replay-overlay-v4-final-"
                           "ownership-targeted11-output-v10/"
                           "results.jsonl.final-ownership.jsonl")
PATHS = {
    "renderer": ROOT / "src/Billiards/render/modern/modern_renderer.cpp",
    "renderer_header": ROOT / "src/Billiards/render/modern/modern_renderer.h",
    "instrumentation": ROOT / "src/Billiards/training/phase4_final_ownership_instrumentation.cpp",
    "instrumentation_header": ROOT / "src/Billiards/training/phase4_final_ownership_instrumentation.h",
    "runner_generator": ROOT / "scripts/generate_phase4_overlay_v4_gpu_runner.py",
    "telemetry_test": ROOT / "tests/phase4_final_ownership_telemetry_tests.cpp",
    "trajectory": ROOT / "src/Billiards/training/trajectory_overlay_renderer.cpp",
    "trajectory_header": ROOT / "src/Billiards/training/trajectory_overlay_renderer.h",
    "ownership": ROOT / "src/Billiards/training/final_semantic_ownership.cpp",
    "ownership_header": ROOT / "src/Billiards/training/final_semantic_ownership.h",
    "visibility": ROOT / "src/Billiards/training/trajectory_compound_visibility.cpp",
    "visibility_header": ROOT / "src/Billiards/training/trajectory_compound_visibility.h",
}
IMMUTABLE = {
    "trajectory": "09852cd4ad0a9c8aebeeac5b026d84f2d3c24624deddf7aab46330cfb1afd09e",
    "trajectory_header": "19e8c1b5cae26384c55b8678289d5cf2c55bb1901425a6294211de6a57010ab6",
    "ownership": "7d182d918e00920ad178606fda6371319c4f5cb10d5bf9ca0688434ac9a2f704",
    "ownership_header": "6b66fd2f6a186ace953c24a5214fda16e56314209fffe413d2f27fc4665bc1ac",
    "visibility": "213f78937c753c30fd65e94501f5ce0172348b29b32dc51d3e28de88236bc160",
    "visibility_header": "e8f77f7fc68de63c63422acf877e133e2fd4b10111b184696bfe275cbdf726d7",
}
EXPECTED_PREDICATES = [
    "ownership_captured", "ownership_ok",
    "ordinal1_source_probe_count_exact_95", "cpu_package_ok",
    "endpoint_contract", "track_contract", "topology_contract",
    "physical_style_contract", "rails_text_contract",
    "actual_only_legend_contract", "advanced_text_contract",
    "content_audit_ok", "trajectory_draw_submissions_exact_2",
    "trajectory_actual_draw_submitted",
    "trajectory_actual_backdrop_frozen_midpoint",
    "trajectory_predicted_draw_submitted",
    "trajectory_submission_order_actual_freeze_predicted",
    "trajectory_texture_count_zero", "trajectory_shared_projection",
    "trajectory_homogeneous_clip", "trajectory_safe_area_clip",
    "trajectory_play_surface_clip", "trajectory_error_empty",
    "complete_draw_calls_at_most_100",
    "texture_resident_bytes_at_most_256_mib",
    "predicted_vertices_at_most_8192", "actual_vertices_at_most_8192",
    "trajectory_texture_residency_delta_zero", "ui_safe_zero",
    "ui_truncation_zero", "ui_overlap_zero", "ui_bounded_inspector",
    "ui_gameplay_corridor", "ui_inspector_backdrop_bounded",
    "ui_geometry_gate", "ui_protected_intersection_area_zero",
    "ui_debug_nodes_zero", "ui_unexpanded_icons_zero",
    "ui_opaque_icons_zero", "ui_panel_alpha_violations_zero",
    "fixture_authority_unchanged",
]


def sha_bytes(value: str) -> str:
    return hashlib.sha256(value.encode()).hexdigest()


def valid_submission_sequence(events: list[str], draws: int) -> bool:
    return draws == 2 and events == [
        "actual_draw", "actual_backdrop_freeze", "predicted_draw"]


def load_texts() -> dict[str, str]:
    return {name: path.read_text() for name, path in PATHS.items()}


def validate_texts(texts: dict[str, str]) -> list[str]:
    errors: list[str] = []

    def require(key: str, token: str, error: str) -> None:
        if token not in texts[key]:
            errors.append(error)

    for name, expected in IMMUTABLE.items():
        if sha_bytes(texts[name]) != expected:
            errors.append(f"forbidden_source_drift:{name}")
    renderer = texts["renderer"]
    ordered_tokens = [
        "frameMetrics.trajectoryOverlayActualDrawSubmitted = true;",
        "frameMetrics.trajectoryOverlayActualBackdropFrozen = true;",
        "frameMetrics.trajectoryOverlayPredictedDrawSubmitted = true;",
        "frameMetrics.trajectoryOverlaySubmissionOrderValid =\n"
        "            submissionSequence == 3 && draws == 2;",
    ]
    positions = [renderer.find(token) for token in ordered_tokens]
    if any(position < 0 for position in positions) or positions != sorted(positions):
        errors.append("actual_freeze_predicted_observation_order_invalid")
    require("renderer", "submissionSequence = 1;",
            "actual_submission_sequence_missing")
    require("renderer", "if (submissionSequence == 1) submissionSequence = 2;",
            "midpoint_freeze_sequence_missing")
    require("renderer", "if (submissionSequence == 2) submissionSequence = 3;",
            "predicted_submission_sequence_missing")
    for field in (
        "trajectoryOverlayActualDrawSubmitted",
        "trajectoryOverlayActualBackdropFrozen",
        "trajectoryOverlayPredictedDrawSubmitted",
        "trajectoryOverlaySubmissionOrderValid",
    ):
        require("renderer_header", f"bool {field} = false;",
                f"frame_metric_missing:{field}")

    generator = texts["runner_generator"]
    for predicate in EXPECTED_PREDICATES:
        if generator.count(f'addMachinePredicate("{predicate}"') != 1:
            errors.append(f"machine_predicate_missing_or_duplicate:{predicate}")
    require("runner_generator",
            'metrics.trajectoryOverlayDrawCalls == 2',
            "strict_exact_two_draw_predicate_missing")
    if "metrics.trajectoryOverlayDrawCalls == 1" in generator or \
            "metrics.trajectoryOverlayDrawCalls <= 2" in generator:
        errors.append("draw_predicate_not_strict_exact_two")
    require("runner_generator",
            "for (const auto& predicate : machinePredicateLedger)\n"
            "                    machinePass = machinePass && predicate.result;",
            "machine_pass_not_ledger_conjunction")
    require("runner_generator", "telemetry.machinePredicateLedgerRequired = true;",
            "runner_ledger_not_required")
    require("runner_generator", "telemetry.machinePredicateLedger = machinePredicateLedger;",
            "runner_ledger_not_serialized")
    preserve = generator.find(
        "training::preservePhase4TelemetryThenEnforceMachineGate(")
    append_require_anchor = generator.find("''' + require_anchor", preserve)
    if preserve < 0 or append_require_anchor < 0 or \
            preserve >= append_require_anchor:
        errors.append("predicate_ledger_not_preserved_before_require")

    require("instrumentation_header", "struct Phase4MachinePredicateTelemetry",
            "predicate_telemetry_type_missing")
    require("instrumentation_header",
            "std::vector<Phase4MachinePredicateTelemetry> machinePredicateLedger;",
            "predicate_ledger_input_missing")
    require("instrumentation", "phase4_telemetry_machine_predicate_ledger_missing",
            "missing_ledger_not_rejected")
    require("instrumentation", "phase4_telemetry_machine_predicate_ledger_invalid",
            "invalid_ledger_not_rejected")
    require("instrumentation", "phase4_telemetry_machine_predicate_decision_mismatch",
            "ledger_decision_mismatch_not_rejected")
    require("instrumentation", 'row["machine_predicate_ledger"]',
            "predicate_ledger_json_missing")
    require("instrumentation", 'row["failed_machine_predicate_names"]',
            "failed_predicate_names_json_missing")
    require("telemetry_test", "testRequiredMachinePredicateLedgerMissingFailsClosed",
            "missing_ledger_fixture_missing")
    require("telemetry_test", "testDuplicateMachinePredicateNameFailsClosed",
            "duplicate_ledger_fixture_missing")
    require("telemetry_test", "testMachineDecisionMustEqualPredicateConjunction",
            "decision_mismatch_fixture_missing")

    sequence_cases = {
        "positive": valid_submission_sequence(
            ["actual_draw", "actual_backdrop_freeze", "predicted_draw"], 2),
        "zero": valid_submission_sequence([], 0),
        "one": valid_submission_sequence(["actual_draw"], 1),
        "three": valid_submission_sequence(
            ["actual_draw", "actual_backdrop_freeze", "predicted_draw"], 3),
        "reverse": valid_submission_sequence(
            ["predicted_draw", "actual_backdrop_freeze", "actual_draw"], 2),
        "missing_freeze": valid_submission_sequence(
            ["actual_draw", "predicted_draw"], 2),
        "late_freeze": valid_submission_sequence(
            ["actual_draw", "predicted_draw", "actual_backdrop_freeze"], 2),
    }
    if sequence_cases != {"positive": True, "zero": False, "one": False,
                          "three": False, "reverse": False,
                          "missing_freeze": False, "late_freeze": False}:
        errors.append("ordered_exact_two_negative_fixture_failure")
    return errors


def main() -> None:
    errors: list[str] = []
    if hashlib.sha256(GATE.read_bytes()).hexdigest() != \
            "98575c3e3c48e99e9a056c047ba59e268a23c038d2685ed7252edd5d0cbe525a":
        errors.append("design_gate_hash_drift")
    if hashlib.sha256(EVIDENCE.read_bytes()).hexdigest() != \
            "c78fa8df1e74dbe377b97bdf3f28949bb8a778abc743952abcdb308f3693ffdc":
        errors.append("failure_evidence_hash_drift")
    frozen = json.loads(FROZEN_TELEMETRY.read_text())
    if [frozen.get(key) for key in
            ("source_probe_count", "visible_probe_count", "hidden_probe_count")] != \
            [95, 89, 6] or frozen.get("machine_status") != "FAILED":
        errors.append("frozen_v10_replay_semantics_invalid")
    errors.extend(validate_texts(load_texts()))
    result = {"schema":
        "billiardgl.phase4.v10_machine_contract_implementation_static.v1",
        "status": "PASS" if not errors else "FAIL",
        "gpu_cocoa_opengl_invoked": False,
        "capture_authorized": False,
        "predicate_count": len(EXPECTED_PREDICATES),
        "ordered_exact_two_fixture_count": 7,
        "errors": errors}
    print(json.dumps(result, indent=2, sort_keys=True))
    raise SystemExit(0 if not errors else 1)


if __name__ == "__main__":
    main()
