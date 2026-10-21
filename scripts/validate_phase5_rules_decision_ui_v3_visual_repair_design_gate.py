#!/usr/bin/env python3
"""Validate the Phase-5 Rules Decision UI v3 visual-repair design gate.

This validator is offline-only. It validates frozen evidence bindings, the exact
v2 manual failure inventory, the scoped visual contract, and future evidence
requirements. It never runs GPU work or edits production files.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GATE = (
    ROOT / "docs/art/"
    "phase5-rules-decision-ui-v3-visual-repair-design-gate-v1.json"
)

EXPECTED_BINDINGS = {
    "v2_final_manual_failure_authority": (
        "docs/evidence/phase5/"
        "rules-decision-ui-v2-independent-manual-review-v1.json",
        "40ab295412081bba3ecb87351dabebc76a85d522c75b4bbddd9c1ed90f6540c0",
    ),
    "v2_stable_png_manifest": (
        "docs/evidence/phase5/actual-gpu-v2-evidence/"
        "rules-decision-ui-actual-gpu-evidence-v2.json",
        "cf21196bcc834a9658a2748a157bf2b7fe1f68567af4b7e52022c3013e3afb66",
    ),
    "v2_actual_gpu_machine_pass_receipt": (
        "docs/evidence/phase5/"
        "rules-decision-ui-v2-gpu-machine-pass-receipt-v1.json",
        "3b9a807ce6c906dc2f3d29247edf15f747740c52d955a74ff50fef70d7da3cc0",
    ),
    "original_rules_ui_design_gate": (
        "docs/art/phase5-rules-decision-ui-design-gate-v1.json",
        "26567d0547163561ed77e6e41b32753c4cc55efcd693c9ce22faf847d43f5a08",
    ),
}
EXPECTED_PNG_AGGREGATE = (
    "f05e668df68d61fa7d5eff84b6fc87f3b09768fe1c545be11804636c4a9ba248"
)
EXPECTED_AXES = [
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
EXPECTED_STATES = [
    "group_choice_open_unselected",
    "group_choice_armed_confirm",
    "full_table_preview_valid",
    "full_table_preview_invalid",
    "behind_head_string_preview_valid",
    "behind_head_string_preview_invalid",
    "eight_legal_open_unselected",
    "eight_foul_open_unselected",
    "break_illegal_open_unselected",
    "break_shot_foul_open_unselected",
    "submitting_progress",
    "authority_error",
    "state_drift_refreshing",
    "ai_or_remote_pending",
    "section_6g_classification_pending",
]
EXPECTED_REGRESSION_CONTROLS = [
    "group_choice_open_unselected--p1--en",
    "group_choice_open_unselected--p2--zh_cn",
    "group_choice_open_unselected--p3--en",
    "group_choice_open_unselected--p4--zh_cn",
    "eight_legal_open_unselected--p1--en",
    "break_illegal_open_unselected--p2--zh_cn",
    "submitting_progress--p3--en",
    "authority_error--p4--en",
]
EXPECTED_DEFECTS = [
    "D01_CONFIRM_COPY_OMITS_EXACT_ACTION_AND_ACTOR",
    "D02_ARMED_FOCUS_CONFIRM_NON_COLOR_TOKENS_MISSING",
    "D03_PREVIEW_MARKER_NOT_GHOST_CUE_BALL",
    "D04_INVALID_MARKER_MISSING_ERROR_CROSS_OCTAGON_AND_DASHED_RING",
    "D05_HEAD_STRING_LEGAL_REGION_PATTERN_CONTRACT_MISMATCH",
]
EXPECTED_ALLOWED_STATES = [
    "group_choice_armed_confirm",
    "full_table_preview_valid",
    "full_table_preview_invalid",
    "behind_head_string_preview_valid",
    "behind_head_string_preview_invalid",
]
EXPECTED_EVIDENCE_BOUNDARY = {
    "design_contract_only": True,
    "production_source_modified_by_this_gate": False,
    "shipping_asset_modified_by_this_gate": False,
    "gpu_capture_performed_by_this_gate": False,
    "machine_pass_claimed_by_this_gate": False,
    "technical_art_re_review_claimed_by_this_gate": False,
    "external_human_review_claimed_by_this_gate": False,
    "true_keyboard_or_pointer_session_claimed_by_this_gate": False,
    "screen_reader_session_claimed_by_this_gate": False,
    "implementation_authorized": True,
    "gpu_execution_authorized": False,
    "product_exposure_authorized": False,
    "beta_authorized": False,
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def bound_path(value: str) -> Path:
    path = Path(value)
    return path if path.is_absolute() else ROOT / path


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def manual_failures(
    manual: dict[str, Any],
) -> tuple[list[str], dict[str, list[str]]]:
    failed_cases: list[str] = []
    failed_axes = {axis: [] for axis in EXPECTED_AXES}
    for case in manual.get("cases", []):
        case_id = case.get("case_id")
        findings = case.get("axis_findings", {})
        require(set(findings) == set(EXPECTED_AXES)
                and len(findings) == len(EXPECTED_AXES),
                f"manual axis inventory mismatch: {case_id}")
        case_failed = False
        for axis in EXPECTED_AXES:
            if findings[axis].get("status") == "FAIL":
                failed_axes[axis].append(case_id)
                case_failed = True
        require((case.get("manual_case_status") == "FAIL") == case_failed,
                f"manual case/axis result mismatch: {case_id}")
        if case_failed:
            failed_cases.append(case_id)
    return failed_cases, {
        axis: ids for axis, ids in failed_axes.items() if ids
    }


def validate_manual_and_manifest(
    manual: dict[str, Any],
    manifest: dict[str, Any],
    receipt: dict[str, Any],
) -> tuple[list[str], dict[str, list[str]]]:
    require(
        manual.get("schema") ==
        "phase5_rules_decision_ui_v2_independent_manual_visual_review_v1",
        "manual schema mismatch",
    )
    require(
        manual.get("status") == "MANUAL_VISUAL_REVIEW_FAIL_BLOCKING"
        and manual.get("overall_result") == "FAIL",
        "manual must remain a blocking FAIL",
    )
    require(
        manual.get("review_counts") == {
            "axes_fail": 32,
            "axes_na": 96,
            "axes_pass": 472,
            "axes_pending": 0,
            "axes_total": 600,
            "cases_fail": 20,
            "cases_pass": 40,
            "cases_pending": 0,
            "cases_reviewed": 60,
            "cases_total": 60,
        },
        "manual review counts mismatch",
    )
    boundary = manual.get("evidence_boundary", {})
    require(
        boundary.get("product_exposed") is False
        and boundary.get("beta_pass") is False
        and boundary.get("required_real_keyboard_mouse_sessions") == "NOT_RUN"
        and boundary.get("required_screen_reader_sessions") == "NOT_RUN"
        and boundary.get("required_external_human_cvd_localization_qa")
        == "NOT_RUN",
        "manual evidence boundary promoted",
    )
    failed_cases, failed_axes = manual_failures(manual)
    require(len(failed_cases) == 20, "manual failed-case count mismatch")
    require(sum(map(len, failed_axes.values())) == 32,
            "manual failed-axis occurrence count mismatch")
    require(manual.get("failed_case_ids") == failed_cases,
            "manual failed-case inventory mismatch")

    require(
        manifest.get("schema") ==
        "phase5_rules_decision_ui_actual_gpu_evidence_v2"
        and manifest.get("case_count") == 60,
        "stable PNG manifest header mismatch",
    )
    frozen = manifest.get("frozen_bindings", {})
    require(frozen.get("png_aggregate_sha256") == EXPECTED_PNG_AGGREGATE,
            "stable PNG aggregate mismatch")
    claims = manifest.get("claims", {})
    require(
        claims.get("product_exposed") is False
        and claims.get("profile_complete") is False
        and claims.get("true_input_session_completed") is False
        and claims.get("screen_reader_session_completed") is False
        and claims.get("external_beta_qa_completed") == 0,
        "stable PNG claims promoted",
    )
    require(
        receipt.get("schema") ==
        "phase5_rules_decision_ui_v2_gpu_machine_pass_receipt_v1"
        and receipt.get("counts", {}).get("actual_gpu_captures_completed") == 60
        and receipt.get("counts", {}).get(
            "independent_manual_reviews_completed") == 0,
        "v2 machine receipt boundary mismatch",
    )
    return failed_cases, failed_axes


def validate_original_thresholds(
    gate: dict[str, Any], original: dict[str, Any]
) -> None:
    inherited = gate.get("inherited_thresholds_immutable", {})
    require(inherited.get("source") == "original_rules_ui_design_gate"
            and inherited.get("modification_allowed") is False,
            "threshold inheritance boundary mismatch")
    visual = original["visual_and_accessibility"]
    require(inherited.get("decision_modal") == visual["decision_modal"],
            "decision-modal threshold drift")
    require(inherited.get("contrast") == visual["contrast"],
            "contrast threshold drift")
    expected_typography = {
        key: visual["typography"][key]
        for key in (
            "minimum_base_logical_px", "body_logical_px",
            "action_logical_px", "title_logical_px",
            "line_height_multiplier_minimum", "maximum_action_lines",
            "maximum_body_lines", "no_ellipsis_for_rule_action_or_error_copy",
        )
    }
    require(inherited.get("typography") == expected_typography,
            "typography threshold drift")
    require(
        inherited.get("placement_overlay") == {
            "table_occlusion_limit_fraction":
            visual["placement_overlay"]["table_occlusion_limit_fraction"]
        },
        "placement occlusion threshold drift",
    )
    expected_performance = {
        key: original["performance_budget"][key]
        for key in (
            "total_frame_p95_ms_max", "rules_ui_cpu_build_p95_ms_max",
            "rules_ui_gpu_submit_p95_ms_max",
            "rules_ui_total_incremental_p95_ms_max",
            "incremental_draw_calls_max", "complete_scene_draw_calls_max",
            "incremental_texture_residency_mib_max",
        )
    }
    require(inherited.get("performance") == expected_performance,
            "performance threshold drift")


def validate_gate_structure(
    gate: dict[str, Any],
    manual: dict[str, Any],
    manifest: dict[str, Any],
    receipt: dict[str, Any],
    original: dict[str, Any],
) -> None:
    require(
        gate.get("schema") ==
        "phase5_rules_decision_ui_v3_visual_repair_design_gate_v1"
        and gate.get("schema_version") == 1
        and gate.get("status") ==
        "APPROVED_FOR_SCOPED_VISUAL_REPAIR_IMPLEMENTATION_ONLY",
        "gate header mismatch",
    )
    require(gate.get("product_exposed") is False
            and gate.get("profile_complete") is False
            and gate.get("beta_pass") is False,
            "gate exposure claim promoted")
    require(gate.get("evidence_boundary") == EXPECTED_EVIDENCE_BOUNDARY,
            "gate evidence boundary mismatch")

    failed_cases, failed_axes = validate_manual_and_manifest(
        manual, manifest, receipt)
    bound = gate.get("bound_v2_failure_set", {})
    require(bound.get("manual_result") == "FAIL"
            and bound.get("failed_case_ids") == failed_cases
            and bound.get("failed_axis_case_ids") == failed_axes,
            "gate/manual failure inventory drift")
    require(bound.get("case_counts") == {
        "reviewed": 60, "pass": 40, "fail": 20, "pending": 0
    }, "bound case counts mismatch")
    require(bound.get("axis_counts") == {
        "total": 600, "pass": 472, "fail": 32, "na": 96, "pending": 0
    }, "bound axis counts mismatch")
    require(bound.get("blocking_defect_ids") == EXPECTED_DEFECTS,
            "blocking defect inventory drift")

    contract = gate.get("repair_contract", {})
    require(contract.get("allowed_state_ids") == EXPECTED_ALLOWED_STATES,
            "repair state scope widened or reordered")
    require(contract.get("allowed_layers") == [
        "rules_decision_presentation_copy_formatting",
        "rules_decision_product_draw_shape_emission",
        "table_world_placement_overlay",
        "offline_validation_and_test_scaffolding",
    ], "repair layer scope mismatch")

    armed = contract.get("armed_confirm", {})
    expected_armed = [
        case_id for case_id in failed_cases
        if case_id.startswith("group_choice_armed_confirm--")
    ]
    require(armed.get("affected_case_ids") == expected_armed
            and len(expected_armed) == 4,
            "armed/confirm affected cases mismatch")
    require(armed.get("armed_copy_templates") == {
        "en": "Armed — {actor_display}: {exact_action_result}",
        "zh_cn": "已选 — {actor_display}：{exact_action_result}",
    }, "armed copy template mismatch")
    require(armed.get("confirm_copy_templates") == {
        "en": "Confirm — {actor_display}: {exact_action_result}",
        "zh_cn": "确认 — {actor_display}：{exact_action_result}",
    }, "confirm copy template mismatch")
    tokens = armed.get("non_color_shape_tokens", {})
    require(set(tokens) == {"armed", "color_policy", "confirm", "focused"}
            and "focus_chevron" in tokens["focused"]
            and "selected_check_diamond" in tokens["armed"]
            and "confirm_double_frame_plus_enter_arrow" in tokens["confirm"],
            "independent armed/focus/confirm tokens missing")
    require("First action activation arms only" in
            armed.get("submission_semantics_unchanged", ""),
            "two-step submission contract drift")

    placement = contract.get("placement_world_overlay", {})
    expected_placement = [
        case_id for case_id in failed_cases if "_preview_" in case_id
    ]
    require(placement.get("affected_case_ids") == expected_placement
            and len(expected_placement) == 16,
            "placement affected cases mismatch")
    require(placement.get("valid_candidate", {}).get("required_shape")
            == "ghost_cue_ball_plus_crosshair",
            "valid placement shape contract mismatch")
    require(placement.get("invalid_candidate", {}).get("required_shape")
            == ("ghost_cue_ball_plus_error_cross_octagon_plus_"
                "dashed_collision_ring"),
            "invalid placement shape contract mismatch")
    head = placement.get("behind_head_string", {})
    require(head.get("required_region_pattern") ==
            "diagonal_hatch_plus_boundary_line_plus_in_world_text",
            "behind-head region pattern mismatch")
    copy = head.get("in_world_copy", {})
    require(
        set(copy) == {"en", "zh_cn"}
        and "behind head string" in copy["en"]
        and "开球线后" in copy["zh_cn"]
        and all(token in copy["en"] for token in ("{x_cm}", "{z_cm}"))
        and all(token in copy["zh_cn"] for token in ("{x_cm}", "{z_cm}")),
        "behind-head bilingual in-world coordinate copy mismatch",
    )

    forbidden = gate.get("forbidden_changes", {})
    require(set(forbidden) == {
        "authority_and_rules", "evidence", "exposure",
        "scope_expansion", "thresholds",
    }, "forbidden-change boundary missing")
    require("No stable authority" in forbidden["authority_and_rules"][0]
            and "No contrast" in forbidden["thresholds"][0]
            and "No product_exposed" in forbidden["exposure"][0],
            "forbidden-change policy weakened")
    validate_original_thresholds(gate, original)

    machine = gate.get("machine_validation_gate", {})
    cross = machine.get("cross_product", {})
    require(machine.get("required_case_count") == 540
            and cross == {
                "canonical_state_count": 15,
                "formula": "15*2*2*3*3",
                "locale_count": 2,
                "palette_count": 2,
                "required_case_count": 540,
                "ui_scale_count": 3,
                "viewport_density_count": 3,
            }, "540-case machine cross product mismatch")
    require(machine.get("canonical_states") == EXPECTED_STATES
            and machine.get("locales") == ["en", "zh_cn"]
            and machine.get("palettes") ==
            ["standard", "color_vision_safe"]
            and machine.get("ui_scales") == [0.85, 1.0, 1.35],
            "machine matrix dimension drift")
    checks = machine.get("per_case_hard_checks", [])
    required_phrases = (
        "exact localized action result and actor",
        "focus_chevron, selected_check_diamond",
        "ghost cue-ball plus crosshair",
        "error-cross-octagon plus dashed collision ring",
        "diagonal hatch, boundary line, bilingual in-world",
        "authority hash/revision/events/actions",
    )
    require(all(any(phrase in check for check in checks)
                for phrase in required_phrases),
            "machine repair assertion missing")

    targeted = gate.get("targeted_actual_gpu_gate", {})
    require(targeted.get("authorization_status") ==
            "NOT_AUTHORIZED_BY_THIS_DESIGN_GATE",
            "GPU authorization was improperly granted")
    require(targeted.get("minimum_case_count") == 28
            and targeted.get("repair_case_count") == 20
            and targeted.get("regression_control_count") == 8,
            "targeted GPU counts mismatch")
    require(targeted.get("repair_case_ids") == failed_cases,
            "targeted GPU repair inventory mismatch")
    require(targeted.get("regression_control_case_ids") ==
            EXPECTED_REGRESSION_CONTROLS,
            "targeted GPU regression controls mismatch")
    require(not set(failed_cases) & set(EXPECTED_REGRESSION_CONTROLS),
            "repair and regression target overlap")
    stable_ids = {
        case.get("capture_id") for case in manifest.get("cases", [])
    }
    require(set(failed_cases + EXPECTED_REGRESSION_CONTROLS) <= stable_ids,
            "targeted GPU ID is not bound by stable v2 manifest")

    ta = gate.get("technical_art_re_review_gate", {})
    require(ta.get("status") == "NOT_RUN"
            and ta.get("image_count") == 28
            and ta.get("axes_per_image") == EXPECTED_AXES
            and ta.get("axis_review_count") == 280
            and ta.get(
                "does_not_satisfy_external_human_or_true_input_sessions")
            is True,
            "technical-art re-review boundary mismatch")
    require(all(item.get("status") == "NOT_RUN"
                for item in gate.get("required_external_sessions", []))
            and len(gate.get("required_external_sessions", [])) == 6,
            "external session was claimed or omitted")
    require(gate.get("acceptance", {}).get("result") ==
            ("SCOPED_VISUAL_REPAIR_DESIGN_APPROVED_NOT_IMPLEMENTED_"
             "NOT_GPU_AUTHORIZED_NOT_PRODUCT_PASS"),
            "acceptance result mismatch")


def validate_bindings(gate: dict[str, Any]) -> dict[str, dict[str, Any]]:
    bindings = gate.get("read_only_bindings", [])
    require(len(bindings) == 4, "read-only binding count mismatch")
    by_role = {binding.get("role"): binding for binding in bindings}
    require(set(by_role) == set(EXPECTED_BINDINGS),
            "read-only binding role mismatch")
    loaded: dict[str, dict[str, Any]] = {}
    for role, (path_text, expected_sha) in EXPECTED_BINDINGS.items():
        binding = by_role[role]
        require(binding.get("path") == path_text
                and binding.get("sha256") == expected_sha,
                f"binding metadata mismatch: {role}")
        path = bound_path(path_text)
        require(path.is_file() and sha(path) == expected_sha,
                f"binding hash mismatch: {role}")
        loaded[role] = load_json(path)
    require(by_role["v2_stable_png_manifest"].get(
        "png_aggregate_sha256") == EXPECTED_PNG_AGGREGATE,
        "bound PNG aggregate mismatch")
    return loaded


def validate(gate_path: Path = DEFAULT_GATE) -> dict[str, Any]:
    gate = load_json(gate_path)
    loaded = validate_bindings(gate)
    validate_gate_structure(
        gate,
        loaded["v2_final_manual_failure_authority"],
        loaded["v2_stable_png_manifest"],
        loaded["v2_actual_gpu_machine_pass_receipt"],
        loaded["original_rules_ui_design_gate"],
    )
    return {
        "status": "PASS",
        "gate": str(gate_path.relative_to(ROOT)),
        "gate_sha256": sha(gate_path),
        "bound_failed_cases": 20,
        "bound_failed_axis_occurrences": 32,
        "machine_cases_required": 540,
        "targeted_gpu_cases_required": 28,
        "technical_art_images_required": 28,
        "technical_art_axes_required": 280,
        "gpu_executed": False,
        "production_modified": False,
        "product_exposed": False,
        "beta_pass": False,
        "true_input_sessions": "NOT_RUN",
        "screen_reader_sessions": "NOT_RUN",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gate", type=Path, default=DEFAULT_GATE)
    args = parser.parse_args()
    try:
        result = validate(args.gate.resolve())
    except (OSError, ValueError, KeyError, RuntimeError) as exc:
        print(json.dumps({"status": "FAIL", "error": str(exc)},
                         sort_keys=True))
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
