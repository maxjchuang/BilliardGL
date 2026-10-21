#!/usr/bin/env python3
"""Validate the Phase5 Rules UI v4 exact-shape correction design gate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GATE = ROOT / (
    "docs/art/phase5-rules-decision-ui-v4-exact-shape-"
    "correction-design-gate-v1.json"
)
MANUAL_PATH = (
    "docs/evidence/phase5/rules-decision-ui-v3-binding-corrected-"
    "targeted28-independent-ta-manual-review-v1.json"
)
MANUAL_SHA256 = (
    "f044e39c7765f3026bd53521e7cca8cf11b619ca5e6369c6e37e338da0afb954"
)
V3_GATE_PATH = (
    "docs/art/phase5-rules-decision-ui-v3-visual-repair-design-gate-v1.json"
)
V3_GATE_SHA256 = (
    "6b587c67721521607d81a28beaafa59ad4a76d664399805e922ecdc969905e25"
)
MACHINE_RECEIPT_PATH = (
    "docs/evidence/phase5-rules-decision-ui-v3-binding-corrected-"
    "targeted-machine-pass-receipt-v1.json"
)
MACHINE_RECEIPT_SHA256 = (
    "c7be73e9759dca152da94108efd1a87893242d8d4c72b6362c66e002723e1c4c"
)
MANIFEST_PATH = (
    "docs/evidence/phase5/rules-decision-ui-v3-binding-corrected-"
    "targeted-gpu-run-1/rules-decision-ui-v3-targeted-evidence.json"
)
MANIFEST_SHA256 = (
    "b7dc8fcc3ee266b8e8b30f818a8df98aade195efeb08774a217f0db1b833d612"
)
CASE_AGGREGATE = (
    "ac9288a18896384b0f2a40ed8f000e3b7b3830e5ccd391d21b1c3c982700f7e4"
)
OUTPUT_AGGREGATE = (
    "741ac3a09b678ffc7b2f355e11248172195ee58752d5561418bda209c3a89284"
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
ALLOWED_CATEGORIES = [
    "rules_decision_scoped_shape_token_definition",
    "rules_decision_armed_confirm_draw_list_emission",
    "rules_decision_product_ui_shape_rasterization",
    "offline_validation_and_test_scaffolding",
]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(value, dict), f"{path}: object required")
    return value


def validate_binding(path: str, digest: str, label: str) -> dict[str, Any]:
    file_path = (ROOT / path).resolve(strict=True)
    require(file_path.is_file() and sha256(file_path) == digest,
            label + ": byte binding mismatch")
    return load_json(file_path)


def validate_read_only_bindings(gate: dict[str, Any]) -> None:
    require(gate.get("read_only_bindings") == [
        {"role": "v3_corrected_targeted28_final_manual_failure_authority",
         "path": MANUAL_PATH, "sha256": MANUAL_SHA256},
        {"role": "v3_visual_repair_design_gate",
         "path": V3_GATE_PATH, "sha256": V3_GATE_SHA256},
        {"role": "binding_corrected_targeted_machine_pass_receipt",
         "path": MACHINE_RECEIPT_PATH, "sha256": MACHINE_RECEIPT_SHA256,
         "case_evidence_aggregate_sha256": CASE_AGGREGATE,
         "output_aggregate_sha256": OUTPUT_AGGREGATE},
        {"role": "binding_corrected_targeted_actual_gpu_manifest",
         "path": MANIFEST_PATH, "sha256": MANIFEST_SHA256},
    ], "exact four read-only bindings")
    manual = validate_binding(MANUAL_PATH, MANUAL_SHA256, "final manual")
    require(manual.get("schema") ==
            "phase5_rules_decision_ui_v3_binding_corrected_targeted28_"
            "independent_ta_manual_review_v1"
            and manual.get("overall_result") == "FAIL"
            and manual.get("review_counts") == {
                "cases_total": 28, "cases_reviewed": 28,
                "cases_pass": 24, "cases_fail": 4, "cases_pending": 0,
                "axes_total": 280, "axes_pass": 272,
                "axes_fail": 8, "axes_pending": 0}
            and manual.get("failed_case_ids") == REPAIR_CASES,
            "bound final manual exact 4-case/8-axis failure")
    for case in manual.get("cases", [])[:4]:
        require(case.get("case_id") in REPAIR_CASES
                and case.get("blocking_axes") == FAILED_AXES,
                "bound final manual failed-axis mapping")
    old_gate = validate_binding(V3_GATE_PATH, V3_GATE_SHA256, "v3 gate")
    require(old_gate.get("product_exposed") is False
            and old_gate.get("beta_pass") is False,
            "v3 gate exposure boundary")
    receipt = validate_binding(
        MACHINE_RECEIPT_PATH, MACHINE_RECEIPT_SHA256, "machine receipt")
    require(receipt.get("claims", {}).get("targeted_machine_28_pass") is True
            and receipt.get("stable_aggregates", {}).get(
                "case_evidence_aggregate_sha256") == CASE_AGGREGATE
            and receipt.get("stable_aggregates", {}).get(
                "output_aggregate_sha256") == OUTPUT_AGGREGATE
            and receipt.get("claims", {}).get("product_exposed") is False,
            "machine receipt aggregates/boundary")
    manifest = validate_binding(MANIFEST_PATH, MANIFEST_SHA256, "manifest")
    require(manifest.get("case_count") == 28
            and manifest.get("status") ==
            "TARGETED_GPU_MACHINE_EVIDENCE_TA_REVIEW_PENDING",
            "manifest identity/count")


def validate_failure_scope(gate: dict[str, Any]) -> None:
    failed = gate.get("bound_failure_set", {})
    require(failed.get("manual_result") == "FAIL"
            and failed.get("failed_case_count") == 4
            and failed.get("failed_axis_occurrence_count") == 8
            and failed.get("failed_case_ids") == REPAIR_CASES
            and failed.get("failed_axes_per_case") == FAILED_AXES
            and failed.get("blocking_defect_id") ==
            "D06_ARMED_FOCUS_CONFIRM_EXACT_SHAPES_ABSENT",
            "exact bound 4-case/8-axis failure scope")
    repair = gate.get("repair_contract", {})
    require(repair.get("allowed_state_ids") == [
                "group_choice_armed_confirm"]
            and repair.get("allowed_case_ids") == REPAIR_CASES
            and repair.get("allowed_failed_axes") == FAILED_AXES
            and repair.get("repair_axis_occurrence_count") == 8,
            "exact allowed repair scope")
    copy = repair.get("copy_freeze", {})
    require(len(copy.get("requirements", [])) == 3
            and all(term in json.dumps(copy, ensure_ascii=False)
                    for term in ("actor", "action", "localization",
                                 "submission")),
            "corrected actor/action copy freeze")


def validate_exact_shapes(gate: dict[str, Any]) -> None:
    shapes = gate.get("repair_contract", {}).get("exact_shape_tokens", {})
    require(list(shapes) == [
        "focus_chevron", "selected_check_diamond",
        "confirm_double_frame_plus_enter_arrow"],
        "exact three-shape inventory/order")
    focus = shapes["focus_chevron"]
    selected = shapes["selected_check_diamond"]
    confirm = shapes["confirm_double_frame_plus_enter_arrow"]
    token_ids = [shape.get("token_id") for shape in shapes.values()]
    require(len(set(token_ids)) == 3 and all(token_ids),
            "three distinct non-aliased token IDs")
    require(focus.get("minimum_physical_bounding_box_px") == [18, 18]
            and focus.get("minimum_physical_stroke_px") == 2
            and focus.get("hud_diamond_confusion_forbidden") is True
            and "leading-edge chevron" in focus.get("required_geometry", "")
            and "Player HUD" in focus.get("placement", ""),
            "focus chevron geometry/pixel/HUD distinction")
    require(selected.get("minimum_physical_bounding_box_px") == [18, 18]
            and selected.get("minimum_outline_stroke_px") == 2
            and selected.get("minimum_check_stroke_px") == 2
            and selected.get("minimum_check_to_outline_clearance_px") == 2
            and selected.get("closed_diamond_outline_required") is True
            and "fully enclosed" in selected.get("required_geometry", ""),
            "selected check-diamond enclosure/pixels")
    require(confirm.get("minimum_physical_bounding_box_px") == [18, 18]
            and confirm.get("minimum_outer_frame_stroke_px") == 2
            and confirm.get("minimum_inner_frame_stroke_px") == 2
            and confirm.get("minimum_frame_separation_px") == 2
            and confirm.get("minimum_enter_arrow_stroke_px") == 2
            and confirm.get("two_closed_rectangular_frames_required") is True
            and confirm.get("bent_enter_arrow_required") is True
            and "Two closed nested rectangular frames" in
            confirm.get("required_geometry", ""),
            "confirm double-frame enter-arrow geometry/pixels")
    simultaneous = gate.get("repair_contract", {}).get(
        "simultaneous_capture_contract", [])
    require(len(simultaneous) == 4
            and all(term in " ".join(simultaneous) for term in (
                "all three", "leading focus chevron",
                "enclosed check-diamond", "double-frame enter-arrow",
                "distinct token_id", "Standard", "color_vision_safe")),
            "simultaneous independent standard/CVD shape contract")


def validate_boundaries_and_validation(gate: dict[str, Any]) -> None:
    thresholds = gate.get("inherited_thresholds_immutable", {})
    require(thresholds.get("normal_text_contrast_minimum") == 4.5
            and thresholds.get(
                "large_text_and_active_shape_contrast_minimum") == 3.0
            and thresholds.get("critical_shape_minimum_physical_px") == 18
            and thresholds.get("focus_stroke_minimum_physical_px") == 2
            and thresholds.get("action_minimum_logical_height_px") == 48
            and thresholds.get("table_occlusion_limit_fraction") == 0.18
            and thresholds.get("modification_allowed") is False,
            "inherited thresholds immutable")
    categories = gate.get("allowed_modification_categories", [])
    require([item.get("category") for item in categories] ==
            ALLOWED_CATEGORIES and all(item.get("boundary") for item in categories),
            "exact four allowed modification categories")
    forbidden_text = json.dumps(gate.get("forbidden_changes", {}),
                                ensure_ascii=False)
    for term in ("authority", "corrected actor/action copy", "threshold",
                 "ghost cue-ball", "Player HUD diamond", "product exposure",
                 "screen-reader"):
        require(term in forbidden_text, "forbidden boundary missing: " + term)
    machine = gate.get("cpu_and_machine_gate", {})
    require(machine.get("status") == "NOT_RUN"
            and machine.get("required_case_count") == 540
            and len(machine.get("required_checks", [])) == 6
            and "Two clean offline runs" in
            machine.get("required_reproducibility", "")
            and len(machine.get(
                "implementation_preflight_before_any_gpu_request", [])) == 3,
            "CPU/540 machine/reproducibility gate")
    targeted = gate.get("future_targeted_actual_gpu_gate", {})
    require(targeted.get("status") == "NOT_AUTHORIZED"
            and targeted.get("authorization_required") is True
            and targeted.get("minimum_case_count") == 16
            and targeted.get("repair_case_ids") == REPAIR_CASES
            and targeted.get("regression_control_case_ids") == CONTROLS
            and len(targeted.get("capture_requirements", [])) == 4,
            "future exact 4+12 targeted actual-GPU gate")
    ta = gate.get("technical_art_re_review_gate", {})
    require(ta.get("status") == "NOT_RUN"
            and ta.get("canonical_axes") == CANONICAL_AXES
            and ta.get("image_count") == 16
            and ta.get("axis_review_count") == 160
            and ta.get("repair_axis_occurrence_count") == 8
            and ta.get("required_result") == {
                "images_reviewed": 16, "axes_reviewed": 160,
                "axes_pending": 0,
                "prior_failed_axis_occurrences_remaining": 0,
                "new_axis_failures": 0,
                "overall":
                "PASS_REQUIRED_FOR_V4_EXACT_SHAPE_RASTER_GATE_ONLY"},
            "TA 16-image/160-axis/8-repair-axis gate")
    boundary = gate.get("evidence_boundary", {})
    require(boundary == {
        "design_contract_only": True,
        "implementation_authorized": True,
        "implementation_performed_by_this_gate": False,
        "production_source_modified_by_this_gate": False,
        "shipping_asset_modified_by_this_gate": False,
        "machine_pass_claimed_by_this_gate": False,
        "gpu_execution_authorized": False,
        "gpu_capture_performed_by_this_gate": False,
        "technical_art_re_review_claimed_by_this_gate": False,
        "scoped_visual_repair_gate_pass": False,
        "product_exposure_authorized": False,
        "product_exposed": False,
        "golden_promotion": False,
        "m2_promotion": False,
        "beta_pass": False,
        "true_keyboard_or_pointer_session_claimed": False,
        "screen_reader_session_claimed": False,
        "external_human_review_claimed": False,
    }, "design-only no-GPU/no-production/no-exposure boundary")


def validate(path: Path = DEFAULT_GATE) -> dict[str, Any]:
    path = path.resolve(strict=True)
    gate = load_json(path)
    require(gate.get("schema") ==
            "phase5_rules_decision_ui_v4_exact_shape_"
            "correction_design_gate_v1"
            and gate.get("schema_version") == 1
            and gate.get("status") ==
            "APPROVED_FOR_SCOPED_EXACT_SHAPE_IMPLEMENTATION_ONLY",
            "v4 gate schema/version/status")
    validate_read_only_bindings(gate)
    validate_failure_scope(gate)
    validate_exact_shapes(gate)
    validate_boundaries_and_validation(gate)
    return {
        "status": "PASS_V4_EXACT_SHAPE_CORRECTION_DESIGN_GATE",
        "gate_sha256": sha256(path),
        "bound_manual_sha256": MANUAL_SHA256,
        "repair_cases": 4,
        "repair_axis_occurrences": 8,
        "machine_cases_required": 540,
        "future_targeted_cases_required": 16,
        "future_ta_axes_required": 160,
        "implementation_performed": False,
        "gpu_authorized": False,
        "gpu_executed": False,
        "production_modified": False,
        "product_exposed": False,
        "golden_promotion": False,
        "m2_promotion": False,
        "beta_pass": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--gate", type=Path, default=DEFAULT_GATE)
    arguments = parser.parse_args()
    try:
        print(json.dumps(validate(arguments.gate), indent=2, sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
