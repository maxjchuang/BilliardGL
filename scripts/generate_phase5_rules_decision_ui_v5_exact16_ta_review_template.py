#!/usr/bin/env python3
"""Generate the unexecuted Phase5 Rules UI v5 exact16 TA template."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / (
    "docs/art/phase5-rules-decision-ui-v5-exact16-"
    "independent-ta-review-template-v1.json"
)
V4_MANUAL_PATH = (
    "docs/evidence/phase5/rules-decision-ui-v4-exact16-"
    "independent-ta-manual-review-v1.json"
)
V4_MANUAL_SHA256 = (
    "8847ae7af29ee16cba968f2c23704fe7968cdc113581877c7d4bf3a291381014"
)
V5_GATE_PATH = (
    "docs/art/phase5-rules-decision-ui-v5-pixel-visible-"
    "shape-repair-design-gate-v1.json"
)
V5_GATE_SHA256 = (
    "657f3ab6f4ccac4db23fb0ab72889bc49d091f27fd5f63bd5610b736b1aa2a10"
)
PREORDINAL_FAILURE_PATH = (
    "docs/evidence/phase5/rules-decision-ui-v5-exact16-"
    "preordinal-contract-failure-receipt-v1.json"
)
PREORDINAL_FAILURE_SHA256 = (
    "4d261779bdcb4c9efa938147267425bfd3fb20685cd57b5482ca91cb79ddd588"
)
PRODUCER_GATE_PATH = (
    "docs/art/phase5-rules-decision-ui-v5-exact16-"
    "producer-contract-correction-gate-v1.json"
)
PRODUCER_GATE_SHA256 = (
    "b0bf1808f381b2dc5bc7a89bd8001ee08565f95227d0194db771722187df336a"
)

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
REPAIR_CASE_IDS = [
    "group_choice_armed_confirm--p1--zh_cn",
    "group_choice_armed_confirm--p2--en",
    "group_choice_armed_confirm--p3--zh_cn",
    "group_choice_armed_confirm--p4--en",
]
CONTROL_CASE_IDS = [
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
CASE_IDS = REPAIR_CASE_IDS + CONTROL_CASE_IDS
PROFILE_DIMENSIONS = {
    "p1": [1280, 720], "p2": [1280, 720],
    "p3": [1920, 1080], "p4": [2560, 1440],
}
PRIOR_BLOCKING_AXES = [
    "focus_pointer_keyboard_redundancy",
    "standard_cvd_non_color_redundancy",
]
REPAIR_REQUIRED_AXES = [
    "action_semantics_and_branch_correctness",
    "actor_revision_and_authority_status_readability",
    "no_default_selection_or_accidental_submission_affordance",
    "focus_pointer_keyboard_redundancy",
    "text_clipping_collision_and_minimum_size",
    "standard_cvd_non_color_redundancy",
]
GROUP_OPEN_AXES = [
    "no_default_selection_or_accidental_submission_affordance",
    "focus_pointer_keyboard_redundancy",
    "standard_cvd_non_color_redundancy",
]
SHARED_ACTION_AXES = [
    "action_semantics_and_branch_correctness",
    "no_default_selection_or_accidental_submission_affordance",
    "standard_cvd_non_color_redundancy",
    "foundation_and_unoffered_action_absence",
]
STATUS_AXES = [
    "actor_revision_and_authority_status_readability",
    "standard_cvd_non_color_redundancy",
    "stale_error_ai_pending_fail_closed_clarity",
]
PLACEMENT_AXES = [
    "action_semantics_and_branch_correctness",
    "text_clipping_collision_and_minimum_size",
    "standard_cvd_non_color_redundancy",
    "placement_scope_and_legality_readability",
]


def profile(case_id: str) -> str:
    return case_id.split("--")[1]


def control_axes(case_id: str) -> list[str]:
    if case_id.startswith("group_choice_open_unselected--"):
        return GROUP_OPEN_AXES
    if (case_id.startswith("eight_legal_open_unselected--") or
            case_id.startswith("break_illegal_open_unselected--")):
        return SHARED_ACTION_AXES
    if (case_id.startswith("submitting_progress--") or
            case_id.startswith("authority_error--")):
        return STATUS_AXES
    return PLACEMENT_AXES


def control_visual_contract(case_id: str) -> list[str]:
    if case_id.startswith("group_choice_open_unselected--"):
        return [
            "No focus-chevron, selected-check-diamond, or confirm-double-frame token leaks into the open/unselected state.",
            "No default selection or accidental submission affordance appears in standard or CVD presentation.",
        ]
    if (case_id.startswith("eight_legal_open_unselected--") or
            case_id.startswith("break_illegal_open_unselected--")):
        return [
            "Shared action copy/order/foundation behavior remains correct and no v5 armed-confirm shape leaks.",
            "Standard/CVD non-color action separation and no-default-selection behavior remain unchanged.",
        ]
    if case_id.startswith("submitting_progress--"):
        return [
            "Submitting/progress status remains readable, blocking, and free of armed-confirm shape leakage.",
            "Authority/revision/status and standard/CVD presentation show no regression.",
        ]
    if case_id.startswith("authority_error--"):
        return [
            "Authority-error state remains readable, fail-closed, and free of armed-confirm shape leakage.",
            "Actor/revision/status and standard/CVD error presentation show no regression.",
        ]
    if case_id.startswith("full_table_preview_valid--"):
        return [
            "World overlay shows ghost cue-ball plus crosshair and no invalid octagon/dashed collision ring.",
            "Placement scope, legality, coordinates, confirm enablement, copy, and CVD geometry remain readable.",
        ]
    if case_id.startswith("full_table_preview_invalid--"):
        return [
            "World overlay shows ghost cue-ball plus error-cross-octagon and dashed collision ring, with no valid crosshair.",
            "Invalid placement scope, legality, coordinates, disabled confirm, copy, and CVD geometry remain readable.",
        ]
    if case_id.startswith("behind_head_string_preview_valid--"):
        return [
            "World overlay shows ghost cue-ball, crosshair, diagonal hatch, head-string boundary, and bilingual in-world region/coordinates.",
            "Behind-head valid scope, legality, confirm enablement, copy, and CVD geometry remain readable.",
        ]
    return [
        "World overlay shows ghost cue-ball, error-cross-octagon, dashed collision ring, diagonal hatch, head-string boundary, and bilingual in-world region/coordinates.",
        "Behind-head invalid scope, legality, disabled confirm, copy, and CVD geometry remain readable.",
    ]


REPAIR_VISUAL_CONTRACT = {
    "leading_focus_chevron": (
        "An independently visible, joined two-arm leading focus chevron occupies "
        "its reserved slot, spans upper/lower slot regions, and does not merge "
        "with text or a Player HUD diamond."),
    "selected_closed_diamond_plus_check": (
        "A visibly closed four-sided diamond outline encloses the selected check; "
        "a bare check or open outline is FAIL."),
    "confirm_double_frame_plus_bent_arrow": (
        "Two visibly closed nested rectangular frames enclose the bent enter-arrow; "
        "a bare arrow, open frame, or single frame is FAIL."),
    "exact_actor_action_copy": (
        "Armed and confirm rows retain the exact localized actor and exact action "
        "result with no truncation, collision, stale actor, or wording change."),
    "focus_submission_semantics": (
        "Focus remains on the explicit confirm control without submission; first "
        "activation arms only and only the separate confirm activation submits."),
    "standard_cvd_non_color_geometry": (
        "All three shapes remain independently recognizable from geometry in "
        "standard and color_vision_safe presentation without relying on hue."),
}
STABLE_AGGREGATE_ALGORITHM = (
    "sha256_canonical_json_utf8_v1_ordered_exact16_ppm_png_dimensions"
)


def build_case(ordinal: int, case_id: str) -> dict[str, Any]:
    repair = case_id in REPAIR_CASE_IDS
    return {
        "ordinal": ordinal,
        "case_id": case_id,
        "coverage_kind": "REPAIR" if repair else "REGRESSION_CONTROL",
        "profile_id": profile(case_id),
        "expected_physical_dimensions": PROFILE_DIMENSIONS[profile(case_id)],
        "prior_blocking_axes": PRIOR_BLOCKING_AXES if repair else [],
        "required_review_axes": (
            REPAIR_REQUIRED_AXES if repair else control_axes(case_id)),
        "required_visual_contract": (
            list(REPAIR_VISUAL_CONTRACT.values()) if repair
            else control_visual_contract(case_id)),
        "future_evidence": {
            "ppm_path": None,
            "ppm_sha256": None,
            "ppm_physical_dimensions": None,
            "png_path": None,
            "png_sha256": None,
            "png_physical_dimensions": None,
        },
        "manual_case_result": "NOT_RUN",
        "blocking_axes": [],
        "axis_findings": {
            axis: {"status": "PENDING", "observation": None}
            for axis in CANONICAL_AXES
        },
    }


def build_template() -> dict[str, Any]:
    cases = [build_case(index, case_id)
             for index, case_id in enumerate(CASE_IDS, start=1)]
    return {
        "schema": (
            "phase5_rules_decision_ui_v5_exact16_"
            "independent_ta_review_v1"),
        "schema_version": 1,
        "status": "TEMPLATE_NOT_RUN_WAITING_FOR_SUCCESSOR_EVIDENCE",
        "overall_result": "NOT_RUN",
        "review_scope": {
            "case_ids_in_order": CASE_IDS,
            "repair_case_ids_in_order": REPAIR_CASE_IDS,
            "regression_control_case_ids_in_order": CONTROL_CASE_IDS,
            "case_count": 16,
            "repair_case_count": 4,
            "regression_control_case_count": 12,
            "canonical_axes_in_order": CANONICAL_AXES,
            "axes_per_case": 10,
            "total_axis_count": 160,
            "prior_blocking_axis_occurrence_count": 8,
            "original_physical_resolution_required": True,
        },
        "frozen_bindings": {
            "v4_independent_ta_manual": {
                "path": V4_MANUAL_PATH, "sha256": V4_MANUAL_SHA256},
            "v5_pixel_visible_design_gate": {
                "path": V5_GATE_PATH, "sha256": V5_GATE_SHA256},
            "v5_preordinal_failure_receipt": {
                "path": PREORDINAL_FAILURE_PATH,
                "sha256": PREORDINAL_FAILURE_SHA256},
            "v5_producer_contract_correction_gate": {
                "path": PRODUCER_GATE_PATH,
                "sha256": PRODUCER_GATE_SHA256},
        },
        "future_successor_evidence": {
            "readiness": {
                "status": "NOT_AVAILABLE", "path": None, "sha256": None,
                "required_schema":
                    "phase5_rules_decision_ui_v5_exact16_readiness_v2",
                "required_status":
                    "V5_PIXEL_VISIBLE_EXACT16_CORRECTED_PRODUCER_NOT_RUN_NO_AUTH"},
            "machine_receipt": {
                "status": "NOT_AVAILABLE", "path": None, "sha256": None,
                "required_schema":
                    "phase5_rules_decision_ui_v5_exact16_machine_pass_receipt_v1",
                "required_status": "TARGETED16_MACHINE_PASS",
                "required_case_count": 16,
                "required_machine_pass_count": 16,
                "required_failure_count": 0},
            "stable_png_manifest": {
                "status": "NOT_AVAILABLE", "path": None, "sha256": None,
                "required_schema":
                    "phase5_rules_decision_ui_v5_exact16_stable_png_manifest_v1"},
            "ppm_aggregate_sha256": None,
            "png_aggregate_sha256": None,
            "stable_aggregate_sha256": None,
            "aggregate_algorithm": STABLE_AGGREGATE_ALGORITHM,
        },
        "completion_preconditions": [
            "A fresh successor v2 readiness is frozen and bound by repository-relative path and SHA-256.",
            "A future machine receipt binds that readiness and reports exact 16/16 targeted machine PASS with zero failed or unrun cases.",
            "A stable PNG manifest and all 16 ordered PPM+PNG path/SHA/decoded physical-dimension bindings are complete.",
            "PPM, PNG, and combined stable aggregates are independently recomputed over the exact ordered 16-case inventory.",
            "Every repair image is opened at original resolution and its leading chevron, closed diamond+check, double frame+bent arrow, exact copy/focus semantics, and standard/CVD geometry are explicitly observed.",
            "All 12 controls are opened at original resolution and every required regression axis, including all four placement controls, is explicitly observed.",
            "Only after every evidence precondition passes may all 160 axes atomically change from PENDING to PASS or FAIL; partial review state is forbidden.",
        ],
        "allowed_statuses": {
            "template_case": ["NOT_RUN"],
            "template_axis": ["PENDING"],
            "completed_case": ["PASS", "FAIL"],
            "completed_axis": ["PASS", "FAIL"],
        },
        "cases": cases,
        "review_counts": {
            "cases_total": 16,
            "repair_cases_total": 4,
            "regression_control_cases_total": 12,
            "cases_not_run": 16,
            "cases_pass": 0,
            "cases_fail": 0,
            "axes_total": 160,
            "axes_pending": 160,
            "axes_pass": 0,
            "axes_fail": 0,
            "prior_blocking_axis_occurrences_total": 8,
            "prior_blocking_axis_occurrences_pass": 0,
            "prior_blocking_axis_occurrences_fail": 0,
        },
        "failed_case_ids": [],
        "evidence_boundary": {
            "template_only": True,
            "successor_readiness_bound": False,
            "machine_receipt_bound": False,
            "stable_png_manifest_bound": False,
            "ppm_bound_count": 0,
            "png_bound_count": 0,
            "stable_aggregate_bound": False,
            "human_ta_review_performed": False,
            "targeted16_ta_pass": False,
            "authorization_created_by_template": False,
            "gpu_executed_by_template": False,
            "production_modified": False,
            "product_exposed": False,
            "golden_promotion": False,
            "m2_promotion": False,
            "beta_pass": False,
            "real_input_pass": False,
            "screen_reader_pass": False,
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    arguments = parser.parse_args()
    output = arguments.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(build_template(), ensure_ascii=False,
                                 indent=2) + "\n", encoding="utf-8")
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
