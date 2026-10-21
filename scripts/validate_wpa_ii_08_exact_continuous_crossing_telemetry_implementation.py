#!/usr/bin/env python3
"""Validate the WPA-II-08 exact continuous crossing implementation receipt."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_RECEIPT = ROOT / "docs/rules/wpa-ii-08-exact-continuous-crossing-telemetry-implementation-receipt-v1.json"
TOP_LEVEL_KEYS = {
    "schema", "status", "authority_bindings", "implementation_contract",
    "fixture_matrix", "source_bindings", "test_source_bindings",
    "implementation_aggregate_sha256", "cmake_binding", "cpu_verification",
    "deferred_boundary", "claims",
}
DIGESTS = {
    "authority_bindings": "f4aa0409e355a61d35a8b8fef295f938e1b2a67c8b6d0c7d15ab8cbd12e5fbea",
    "implementation_contract": "0f9b7b6f73d496fb563937b5a1d88cff61fd870d0965d1346682395918bc7415",
    "fixture_matrix": "ee067365f1cac9ac8475c671c40552db5c27c03c31e973940e6b7a5988b77979",
    "source_bindings": "ff63915f7f0732fbe88ea3b53e47ea433b1b0bc6147728bb01d51791f2663361",
    "test_source_bindings": "c12d6d33719f6cd95ef835bef46d7f3f5e93c9af23da77dff15f96e0dadecbbd",
    "implementation_aggregate_sha256": "69fb136b8c9fb74d725e620b94a33ee7de809923e9c6da7bdc697720161af909",
    "cmake_binding": "83b56c79ce45b39ac8a24bac0f0aa434e9c9bbc5390d971c540e4112cf8c7847",
    "cpu_verification": "a6ea95ee00542dba0a113008de59d6da294fdfb75c35669b3b242618fd1e7dfc",
    "deferred_boundary": "45ea1d33d6bd6ead04f05ae496fb689e8365fda1e10722e7ae92658dc2e02030",
    "claims": "be769aa38750926c351fdd8d70d9704f243e16f4d845a98559b27b816b8693d3",
}
PRODUCTION_PATHS = [
    "src/Billiards/surface_motion.h", "src/Billiards/surface_motion.cpp",
    "src/Billiards/physics.h", "src/Billiards/physics_telemetry.h",
    "src/Billiards/physics.cpp", "src/Billiards/rules/chinese_eight_ball_rules.h",
    "src/Billiards/rules/chinese_eight_ball_rules.cpp",
    "src/Billiards/rules/shot_fact_collector.h",
    "src/Billiards/rules/shot_fact_collector.cpp", "src/Billiards/game_runtime.h",
    "src/Billiards/game_runtime.cpp",
]
TEST_PATHS = [
    "tests/surface_motion_tests.cpp", "tests/physics_tests.cpp",
    "tests/physics_telemetry_tests.cpp", "tests/shot_fact_collector_tests.cpp",
    "tests/chinese_eight_ball_rules_tests.cpp", "tests/rules_runtime_integration_tests.cpp",
]
POSITIVE_IDS = [
    "CROSSING_IN_EARLIER_TICK_THAN_CONTACT", "SAME_TICK_CROSSING_STRICTLY_BEFORE_CONTACT",
    "CROSSING_BEFORE_CUSHION_THEN_RETURN_CONTACT", "SLIDING_QUADRATIC_CROSSING_BEFORE_CONTACT",
    "SLIDING_TO_ROLLING_PIECEWISE_CROSSING_BEFORE_CONTACT",
    "ON_LINE_OBJECT_DIRECT_CONTACT_WITHOUT_CROSSING",
    "FOOT_SIDE_OBJECT_DIRECT_CONTACT_WITHOUT_CROSSING",
    "EXACT_DUPLICATE_SUCCESSFUL_TICK_AND_RECORD_ZERO_DELTA",
]
NEGATIVE_IDS = [
    "SAME_TICK_CONTACT_BEFORE_CROSSING", "SAME_TIME_CROSSING_AND_CONTACT_UNORDERED",
    "ENDPOINTS_CROSS_BUT_INTERMEDIATE_CUSHION_ORDER_UNATTESTED",
    "FAILED_PHYSICS_STEP_CROSSING_DISCARDED_CONTEXT_PRESERVED",
    "MISSING_OR_INCOMPLETE_TICK_ATTESTATION_FAILS_CLOSED",
    "NONFINITE_PLANE_OR_CROSSING_RECORD_FAILS_CLOSED",
    "CONFLICTING_DUPLICATE_TICK_OR_RECORD_ZERO_PARTIAL_DELTA",
    "DIRECT_OR_SYNTHETIC_FACTS_CANNOT_CLAIM_CROSSING",
    "REJECTED_LAUNCH_WRONG_ACTOR_OR_RESET_CANNOT_LEAK_STALE_EVIDENCE",
]


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def canonical_digest(value: Any) -> str:
    return sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode("utf-8"))


def resolve(raw: Any) -> Path | None:
    if not isinstance(raw, str):
        return None
    path = Path(raw)
    if path.is_absolute() or ".." in path.parts:
        return None
    resolved = (ROOT / path).resolve()
    return resolved if resolved.is_relative_to(ROOT.resolve()) else None


def binding_ok(binding: Any) -> bool:
    if not isinstance(binding, dict) or set(binding) != {"path", "sha256"}:
        return False
    path = resolve(binding.get("path"))
    return path is not None and path.is_file() and sha256(path.read_bytes()) == binding.get("sha256")


def validate(value: Any, check_files: bool = True) -> list[str]:
    if not isinstance(value, dict):
        return ["root_not_object"]
    errors: list[str] = []
    if set(value) != TOP_LEVEL_KEYS:
        errors.append("top_level_keys")
    if value.get("schema") != "wpa_ii_08_exact_continuous_crossing_telemetry_implementation_receipt_v1":
        errors.append("schema")
    if value.get("status") != "IMPLEMENTED_POLICY_SELECTED_TELEMETRY_FOUNDATION_CPU_VERIFIED_NO_ALL_BEHIND_PRODUCT":
        errors.append("status")
    for key, digest in DIGESTS.items():
        if key not in value or canonical_digest(value.get(key)) != digest:
            errors.append(f"contract_digest:{key}")

    authority = value.get("authority_bindings")
    gate = authority.get("implementation_design_gate") if isinstance(authority, dict) else None
    selected = authority.get("root_selected_policy") if isinstance(authority, dict) else None
    blocker = authority.get("feasibility_blocker") if isinstance(authority, dict) else None
    if not isinstance(gate, dict) or gate.get("sha256") != \
            "8afb026f14f62f96b0f18da2a18c05c68ca97478a74ef6329a0e6c4f2173bb7e" or \
            gate.get("root_independently_validated") is not True or \
            gate.get("root_authorized_exact_allowlist_tests_first_implementation") is not True:
        errors.append("gate_authority")
    if not isinstance(selected, dict) or selected.get("sha256") != \
            "fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f" or \
            selected.get("selected_option_id") != "CENTER_CROSSING_EXACT_CONTINUOUS_ORDER":
        errors.append("selected_policy")
    if not isinstance(blocker, dict) or blocker.get("sha256") != \
            "f10525b49619c2a67e6ae0530da1c0eb8b80a54a0d64e134e42158a64ea3095d" or \
            blocker.get("telemetry_blocker_resolved_by_this_receipt") is not True or \
            blocker.get("all_behind_parent_product_blocker_resolved") is not False:
        errors.append("blocker_lineage")

    contract = value.get("implementation_contract")
    if not isinstance(contract, dict) or contract.get("coverage_contiguity_tolerance_seconds") != 0.000001 or \
            contract.get("coverage_tolerance_is_not_an_order_tie_breaker") is not True or \
            "exact whole-tick" not in str(contract.get("order_policy")) or \
            contract.get("direct_or_synthetic_complete_claim_allowed") is not False or \
            contract.get("product_reclassification_allowed") is not False:
        errors.append("implementation_contract")

    fixtures = value.get("fixture_matrix")
    positive = fixtures.get("positive") if isinstance(fixtures, dict) else None
    negative = fixtures.get("negative") if isinstance(fixtures, dict) else None
    if not isinstance(positive, list) or [row.get("id") for row in positive if isinstance(row, dict)] != POSITIVE_IDS or \
            not isinstance(negative, list) or [row.get("id") for row in negative if isinstance(row, dict)] != NEGATIVE_IDS or \
            fixtures.get("positive_count") != 8 or fixtures.get("negative_count") != 9 or \
            fixtures.get("all_headless_cpu") is not True:
        errors.append("fixture_matrix")
    else:
        for row in positive + negative:
            if not isinstance(row.get("evidence"), str) or not row["evidence"].startswith("tests/"):
                errors.append("fixture_evidence")

    sources = value.get("source_bindings")
    tests = value.get("test_source_bindings")
    if not isinstance(sources, list) or [row.get("path") for row in sources if isinstance(row, dict)] != PRODUCTION_PATHS:
        errors.append("source_allowlist")
    elif value.get("implementation_aggregate_sha256") != canonical_digest(sources):
        errors.append("implementation_aggregate")
    if not isinstance(tests, list) or [row.get("path") for row in tests if isinstance(row, dict)] != TEST_PATHS:
        errors.append("test_allowlist")
    if check_files:
        for label, rows in (("source", sources), ("test", tests)):
            if isinstance(rows, list):
                for row in rows:
                    if not binding_ok(row):
                        errors.append(f"{label}_drift:{row.get('path', 'unknown') if isinstance(row, dict) else 'malformed'}")
        for binding, label in ((gate, "gate"), (selected, "selection"), (blocker, "blocker")):
            if not isinstance(binding, dict) or not binding_ok({"path": binding.get("path"), "sha256": binding.get("sha256")}):
                errors.append(f"{label}_drift")

    cmake = value.get("cmake_binding")
    if not isinstance(cmake, dict) or cmake.get("modified_in_task") is not False:
        errors.append("cmake_boundary")
    elif check_files and not binding_ok({"path": cmake.get("path"), "sha256": cmake.get("sha256")}):
        errors.append("cmake_drift")

    cpu = value.get("cpu_verification")
    required = cpu.get("required_six") if isinstance(cpu, dict) else None
    wide = cpu.get("wide_regression") if isinstance(cpu, dict) else None
    if not isinstance(required, list) or len(required) != 6 or any(row.get("exit_code") != 0 for row in required) or \
            not isinstance(wide, list) or len(wide) != 8 or any(row.get("exit_code") != 0 for row in wide) or \
            cpu.get("required_passed") != 6 or cpu.get("wide_passed") != 8 or cpu.get("failed") != 0 or \
            cpu.get("gpu_cocoa_opengl_executed") is not False:
        errors.append("cpu_verification")
    elif check_files:
        for row in required:
            if not binding_ok({"path": row.get("path"), "sha256": row.get("sha256")}):
                errors.append(f"binary_drift:{row.get('path', 'unknown')}")
    workaround = cpu.get("shot_fact_collector_compile_workaround") if isinstance(cpu, dict) else None
    if not isinstance(workaround, dict) or workaround.get("cmake_modified_to_hide_gap") is not False:
        errors.append("compile_workaround_boundary")

    deferred = value.get("deferred_boundary")
    for key in ("all_behind_parent_authority_implemented", "all_behind_respot_request_or_commit_implemented",
                "all_behind_product_action_reachable", "global_rules_ui_exposed",
                "audit_v5_modified_or_promoted", "wpa_ii_08_row_closed",
                "fresh_gpu_ta_or_readiness_created"):
        if not isinstance(deferred, dict) or deferred.get(key) is not False:
            errors.append(f"deferred_false:{key}")
    if not isinstance(deferred, dict) or deferred.get("requires_separate_post_telemetry_gate") is not True:
        errors.append("separate_gate_required")

    claims = value.get("claims")
    for key in ("root_selected_telemetry_foundation_implemented",
                "exact_continuous_crossing_order_cpu_verified",
                "failed_step_and_duplicate_boundaries_cpu_verified"):
        if not isinstance(claims, dict) or claims.get(key) is not True:
            errors.append(f"claim_true:{key}")
    for key in ("all_behind_respot_implemented", "product_action_reachable", "wpa_ii_08_complete",
                "cmake_modified", "ui_modified", "gpu_executed", "readiness_or_authorization_created"):
        if not isinstance(claims, dict) or claims.get(key) is not False:
            errors.append(f"false_completion:{key}")

    if check_files:
        telemetry = (ROOT / "src/Billiards/physics_telemetry.h").read_text(encoding="utf-8")
        physics = (ROOT / "src/Billiards/physics.cpp").read_text(encoding="utf-8")
        collector = (ROOT / "src/Billiards/rules/shot_fact_collector.cpp").read_text(encoding="utf-8")
        facts = (ROOT / "src/Billiards/rules/chinese_eight_ball_rules.h").read_text(encoding="utf-8")
        runtime = (ROOT / "src/Billiards/game_runtime.cpp").read_text(encoding="utf-8")
        for token, text in (("PhysicsPlaneObservationRequest", telemetry),
                            ("PhysicsPlaneCrossingRecord", telemetry),
                            ("wholeTickOffsetSeconds", physics),
                            ("UNORDERED", json.dumps(value)),
                            ("headStringEvidenceComplete_", collector),
                            ("cueBallCrossedHeadStringBeforeFirstContact", facts),
                            ("wpa_ii_08_head_string_cue_center", runtime)):
            if token not in text:
                errors.append(f"implementation_token:{token}")
        product = ROOT / "src/Billiards/render/modern/modern_product_session.cpp"
        if product.is_file():
            product_text = product.read_text(encoding="utf-8")
            if "rules.ball_in_hand.request_nearest_legal_respot" in product_text or \
                    "rules.ball_in_hand.choose_equidistant_respot_ball" in product_text:
                errors.append("all_behind_product_action_unexpected")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("receipt", nargs="?", default=str(DEFAULT_RECEIPT))
    parser.add_argument("--no-file-checks", action="store_true")
    args = parser.parse_args()
    try:
        value = json.loads(Path(args.receipt).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        print(f"INVALID: {exc}")
        return 1
    errors = validate(value, check_files=not args.no_file_checks)
    if errors:
        print("INVALID: " + ", ".join(errors))
        return 1
    print("PASS_WPA_II_08_EXACT_CONTINUOUS_CROSSING_TELEMETRY_IMPLEMENTATION")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
