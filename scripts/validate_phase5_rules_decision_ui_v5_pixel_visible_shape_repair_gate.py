#!/usr/bin/env python3
"""Validate the Phase5 Rules UI v5 pixel-visible shape repair design gate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GATE = ROOT / (
    "docs/art/phase5-rules-decision-ui-v5-pixel-visible-shape-"
    "repair-design-gate-v1.json"
)
FAILURE_RECEIPT_PATH = (
    "docs/evidence/phase5/rules-decision-ui-v4-exact-shape-"
    "visual-failure-evidence-receipt-v1.json"
)
FAILURE_RECEIPT_SHA256 = (
    "28c514cfe08e6cec2ccd94d28b593419fa9d8fa54d656362865358822cc5ba56"
)
ROOT_CAUSE_PATH = (
    "docs/art/phase5-rules-decision-ui-v4-exact-shape-"
    "offline-root-cause-report-v1.json"
)
ROOT_CAUSE_SHA256 = (
    "0f5cc2c9c4941c4fa1782c0ffc9e6c2db7b5e8aebe284d526ed5ffe347eccb96"
)
MANUAL_PATH = (
    "docs/evidence/phase5/rules-decision-ui-v4-exact16-"
    "independent-ta-manual-review-v1.json"
)
MANUAL_SHA256 = (
    "8847ae7af29ee16cba968f2c23704fe7968cdc113581877c7d4bf3a291381014"
)
MACHINE_RECEIPT_PATH = (
    "docs/evidence/phase5/rules-decision-ui-v4-exact16-"
    "machine-pass-freeze-v1/machine-pass-freeze-receipt.json"
)
MACHINE_RECEIPT_SHA256 = (
    "db59bc6b5e7bfd0b2038043b147deac13cde20fa6ebaafef7085bcef94d85fd5"
)
PNG_MANIFEST_PATH = (
    "docs/evidence/phase5/rules-decision-ui-v4-exact16-"
    "machine-pass-freeze-v1/stable-png-manifest.json"
)
PNG_MANIFEST_SHA256 = (
    "f1242dba2c5fdde4728fbf474ae8163475f0108df16a074e0f80bc0a63076bec"
)
V4_GATE_PATH = (
    "docs/art/phase5-rules-decision-ui-v4-exact-shape-"
    "correction-design-gate-v1.json"
)
V4_GATE_SHA256 = (
    "f93ca59634875ed5256f5c22678a55817e0f3e8bf8df582b3b04ac527b886051"
)
REPAIR_CASES = [
    "group_choice_armed_confirm--p1--zh_cn",
    "group_choice_armed_confirm--p2--en",
    "group_choice_armed_confirm--p3--zh_cn",
    "group_choice_armed_confirm--p4--en",
]
FAILED_AXES = [
    "focus_pointer_keyboard_redundancy",
    "standard_cvd_non_color_redundancy",
]
CONTROLS = [
    "group_choice_open_unselected--p1--en",
    "group_choice_open_unselected--p2--zh_cn",
    "group_choice_open_unselected--p3--en",
    "group_choice_open_unselected--p4--zh_cn",
    "eight_legal_open_unselected--p1--en",
    "break_illegal_open_unselected--p2--zh_cn",
    "submitting_progress--p3--en",
    "authority_error--p4--en",
    "full_table_preview_valid--p1--en",
    "full_table_preview_invalid--p2--en",
    "behind_head_string_preview_valid--p3--en",
    "behind_head_string_preview_invalid--p4--en",
]
CANONICAL_AXES = [
    "action_semantics_and_branch_correctness",
    "actor_revision_and_authority_status_readability",
    "no_default_selection_or_accidental_submission_affordance",
    "focus_pointer_keyboard_redundancy",
    "text_clipping_collision_and_minimum_size",
    "standard_cvd_non_color_redundancy",
    "placement_scope_and_legality_readability",
    "stale_error_ai_pending_fail_closed_clarity",
    "foundation_and_unoffered_action_absence",
    "debug_and_product_surface_separation",
]
SOURCE_BINDINGS = {
    "src/Billiards/render/ui/rules_decision_ui_draw_list.h":
        "a8a0f1f4df440d6c8008c3d9c343fa9f4056a4b6e0db76834a438f1ab9b060c3",
    "src/Billiards/render/ui/rules_decision_ui_draw_list.cpp":
        "fe5522e17c1c39ae57fd87aa87a0f9881dc95ad6cf4d35880694f8d6a8560231",
    "src/Billiards/render/ui/rules_decision_product_draw.cpp":
        "68f0b8e65d7220a2c362e66c00b198d89e1b3ac44333f2682cf95cd8d30a3f80",
    "src/Billiards/render/ui/product_ui_gpu_renderer.cpp":
        "85f547348ee9b162968df03032e6a84c741b1a5aeed87e9bab7070242e226453",
    "tests/rules_decision_product_runtime_tests.cpp":
        "faf0dabaee29d6f4d2abd4c07fe4650347020d6cc85c01783a356f9f96428bac",
    "scripts/run_phase5_rules_decision_ui_v4_exact_shape_machine.py":
        "66fc7fec1e5497467f6e94d7d8401f90ebd73c0b52dff40cc1c2265daad3e7be",
    "scripts/validate_phase5_rules_decision_ui_v4_exact_shape_machine.py":
        "557df28b4117a78c43497a2d7bb2205d5bd6e0acb4d3696cc1788f86068bf488",
}
PNG_SHAS = {
    REPAIR_CASES[0]: "4e9e398b1a4bf6d7e242a5fc5ad1c8069298549f76bd2826a90532dcbf983b85",
    REPAIR_CASES[1]: "661b071f5bfad6ed5420b6b9f1237655bfbecc1fa173f6cc1bbf3cedd580af72",
    REPAIR_CASES[2]: "4febdaf6514b45d02cf77b239a6740e90746584a29e631ea9082c3ea72688b18",
    REPAIR_CASES[3]: "0da002ea913bde50a55ef11a293a132fdd795a5ad56b90013c5b7ca1590cf829",
}
EXPECTED_PROFILES = [
    {"profile": "p1", "logical": [1280, 720], "physical": [1280, 720],
     "density": 1, "ui_scale": 0.85, "palette": "standard", "locale": "zh_cn"},
    {"profile": "p2", "logical": [1280, 720], "physical": [1280, 720],
     "density": 1, "ui_scale": 1.35, "palette": "color_vision_safe", "locale": "en"},
    {"profile": "p3", "logical": [1920, 1080], "physical": [1920, 1080],
     "density": 1, "ui_scale": 1.0, "palette": "color_vision_safe", "locale": "zh_cn"},
    {"profile": "p4", "logical": [1280, 720], "physical": [2560, 1440],
     "density": 2, "ui_scale": 1.0, "palette": "standard", "locale": "en"},
]
NEGATIVE_FIXTURES = [
    "outline_color_equals_row_background",
    "outline_alpha_zero",
    "focus_lower_arm_missing",
    "focus_intersects_text_mask",
    "diamond_one_edge_missing",
    "diamond_outline_absent_bare_check_only",
    "confirm_outer_frame_open",
    "confirm_inner_frame_absent",
    "confirm_frames_absent_bare_arrow_only",
    "shape_clipped_or_overwritten",
    "token_bbox_only_without_raster",
    "standard_only_without_cvd_transforms",
]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_json(path: Path | str) -> dict[str, Any]:
    candidate = Path(path)
    if not candidate.is_absolute():
        candidate = ROOT / candidate
    return json.loads(candidate.read_text(encoding="utf-8"))


def validate_file(path: str, digest: str, role: str) -> dict[str, Any]:
    candidate = ROOT / path
    require(candidate.is_file(), role + " missing")
    require(sha256(candidate) == digest, role + " SHA drift")
    return load_json(candidate)


def validate_failure_receipt(receipt: dict[str, Any]) -> None:
    require(receipt.get("schema") ==
            "phase5_rules_decision_ui_v4_exact_shape_visual_"
            "failure_evidence_receipt_v1"
            and receipt.get("status") ==
            "FAIL_CONFIRMED_FROZEN_PIXELS_AND_CURRENT_CONTROL_FLOW_NO_GPU",
            "failure receipt identity/status")
    authority = receipt.get("failure_authority", {})
    require(authority == {
        "path": MANUAL_PATH, "sha256": MANUAL_SHA256,
        "overall_result": "FAIL", "failed_case_count": 4,
        "failed_axis_occurrence_count": 8,
        "failed_axes_per_case": FAILED_AXES,
    }, "failure receipt exact manual authority")
    source_rows = receipt.get("current_source_bindings", [])
    require({row.get("path"): row.get("sha256") for row in source_rows} ==
            SOURCE_BINDINGS, "failure receipt current source binding set")
    for path, digest in SOURCE_BINDINGS.items():
        require(sha256(ROOT / path) == digest,
                "bound diagnosis source drift: " + path)
    cases = receipt.get("case_pixel_findings", [])
    require([case.get("capture_id") for case in cases] == REPAIR_CASES,
            "failure receipt exact four pixel cases/order")
    for case in cases:
        cid = case["capture_id"]
        require(case.get("png_sha256") == PNG_SHAS[cid]
                and sha256(ROOT / case["png_path"]) == PNG_SHAS[cid],
                "frozen repair PNG binding: " + cid)
        slots = case.get("slots", {})
        require(slots.get("selected", {}).get(
                    "visible_closed_outline_count") == 0
                and slots.get("confirm", {}).get(
                    "visible_closed_frame_count") == 0
                and slots.get("focus", {}).get(
                    "independent_chevron_component_count") == 0
                and slots.get("focus", {}).get(
                    "visible_foreground_pixels_in_lower_40_percent") == 0,
                "frozen failure pixel topology: " + cid)
        require(slots["selected"].get("visible_component_count") == 1
                and slots["confirm"].get("visible_component_count") == 1,
                "bare selected/confirm glyph components: " + cid)
    aggregate = receipt.get("aggregate_pixel_result", {})
    require(aggregate.get("repair_images_inspected") == 4
            and aggregate.get("standard_images") == 2
            and aggregate.get("color_vision_safe_images") == 2
            and aggregate.get("selected_slots_with_visible_closed_diamond") == 0
            and aggregate.get("confirm_slots_with_two_visible_closed_frames") == 0
            and aggregate.get("focus_slots_with_independent_joined_chevron") == 0
            and aggregate.get("result") == "FAIL_4_OF_4_SAME_PIXEL_TOPOLOGY",
            "failure receipt aggregate pixel result")
    classification = receipt.get("control_flow_classification", {})
    for ruled_out in (
            "draw_list_geometry_missing", "shape_tokens_missing",
            "triangle_primitive_emission_missing", "alpha_zero_or_transparent",
            "clipped_by_shape_or_row_slot", "overwritten_by_later_panel"):
        require(classification.get(ruled_out) is False,
                "control-flow ruled-out classification: " + ruled_out)
    require("exact same accent RGB" in classification.get("primary_failure", "")
            and "composited CPU raster" in classification.get("oracle_gap", ""),
            "primary failure/oracle gap classification")
    boundary = receipt.get("proof_boundary", {})
    require(boundary.get("offline_pixel_diagnosis_complete") is True
            and boundary.get("current_source_control_flow_inspected") is True
            and all(boundary.get(key) is False for key in (
                "production_source_modified", "gpu_authorized", "gpu_executed",
                "v5_implementation_complete", "v5_machine_pass",
                "v5_technical_art_pass", "product_exposed",
                "golden_promotion", "m2_promotion", "beta_pass",
                "real_input_pass", "screen_reader_pass")),
            "failure receipt no-GPU/no-production proof boundary")


def validate_root_cause(report: dict[str, Any]) -> None:
    require(report.get("schema") ==
            "phase5_rules_decision_ui_v4_exact_shape_offline_"
            "root_cause_report_v1"
            and report.get("status") ==
            "ROOT_CAUSE_CONFIRMED_NO_GPU_NO_PRODUCTION_EDIT",
            "root-cause report identity/status")
    require(report.get("failure_receipt") == {
        "path": FAILURE_RECEIPT_PATH, "sha256": FAILURE_RECEIPT_SHA256},
        "root-cause receipt binding")
    trace = report.get("control_flow_trace", [])
    require([row.get("stage") for row in trace] == [
        "draw_list_geometry", "row_fill_color", "focus_raster",
        "selected_raster", "confirm_raster", "clip_and_order",
        "current_cpp_oracle", "current_python_oracle"],
        "root-cause exact control-flow trace")
    findings = report.get("causal_findings", {})
    require(findings.get("primary_root_cause", {}).get("id") ==
            "V4_ACCENT_ON_ACCENT_ZERO_CONTRAST"
            and findings.get("secondary_layout_defect", {}).get("id") ==
            "V4_FOCUS_SLOT_NOT_RESERVED_FROM_COPY"
            and findings.get("verification_escape", {}).get("id") ==
            "V4_TOKEN_BBOX_WITHOUT_RASTER_TOPOLOGY",
            "root-cause exact three-part diagnosis")
    require(len(report.get("ruled_out", [])) == 6
            and len(report.get("required_correction_direction", [])) == 4,
            "root-cause ruled-out/correction inventory")
    correction = " ".join(report["required_correction_direction"])
    for term in ("at least 4.0:1", "dedicated", "CPU raster snapshots",
                 "final pixels", "never independently"):
        require(term in correction, "root-cause correction missing: " + term)


def validate_read_only_bindings(gate: dict[str, Any]) -> None:
    expected = [
        (MANUAL_PATH, MANUAL_SHA256),
        (MACHINE_RECEIPT_PATH, MACHINE_RECEIPT_SHA256),
        (PNG_MANIFEST_PATH, PNG_MANIFEST_SHA256),
        (FAILURE_RECEIPT_PATH, FAILURE_RECEIPT_SHA256),
        (ROOT_CAUSE_PATH, ROOT_CAUSE_SHA256),
        (V4_GATE_PATH, V4_GATE_SHA256),
    ]
    rows = gate.get("read_only_bindings", [])
    require([(row.get("path"), row.get("sha256")) for row in rows] == expected,
            "v5 exact read-only bindings/order")
    for path, digest in expected:
        validate_file(path, digest, "v5 read-only binding " + path)
    manual = load_json(MANUAL_PATH)
    require(manual.get("overall_result") == "FAIL"
            and manual.get("failed_case_ids") == REPAIR_CASES
            and manual.get("review_counts", {}).get("axes_fail") == 8,
            "bound v4 manual exact failure")
    receipt = load_json(FAILURE_RECEIPT_PATH)
    validate_failure_receipt(receipt)
    report = load_json(ROOT_CAUSE_PATH)
    validate_root_cause(report)


def validate_scope_and_repair(gate: dict[str, Any]) -> None:
    failure = gate.get("bound_failure_set", {})
    require(failure == {
        "result": "FAIL", "failed_case_count": 4,
        "failed_axis_occurrence_count": 8,
        "failed_case_ids": REPAIR_CASES,
        "failed_axes_per_case": FAILED_AXES,
        "root_cause_id": "V4_ACCENT_ON_ACCENT_ZERO_CONTRAST",
        "secondary_layout_defect_id": "V4_FOCUS_SLOT_NOT_RESERVED_FROM_COPY",
        "verification_escape_id": "V4_TOKEN_BBOX_WITHOUT_RASTER_TOPOLOGY",
    }, "v5 exact failure/root-cause scope")
    repair = gate.get("repair_contract", {})
    require(repair.get("state_id") == "group_choice_armed_confirm"
            and list(repair.get("token_ids_immutable", {})) == [
                "focus_chevron", "selected_check_diamond",
                "confirm_double_frame_plus_enter_arrow"]
            and len(set(repair.get("token_ids_immutable", {}).values())) == 3,
            "v5 state/exact immutable tokens")
    contrast = repair.get("row_foreground_contrast", {})
    require(contrast.get("minimum_ratio") == 4.0
            and contrast.get("applies_to") == [
                "standard", "color_vision_safe"]
            and contrast.get("supplemental_stricter_repair_constraint") is True
            and contrast.get("inherited_thresholds_changed") is False
            and contrast.get("accent_on_accent_forbidden") is True
            and "Final composited CPU raster pixels" in
                contrast.get("measurement_surface", ""),
            "v5 final-pixel >=4 contrast contract")
    focus = repair.get("focus_chevron", {})
    require(focus.get("minimum_physical_bbox_px") == [18, 18]
            and focus.get("minimum_physical_stroke_px") == 2
            and focus.get("required_joined_component_count") == 1
            and focus.get("must_span_upper_and_lower_slot_regions") is True
            and focus.get("minimum_text_clearance_px") == 2
            and focus.get("copy_leading_inset_must_reserve_slot") is True
            and focus.get("must_not_share_pixels_with_text_or_hud_diamond") is True,
            "v5 focus visible independent component/slot")
    selected = repair.get("selected_check_diamond", {})
    require(selected.get("minimum_visible_closed_outline_count") == 1
            and selected.get("check_enclosed_by_outline_required") is True
            and selected.get("bare_check_forbidden") is True
            and selected.get("minimum_physical_bbox_px") == [18, 18]
            and selected.get("minimum_outline_stroke_px") == 2
            and selected.get("minimum_check_stroke_px") == 2,
            "v5 selected final-pixel closure")
    confirm = repair.get("confirm_double_frame_plus_enter_arrow", {})
    require(confirm.get("minimum_visible_nested_closed_frame_count") == 2
            and confirm.get("arrow_inside_innermost_frame_required") is True
            and confirm.get("bare_arrow_forbidden") is True
            and confirm.get("minimum_physical_bbox_px") == [18, 18]
            and confirm.get("minimum_outer_frame_stroke_px") == 2
            and confirm.get("minimum_inner_frame_stroke_px") == 2
            and confirm.get("minimum_frame_separation_px") == 2,
            "v5 confirm final-pixel two-frame closure")
    require(len(repair.get("copy_and_interaction_freeze", [])) == 3
            and all(term in " ".join(repair["copy_and_interaction_freeze"])
                    for term in ("exact localized", "leading content inset",
                                 "no wording", "hit rectangle", "distinct token")),
            "v5 copy/interaction freeze")


def validate_cpu_raster_gate(gate: dict[str, Any]) -> None:
    cpu = gate.get("cpu_raster_snapshot_gate", {})
    require(cpu.get("status") == "NOT_RUN"
            and cpu.get("required_profiles") == EXPECTED_PROFILES,
            "v5 exact multi-resolution standard/CVD profiles")
    raster = " ".join(cpu.get("raster_contract", []))
    for term in ("real ProductUiFrame HudDrawList", "final composited pixels",
                 "token-attributed role masks", "protanopia", "deuteranopia",
                 "tritanopia", "No token declaration", "triangle count"):
        require(term in raster, "v5 raster contract missing: " + term)
    checks = cpu.get("required_final_pixel_checks_per_profile", {})
    require(checks == {
        "row_outline_contrast_ratio_minimum": 4.0,
        "focus_joined_independent_component_count": 1,
        "focus_spans_upper_and_lower_slot_regions": True,
        "focus_text_mask_intersection_pixels": 0,
        "selected_visible_closed_outline_count_minimum": 1,
        "selected_check_inside_closed_outline": True,
        "confirm_visible_nested_closed_frame_count_minimum": 2,
        "confirm_arrow_inside_innermost_frame": True,
        "three_role_masks_pairwise_non_overlapping": True,
        "final_visible_role_pixel_survival_required": True,
        "minimum_role_physical_bbox_px": [18, 18],
        "minimum_role_stroke_px": 2,
    }, "v5 exact final-pixel topology checks")
    topology = " ".join(cpu.get("topology_algorithm_requirements", []))
    for term in ("Connected-component", "border flood-fill", "two nested",
                 "accent-on-accent", "alpha zero", "clipping",
                 "bare glyph", "stable SHA aggregates"):
        require(term in topology, "v5 topology algorithm missing: " + term)
    require(cpu.get("required_snapshot_cases") == REPAIR_CASES + CONTROLS,
            "v5 exact 4 repair + 12 control CPU snapshots")
    require(cpu.get("negative_fixtures_required") == NEGATIVE_FIXTURES,
            "v5 exact fail-closed raster negative fixtures")


def validate_boundaries_and_future(gate: dict[str, Any]) -> None:
    allowed = gate.get("allowed_modification_boundary", {})
    production = allowed.get("production_files", [])
    require([row.get("path") for row in production] == [
        "src/Billiards/render/ui/rules_decision_ui_draw_list.cpp",
        "src/Billiards/render/ui/rules_decision_product_draw.cpp",
    ] and all(row.get("allowed") for row in production)
        and allowed.get("all_unlisted_production_files_forbidden") is True
        and len(allowed.get("test_and_evidence_categories", [])) == 3,
        "v5 minimal production/test allowlist")
    forbidden = json.dumps(gate.get("forbidden_changes", {}),
                           ensure_ascii=False)
    for term in ("authority", "actor/action wording", "ghost cue-ball",
                 "No inherited contrast", "supplemental and stricter",
                 "product exposure", "real-input", "screen-reader"):
        require(term in forbidden, "v5 forbidden boundary missing: " + term)
    machine = gate.get("machine_and_static_gate", {})
    require(machine.get("status") == "NOT_RUN"
            and machine.get("required_case_count") == 540
            and "Two clean offline runs" in
                machine.get("required_reproducibility", "")
            and len(machine.get("requirements", [])) == 5,
            "v5 540 machine/static reproducibility gate")
    future = gate.get("future_targeted_actual_gpu_gate", {})
    require(future.get("status") == "NOT_AUTHORIZED"
            and future.get("authorization_required") is True
            and future.get("minimum_case_count") == 16
            and future.get("repair_case_ids") == REPAIR_CASES
            and future.get("regression_control_case_ids") == CONTROLS
            and len(future.get("preconditions", [])) == 3
            and len(future.get("capture_requirements", [])) == 3,
            "v5 future GPU exact 4+12 not-authorized gate")
    ta = gate.get("future_technical_art_gate", {})
    require(ta.get("status") == "NOT_RUN"
            and ta.get("image_count") == 16
            and ta.get("canonical_axes") == CANONICAL_AXES
            and ta.get("axis_review_count") == 160
            and ta.get("repair_axis_occurrence_count") == 8
            and len(ta.get("requirements", [])) == 3,
            "v5 future independent TA 16x10 gate")
    boundary = gate.get("evidence_boundary", {})
    require(boundary.get("design_contract_only") is True
            and boundary.get("implementation_authorized") is True,
            "v5 design/implementation authorization identity")
    for field in (
            "implementation_performed_by_this_gate",
            "production_source_modified_by_this_gate",
            "shipping_asset_modified_by_this_gate", "cpu_raster_pass_claimed",
            "machine_pass_claimed", "gpu_execution_authorized",
            "gpu_capture_performed", "technical_art_pass_claimed",
            "scoped_visual_repair_pass", "product_exposure_authorized",
            "product_exposed", "golden_promotion", "m2_promotion",
            "beta_pass", "real_input_pass", "screen_reader_pass",
            "external_human_pass"):
        require(boundary.get(field) is False,
                "v5 design-only evidence boundary: " + field)


def validate(path: Path = DEFAULT_GATE) -> dict[str, Any]:
    gate = load_json(path.resolve(strict=True))
    require(gate.get("schema") ==
            "phase5_rules_decision_ui_v5_pixel_visible_shape_"
            "repair_design_gate_v1"
            and gate.get("schema_version") == 1
            and gate.get("status") ==
            "APPROVED_FOR_SCOPED_V5_PIXEL_VISIBLE_IMPLEMENTATION_ONLY",
            "v5 gate identity/status")
    validate_read_only_bindings(gate)
    validate_scope_and_repair(gate)
    validate_cpu_raster_gate(gate)
    validate_boundaries_and_future(gate)
    return {
        "status": "PASS_V5_PIXEL_VISIBLE_SHAPE_REPAIR_DESIGN_GATE",
        "failure_receipt_sha256": FAILURE_RECEIPT_SHA256,
        "root_cause_report_sha256": ROOT_CAUSE_SHA256,
        "repair_cases": 4,
        "repair_axis_occurrences": 8,
        "cpu_raster_profiles_required": 4,
        "cpu_snapshot_cases_required": 16,
        "machine_cases_required": 540,
        "future_targeted_gpu_cases_required": 16,
        "future_ta_axes_required": 160,
        "production_modified": False,
        "gpu_authorized": False,
        "product_exposed": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--gate", type=Path, default=DEFAULT_GATE)
    arguments = parser.parse_args()
    result = validate(arguments.gate)
    print(json.dumps(result, ensure_ascii=False, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
