#!/usr/bin/env python3
"""Validate the CPU-only WPA II 6(f) entitlement timing implementation."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_RECEIPT = ROOT / "docs/rules/wpa-ii-06-f-entitlement-timing-implementation-receipt-v1.json"
EXPECTED_STATUS = "IMPLEMENTED_CPU_VERIFIED_AUDIT_REMAINS_PARTIAL_NO_GPU"
EXPECTED_CONTRACT_SHA = "1ce76ae3cc72f71f2d458c107454d291a527f9f7080901b30ba07db25ea7809f"
EXPECTED_DESIGN_GATE = {
    "path": "docs/rules/wpa-ii-06-f-eight-on-break-choice-design-gap-gate-v1.json",
    "sha256": "f008d99e222289ba8279518a4aa21577d238d793140ae4071598684cc3939ae9",
    "frozen_contract_sha256": "dd55f95a16cb28e4d0185e35dd7bf80de0b7b95500b2798eb5972589327286e4",
}
EXPECTED_SOURCES: dict[str, dict[str, str]] = {
    "production_before": {"path": "src/Billiards/game_runtime.cpp", "sha256": "3292df4f4546d9fd1639de8d5db33b7dab526f13b129f496d1420a489253fb9a"},
    "production_after": {"path": "src/Billiards/game_runtime.cpp", "sha256": "ea6f3754a9a08cca0447e9fed462de9961f9bb6c8c507f71b78c68ce7ce89fd5"},
    "test_before": {"path": "tests/rules_runtime_integration_tests.cpp", "sha256": "2abb5a981251e05b179c03fb82adc2bd3da1076f291a4c8e0a174743baef8a55"},
    "test_after": {"path": "tests/rules_runtime_integration_tests.cpp", "sha256": "c5030b6330432e4f54437239d2589f2686ad5840c28612600197cd16eeef8d3b"},
    "runtime_header_unchanged": {"path": "src/Billiards/game_runtime.h", "sha256": "deacceaa4ffff8b0e5080963d958f5fc49a77a197c744c2693be6f2d82b96ec3"},
    "core_f_contract_unchanged": {"path": "src/Billiards/rules/eight_on_break_authority.h", "sha256": "dacb83140b687b285f1dc67bcbb38d571349a60caa01a45664f0bbdcc2981b2e"},
    "core_g_contract_unchanged": {"path": "src/Billiards/rules/ball_in_hand_authority.h", "sha256": "026fd6196263b1c6dab180fa7f94d29baf277ff56bdff450ca189e7d8ba795d6"},
    "core_e_implementation_unchanged": {"path": "src/Billiards/rules/rack_executor.cpp", "sha256": "d1805c8e197f6b1a8f712f80ba8be807217e875d56def3c7d00186293cd9fb17"},
    "adjudicator_unchanged": {"path": "src/Billiards/rules/chinese_eight_ball_rules.cpp", "sha256": "5a298f2bd492dfd6f57b8bb478cabd1a3041716e4411c93642e1aeb85b8727b3"},
    "product_session_unchanged": {"path": "src/Billiards/render/modern/modern_product_session.cpp", "sha256": "8e962dfc76149a4461e011941231e5e72860aed9f8cb6f006169e269455cd174"},
    "audit_v3_unchanged": {"path": "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v3.json", "sha256": "d7ab2104fac9fd85fe8d7336ae658cc9baaaa6b4fa866cd4e7815c51404ad033"},
}
LIVE_SOURCE_KEYS = {
    "production_after", "test_after", "runtime_header_unchanged",
    "core_f_contract_unchanged", "core_g_contract_unchanged",
    "core_e_implementation_unchanged", "adjudicator_unchanged",
    "product_session_unchanged", "audit_v3_unchanged",
}
EXPECTED_CPU = {
    "BilliardsRulesRuntimeIntegrationTests": "952aa779877870ad46975e252adc3893742aa0a9b625cded5ed2a12b4cf95bac",
    "BilliardsRulesDecisionSessionIntegrationTests": "44c54d235078bddce609aa61f9a645107b13b1e76ad90600d36b60e0620841c2",
    "BilliardsRulesDecisionProductRuntimeTests": "4be7e994da0c44a14b380e0f8fbbc856dbad73ea3c2652db5ab50ec765912adf",
    "BilliardsAutomationControllerTests": "bf7187884ff288febbde27716d5d7ad2b476b194ca05a5b0c08e94cd604d3472",
    "BilliardsModernRenderFoundationTests": "6b8598a237cf8156370b15c963b6aa93bfbee556b5385e799c136ed1cb736dd6",
    "BilliardsCameraShotModelTests": "82cd8468dfe195b32008293b89751ab7ec086eae5a2cd872c0e61f265d27389c",
}


def canonical_sha(value: Any) -> str:
    payload = json.dumps(
        value, sort_keys=True, separators=(",", ":"), ensure_ascii=False
    ).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def file_sha(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def safe_path(raw: Any) -> Path | None:
    if not isinstance(raw, str) or not raw or Path(raw).is_absolute() or \
            ".." in Path(raw).parts:
        return None
    path = (ROOT / raw).resolve()
    try:
        path.relative_to(ROOT.resolve())
    except ValueError:
        return None
    return path


def validate(value: Any, *, check_files: bool = True) -> list[str]:
    errors: list[str] = []
    if not isinstance(value, dict):
        return ["receipt_not_object"]
    if set(value) != {
        "schema", "status", "design_gate", "source_bindings",
        "implementation_contract_sha256", "implementation_contract",
    }:
        errors.append("top_level_keys")
    if value.get("schema") != "wpa_ii_06_f_entitlement_timing_implementation_receipt_v1":
        errors.append("schema")
    if value.get("status") != EXPECTED_STATUS:
        errors.append("status")
    if value.get("design_gate") != EXPECTED_DESIGN_GATE:
        errors.append("design_gate")

    contract = value.get("implementation_contract")
    if not isinstance(contract, dict):
        errors.append("implementation_contract")
    else:
        if value.get("implementation_contract_sha256") != EXPECTED_CONTRACT_SHA:
            errors.append("implementation_contract_declared_sha")
        if canonical_sha(contract) != EXPECTED_CONTRACT_SHA:
            errors.append("implementation_contract_content_sha")
        scope = contract.get("change_scope", {})
        if scope.get("production_files_changed") != ["src/Billiards/game_runtime.cpp"] or \
                scope.get("test_files_changed") != ["tests/rules_runtime_integration_tests.cpp"] or \
                scope.get("cmake_changed") is not False or \
                scope.get("modern_product_session_changed") is not False:
            errors.append("change_scope")
        predecessor = contract.get("predecessor_design_gate_live_validation", {})
        if predecessor != {
            "status": "EXPECTED_APPROVED_SUCCESSOR_SOURCE_DRIFT",
            "exact_errors": ["source_runtime_drift", "source_runtime_tests_drift"],
            "all_other_gate_bindings_stable": True,
            "predecessor_gate_or_validator_modified_to_hide_drift": False,
        }:
            errors.append("predecessor_design_gate_live_validation")
        staging = contract.get("production_correction", {}).get("foul_parent_staging", {})
        if staging != {
            "match_state_ball_in_hand": False,
            "last_adjudication_ball_in_hand": False,
            "last_adjudication_ball_in_hand_behind_head_string": False,
            "ball_in_hand_placement_scope": "None",
            "ball_in_hand_placement_pending": False,
            "ball_in_hand_placement_offer_exists": False,
        }:
            errors.append("foul_parent_staging")
        spot = contract.get("production_correction", {}).get("spot_plus_bih_commit", {})
        if spot.get("requires_exact_action") != \
                "rules.eight_on_break.spot_ball_in_hand_head_string" or \
                spot.get("one_core_g_child_created") is not True or \
                spot.get("physical_cue_ball_placement_in_parent") is not False or \
                spot.get("last_adjudication_ball_in_hand_after_commit") is not True or \
                spot.get("last_adjudication_ball_in_hand_behind_head_string_after_commit") is not True or \
                spot.get("last_adjudication_next_state_matches_match_state") is not True:
            errors.append("spot_plus_bih_commit")
        consumers = contract.get("production_correction", {}).get("consumer_consistency", {})
        if consumers != {
            "automation_last_adjudication_reads_scalar_ball_in_hand": True,
            "rules_presentation_reads_match_state_ball_in_hand": True,
            "accepted_parent_now_keeps_ack_match_state_last_adjudication_and_child_scope_consistent": True,
        }:
            errors.append("consumer_consistency")
        boundary = contract.get("audit_and_product_boundary", {})
        if boundary.get("audit_status") != "PARTIAL" or \
                boundary.get("complete_clause_6f_product_reachable") is not False or \
                boundary.get("rules_decision_ui_product_exposed") is not False or \
                boundary.get("existing_narrow_spot_seam_broadened") is not False or \
                boundary.get("upstream_foul_classification_status") != \
                "UPSTREAM_BREAK_FOUL_CLASSIFICATION_PENDING":
            errors.append("audit_or_product_boundary")
        absent = contract.get("forbidden_and_absent", {})
        if any(absent.get(key) is not False for key in (
            "gpu_cocoa_or_opengl_executed", "billiards_product_executable_started",
            "readiness_generated", "authorization_issued",
            "phase4_or_phase5_renderer_changed", "visual_or_evidence_threshold_changed",
            "official_policy_invented",
        )):
            errors.append("forbidden_or_absent")
        claims = contract.get("claims", {})
        if claims.get("wpa_06f_entitlement_timing_implemented") is not True or \
                claims.get("headless_cpu_verified") is not True or \
                claims.get("complete_clause_6f_product_reachable") is not False or \
                claims.get("audit_promoted") is not False or \
                claims.get("classification_complete") is not False or \
                claims.get("gpu_executed") is not False:
            errors.append("claims")
        cpu = contract.get("cpu_results")
        if not isinstance(cpu, list) or len(cpu) != len(EXPECTED_CPU):
            errors.append("cpu_results")
        else:
            actual = {row.get("target"): row for row in cpu if isinstance(row, dict)}
            if set(actual) != set(EXPECTED_CPU):
                errors.append("cpu_result_targets")
            for target, digest in EXPECTED_CPU.items():
                row = actual.get(target, {})
                if row.get("sha256") != digest or row.get("exit_code") != 0:
                    errors.append(f"cpu_result_{target}")

    sources = value.get("source_bindings")
    if sources != EXPECTED_SOURCES:
        errors.append("source_bindings")
    if isinstance(sources, dict) and check_files:
        for name in LIVE_SOURCE_KEYS:
            binding = sources.get(name, {})
            path = safe_path(binding.get("path"))
            if path is None:
                errors.append(f"source_{name}_unsafe")
            elif not path.is_file():
                errors.append(f"source_{name}_missing")
            elif file_sha(path) != binding.get("sha256"):
                errors.append(f"source_{name}_drift")
        gate_path = safe_path(EXPECTED_DESIGN_GATE["path"])
        if gate_path is None or not gate_path.is_file() or \
                file_sha(gate_path) != EXPECTED_DESIGN_GATE["sha256"]:
            errors.append("design_gate_drift")
        build_root = ROOT / "build-wpa-06f-entitlement-cpu-v1"
        for target, digest in EXPECTED_CPU.items():
            path = build_root / target
            if not path.is_file() or file_sha(path) != digest:
                errors.append(f"cpu_binary_{target}_drift")

        runtime_text = (ROOT / "src/Billiards/game_runtime.cpp").read_text(encoding="utf-8")
        required_runtime = (
            "const bool eightOnBreakChoiceRequired =",
            "if (eightOnBreakChoiceRequired && adjudication.foul)",
            "matchState_.ballInHand = false;",
            "lastAdjudication_.ballInHandBehindHeadString = false;",
            "lastAdjudication_.ballInHand = foulSpot;",
            "lastAdjudication_.ballInHandBehindHeadString = foulSpot;",
            "!eightOnBreakChoiceRequired)",
            "if (eightOnBreakChoiceRequired)",
        )
        if any(token not in runtime_text for token in required_runtime):
            errors.append("runtime_static_contract")
        test_text = (ROOT / "tests/rules_runtime_integration_tests.cpp").read_text(encoding="utf-8")
        required_tests = (
            "foul 8-on-break parent stages with no BIH entitlement",
            "wrong foul actor cannot grant an early BIH entitlement",
            "foul respot failure rolls back without early or residual BIH state",
            "rerack retry/conflict cannot create or retain BIH state",
            "foul parent reset invalidates the request without residual BIH",
            "lastAdjudication().ballInHandBehindHeadString",
        )
        if any(token not in test_text for token in required_tests):
            errors.append("test_static_contract")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--receipt", type=Path, default=DEFAULT_RECEIPT)
    parser.add_argument("--skip-file-checks", action="store_true")
    args = parser.parse_args()
    try:
        value = json.loads(args.receipt.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        print(json.dumps({"status": "FAIL", "errors": [f"load:{error}"]}))
        return 1
    errors = validate(value, check_files=not args.skip_file_checks)
    print(json.dumps({
        "status": "PASS" if not errors else "FAIL",
        "errors": errors,
        "receipt": str(args.receipt),
    }, sort_keys=True))
    if errors:
        return 1
    print("PASS_WPA_II_06_F_ENTITLEMENT_TIMING_IMPLEMENTATION")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
