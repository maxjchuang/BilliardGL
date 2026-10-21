#!/usr/bin/env python3
"""Generate the frozen Phase5 Rules UI v3 targeted28 TA manual review."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / (
    "docs/evidence/phase5/rules-decision-ui-v3-binding-corrected-"
    "targeted28-independent-ta-manual-review-v1.json"
)
TEMPLATE_PATH = (
    "docs/art/phase5-rules-decision-ui-v3-targeted28-"
    "independent-ta-review-template-v1.json"
)
TEMPLATE_SHA256 = (
    "86dd0731ac58cba60f3b0cf6921881f94d18fa45b3d241ae02b5527d333e261f"
)
GATE_PATH = (
    "docs/art/phase5-rules-decision-ui-v3-visual-repair-design-gate-v1.json"
)
GATE_SHA256 = (
    "6b587c67721521607d81a28beaafa59ad4a76d664399805e922ecdc969905e25"
)
PREORDINAL_FAILURE_PATH = (
    "docs/evidence/phase5-rules-decision-ui-v3-targeted-"
    "preordinal-failure-receipt-v1.json"
)
PREORDINAL_FAILURE_SHA256 = (
    "f29fe3ee3ca206eed63a8a9f7924f499f99e531b874c04b8dcdcdf61f9e93a8a"
)
MACHINE_RECEIPT_PATH = (
    "docs/evidence/phase5-rules-decision-ui-v3-binding-corrected-"
    "targeted-machine-pass-receipt-v1.json"
)
MACHINE_RECEIPT_SHA256 = (
    "c7be73e9759dca152da94108efd1a87893242d8d4c72b6362c66e002723e1c4c"
)
EVIDENCE_ROOT = (
    "docs/evidence/phase5/rules-decision-ui-v3-binding-corrected-"
    "targeted-gpu-run-1"
)
EVIDENCE_MANIFEST_PATH = EVIDENCE_ROOT + \
    "/rules-decision-ui-v3-targeted-evidence.json"
EVIDENCE_MANIFEST_SHA256 = (
    "b7dc8fcc3ee266b8e8b30f818a8df98aade195efeb08774a217f0db1b833d612"
)
RESULTS_JSONL_PATH = EVIDENCE_ROOT + \
    "/rules-decision-ui-v3-targeted-results.jsonl"
RESULTS_JSONL_SHA256 = (
    "0368c8086f68a1f744cd47173d79dbfef8b29ba488c151132c216ea1fa8f49ad"
)
CASE_EVIDENCE_AGGREGATE_SHA256 = (
    "ac9288a18896384b0f2a40ed8f000e3b7b3830e5ccd391d21b1c3c982700f7e4"
)
OUTPUT_AGGREGATE_SHA256 = (
    "741ac3a09b678ffc7b2f355e11248172195ee58752d5561418bda209c3a89284"
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
ARMED_CASE_IDS = REPAIR_CASE_IDS[:4]
ARMED_FAIL_AXES = [
    "focus_pointer_keyboard_redundancy",
    "standard_cvd_non_color_redundancy",
]
PROFILE = {
    "p1": {"palette": "standard", "ui_scale": 0.85,
           "physical_dimensions": [1280, 720]},
    "p2": {"palette": "color_vision_safe", "ui_scale": 1.35,
           "physical_dimensions": [1280, 720]},
    "p3": {"palette": "color_vision_safe", "ui_scale": 1.0,
           "physical_dimensions": [1920, 1080]},
    "p4": {"palette": "standard", "ui_scale": 1.0,
           "physical_dimensions": [2560, 1440]},
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_json(relative: str) -> dict[str, Any]:
    return json.loads((ROOT / relative).read_text(encoding="utf-8"))


def relative_path(value: str) -> str:
    path = Path(value)
    if path.is_absolute():
        return path.resolve().relative_to(ROOT.resolve()).as_posix()
    return path.as_posix()


def observation(case_id: str, axis: str) -> str:
    state, _, _ = case_id.split("--")
    armed = case_id in ARMED_CASE_IDS
    placement = "preview_" in state
    invalid = placement and state.endswith("_invalid")
    behind = placement and state.startswith("behind_head_string")
    if axis == "action_semantics_and_branch_correctness":
        if armed:
            return ("Armed and confirm rows both visibly name Actor Player 1 "
                    "and the exact Choose solids result; Choose stripes remains "
                    "the only other offered branch action.")
        if placement:
            return ("The visible placement action matches the captured full-table "
                    "or behind-head-string branch; invalid capture visibly disables "
                    "confirm while valid capture leaves it available.")
        if state in ("submitting_progress", "authority_error"):
            return ("The status-only surface exposes no actionable branch while "
                    "submission or authority error remains pending/blocked.")
        return ("The original-resolution action inventory and ordering match the "
                "captured authority branch with no extra visible action.")
    if axis == "actor_revision_and_authority_status_readability":
        return ("Actor Player 1 and revision 42 are readable at original physical "
                "resolution; the title and current authority/status message remain "
                "visually distinct.")
    if axis == "no_default_selection_or_accidental_submission_affordance":
        if armed:
            return ("The capture intentionally shows one armed action and a separate "
                    "exact confirm row; the revised copy no longer implies a generic "
                    "or default submission target.")
        if invalid:
            return ("The placement candidate is visibly invalid and its exact confirm "
                    "row is disabled, so no accidental submission affordance is shown.")
        if state in ("submitting_progress", "authority_error"):
            return ("The blocking status surface has no selected action, confirm row, "
                    "or clickable recovery affordance.")
        if placement:
            return ("The valid local preview exposes one explicit placement confirm "
                    "without a selected alternate action or implicit submission cue.")
        return ("All offered rows are visibly unselected at modal open; no armed, "
                "checked, confirm, or submitted state leaks into this control capture.")
    if axis == "focus_pointer_keyboard_redundancy":
        if armed:
            return ("FAIL: row-end symbols remain a bare checkmark and bare right "
                    "arrow; no independent focus_chevron, selected_check_diamond, "
                    "or confirm_double_frame_plus_enter_arrow is visibly formed.")
        return ("Static rows, disabled state, placement marker, or blocking status "
                "remain visually separable at this profile; runtime keyboard/pointer "
                "behavior is not inferred and remains an external NOT_RUN session.")
    if axis == "text_clipping_collision_and_minimum_size":
        return ("At original physical resolution, title, body, actor/revision, action, "
                "coordinate, and region text show no ellipsis, unsafe-edge clipping, "
                "or unreadable glyph collision.")
    if axis == "standard_cvd_non_color_redundancy":
        if armed:
            return ("FAIL: standard/CVD captures still use fills plus bare check/arrow; "
                    "the required selected-check-diamond and confirm-double-frame-plus-"
                    "enter-arrow shapes are absent, so state is not independently "
                    "shape-redundant.")
        if placement:
            return ("Placement meaning is carried by silhouette, crosshair or error "
                    "cross/octagon/dashed ring, hatch/boundary, and readable text; hue "
                    "is supplementary across standard/CVD profiles.")
        return ("Visible state meaning is carried by readable text, luminance, row "
                "structure, and status geometry without relying on hue alone; no new "
                "confirm or invalid token leaks into this control.")
    if axis == "placement_scope_and_legality_readability":
        if not placement:
            return ("This state does not offer placement; absence of a world candidate, "
                    "legality marker, hatch region, and placement confirm is correct.")
        candidate = (
            "ghost cue-ball plus error-cross-octagon and visibly dashed collision ring"
            if invalid else "ghost cue-ball plus orthogonal crosshair")
        suffix = (
            "; diagonal legal-region hatch, continuous head-string boundary, localized "
            "in-world region/scope text, and exact X/Z coordinates are also readable"
            if behind else "; exact in-world X/Z coordinates are also readable")
        return ("The table/world overlay visibly provides " + candidate + suffix + ".")
    if axis == "stale_error_ai_pending_fail_closed_clarity":
        if state == "submitting_progress":
            return ("Submitting ruling is clearly labeled as waiting for exact authority "
                    "acknowledgement and exposes no action while pending.")
        if state == "authority_error":
            return ("Authority error explicitly says the ruling remains blocked and "
                    "requires authority refresh before acting; no action is exposed.")
        return ("No stale/error/AI-pending message is expected in this state, and no "
                "unrelated recovery or pending control is visibly injected.")
    if axis == "foundation_and_unoffered_action_absence":
        return ("No foundation action, debug recovery row, or unoffered authority action "
                "is visible; the surface contains only the exact captured inventory.")
    if axis == "debug_and_product_surface_separation":
        return ("The Rules UI appears in the product_ui pass after ACES tone map with no "
                "capture, proof, diagnostic, or debug-only label/control on the product "
                "surface.")
    raise KeyError(axis)


def build_manual() -> dict[str, Any]:
    manifest = load_json(EVIDENCE_MANIFEST_PATH)
    by_id = {case["capture_id"]: case for case in manifest["cases"]}
    if list(by_id) != CASE_IDS:
        raise RuntimeError("evidence manifest case order drift")
    cases = []
    for ordinal, case_id in enumerate(CASE_IDS, start=1):
        source = by_id[case_id]
        state, profile_id, locale = case_id.split("--")
        failed_axes = ARMED_FAIL_AXES if case_id in ARMED_CASE_IDS else []
        dimensions = [source["png_physical_width"],
                      source["png_physical_height"]]
        cases.append({
            "ordinal": ordinal,
            "case_id": case_id,
            "coverage_kind": (
                "REPAIR" if case_id in REPAIR_CASE_IDS
                else "REGRESSION_CONTROL"),
            "state": state,
            "profile_id": profile_id,
            "locale": locale,
            "palette": PROFILE[profile_id]["palette"],
            "ui_scale": PROFILE[profile_id]["ui_scale"],
            "physical_dimensions": dimensions,
            "actual_gpu_capture": source["actual_gpu_capture"],
            "machine_canonical_sha256": source["canonical_sha256"],
            "ppm": {
                "path": relative_path(source["ppm_path"]),
                "sha256": source["ppm_sha256"],
                "physical_dimensions": dimensions,
            },
            "png": {
                "path": relative_path(source["png_path"]),
                "sha256": source["png_sha256"],
                "physical_dimensions": dimensions,
            },
            "render_passes": source["render_passes"],
            "rules_ui_after_tone_map": source["rules_ui_after_tone_map"],
            "reviewed_original_resolution": True,
            "manual_case_result": "FAIL" if failed_axes else "PASS",
            "blocking_axes": list(failed_axes),
            "axis_findings": {
                axis: {
                    "status": "FAIL" if axis in failed_axes else "PASS",
                    "blocking": axis in failed_axes,
                    "observation": observation(case_id, axis),
                }
                for axis in CANONICAL_AXES
            },
        })
    return {
        "schema": (
            "phase5_rules_decision_ui_v3_binding_corrected_targeted28_"
            "independent_ta_manual_review_v1"),
        "schema_version": 1,
        "status": "MANUAL_VISUAL_REVIEW_FAIL_BLOCKING",
        "overall_result": "FAIL",
        "reviewer": {
            "identity": "codex_subagent_technical_art",
            "role": "independent_technical_art_visual_reviewer",
            "reviewer_type": "AI_AGENT_MANUAL_VISUAL_REVIEW",
            "external_human_reviewer": False,
            "implementation_participation_in_this_review": False,
            "gpu_or_production_modification_in_this_review": False,
        },
        "review_method": {
            "all_images_opened_individually_at_original_resolution": True,
            "inspection_format": "BOUND_PNG_WITH_BOUND_PPM_SOURCE",
            "case_ids_in_order": CASE_IDS,
            "axes_in_order": CANONICAL_AXES,
            "case_count": 28,
            "axes_per_case": 10,
            "axis_review_count": 280,
            "partial_or_inherited_manual_pass_allowed": False,
        },
        "input_bindings": {
            "visual_repair_design_gate": {
                "path": GATE_PATH, "sha256": GATE_SHA256},
            "targeted28_ta_coverage_template": {
                "path": TEMPLATE_PATH, "sha256": TEMPLATE_SHA256,
                "role": "case_and_axis_coverage_order_only"},
            "preordinal_failure_receipt": {
                "path": PREORDINAL_FAILURE_PATH,
                "sha256": PREORDINAL_FAILURE_SHA256},
            "binding_corrected_machine_receipt": {
                "path": MACHINE_RECEIPT_PATH,
                "sha256": MACHINE_RECEIPT_SHA256,
                "machine_result": "28_OF_28_PASS_INPUT_ONLY"},
            "actual_gpu_evidence_manifest": {
                "path": EVIDENCE_MANIFEST_PATH,
                "sha256": EVIDENCE_MANIFEST_SHA256,
                "case_count": 28},
            "actual_gpu_results_jsonl": {
                "path": RESULTS_JSONL_PATH,
                "sha256": RESULTS_JSONL_SHA256,
                "line_count": 28},
            "stable_aggregates": {
                "case_evidence_algorithm": (
                    "sha256 ordered capture_id NUL ppm_sha256 NUL "
                    "png_sha256 NUL WIDTHxHEIGHT LF"),
                "case_evidence_sha256": CASE_EVIDENCE_AGGREGATE_SHA256,
                "output_algorithm": (
                    "sha256 sorted relative_path NUL file_sha256 LF "
                    "over exact 58 output files"),
                "output_file_count": 58,
                "output_sha256": OUTPUT_AGGREGATE_SHA256,
            },
        },
        "review_counts": {
            "cases_total": 28,
            "cases_reviewed": 28,
            "cases_pass": 24,
            "cases_fail": 4,
            "cases_pending": 0,
            "axes_total": 280,
            "axes_pass": 272,
            "axes_fail": 8,
            "axes_pending": 0,
        },
        "failed_case_ids": ARMED_CASE_IDS,
        "blocking_axis_occurrences": [
            f"{case_id}:{axis}"
            for case_id in ARMED_CASE_IDS for axis in ARMED_FAIL_AXES
        ],
        "blocking_defects": [{
            "defect_id": "D06_ARMED_FOCUS_CONFIRM_EXACT_SHAPES_ABSENT",
            "severity": "BLOCKING",
            "affected_case_ids": ARMED_CASE_IDS,
            "affected_case_count": 4,
            "failed_axes": ARMED_FAIL_AXES,
            "finding": (
                "All four armed-confirm captures show a bare checkmark and bare "
                "right arrow. They do not visibly form the independently distinct "
                "focus_chevron, selected_check_diamond, and "
                "confirm_double_frame_plus_enter_arrow required by the frozen gate."),
        }],
        "cases": cases,
        "evidence_boundary": {
            "actual_gpu_machine_input": "28_OF_28_PASS_INPUT_ONLY",
            "manual_original_resolution_cases_reviewed": "28_OF_28",
            "manual_axes_reviewed": "280_OF_280",
            "manual_visual_result": "FAIL",
            "machine_pass_inherited_as_manual_pass": False,
            "scoped_visual_repair_gate_pass": False,
            "product_exposed": False,
            "product_pass": False,
            "golden_promotion": False,
            "m2_promotion": False,
            "beta_pass": False,
            "required_real_keyboard_pointer_sessions": "NOT_RUN",
            "required_screen_reader_sessions": "NOT_RUN",
            "required_external_human_cvd_localization_qa": "NOT_RUN",
            "gpu_executed_by_this_review": False,
            "production_modified_by_this_review": False,
        },
    }


def generate(output: Path = DEFAULT_OUTPUT) -> Path:
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(build_manual(), indent=2, ensure_ascii=False) + "\n",
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
