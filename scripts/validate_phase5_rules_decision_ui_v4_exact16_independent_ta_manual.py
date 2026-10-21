#!/usr/bin/env python3
"""Validate the frozen Phase5 Rules UI v4 exact16 independent TA manual."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
MANUAL = ROOT / "docs/evidence/phase5/rules-decision-ui-v4-exact16-independent-ta-manual-review-v1.json"
MANUAL_SHA256 = "8847ae7af29ee16cba968f2c23704fe7968cdc113581877c7d4bf3a291381014"
RECEIPT = ROOT / "docs/evidence/phase5/rules-decision-ui-v4-exact16-machine-pass-freeze-v1/machine-pass-freeze-receipt.json"
MANIFEST = ROOT / "docs/evidence/phase5/rules-decision-ui-v4-exact16-machine-pass-freeze-v1/stable-png-manifest.json"

AXES = [
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
FAIL_AXES = ["focus_pointer_keyboard_redundancy",
             "standard_cvd_non_color_redundancy"]
CASES = [
    "group_choice_armed_confirm--p1--zh_cn",
    "group_choice_armed_confirm--p2--en",
    "group_choice_armed_confirm--p3--zh_cn",
    "group_choice_armed_confirm--p4--en",
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
FROZEN = {
    "v3_failure_authority": (
        "docs/evidence/phase5/rules-decision-ui-v3-binding-corrected-targeted28-independent-ta-manual-review-v1.json",
        "f044e39c7765f3026bd53521e7cca8cf11b619ca5e6369c6e37e338da0afb954"),
    "v4_exact_shape_design_gate": (
        "docs/art/phase5-rules-decision-ui-v4-exact-shape-correction-design-gate-v1.json",
        "f93ca59634875ed5256f5c22678a55817e0f3e8bf8df582b3b04ac527b886051"),
    "v4_exact16_machine_pass_freeze_receipt": (
        "docs/evidence/phase5/rules-decision-ui-v4-exact16-machine-pass-freeze-v1/machine-pass-freeze-receipt.json",
        "db59bc6b5e7bfd0b2038043b147deac13cde20fa6ebaafef7085bcef94d85fd5"),
    "v4_exact16_stable_png_manifest": (
        "docs/evidence/phase5/rules-decision-ui-v4-exact16-machine-pass-freeze-v1/stable-png-manifest.json",
        "f1242dba2c5fdde4728fbf474ae8163475f0108df16a074e0f80bc0a63076bec"),
}
AGGREGATES = {
    "case_evidence_aggregate_sha256": "602dd26c0a2b3bbfbb99b08f543614c420c2165dd2db5d3c14345ed6fedc2fff",
    "original_gpu_output_inventory_aggregate_sha256": "1fd702a74751db0e51f57f7a5c94e2e84002c5347edd3ad45418d84b078ee670",
    "png_path_sha_aggregate_sha256": "f9db1e2b883d05a3507a98fa7ec6edf0df88f787ecd16a9e2420dbcc3d0e5de2",
    "ppm_path_sha_aggregate_sha256": "4437b3eb1e97ad4221f6ff82de4e5ee8f529dbd39450cbb374527d104f006ef3",
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(value, dict), "JSON object required")
    return value


def dimensions(path: Path) -> list[int]:
    if path.suffix.lower() == ".png":
        with path.open("rb") as stream:
            header = stream.read(24)
        require(header[:8] == b"\x89PNG\r\n\x1a\n" and
                header[12:16] == b"IHDR", "invalid PNG header")
        return list(struct.unpack(">II", header[16:24]))
    with path.open("rb") as stream:
        tokens = []
        while len(tokens) < 4:
            line = stream.readline()
            require(line, "truncated PPM header")
            tokens.extend(line.split(b"#", 1)[0].split())
    require(tokens[0] == b"P6" and tokens[3] == b"255", "invalid PPM header")
    return [int(tokens[1]), int(tokens[2])]


def validate(path: Path = MANUAL, enforce_frozen_sha: bool = True,
             enforce_live_evidence: bool = True) -> dict:
    path = path.resolve(strict=True)
    if enforce_frozen_sha:
        require(path == MANUAL.resolve() and sha(path) == MANUAL_SHA256,
                "frozen manual SHA mismatch")
    value = load(path)
    require(value.get("schema") ==
            "phase5_rules_decision_ui_v4_exact16_independent_ta_manual_review_v1" and
            value.get("schema_version") == 1 and value.get("status") ==
            "COMPLETED_INDEPENDENT_ORIGINAL_RESOLUTION_REVIEW" and
            value.get("overall_result") == "FAIL", "manual identity/result mismatch")
    bindings = value.get("frozen_bindings", [])
    actual = {row.get("role"): (row.get("path"), row.get("sha256"))
              for row in bindings}
    require(len(bindings) == 4 and actual == FROZEN, "frozen lineage mismatch")
    if enforce_live_evidence:
        for role, (relative, digest) in FROZEN.items():
            live = ROOT / relative
            require(live.is_file() and sha(live) == digest,
                    "live frozen binding drift: " + role)
    require(value.get("stable_aggregates") == AGGREGATES,
            "stable aggregate binding mismatch")
    method = value.get("review_method", {})
    require(method.get("images_opened_individually") == 16 and
            method.get("original_physical_resolution_required") is True and
            method.get("png_rgb_byte_faithful_to_ppm_required") is True and
            method.get("thumbnail_or_contact_sheet_only_review") is False and
            method.get("machine_or_token_claim_inherited_as_visual_pass") is False and
            method.get("v3_manual_conclusion_inherited") is False and
            method.get("decision_values") == ["PASS", "FAIL"],
            "independent original-resolution review method mismatch")
    require(value.get("canonical_axes") == AXES, "canonical axis order mismatch")

    receipt, manifest = load(RECEIPT), load(MANIFEST)
    require(receipt.get("schema") ==
            "phase5_rules_decision_ui_v4_exact16_machine_pass_freeze_receipt_v1" and
            receipt.get("status") == "TARGETED_GPU_MACHINE_16_PASS_TA_REVIEW_PENDING" and
            receipt.get("verified_counts", {}).get("planned_cases") == 16 and
            receipt.get("verified_counts", {}).get("failed_cases") == 0 and
            receipt.get("claims", {}).get("targeted_machine_16_pass") is True and
            receipt.get("claims", {}).get("product_exposed") is False,
            "machine freeze receipt identity/claims mismatch")
    require(receipt.get("stable_aggregates") == AGGREGATES,
            "machine receipt aggregates mismatch")
    require(manifest.get("case_count") == 16 and
            manifest.get("case_ids_in_order") == CASES and
            manifest.get("aggregates") == {key: AGGREGATES[key] for key in (
                "case_evidence_aggregate_sha256", "png_path_sha_aggregate_sha256",
                "ppm_path_sha_aggregate_sha256")}, "stable PNG manifest mismatch")
    evidence = value.get("evidence_bindings", [])
    manifest_cases = manifest.get("cases", [])
    require(len(evidence) == len(manifest_cases) == 16, "evidence count mismatch")
    for review_row, frozen_row, capture_id in zip(evidence, manifest_cases, CASES):
        expected = {
            "ordinal": frozen_row["ordinal"], "capture_id": frozen_row["capture_id"],
            "physical_dimensions": [frozen_row["physical_width"],
                                    frozen_row["physical_height"]],
            "png_path": frozen_row["png_path"], "png_sha256": frozen_row["png_sha256"],
            "ppm_path": frozen_row["ppm_path"], "ppm_sha256": frozen_row["ppm_sha256"],
            "decoded_rgb_sha256": frozen_row["decoded_rgb_sha256"],
            "png_decoded_rgb_equals_ppm_rgb": True,
        }
        require(review_row == expected, "per-image evidence binding mismatch: " + capture_id)
        if enforce_live_evidence:
            png, ppm = ROOT / review_row["png_path"], ROOT / review_row["ppm_path"]
            require(png.is_file() and ppm.is_file() and
                    sha(png) == review_row["png_sha256"] and
                    sha(ppm) == review_row["ppm_sha256"] and
                    dimensions(png) == review_row["physical_dimensions"] and
                    dimensions(ppm) == review_row["physical_dimensions"],
                    "live image SHA/dimensions mismatch: " + capture_id)

    reviews = value.get("case_reviews", [])
    require(len(reviews) == 16, "case review count mismatch")
    axis_pass = axis_fail = case_pass = case_fail = 0
    for index, (row, capture_id) in enumerate(zip(reviews, CASES)):
        require(row.get("ordinal") == index + 1 and
                row.get("capture_id") == capture_id and
                row.get("reviewed_at_original_resolution") is True and
                isinstance(row.get("observation"), str) and row["observation"],
                "case review identity/method mismatch: " + capture_id)
        statuses = row.get("axis_statuses", {})
        require(list(statuses) == AXES and
                all(status in {"PASS", "FAIL"} for status in statuses.values()),
                "axis inventory/status mismatch: " + capture_id)
        expected_failed = FAIL_AXES if index < 4 else []
        failed = [axis for axis in AXES if statuses[axis] == "FAIL"]
        require(failed == expected_failed and row.get("blocking_axes") == expected_failed and
                row.get("case_result") == ("FAIL" if expected_failed else "PASS"),
                "case blocking-axis/result mismatch: " + capture_id)
        axis_fail += len(failed); axis_pass += 10 - len(failed)
        case_fail += bool(failed); case_pass += not failed
    require([axis_pass, axis_fail, case_pass, case_fail] == [152, 8, 12, 4],
            "derived manual counts mismatch")
    require(value.get("failed_case_ids") == CASES[:4], "failed case inventory mismatch")
    finding = value.get("blocking_finding", {})
    require(finding.get("defect_id") ==
            "D07_V4_EXACT_SHAPES_STILL_NOT_VISUALLY_RECOGNIZABLE" and
            finding.get("failed_axes_per_repair_case") == FAIL_AXES and
            finding.get("failed_axis_occurrence_count") == 8 and
            all(term in finding.get("pixel_level_finding", "") for term in
                ("focus chevron", "closed diamond", "double rectangular frame")) and
            "cannot" in finding.get("machine_claim_boundary", ""),
            "pixel-level blocker finding mismatch")
    require(value.get("review_counts") == {
        "cases_expected": 16, "cases_reviewed": 16, "cases_pass": 12,
        "cases_fail": 4, "axes_expected": 160, "axes_reviewed": 160,
        "axes_pass": 152, "axes_fail": 8, "axes_not_run": 0,
        "repair_cases_expected": 4, "repair_cases_pass": 0,
        "repair_cases_fail": 4, "regression_controls_expected": 12,
        "regression_controls_pass": 12, "regression_controls_fail": 0},
        "declared review counts mismatch")
    require(value.get("proof_boundary") == {
        "targeted_machine_16_pass": True,
        "independent_ta_review_complete": True,
        "independent_ta_pass": False,
        "v4_exact_shape_raster_gate_pass": False,
        "product_exposed": False, "product_pass": False,
        "golden_promotion": False, "m2_promotion": False,
        "beta_pass": False, "real_input_status": "NOT_RUN",
        "real_input_pass": False, "screen_reader_status": "NOT_RUN",
        "screen_reader_pass": False}, "proof boundary mismatch")
    return {"status": "PASS_VALIDATED_PHASE5_V4_EXACT16_TA_FAILURE",
            "manual_sha256": sha(path), "cases_reviewed": 16,
            "case_pass": 12, "case_fail": 4, "axes_reviewed": 160,
            "axes_pass": 152, "axes_fail": 8, "overall": "FAIL",
            "product_exposed": False, "beta_pass": False,
            "real_input_status": "NOT_RUN", "screen_reader_status": "NOT_RUN"}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manual", type=Path, nargs="?", default=MANUAL)
    args = parser.parse_args()
    try:
        print(json.dumps(validate(args.manual,
                                  enforce_frozen_sha=args.manual.resolve() == MANUAL.resolve()),
                         sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
