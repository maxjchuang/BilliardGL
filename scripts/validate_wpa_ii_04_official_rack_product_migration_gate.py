#!/usr/bin/env python3
"""Validate the frozen no-GPU official-rack product migration gate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GATE = ROOT / "docs/rules/wpa-ii-04-official-rack-product-migration-gate-v1.json"
PREDECESSOR_SHA = "a1e675cc32057880a4805ddafaf93c2192b6f127ae6bc2f28c71f5390d444809"
EXPECTED_SOURCES = {
    "current_layout": ("src/Billiards/game_state.cpp", "a91185ba5fad8c8571aa1b0efd36de967d0fc3c307fef01b2903c2f910de88c3"),
    "official_contract": ("src/Billiards/rules/rack_executor.cpp", "d1805c8e197f6b1a8f712f80ba8be807217e875d56def3c7d00186293cd9fb17"),
    "official_contract_header": ("src/Billiards/rules/rack_executor.h", "d42dc8ae486ad9df8911e4f82cb3d657c0e51e76b35082edb3fd909ee37ef242"),
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate(value: object, *, check_files: bool = True) -> list[str]:
    errors: list[str] = []
    if not isinstance(value, dict):
        return ["gate root must be an object"]
    if value.get("schema") != "wpa_ii_04_official_rack_product_migration_gate_v1" or \
            value.get("status") != "APPROVED_NO_GPU_PRODUCT_LAYOUT_MIGRATION_REQUIRED" or \
            value.get("approved_scope") != ["WPA-II-04", "WPA-II-07"]:
        errors.append("gate identity/scope differs")
    predecessor = value.get("frozen_predecessor")
    if predecessor != {
        "path": "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v2.json",
        "sha256": PREDECESSOR_SHA,
    }:
        errors.append("frozen audit-v2 binding differs")
    elif check_files and sha256(ROOT / predecessor["path"]) != PREDECESSOR_SHA:
        errors.append("frozen audit-v2 bytes changed")

    sources = value.get("source_bindings")
    if not isinstance(sources, dict):
        errors.append("source bindings missing")
    else:
        for key, (path_text, digest) in EXPECTED_SOURCES.items():
            if sources.get(key) != {"path": path_text, "sha256": digest}:
                errors.append(f"source binding differs: {key}")
            elif check_files and sha256(ROOT / path_text) != digest:
                errors.append(f"frozen source bytes changed: {path_text}")

    comparison = value.get("comparison")
    expected_comparison = {
        "profile": "chinese_pool_legacy_v1",
        "fixed_seed_hex": "0x48455942414c4c25",
        "ball_count": 16,
        "exact_position_match_count": 2,
        "exact_position_mismatch_count": 14,
        "all_current_balls_active_stationary": True,
        "cue_ball_exact": True,
        "current_base_corner_ball_ids": [11, 15],
        "current_base_corners_are_opposite_groups": False,
        "official_base_corner_ball_ids": [5, 10],
        "official_base_corners_are_opposite_groups": True,
        "eight_ball_current_z": 73.3986664,
        "eight_ball_official_z": 73.398674,
        "eight_ball_position_byte_exact": False,
        "official_same_group_adjacency_count": 7,
        "official_minimum_possible_same_group_adjacency_count": 7,
        "numbered_ball_order_is_official": False,
        "exact_position_match_ball_ids": [0, 9],
    }
    if comparison != expected_comparison:
        errors.append("exact current/official layout comparison differs")

    decision = value.get("decision")
    if not isinstance(decision, dict) or \
            decision.get("preserve_noncompliant_layout") is not False or \
            decision.get("migrate_product_start_to_fixed_official_contract") is not True or \
            decision.get("numbered_ball_order_semantics") != "implementation_deterministic_non_normative" or \
            decision.get("texture_assets_may_change") is not False or \
            decision.get("product_start_commit_before_first_visible_frame") is not True or \
            decision.get("single_atomic_commit_required") is not True or \
            decision.get("failure_rollback_required") is not True or \
            decision.get("foundation_action_product_exposed") is not False or \
            decision.get("rules_decision_ui_product_exposed") is not False:
        errors.append("approved migration decision differs")

    evidence = value.get("historical_visual_evidence")
    if not isinstance(evidence, dict) or \
            any(evidence.get(f"phase{phase}_inheritable") is not False for phase in (2, 4, 5)) or \
            evidence.get("classification") != "HISTORICAL_ONLY_AFTER_OFFICIAL_RACK_MIGRATION" or \
            not isinstance(evidence.get("reason"), str) or not evidence["reason"]:
        errors.append("historical visual evidence boundary differs")

    boundaries = value.get("implementation_boundaries")
    if not isinstance(boundaries, dict) or \
            "src/Billiards/product_shell_controller.cpp" not in boundaries.get("allowed_product_sources", []) or \
            "src/Billiards/render/modern/modern_product_session.cpp" not in boundaries.get("allowed_product_sources", []) or \
            "tests/product_shell_tests.cpp" not in boundaries.get("allowed_cpu_tests", []) or \
            "GPU execution" not in boundaries.get("forbidden", []) or \
            "rulesDecisionUiProductExposed enablement" not in boundaries.get("forbidden", []):
        errors.append("implementation boundary differs")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--gate", type=Path, default=DEFAULT_GATE)
    parser.add_argument("--no-file-checks", action="store_true")
    args = parser.parse_args()
    try:
        value = json.loads(args.gate.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        print(json.dumps({"status": "FAIL", "errors": [str(error)]}, sort_keys=True))
        return 1
    errors = validate(value, check_files=not args.no_file_checks)
    print(json.dumps({
        "status": "PASS_WPA_II_04_OFFICIAL_RACK_PRODUCT_MIGRATION_GATE" if not errors else "FAIL",
        "errors": errors,
        "gpu_executed": False,
    }, sort_keys=True))
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
