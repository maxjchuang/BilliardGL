#!/usr/bin/env python3
"""Fail-closed validator for the Phase 4 v4 ownership instrumentation gate."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
from typing import Any


ROOT = pathlib.Path(__file__).resolve().parents[1]
DEFAULT_GATE = ROOT / "docs/art/phase4-overlay-v4-final-ownership-instrumentation-implementation-design-gate-v1.json"

EXPECTED_BINDINGS = {
    "offline_diagnostic_manifest": (
        "docs/art/phase4-overlay-v4-ordinal1-offline-diagnostic-v1/diagnostic-evidence-manifest-v1.json",
        "fb041c6e96c83f30df78742ead8a59251e3425f8b62999dd16390b349bdf7349"),
    "offline_root_cause_report": (
        "docs/art/phase4-overlay-v4-ordinal1-offline-diagnostic-v1/root-cause-diagnostic-report-v1.json",
        "54d0bef6af28085f6e87456645802d4eb3e2537c6ca1869a57fece27141ab50b"),
    "ordinal1_root_cause_diagnostic_gate": (
        "docs/art/phase4-overlay-v4-targeted-ordinal1-root-cause-diagnostic-gate-v1.json",
        "ad95db70125384a1de289792002aba05c3557d587a70ea849d5d168f415e57fa"),
    "frozen_ordinal1_failure_receipt": (
        "docs/art/phase4-training-replay-overlay-v4-targeted-failure-receipt-v1.json",
        "2dbd499249df1e44631669165a891f94d232f4fa436c9fd0910716de35d2717f"),
    "frozen_ordinal1_ppm": (
        "docs/art/phase4-training-replay-overlay-v4-targeted-gpu-v1/camera_player_low_1080.ppm",
        "f3626f44ead6cfe4729e6ff3b76168747154b2452d0ca2afd70e1520b0e09744"),
    "frozen_v4_runner_source": (
        "build-phase4-overlay-v4-ready-v3/generated/phase4_overlay_v4_gpu_capture_runner.mm",
        "35fa98ad44e438f11a6a5b50a80c06de00adb518de4d345d51e741283b195d43"),
    "compound_visibility_implementation": (
        "src/Billiards/training/trajectory_compound_visibility.cpp",
        "b2149fad4bf11151ba8779132ccc5fc377906fd8fa81e835ccd98186074b673a"),
    "compound_visibility_contract": (
        "src/Billiards/training/trajectory_compound_visibility.h",
        "5d8af530e49fa08f8dca8ca08db304db9aee4c3e959203e1251a43b76d820360"),
}

AUTHORIZATION = {
    "instrumentation_implementation_design_approved": True,
    "future_instrumentation_implementation_allowed_after_this_gate": True,
    "implementation_performed_by_this_gate": False,
    "production_visual_change_authorized": False,
    "asset_change_authorized": False,
    "route_or_trajectory_change_authorized": False,
    "gpu_capture_authorized": False,
    "targeted_rerun_resume_or_overwrite_authorized": False,
    "formal68_authorized": False,
    "technical_art_review_authorized": False,
    "m3_authorized_or_claimed": False,
    "golden_authorized_or_claimed": False,
    "beta_authorized_or_claimed": False,
}

INVARIANTS = {
    "aggregate_visible_probe_coverage_min": 0.9,
    "predicted_visible_probe_coverage_min": 0.9,
    "actual_visible_probe_coverage_min": 0.9,
    "minimum_local_contrast_ratio": 4.0,
    "contrast_4_5_target_coverage_min": 0.99,
    "predicted_actual_luma_delta_srgb8_standard_min": 32.0,
    "predicted_actual_luma_delta_srgb8_cvd_min": 36.0,
    "ordinal1_source_probe_denominator": 95,
    "predicted_lateral_offset_px": 0.0,
    "actual_lateral_offset_px": 0.0,
    "maximum_screen_space_offset_px": 0.0,
    "compositing_order": "actual_then_predicted",
    "probe_deletion_allowed": False,
    "probe_merge_allowed": False,
    "threshold_relaxation_allowed": False,
    "route_or_world_sample_change_allowed": False,
    "endpoint_change_allowed": False,
    "asset_or_palette_change_allowed": False,
    "z_order_change_allowed": False,
    "product_ui_layout_change_allowed": False,
}

STAGE_A_FIELDS = {
    "capture_id", "plan_ordinal_one_based", "machine_status",
    "machine_error_code", "ppm_path", "ppm_sha256", "source_probe_count",
    "visible_probe_count", "hidden_probe_count",
    "aggregate_visible_probe_coverage",
    "predicted_source_visible_hidden_and_coverage",
    "actual_source_visible_hidden_and_coverage", "component_source_counts",
    "component_visible_counts", "component_hidden_reason_counts",
    "ordered_source_probe_ids", "ordered_source_probe_digest",
    "ordered_visible_probe_ids", "ordered_visible_probe_digest",
    "hidden_probe_id_track_role_reason_coordinates",
    "ownership_candidate_records", "exclusive_arbitration_records",
    "double_assignment_count", "foreign_track_evidence_count",
    "unowned_evidence_count", "instrumentation_schema_version",
    "source_and_binary_bindings",
}

CANDIDATE_FIELDS = {
    "source_probe_id", "physical_pixel", "resolved_sample_count",
    "resolved_pixel_equivalent_area", "owner_track", "owner_component",
    "owner_primitive_id", "probe_local_tangent_distance_px",
    "probe_local_normal_distance_px", "final_rgb_srgb8",
    "adjacent_non_self_rgb_srgb8", "edge_contrast_ratio",
    "eligibility_or_rejection_reason",
}

ARBITRATION_FIELDS = {
    "physical_sample_or_pixel_id", "claiming_source_probe_ids",
    "winner_source_probe_id",
    "sort_key_same_track_distance_primitive_probe_ordinal", "loser_reason",
}

INTEGRATION_KEYS = {
    "same_render_graph", "same_geometry_and_order", "same_clip", "same_msaa",
    "same_blend_visibility", "same_resolve", "tone_map_pairing",
    "later_ui_ownership", "readback_correspondence",
    "production_color_invariance",
}

FIXTURE_IDS = {
    "N1_TRUE_PANEL_OCCLUSION", "N2_TRUE_CROSS_TRACK_COVERAGE_NO_RESCUE",
    "P1_CROSS_COMPONENT_SAME_TRACK_RESCUE", "N3_FINAL_COLOR_MISMATCH",
    "N4_SHARED_PIXEL_ARBITRATION", "N5_TANGENT_NORMAL_BOUNDS",
    "N6_FOREIGN_TRACK_COLOR_MATCH", "N7_SCENE_OR_BACKGROUND_COLOR_MATCH",
    "N8_MSAA_PARTIAL_COVERAGE_BELOW_AREA",
    "P2_MSAA_PARTIAL_COVERAGE_ABOVE_AREA",
    "N9_CLIPPED_OR_DISCARDED_SAMPLE",
    "N10_FAILURE_ROW_FIELD_OR_FLUSH_MISSING",
}

EXIT_SEQUENCE = [
    "GATE_VALIDATOR_AND_FAIL_CLOSED_TESTS_PASS",
    "FRESH_INSTRUMENTATION_SOURCE_BINDINGS_AND_RELEASE_BUILD",
    "CPU_NEGATIVE_AND_POSITIVE_FIXTURES_ALL_PASS",
    "FROZEN_ORDINAL1_READ_ONLY_FAILURE_REPLAY_PASS",
    "STATIC_RENDER_GRAPH_AND_COLOR_INVARIANCE_CONFORMANCE_PASS",
    "FRESH_TARGETED11_READINESS_MAY_BE_PREPARED_NOT_RUN",
]


class GateValidationError(RuntimeError):
    pass


def require(condition: bool, error: str) -> None:
    if not condition:
        raise GateValidationError(error)


def sha(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate(gate: dict[str, Any]) -> dict[str, Any]:
    require(gate.get("schema") ==
            "billiardgl.phase4.overlay_v4_final_ownership_instrumentation_implementation_design_gate.v1",
            "gate_schema_invalid")
    require(gate.get("schema_version") == 1, "gate_schema_version_invalid")
    require(gate.get("status") ==
            "APPROVED_FOR_INSTRUMENTATION_IMPLEMENTATION_NOT_CAPTURE",
            "gate_status_invalid")

    bindings = gate.get("input_bindings")
    require(isinstance(bindings, dict) and set(bindings) == set(EXPECTED_BINDINGS),
            "gate_input_binding_roles_invalid")
    for role, (path_string, expected_sha) in EXPECTED_BINDINGS.items():
        value = bindings[role]
        require(value == {"path": path_string, "sha256": expected_sha},
                f"gate_input_binding_invalid:{role}")
        path = ROOT / path_string
        require(path.exists() and sha(path) == expected_sha,
                f"gate_input_binding_drift:{role}")

    require(gate.get("authorization_boundary") == AUTHORIZATION,
            "gate_authorization_boundary_invalid")
    frozen = gate.get("frozen_diagnostic_decision", {})
    require(frozen.get("status") ==
            "DIAGNOSTIC_MISSING_FINAL_OWNERSHIP_TELEMETRY_FAIL_CLOSED" and
            frozen.get("root_cause_isolated") is False,
            "gate_frozen_diagnostic_boundary_invalid")
    require(frozen.get("frozen_counts") ==
            {"source": 95, "visible": 46, "hidden": 49, "coverage": 0.484211},
            "gate_frozen_counts_invalid")
    require(frozen.get("offline_exact_role_analytic_counts") ==
            {"source": 95, "visible": 69, "hidden": 26} and
            frozen.get("offline_compound_analytic_counts") ==
            {"source": 95, "visible": 95, "hidden": 0} and
            frozen.get("counterfactual_numbers_are_not_final_ownership_evidence") is True,
            "gate_counterfactual_claim_boundary_invalid")

    stages = gate.get("implementation_sequence")
    require(isinstance(stages, list) and len(stages) == 2,
            "gate_implementation_sequence_invalid")
    require([(stage.get("ordinal"), stage.get("id")) for stage in stages] == [
        (1, "A_FAILURE_TELEMETRY_PRESERVATION"),
        (2, "B_INTEGRATED_FINAL_SEMANTIC_OWNERSHIP")],
        "gate_implementation_sequence_order_invalid")
    stage_a, stage_b = stages
    require(stage_a.get("status") == stage_b.get("status") ==
            "AUTHORIZED_NOT_IMPLEMENTED", "gate_implementation_claim_invalid")
    control_flow = stage_a.get("mandatory_control_flow", {})
    require(set(control_flow) == {"compute_audit", "serialize_before_gate",
                                  "then_fail_closed", "serialization_failure_rule"},
            "gate_stage_a_control_flow_invalid")
    require("before require(machinePass)" in control_flow["serialize_before_gate"],
            "gate_stage_a_not_before_machine_gate")
    require(set(stage_a.get("required_failure_row_fields", [])) == STAGE_A_FIELDS,
            "gate_stage_a_failure_fields_invalid")
    require(set(stage_a.get("ownership_candidate_record_fields", [])) ==
            CANDIDATE_FIELDS, "gate_stage_a_candidate_fields_invalid")
    require(set(stage_a.get("exclusive_arbitration_record_fields", [])) ==
            ARBITRATION_FIELDS, "gate_stage_a_arbitration_fields_invalid")
    require(len(stage_a.get("acceptance_tests", [])) >= 5,
            "gate_stage_a_acceptance_tests_missing")

    require(stage_b.get("depends_on") == "A_FAILURE_TELEMETRY_PRESERVATION",
            "gate_stage_b_dependency_invalid")
    require(set(stage_b.get("integration_contract", {})) == INTEGRATION_KEYS,
            "gate_stage_b_integration_contract_invalid")
    integration = stage_b["integration_contract"]
    require("detached" in integration["same_render_graph"].lower() and
            "forbidden" in integration["same_render_graph"].lower(),
            "gate_detached_attachment_not_forbidden")
    require("exact production MSAA sample count" in integration["same_msaa"],
            "gate_msaa_not_exact")
    require("track=none" in integration["later_ui_ownership"],
            "gate_later_ui_owner_invalid")
    probe_contract = stage_b.get("probe_attribution_contract", {})
    require(probe_contract.get("ordinal1_denominator") == 95 and
            probe_contract.get("same_track_only") is True and
            probe_contract.get("minimum_edge_contrast_ratio") == 4.0,
            "gate_stage_b_probe_contract_invalid")
    require("Euclidean-as-normal" in probe_contract.get("true_coordinate_frame", ""),
            "gate_true_coordinate_frame_missing")

    require(gate.get("immutable_invariants") == INVARIANTS,
            "gate_invariants_invalid")
    fixtures = gate.get("required_cpu_negative_and_positive_fixtures")
    require(isinstance(fixtures, list) and len(fixtures) == 12 and
            {fixture.get("id") for fixture in fixtures} == FIXTURE_IDS and
            all(isinstance(fixture.get("expected"), str) and fixture["expected"]
                for fixture in fixtures), "gate_cpu_fixtures_invalid")

    replay = gate.get("frozen_ordinal1_replay_test", {})
    require(replay.get("inputs_read_only") is True and
            replay.get("expected_source_probe_count") == 95 and
            replay.get("expected_ordered_source_probe_digest") ==
            "d533924980efc85aeb94e4f1d31831d93e2bfce744be3b66481d08c56d10289a" and
            len(replay.get("required_assertions", [])) >= 5,
            "gate_frozen_replay_test_invalid")

    exits = gate.get("required_exit_sequence_before_any_fresh_targeted_readiness")
    require(isinstance(exits, list) and
            [item.get("id") for item in exits] == EXIT_SEQUENCE and
            [item.get("ordinal") for item in exits] == list(range(1, 7)) and
            all(item.get("gpu_allowed") is False for item in exits),
            "gate_exit_sequence_invalid")
    require(exits[2].get("required_fixture_count") == 12 and
            exits[3].get("required_source_probe_count") == 95,
            "gate_exit_fixture_or_replay_contract_invalid")

    capture = gate.get("future_capture_boundary", {})
    require(capture.get("fresh_targeted_readiness_is_not_capture_authorization") is True and
            capture.get("new_root_single_use_authorization_required_after_readiness") is True and
            capture.get("formal68_requires_separate_post_targeted_gate") is True and
            capture.get("technical_art_review_requires_separate_machine_pass_gate") is True,
            "gate_future_capture_boundary_invalid")
    require(len(gate.get("global_forbidden_actions", [])) >= 9,
            "gate_forbidden_actions_incomplete")
    require(gate.get("decision") ==
            "AUTHORIZE FUTURE A-THEN-B TELEMETRY IMPLEMENTATION ONLY; FIRST PASS CPU FIXTURES AND FROZEN FAILURE REPLAY; THEN PREPARE FRESH TARGETED READINESS WITHOUT RUNNING IT",
            "gate_decision_invalid")

    diagnostic_manifest = json.loads((ROOT / EXPECTED_BINDINGS[
        "offline_diagnostic_manifest"][0]).read_text())
    root_report = json.loads((ROOT / EXPECTED_BINDINGS[
        "offline_root_cause_report"][0]).read_text())
    require(diagnostic_manifest.get("actual_gpu_capture") is False and
            diagnostic_manifest.get("targeted_or_formal_runner_invoked") is False,
            "bound_diagnostic_manifest_claim_invalid")
    require(root_report.get("root_cause_isolated") is False and
            not any(root_report.get("claims", {}).values()),
            "bound_root_report_claim_invalid")
    return {
        "schema": gate["schema"],
        "status": gate["status"],
        "implementation_steps": [stage["id"] for stage in stages],
        "cpu_fixture_count": len(fixtures),
        "exit_stage_count": len(exits),
        "gpu_capture_authorized": False,
        "implementation_performed": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--gate", type=pathlib.Path, default=DEFAULT_GATE)
    parser.add_argument("--expect-sha")
    args = parser.parse_args()
    gate_path = args.gate.resolve()
    require(gate_path.exists(), "gate_file_missing")
    if args.expect_sha:
        require(sha(gate_path) == args.expect_sha, "gate_file_sha_mismatch")
    result = validate(json.loads(gate_path.read_text()))
    result["gate_path"] = str(gate_path)
    result["gate_sha256"] = sha(gate_path)
    print("PHASE4_V4_FINAL_OWNERSHIP_INSTRUMENTATION_GATE_VALIDATION_PASS")
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
