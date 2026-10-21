#!/usr/bin/env python3
"""Validate the design-only WPA-II-08 all-behind re-spot product gate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GATE = ROOT / "docs/rules/wpa-ii-08-all-behind-respot-product-subflow-implementation-design-gate-v1.json"
TOP_LEVEL_KEYS = {
    "schema", "status", "authority_bindings", "scope_and_authority",
    "candidate_contract", "transaction_contract", "telemetry_successor_contract",
    "implementation_allowlist", "required_fixtures", "source_bindings",
    "test_source_bindings", "cmake_binding", "completion_boundary", "claims",
}
DIGESTS = {
    "authority_bindings": "80f784aa414b50ab305a8fab7b0ca07fb35459e141aab5497eb1fc403d573ada",
    "scope_and_authority": "581fe05789d444ae820788c778b79635b075d722dba904409e561d5711d8105a",
    "candidate_contract": "b6c04add7e3063c9310064ee364e6ffae116b9d76d0741d6f92fac64119a826f",
    "transaction_contract": "3aeceea1ea9c5fa0d40460273e8830277fa7ec6167b58b6f41ebb85c1c8c9d1a",
    "telemetry_successor_contract": "85cd8b947005c5761ccf86d6da26fc86f8f35fe2193a66954eb01bb616089dd7",
    "implementation_allowlist": "5da76dafbe52e272f2acfd4be6cb348599a30e6cf8bdd134b1762fb470485ced",
    "required_fixtures": "6d287bfd7e7ca8ed0ae5343f60741484f707fa6314bf6fa85fff879a2dc22020",
    "source_bindings": "6a6db073c87fe8593cbdfc843e3cef16b53b514b7a9478fabaa54dab6d45b297",
    "test_source_bindings": "8c309f8d3d5c4849007272d4631e6557a0560b2c90375412648c161bc7ad76c0",
    "cmake_binding": "3b6a1025b6fa25b220b0ceffbd54824f7d30f93e01bbed64394940f91f4b9779",
    "completion_boundary": "fe1f13f7605112a82e8ca263616b5ceb307402ed86553a039d6532076e1b3a95",
    "claims": "fd3b54c6b17d660afc1943a9e0ed5d6bba84e132531f48c2474c0dfc8ba78d71",
}
PRODUCTION = [
    "src/Billiards/rules/ball_in_hand_authority.h",
    "src/Billiards/rules/chinese_eight_ball_rules.h",
    "src/Billiards/rules/chinese_eight_ball_rules.cpp",
    "src/Billiards/game_runtime.h", "src/Billiards/game_runtime.cpp",
    "src/Billiards/render/modern/modern_product_session.h",
    "src/Billiards/render/modern/modern_product_session.cpp",
]
TESTS = [
    "tests/chinese_eight_ball_rules_tests.cpp",
    "tests/rules_runtime_integration_tests.cpp",
    "tests/rules_decision_session_integration_tests.cpp",
]
POSITIVE = [
    "OPEN_TABLE_ALL_BEHIND_UNIQUE_CLOSEST", "ASSIGNED_GROUP_ALL_BEHIND_UNIQUE_CLOSEST",
    "GROUP_CLEARED_EIGHT_ONLY_CLOSEST", "EXACT_TIE_STAGES_ASCENDING_PLAYER_CHOICE",
    "SELECTED_TIE_BALL_RESPOTS_AT_FOOT_SPOT", "OBSTRUCTED_FOOT_SPOT_USES_SECTION7_SEARCH",
    "SUCCESS_PRESERVES_ACTOR_AND_REPLACES_ONE_CORE_G_CHILD",
    "EXACT_RETRY_RETURNS_ORIGINAL_ACK_ZERO_DELTA",
]
NEGATIVE = [
    "NO_LEGAL_OBJECT_REJECTS", "LEGAL_OBJECT_ON_LINE_SUPPRESSES_OFFER",
    "LEGAL_OBJECT_FOOT_SIDE_SUPPRESSES_OFFER",
    "OPPONENT_GROUP_AND_EIGHT_EXCLUDED_UNTIL_GROUP_CLEARED",
    "WRONG_ACTOR_ACTION_FLOW_DECISION_REVISION_OR_HASH_ROLLS_BACK",
    "NONFINITE_OBJECT_GEOMETRY_ROLLS_BACK",
    "STALE_LAYOUT_BETWEEN_STAGE_AND_COMMIT_ROLLS_BACK",
    "FOUNDATION_NO_SOLUTION_PRESERVES_OLD_CORE_G_CHILD",
    "FOUNDATION_OR_SIBLING_ACTION_CANNOT_ENTER_PRODUCT_SEAM",
    "CONFLICTING_RETRY_ROLLS_BACK", "RESET_INVALIDATES_PARENT_TIE_AND_PRODUCT_RETRY",
]


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def digest(value: Any) -> str:
    return sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode())


def resolve(raw: Any) -> Path | None:
    if not isinstance(raw, str): return None
    path = Path(raw)
    if path.is_absolute() or ".." in path.parts: return None
    result = (ROOT / path).resolve()
    return result if result.is_relative_to(ROOT.resolve()) else None


def binding_ok(binding: Any) -> bool:
    if not isinstance(binding, dict) or set(binding) != {"path", "sha256"}: return False
    path = resolve(binding.get("path"))
    return path is not None and path.is_file() and sha256(path.read_bytes()) == binding.get("sha256")


def validate(value: Any, check_files: bool = True) -> list[str]:
    if not isinstance(value, dict): return ["root_not_object"]
    errors: list[str] = []
    if set(value) != TOP_LEVEL_KEYS: errors.append("top_level_keys")
    if value.get("schema") != "wpa_ii_08_all_behind_respot_product_subflow_implementation_design_gate_v1": errors.append("schema")
    if value.get("status") != "DESIGN_COMPLETE_AWAITING_ROOT_IMPLEMENTATION_APPROVAL_NO_PRODUCTION_MUTATION": errors.append("status")
    for key, expected in DIGESTS.items():
        if key not in value or digest(value.get(key)) != expected: errors.append(f"contract_digest:{key}")

    authority = value.get("authority_bindings")
    selected = authority.get("root_selected_policy") if isinstance(authority, dict) else None
    original = authority.get("original_design_gap_gate") if isinstance(authority, dict) else None
    telemetry = authority.get("telemetry_implementation_receipt") if isinstance(authority, dict) else None
    if not isinstance(selected, dict) or selected.get("sha256") != "fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f" or selected.get("selected_option_id") != "CENTER_CROSSING_EXACT_CONTINUOUS_ORDER": errors.append("selection")
    if not isinstance(original, dict) or original.get("sha256") != "2219bab50689979b77e6ba92630c4afae3276396993a73ac81b12e9e968acefe": errors.append("original_gate")
    if not isinstance(telemetry, dict) or telemetry.get("sha256") != "86379eecd2437a6e17643d0964f4460b857615f0eddbc50ab7dbeaefe3afa878" or telemetry.get("root_independently_verified") is not True: errors.append("telemetry_receipt")

    scope = value.get("scope_and_authority")
    for key in ("foundation_action_product_exposed", "global_rules_decision_ui_product_exposed", "product_may_reclassify_legal_balls"):
        if not isinstance(scope, dict) or scope.get(key) is not False: errors.append(f"scope_false:{key}")
    if not isinstance(scope, dict) or scope.get("request_action") != "rules.ball_in_hand.request_nearest_legal_respot" or scope.get("tie_choice_action") != "rules.ball_in_hand.choose_equidistant_respot_ball": errors.append("actions")

    candidate = value.get("candidate_contract")
    if not isinstance(candidate, dict) or "same open-table/assigned-group/group-cleared" not in str(candidate.get("legal_object_predicate")) or candidate.get("on_line") != "exact equality is not behind and suppresses the offer" or "maximum world_z_cm" not in str(candidate.get("closest")) or "ascending canonical ball IDs" not in str(candidate.get("tie")): errors.append("candidate")
    tx = value.get("transaction_contract")
    if not isinstance(tx, dict) or "Core-E" not in str(tx.get("unique_request")) or "one fresh decision identity" not in str(tx.get("successful_child")) or tx.get("partial_commit_allowed") is not False: errors.append("transaction")
    successor = value.get("telemetry_successor_contract")
    if not isinstance(successor, dict) or successor.get("unchanged_implementation_receipt_bound") is not True or successor.get("restriction_arms_only_after_subsequent_Core_G_placement_commit") is not True or successor.get("required_regression") != "all exact 8 positive and 9 negative telemetry fixtures remain green": errors.append("telemetry_successor")

    allowlist = value.get("implementation_allowlist")
    if not isinstance(allowlist, dict) or allowlist.get("production_files") != PRODUCTION or allowlist.get("test_files") != TESTS: errors.append("allowlist")
    for key in ("new_production_file_allowed", "rack_executor_edit_allowed", "cmake_change_allowed", "visual_ui_or_renderer_allowed"):
        if not isinstance(allowlist, dict) or allowlist.get(key) is not False: errors.append(f"allowlist_false:{key}")
    fixtures = value.get("required_fixtures")
    if not isinstance(fixtures, dict) or fixtures.get("positive") != POSITIVE or fixtures.get("negative") != NEGATIVE or fixtures.get("telemetry_regression_positive_count") != 8 or fixtures.get("telemetry_regression_negative_count") != 9 or fixtures.get("headless_cpu_only") is not True: errors.append("fixtures")

    sources, tests = value.get("source_bindings"), value.get("test_source_bindings")
    if not isinstance(sources, list) or [x.get("path") for x in sources if isinstance(x, dict)] != PRODUCTION: errors.append("source_bindings")
    if not isinstance(tests, list) or [x.get("path") for x in tests if isinstance(x, dict)] != TESTS: errors.append("test_bindings")
    if check_files:
        for label, rows in (("source", sources), ("test", tests)):
            if isinstance(rows, list):
                for row in rows:
                    if not binding_ok(row): errors.append(f"{label}_drift")
        for label, binding in (("selection", selected), ("original", original), ("telemetry", telemetry)):
            if not isinstance(binding, dict) or not binding_ok({"path": binding.get("path"), "sha256": binding.get("sha256")}): errors.append(f"{label}_drift")

    cmake = value.get("cmake_binding")
    if not isinstance(cmake, dict) or cmake.get("modified_in_gate") is not False or cmake.get("collector_target_link_gap_must_remain_honestly_recorded") is not True: errors.append("cmake")
    elif check_files and not binding_ok({"path": cmake.get("path"), "sha256": cmake.get("sha256")}): errors.append("cmake_drift")
    boundary = value.get("completion_boundary")
    if not isinstance(boundary, dict) or boundary.get("implementation_authorized_by_this_gate") is not False or boundary.get("root_revalidation_required_before_tests_first_patch") is not True or boundary.get("gpu_ta_readiness_or_authorization") is not False: errors.append("completion_boundary")
    claims = value.get("claims")
    if not isinstance(claims, dict) or claims.get("design_complete") is not True: errors.append("design_claim")
    for key in ("implementation_complete", "all_behind_product_reachable", "wpa_ii_08_complete", "production_modified", "cmake_modified", "visual_ui_modified", "gpu_executed"):
        if not isinstance(claims, dict) or claims.get(key) is not False: errors.append(f"false_completion:{key}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("gate", nargs="?", default=str(DEFAULT_GATE))
    parser.add_argument("--no-file-checks", action="store_true")
    args = parser.parse_args()
    try: value = json.loads(Path(args.gate).read_text())
    except (OSError, json.JSONDecodeError) as exc:
        print(f"INVALID: {exc}"); return 1
    errors = validate(value, check_files=not args.no_file_checks)
    if errors: print("INVALID: " + ", ".join(errors)); return 1
    print("PASS_WPA_II_08_ALL_BEHIND_RESPOT_PRODUCT_SUBFLOW_IMPLEMENTATION_DESIGN_GATE")
    return 0


if __name__ == "__main__": raise SystemExit(main())
