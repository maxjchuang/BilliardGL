#!/usr/bin/env python3
"""Generate the unexecuted Phase5 Rules UI v3 targeted28 TA template."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / (
    "docs/art/phase5-rules-decision-ui-v3-targeted28-"
    "independent-ta-review-template-v1.json"
)
VISUAL_REPAIR_GATE_PATH = (
    "docs/art/phase5-rules-decision-ui-v3-visual-repair-design-gate-v1.json"
)
VISUAL_REPAIR_GATE_SHA256 = (
    "6b587c67721521607d81a28beaafa59ad4a76d664399805e922ecdc969905e25"
)
FAILURE_RECEIPT_PATH = (
    "docs/evidence/phase5-rules-decision-ui-v3-targeted-"
    "preordinal-failure-receipt-v1.json"
)
FAILURE_RECEIPT_SHA256 = (
    "f29fe3ee3ca206eed63a8a9f7924f499f99e531b874c04b8dcdcdf61f9e93a8a"
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
    "full_table_preview_valid--p1--en",
    "full_table_preview_valid--p2--zh_cn",
    "full_table_preview_valid--p3--en",
    "full_table_preview_valid--p4--zh_cn",
    "full_table_preview_invalid--p1--zh_cn",
    "full_table_preview_invalid--p2--en",
    "full_table_preview_invalid--p3--zh_cn",
    "full_table_preview_invalid--p4--en",
    "behind_head_string_preview_valid--p1--en",
    "behind_head_string_preview_valid--p2--zh_cn",
    "behind_head_string_preview_valid--p3--en",
    "behind_head_string_preview_valid--p4--zh_cn",
    "behind_head_string_preview_invalid--p1--zh_cn",
    "behind_head_string_preview_invalid--p2--en",
    "behind_head_string_preview_invalid--p3--zh_cn",
    "behind_head_string_preview_invalid--p4--en",
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
]

CASE_IDS = REPAIR_CASE_IDS + CONTROL_CASE_IDS
PROFILE_PHYSICAL_DIMENSIONS = {
    "p1": [1280, 720],
    "p2": [1280, 720],
    "p3": [1920, 1080],
    "p4": [2560, 1440],
}

ARMED_REPAIR_AXES = [
    "action_semantics_and_branch_correctness",
    "no_default_selection_or_accidental_submission_affordance",
    "focus_pointer_keyboard_redundancy",
    "standard_cvd_non_color_redundancy",
]
PLACEMENT_REPAIR_AXES = ["placement_scope_and_legality_readability"]
REPAIR_BLOCKING_AXES = {
    case_id: (ARMED_REPAIR_AXES if case_id.startswith(
        "group_choice_armed_confirm--") else PLACEMENT_REPAIR_AXES)
    for case_id in REPAIR_CASE_IDS
}

GROUP_OPEN_REGRESSION_AXES = [
    "no_default_selection_or_accidental_submission_affordance",
    "focus_pointer_keyboard_redundancy",
    "standard_cvd_non_color_redundancy",
]
SHARED_ACTION_REGRESSION_AXES = [
    "action_semantics_and_branch_correctness",
    "no_default_selection_or_accidental_submission_affordance",
    "standard_cvd_non_color_redundancy",
    "foundation_and_unoffered_action_absence",
]
STATUS_REGRESSION_AXES = [
    "actor_revision_and_authority_status_readability",
    "standard_cvd_non_color_redundancy",
    "stale_error_ai_pending_fail_closed_clarity",
]
CONTROL_REGRESSION_AXES = {
    case_id: (
        GROUP_OPEN_REGRESSION_AXES
        if case_id.startswith("group_choice_open_unselected--")
        else SHARED_ACTION_REGRESSION_AXES
        if (case_id.startswith("eight_legal_open_unselected--") or
            case_id.startswith("break_illegal_open_unselected--"))
        else STATUS_REGRESSION_AXES
    )
    for case_id in CONTROL_CASE_IDS
}

STABLE_AGGREGATE_ALGORITHM = (
    "sha256_canonical_json_utf8_v1_of_ordered_case_id_ppm_png_"
    "physical_dimensions"
)


def profile_from_case_id(case_id: str) -> str:
    return case_id.split("--")[1]


def build_template() -> dict[str, Any]:
    cases = []
    for ordinal, case_id in enumerate(CASE_IDS, start=1):
        profile = profile_from_case_id(case_id)
        is_repair = case_id in REPAIR_BLOCKING_AXES
        cases.append({
            "ordinal": ordinal,
            "case_id": case_id,
            "coverage_kind": "REPAIR" if is_repair else "REGRESSION_CONTROL",
            "profile_id": profile,
            "expected_physical_dimensions":
                PROFILE_PHYSICAL_DIMENSIONS[profile],
            "prior_blocking_axes": (
                list(REPAIR_BLOCKING_AXES[case_id]) if is_repair else []),
            "required_regression_axes": (
                [] if is_repair else
                list(CONTROL_REGRESSION_AXES[case_id])),
            "future_evidence": {
                "ppm": {
                    "path": None,
                    "sha256": None,
                    "physical_dimensions": None,
                },
                "png": {
                    "path": None,
                    "sha256": None,
                    "physical_dimensions": None,
                },
            },
            "manual_case_result": "NOT_RUN",
            "blocking_axes": [],
            "axis_findings": {
                axis: {"status": "PENDING", "observation": None}
                for axis in CANONICAL_AXES
            },
        })

    return {
        "schema": (
            "phase5_rules_decision_ui_v3_targeted28_"
            "independent_ta_review_v1"),
        "status": "TEMPLATE_NOT_RUN_WAITING_FOR_TARGETED28_MACHINE_PASS",
        "overall_result": "NOT_RUN",
        "template_version": 1,
        "review_scope": {
            "kind": "phase5_rules_ui_v3_targeted28_independent_ta",
            "case_ids_in_order": CASE_IDS,
            "repair_case_ids_in_order": REPAIR_CASE_IDS,
            "regression_control_case_ids_in_order": CONTROL_CASE_IDS,
            "case_count": 28,
            "repair_case_count": 20,
            "regression_control_case_count": 8,
            "canonical_axes_in_order": CANONICAL_AXES,
            "axis_count_per_case": 10,
            "total_axis_count": 280,
            "prior_failed_axis_occurrence_count": 32,
            "original_physical_resolution_required": True,
        },
        "frozen_bindings": {
            "visual_repair_design_gate": {
                "path": VISUAL_REPAIR_GATE_PATH,
                "sha256": VISUAL_REPAIR_GATE_SHA256,
            },
            "targeted_preordinal_failure_receipt": {
                "path": FAILURE_RECEIPT_PATH,
                "sha256": FAILURE_RECEIPT_SHA256,
            },
        },
        "future_machine_pass_binding": {
            "status": "NOT_AVAILABLE",
            "receipt_path": None,
            "receipt_sha256": None,
            "required_schema": (
                "phase5_rules_decision_ui_v3_targeted28_"
                "machine_pass_receipt_v1"),
            "required_status": "TARGETED28_MACHINE_PASS",
            "required_case_count": 28,
            "required_machine_pass_case_count": 28,
            "required_failed_machine_check_count": 0,
            "required_ppm_count": 28,
            "required_png_count": 28,
            "required_aggregate_algorithm": STABLE_AGGREGATE_ALGORITHM,
            "ppm_aggregate_sha256": None,
            "png_aggregate_sha256": None,
            "stable_aggregate_sha256": None,
        },
        "completion_preconditions": [
            "A future receipt binds both frozen inputs and reports exact targeted 28/28 machine PASS with zero failed checks.",
            "Every case binds existing immutable P6 RGB8 PPM and PNG files by path and raw SHA-256.",
            "Every PPM and PNG physical dimension is independently decoded and equals its p1/p2/p3/p4 profile contract.",
            "PPM, PNG, and combined stable aggregate SHA-256 values are independently recomputed over all 28 ordered cases.",
            "All 20 repair cases review every canonical axis and explicitly cover every gate-bound prior blocking axis.",
            "All eight controls review every canonical axis and explicitly cover their gate-derived regression axes.",
            "Only after every machine, image, dimension, and aggregate precondition passes may all 280 axes change from PENDING to PASS or FAIL.",
            "Partial or mixed NOT_RUN/PENDING/PASS/FAIL review state is forbidden.",
        ],
        "allowed_statuses": {
            "template_case": ["NOT_RUN"],
            "template_axis": ["PENDING"],
            "completed_case": ["PASS", "FAIL"],
            "completed_axis": ["PASS", "FAIL"],
        },
        "cases": cases,
        "review_counts": {
            "cases_total": 28,
            "repair_cases_total": 20,
            "regression_control_cases_total": 8,
            "cases_not_run": 28,
            "cases_pass": 0,
            "cases_fail": 0,
            "axes_total": 280,
            "axes_pending": 280,
            "axes_pass": 0,
            "axes_fail": 0,
            "prior_blocking_axis_occurrences_total": 32,
            "prior_blocking_axis_occurrences_pass": 0,
            "prior_blocking_axis_occurrences_fail": 0,
        },
        "failed_case_ids": [],
        "blocking_axis_occurrences": [],
        "evidence_boundary": {
            "template_only": True,
            "machine_receipt_bound": False,
            "capture_ppm_bound_count": 0,
            "capture_png_bound_count": 0,
            "stable_aggregate_bound": False,
            "human_ta_review_performed": False,
            "targeted28_ta_pass": False,
            "scoped_visual_repair_gate_pass": False,
            "authorization_prepared_by_template_work": False,
            "authorization_issued_by_template_work": False,
            "gpu_executed_by_template_work": False,
            "production_modified": False,
            "m2_promotion": False,
            "golden_promotion": False,
            "product_exposed": False,
            "product_pass": False,
            "beta_pass": False,
            "true_keyboard_or_pointer_session_pass": False,
            "screen_reader_session_pass": False,
        },
    }


def generate(output: Path = DEFAULT_OUTPUT) -> Path:
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(build_template(), indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8")
    return output


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    arguments = parser.parse_args()
    print(generate(arguments.output))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
