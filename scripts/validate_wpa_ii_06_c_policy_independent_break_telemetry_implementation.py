#!/usr/bin/env python3
"""Validate the CPU-only WPA II 6(c) policy-independent telemetry closure."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_RECEIPT = ROOT / "docs/rules/wpa-ii-06-c-policy-independent-break-telemetry-implementation-receipt-v1.json"
EXPECTED_STATUS = "POLICY_INDEPENDENT_TELEMETRY_IMPLEMENTED_CPU_VERIFIED_AUDIT_REMAINS_PARTIAL_NO_GPU"
EXPECTED_CONTRACT_SHA = "2304526073ecbc9f422f5dc96114679884f7ce0895cf07f89f425db862180c8b"
EXPECTED_GATE = {
    "path": "docs/rules/wpa-ii-06-c-legal-break-product-choice-design-gap-gate-v1.json",
    "sha256": "b5826bcf9eaf2f1c4f7110a89392885a5b269e2e8644309b42efbac4c2ba89e0",
}
EXPECTED_LIVE_SOURCES = {
    "rule_model_after": ("src/Billiards/rules/chinese_eight_ball_rules.h", "86b6bf332fe5ca182580c57f82c74ddda075fda28b659019c709bc459e785793"),
    "rule_core_after": ("src/Billiards/rules/chinese_eight_ball_rules.cpp", "b984639bdf239814e0a1e0a14a64d6b73de56841273f71d7671c20e86bf475a4"),
    "collector_header_after": ("src/Billiards/rules/shot_fact_collector.h", "88dfc28370b4b56936ac88ac560623a8600d89a295b1f98efff65471de17d615"),
    "collector_after": ("src/Billiards/rules/shot_fact_collector.cpp", "c901ba8591ebd301ccbd4a2ee4fe2ab02f390436cf80aed0596074f65e0ec4c4"),
    "runtime_after": ("src/Billiards/game_runtime.cpp", "4edccb35ba2ff09e0317c2512920ac0df40666a380abf09fc951fa99cc140926"),
    "collector_tests_after": ("tests/shot_fact_collector_tests.cpp", "4a5a4fee924cf62c29551844af718a4b73e6b5917d1b1323dcac29b20d43683c"),
    "core_tests_after": ("tests/chinese_eight_ball_rules_tests.cpp", "4c4079159e1db19545a82c1ad43b1cdfbee8ea4462b9c66c92a8be13322f3b8e"),
    "runtime_tests_after": ("tests/rules_runtime_integration_tests.cpp", "8e29fe8acaa482121058b48077319099c4bbe226db3605c8d7c7d401af551c9d"),
    "physics_telemetry_unchanged": ("src/Billiards/physics_telemetry.h", "d1fa08cb8d1c2878d7a662dd267959ffb8b505fb6afcc22351b1056a5a941178"),
    "physics_producer_unchanged": ("src/Billiards/physics.cpp", "fc497d79650bf2ff9cd8570c320653c8742e4201b8a2428a7892bfcb0124de77"),
    "runtime_header_unchanged": ("src/Billiards/game_runtime.h", "deacceaa4ffff8b0e5080963d958f5fc49a77a197c744c2693be6f2d82b96ec3"),
    "product_session_unchanged": ("src/Billiards/render/modern/modern_product_session.cpp", "8e962dfc76149a4461e011941231e5e72860aed9f8cb6f006169e269455cd174"),
    "audit_v3_unchanged": ("docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v3.json", "d7ab2104fac9fd85fe8d7336ae658cc9baaaa6b4fa866cd4e7815c51404ad033"),
}
EXPECTED_HISTORICAL = {
    "rule_model_before": "5ab7a045dbae177697a72aed97326d56ee14ae31c0434191066f011a8f52145b",
    "rule_core_before": "5a298f2bd492dfd6f57b8bb478cabd1a3041716e4411c93642e1aeb85b8727b3",
    "collector_header_before": "938134962a5a9e4c1c0ebc8a27b9213f8e242318dff73771dbbe633d65de42d1",
    "collector_before": "94fcb372a4a3c9fe16f1db12723a29de9572d8fe260ea8163fc547012ac11498",
    "runtime_immediate_before": "ea6f3754a9a08cca0447e9fed462de9961f9bb6c8c507f71b78c68ce7ce89fd5",
    "runtime_tests_immediate_before": "c5030b6330432e4f54437239d2589f2686ad5840c28612600197cd16eeef8d3b",
}
EXPECTED_CPU = {
    "BilliardsChineseEightBallRulesTests": "cb5be925dfaa1afff61e9e5740d65c47d22f81ad39e6a3812cea6eed02111e29",
    "BilliardsShotFactCollectorTests": "b4090665ce4b7a7bd74edd2f659f815dc6a55e9706ccef1842ec93811eb81c18",
    "BilliardsRulesRuntimeIntegrationTests": "781643112349505bdb105d01ae4cdc7de422340510357d02faaf1b57c187986e",
}


def canonical_sha(value: Any) -> str:
    data = json.dumps(value, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=False).encode("utf-8")
    return hashlib.sha256(data).hexdigest()


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
    result = (ROOT / raw).resolve()
    try:
        result.relative_to(ROOT.resolve())
    except ValueError:
        return None
    return result


def validate(value: Any, *, check_files: bool = True) -> list[str]:
    errors: list[str] = []
    if not isinstance(value, dict):
        return ["receipt_not_object"]
    if set(value) != {"schema", "status", "design_gate", "source_bindings",
                     "implementation_contract_sha256", "implementation_contract"}:
        errors.append("top_level_keys")
    if value.get("schema") != "wpa_ii_06_c_policy_independent_break_telemetry_implementation_receipt_v1":
        errors.append("schema")
    if value.get("status") != EXPECTED_STATUS:
        errors.append("status")
    if value.get("design_gate") != EXPECTED_GATE:
        errors.append("design_gate")

    contract = value.get("implementation_contract")
    if not isinstance(contract, dict):
        errors.append("implementation_contract")
    else:
        if value.get("implementation_contract_sha256") != EXPECTED_CONTRACT_SHA:
            errors.append("contract_declared_sha")
        if canonical_sha(contract) != EXPECTED_CONTRACT_SHA:
            errors.append("contract_content_sha")
        scope = contract.get("change_scope", {})
        if scope.get("production_files_changed") != [
            "src/Billiards/rules/chinese_eight_ball_rules.h",
            "src/Billiards/rules/chinese_eight_ball_rules.cpp",
            "src/Billiards/rules/shot_fact_collector.h",
            "src/Billiards/rules/shot_fact_collector.cpp",
            "src/Billiards/game_runtime.cpp",
        ] or scope.get("cmake_changed_by_task") is not False or \
                scope.get("physics_producer_changed") is not False or \
                scope.get("product_session_changed") is not False or \
                scope.get("phase4_or_phase5_renderer_changed") is not False:
            errors.append("change_scope")
        predecessor = contract.get("predecessor_design_gate_live_validation", {})
        expected_drift = [
            "source binding bytes/path changed: rule_core",
            "source binding bytes/path changed: rule_model",
            "source binding bytes/path changed: shot_collector",
            "source binding bytes/path changed: shot_collector_header",
            "source binding bytes/path changed: runtime",
            "source binding bytes/path changed: collector_tests",
            "source binding bytes/path changed: core_tests",
            "source binding bytes/path changed: runtime_tests",
        ]
        if predecessor.get("status") != "EXPECTED_APPROVED_SUCCESSOR_SOURCE_DRIFT" or \
                predecessor.get("exact_errors") != expected_drift or \
                predecessor.get("all_other_gate_bindings_stable") is not True or \
                predecessor.get("predecessor_gate_or_validator_modified_to_hide_drift") is not False:
            errors.append("predecessor_drift")
        fact = contract.get("canonical_fact", {})
        if fact.get("logical_name") != "break_object_cushion_contacts" or \
                fact.get("exact_duplicate_rows_removed") is not True or \
                fact.get("repeated_physical_contacts_on_later_ticks_retained") is not True or \
                fact.get("legacy_distinct_scalar_is_unique_object_id_projection") is not True:
            errors.append("canonical_fact")
        origin = contract.get("origin_and_completeness", {})
        if origin.get("authoritative_origin") != "authoritative_shot_fact_collector" or \
                origin.get("only_succeeded_physics_steps_contribute") is not True or \
                origin.get("failed_step_advances_tick_or_mutates_facts") is not False or \
                origin.get("zero_succeeded_steps_complete") is not False or \
                origin.get("incomplete_authoritative_break_fails_rule_validation") is not True or \
                origin.get("stable_validation_error") != "rules_facts_break_evidence_invalid":
            errors.append("origin_or_completeness")
        policy = contract.get("policy_independence", {})
        if policy.get("cushion_surface_policy") != "POLICY_PENDING" or \
                policy.get("selected_cushion_surface_policy") is not None or \
                policy.get("physics_contact_validity_policy") != "TELEMETRY_POLICY_PENDING" or \
                policy.get("selected_physics_contact_predicate") is not None or \
                policy.get("pocketed_ball_scope_policy") != "POLICY_PENDING" or \
                policy.get("selected_pocketed_ball_scope") is not None or \
                policy.get("adjudicator_still_uses_existing_distinct_scalar") is not True or \
                policy.get("telemetry_closure_promotes_product_claim") is not False:
            errors.append("policy_independence")
        boundary = contract.get("audit_and_product_boundary", {})
        if boundary.get("audit_status") != "PARTIAL" or \
                boundary.get("product_reachable") is not False or \
                boundary.get("product_choice_exposure_added") is not False or \
                boundary.get("rules_decision_ui_product_exposed") is not False or \
                boundary.get("legal_break_classification_complete") is not False:
            errors.append("audit_or_product_boundary")
        absent = contract.get("forbidden_and_absent", {})
        if any(absent.get(key) is not False for key in (
            "official_policy_invented", "gpu_cocoa_or_opengl_executed",
            "billiards_product_executable_started", "readiness_generated",
            "authorization_issued", "visual_or_evidence_threshold_changed",
            "phase4_or_phase5_renderer_changed",
        )):
            errors.append("forbidden_or_absent")
        claims = contract.get("claims", {})
        if claims != {
            "policy_independent_break_telemetry_complete": True,
            "headless_cpu_verified": True,
            "legal_break_policy_complete": False,
            "product_reachable": False,
            "audit_promoted": False,
            "gpu_executed": False,
        }:
            errors.append("claims")
        cpu = contract.get("cpu_results")
        if not isinstance(cpu, list) or len(cpu) != len(EXPECTED_CPU):
            errors.append("cpu_results")
        else:
            actual = {row.get("target"): row for row in cpu if isinstance(row, dict)}
            if set(actual) != set(EXPECTED_CPU):
                errors.append("cpu_targets")
            for target, digest in EXPECTED_CPU.items():
                row = actual.get(target, {})
                if row.get("sha256") != digest or row.get("exit_code") != 0:
                    errors.append(f"cpu_{target}")

    sources = value.get("source_bindings")
    if not isinstance(sources, dict):
        errors.append("source_bindings")
    else:
        expected_keys = set(EXPECTED_LIVE_SOURCES) | set(EXPECTED_HISTORICAL)
        if set(sources) != expected_keys:
            errors.append("source_binding_keys")
        for name, digest in EXPECTED_HISTORICAL.items():
            if sources.get(name, {}).get("sha256") != digest:
                errors.append(f"historical_{name}")
        for name, (relative, digest) in EXPECTED_LIVE_SOURCES.items():
            if sources.get(name) != {"path": relative, "sha256": digest}:
                errors.append(f"binding_{name}")
            if check_files:
                path = safe_path(relative)
                if path is None or not path.is_file() or file_sha(path) != digest:
                    errors.append(f"source_{name}_drift")

    if check_files:
        gate = safe_path(EXPECTED_GATE["path"])
        if gate is None or not gate.is_file() or file_sha(gate) != EXPECTED_GATE["sha256"]:
            errors.append("design_gate_drift")
        build_root = ROOT / "build-wpa-06c-break-telemetry-cpu-v1"
        for target, digest in EXPECTED_CPU.items():
            path = build_root / target
            if not path.is_file() or file_sha(path) != digest:
                errors.append(f"cpu_binary_{target}_drift")
        model = (ROOT / "src/Billiards/rules/chinese_eight_ball_rules.h").read_text(encoding="utf-8")
        collector = (ROOT / "src/Billiards/rules/shot_fact_collector.cpp").read_text(encoding="utf-8")
        runtime = (ROOT / "src/Billiards/game_runtime.cpp").read_text(encoding="utf-8")
        required_model = ("BreakObjectCushionContact", "BreakEvidenceOrigin",
                          "breakEvidenceComplete", "InvalidBreakEvidence")
        required_collector = ("matchedDiscreteJaw", "PhysicsStepStatus::Succeeded",
                              "lastSucceededPhysicsTick_", "evidenceKey")
        if any(token not in model for token in required_model):
            errors.append("model_static_contract")
        if any(token not in collector for token in required_collector):
            errors.append("collector_static_contract")
        if "shotFactCollector_.consume(before, state_, telemetry, tick_)" not in runtime:
            errors.append("runtime_static_contract")
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
    print(json.dumps({"status": "PASS" if not errors else "FAIL",
                      "errors": errors, "receipt": str(args.receipt)},
                     sort_keys=True))
    if errors:
        return 1
    print("PASS_WPA_II_06_C_POLICY_INDEPENDENT_BREAK_TELEMETRY_IMPLEMENTATION")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
