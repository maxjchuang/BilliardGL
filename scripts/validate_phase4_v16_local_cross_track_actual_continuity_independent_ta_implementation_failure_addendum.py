#!/usr/bin/env python3
"""Validate the independent TA failure addendum for Phase4 V16."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/art/phase4-v16-local-cross-track-actual-continuity-independent-ta-implementation-failure-addendum-v1.json"
SCHEMA = "billiardgl.phase4.v16.local_cross_track_actual_continuity_independent_ta_implementation_failure_addendum.v1"

BINDINGS = {
    "docs/art/phase4-v16-local-cross-track-actual-continuity-implementation-design-gate-v1.json": "f281949f298cb0e07c7f8e427f419455555709277e137f34389ffdb3789c2191",
    "docs/art/phase4-v16-local-cross-track-actual-continuity-premise-falsification-v1/premise-failure-receipt-v1.json": "6e5c729016795a8744fddd7351ae8d9f9d584f9cde91946a33bcbd83f982fa96",
    "scripts/validate_phase4_v16_local_cross_track_actual_continuity_premise_failure.py": "f404c479e0140fd3de4862e41fc88eeb9e373d790aabc801557034a1c91960f1",
    "tests/test_phase4_v16_local_cross_track_actual_continuity_premise_failure.py": "f5c4c475049d214de567c5d57a8bcabb84440a16b653e1702eec5c423b4a55ab",
    "docs/art/phase4-v15-independent-ta-diagnostic-acceptance-decision-gate-v1.json": "2628104e6d219a98fb38fa599058ec3dd22f11c658d7fe3aa1ffc82850bcf23b",
    "docs/art/phase4-v15-broadcast-overwrite-provenance-diagnostic-v1/provenance-ledger-v1.json": "bb2c3ecafb01d42cfddb8eec89a22619bf646ddb64773846747c5c76d2f5538b",
}
BASELINES = {
    "src/Billiards/training/trajectory_overlay_renderer.cpp": "0c108c506641ab4828290a1a4c7b6b5ebbd51ab4fc7c12845bde0f5deb18a840",
    "src/Billiards/training/trajectory_overlay_renderer.h": "9805e6a5f56c17d7645200576505425cddf0d642970e5dc5cec186e003bd564e",
    "src/Billiards/render/modern/modern_renderer.cpp": "b00cc6b370122275a42f106967f935dbb34bd5d3f81f2a750ca9ba6e503af191",
    "src/Billiards/render/modern/modern_renderer.h": "936de5b77c9e3485f2e51f10e9de26c59c8ec94674865e0b50ae125725183c9d",
    "CMakeLists.txt": "f41563c35224dd809f64acedba6455355e70082697c41305c2d1b05426f71d41",
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate(path: Path = DEFAULT) -> list[str]:
    errors: list[str] = []
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        return [f"addendum_invalid:{error}"]

    def require(condition: bool, message: str) -> None:
        if not condition:
            errors.append(message)

    require(value.get("schema") == SCHEMA, "schema_invalid")
    require(value.get("status") == "FAIL_IMPLEMENTATION_GATE_PREMISE_FALSIFIED_CPU_ONLY_NO_GPU_NO_AUTH", "status_invalid")
    observed = {item.get("path"): item.get("sha256") for item in value.get("frozen_bindings", [])}
    require(observed == BINDINGS, "binding_inventory_or_sha_invalid")
    for relative, digest in BINDINGS.items():
        target = ROOT / relative
        require(target.is_file() and sha(target) == digest, f"binding_missing_or_drift:{relative}")
    restored = {item.get("path"): item.get("sha256") for item in value.get("restored_baselines", {}).values()}
    require(restored == BASELINES, "baseline_inventory_or_sha_invalid")
    for relative, digest in BASELINES.items():
        target = ROOT / relative
        require(target.is_file() and sha(target) == digest, f"baseline_missing_or_drift:{relative}")

    audit = value.get("independent_contract_audit", {})
    require(audit.get("allowlisted_candidate_files_only") is True and audit.get("production_selector_contains_capture_camera_or_acceptance_coordinate_input") is False, "scope_or_coordinate_audit_invalid")
    require(audit.get("distinct_crossing_key") == "arc_segment_id + normalized_primitive_stem" and audit.get("same_stem_core_halo_is_cross") is False, "provenance_contract_invalid")
    require(audit.get("ordered_source_probe_count") == 81 and audit.get("ordered_source_probe_digest") == "33511aa46717a208f3db43acd278416b8fe8393e2e003484da8f44dabffe5645", "source_inventory_invalid")
    require(len(audit.get("exact_repair_probe_ids_bound_in_order", [])) == 3, "repair_ids_invalid")
    require(audit.get("synthetic_zero_one_two_selector_fixture_passed") is True and audit.get("generic_full_frame_zero_one_two_contract_passed") is False and audit.get("visual_controls_status") == "NOT_REACHED_NOT_PASS", "synthetic_or_control_boundary_invalid")

    finding = value.get("independent_failure_finding", {})
    require(finding.get("classification") == "V16_GATE_PREMISE_FALSIFIED", "failure_classification_invalid")
    require(finding.get("single_sample_unsatisfiable_islands_standard_cvd") == [27, 27] and finding.get("asymmetric_four_sample_unsatisfiable_islands_standard_cvd") == [76, 76], "failed_island_counts_invalid")
    require(finding.get("selector_error") == "trajectory_continuity_no_minimum_connected_subset" and finding.get("fail_closed_selected_count") == 0, "selector_fail_closed_invalid")
    counter = finding.get("constructive_counterexample", {})
    require(counter.get("physical_pixel") == [840, 451] and counter.get("eligible_component_size") == 1 and counter.get("maximum_largest8_after_any_allowed_subset_of_size_at_most_2") == 1 and counter.get("required_largest8") == 2, "counterexample_invalid")
    require(finding.get("implementation_acceptance") is False and finding.get("gpu_or_readiness_may_follow") is False, "failure_boundary_invalid")

    blocker = value.get("unclosed_pipeline_blocker", {})
    require(blocker.get("code") == "selected_mask_upload_gl_error_not_checked" and blocker.get("resource_upload_failure_is_fail_closed") is False and blocker.get("pipeline_contract_pass") is False, "latent_upload_blocker_invalid")
    disposition = value.get("selected_priority_one_disposition", {})
    require(disposition.get("decision_id") == "LOCAL_CROSS_TRACK_ACTUAL_CONTINUITY_PRIORITY" and disposition.get("implementation_result") == "REJECTED_AFTER_REPRODUCIBLE_PREMISE_FALSIFICATION" and disposition.get("may_be_resumed_under_v16_gate") is False, "priority_one_disposition_invalid")
    successor = value.get("remaining_bounded_product_path", {})
    require(successor.get("decision_id") == "EXPLICIT_NON_OCCLUDING_CROSSING_GRAMMAR" and successor.get("assessment") == "ONLY_REMAINING_BOUNDED_PATH_AMONG_THE_TWO_FROZEN_MUTUALLY_EXCLUSIVE_V15_CHOICES" and successor.get("recommended_for_new_root_product_decision") is True and successor.get("selected_or_authorized_now") is False, "successor_assessment_invalid")
    require(len(successor.get("mandatory_new_gate_constraints", [])) == 5, "successor_constraint_inventory_invalid")
    authorization = value.get("authorization_boundary", {})
    require(len(authorization) == 16 and all(item is False for item in authorization.values()), "authorization_boundary_must_be_all_false")
    return errors


def main() -> int:
    target = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT
    errors = validate(target)
    if errors:
        for error in errors:
            print("FAIL: " + error, file=sys.stderr)
        return 1
    print("PASS_PHASE4_V16_INDEPENDENT_TA_IMPLEMENTATION_FAILURE_ADDENDUM")
    print("v16=FAIL 1x=27/27 4x=76/76 priority1=rejected priority2=recommended_not_selected gpu=false")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
