#!/usr/bin/env python3
"""Validate WPA II §17(a) authoritative cue-scratch telemetry closure."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/rules/wpa-ii-17-a-cue-scratch-policy-independent-telemetry-implementation-receipt-v1.json"
PRE_SHA = "0bfdfed54a82be8b82d4ad696bb5ae56fa20dc9c8e3c6e122ef95ce6ea576d50"
AUDIT_SHA = "2811958f1205d9998b40060b83c577f91359d2164e1ac0eaa9ccc4cac2ad787a"
SOURCE_SHA = {
    "src/Billiards/rules/chinese_eight_ball_rules.h": "7408e24a58c827e3de27b766cd91bdb5ce5831919cea97f733f8191baae59f7f",
    "src/Billiards/rules/chinese_eight_ball_rules.cpp": "a591b919e99580a8e859e98ebfda9de1a0f2ec89e521d47f67ef180c6145f930",
    "src/Billiards/rules/shot_fact_collector.h": "13453f30d0eeb7ce4231b1f5802eaf3317e085f4187e3e16abaff34cb171986c",
    "src/Billiards/rules/shot_fact_collector.cpp": "92b205a1a0025b21a47fbd86e755beceeb4f89208b5db28945ddfcdd1f7e5f7d",
    "src/Billiards/automation_controller.cpp": "47aed0a5a0fe216353d59c8672c773615a298c688c8aaf28aaf14ebfd8788360",
    "tests/chinese_eight_ball_rules_tests.cpp": "950a98501feca064cbaab8c4b1025b3b421dfcbc8b59a8f82739042069d7997c",
    "tests/shot_fact_collector_tests.cpp": "2b592431fdee204a6c7cfb976125d8697770a477a0749e7e7fcc332c66bbd401",
    "tests/rules_runtime_integration_tests.cpp": "f4f67a8f2b34b7c35ce8c555d12d7a4b65922c4f5710d21c8efe66408476152c",
    "tests/rules_decision_session_integration_tests.cpp": "318561223599fae85fa628d03b104f01cd7dedeb169fd6975deac6aac12f3379",
    "tests/ai_turn_controller_tests.cpp": "27c8179be8c2c03c0334e32a337ae417ef04368b61b36b36a975a87a0e3938b1",
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def workspace_path(raw: Any) -> Path | None:
    if not isinstance(raw, str) or not raw or Path(raw).is_absolute() or ".." in Path(raw).parts:
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
    if value.get("schema") != "wpa_ii_17_a_cue_scratch_policy_independent_telemetry_implementation_receipt_v1" or \
            value.get("status") != "IMPLEMENTED_POLICY_INDEPENDENT_CUE_SCRATCH_TELEMETRY_WPA_II_17_REMAINS_PARTIAL" or \
            value.get("scope") != ["WPA-II-17(a)-CUE-BALL-SCRATCH-ONLY"]:
        errors.append("identity_status_scope")

    lineage = value.get("lineage", {})
    pre = lineage.get("preimplementation_audit", {}) if isinstance(lineage, dict) else {}
    audit = lineage.get("frozen_audit", {}) if isinstance(lineage, dict) else {}
    if pre.get("path") != "docs/rules/wpa-ii-17-general-foul-product-lifecycle-subflow-audit-successor-v1.json" or \
            pre.get("sha256") != PRE_SHA or audit.get("path") != "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v5.json" or \
            audit.get("sha256") != AUDIT_SHA or audit.get("row") != {"id": "WPA-II-17", "status": "PARTIAL", "product_reachable": True} or \
            audit.get("promotion_authorized") is not False or lineage.get("superseded_preimplementation_source_bindings_are_expected") is not True:
        errors.append("lineage")
    elif check_files:
        for raw, digest in ((pre["path"], PRE_SHA), (audit["path"], AUDIT_SHA)):
            path = workspace_path(raw)
            if path is None or not path.is_file() or sha256(path) != digest:
                errors.append(f"lineage_drift:{raw}")

    bindings = value.get("source_bindings")
    actual = {entry.get("path"): entry.get("sha256") for entry in bindings.values()
              if isinstance(entry, dict)} if isinstance(bindings, dict) else {}
    if actual != SOURCE_SHA or len(bindings or {}) != len(SOURCE_SHA):
        errors.append("source_bindings")
    elif check_files:
        for raw, digest in SOURCE_SHA.items():
            path = workspace_path(raw)
            if path is None or not path.is_file() or sha256(path) != digest:
                errors.append(f"source_drift:{raw}")

    event = value.get("authoritative_event_contract", {})
    required = event.get("required_contact", {}) if isinstance(event, dict) else {}
    expected_fields = ["pocket_id", "capture_sequence", "physics_tick",
                       "physics_contact_ordinal", "canonical_event_ordinal",
                       "time_of_impact_seconds"]
    if required != {"kind": "Pocket", "first_ball": 0, "second_ball": -1,
                    "boundary_event": "Capture", "phase_before_must_not_equal": "Captured",
                    "phase_after": "Captured", "pocket_id_range": [0, 5],
                    "capture_sequence_minimum": 1, "physics_tick_minimum": 1,
                    "time_of_impact_finite_and_nonnegative": True} or \
            event.get("same_tick_event_required") is not True or event.get("ball_state_pocketed_true_required") is not False or \
            event.get("cue_ball_replacement_and_reset_semantics_unchanged") is not True or event.get("event_fields") != expected_fields:
        errors.append("authoritative_event_contract")

    collector = value.get("collector_contract", {})
    expected_key = ["capture_sequence", "physics_tick", "pocket_id", "time_of_impact_seconds"]
    for key in ("finish_requires_at_least_one_succeeded_tick", "exact_duplicates_deduplicated",
                "more_than_one_distinct_capture_fails_completeness", "capture_without_same_tick_event_fails_completeness",
                "same_tick_event_without_valid_capture_fails_completeness", "failed_step_has_zero_evidence_mutation",
                "begin_resets_evidence_dedupe_and_ordinals", "partial_evidence_never_sets_cue_ball_pocketed_fact"):
        if collector.get(key) is not True:
            errors.append(f"collector_contract:{key}")
    if collector.get("origin") != "AuthoritativeShotFactCollector" or collector.get("exact_duplicate_key") != expected_key:
        errors.append("collector_identity")

    adjudicator = value.get("adjudicator_contract", {})
    for key in ("cue_ball_scratch_consumed_only_when_origin_is_authoritative",
                "cue_ball_scratch_consumed_only_when_evidence_is_complete",
                "cue_ball_scratch_requires_exactly_one_canonical_capture",
                "direct_or_synthetic_cue_ball_pocketed_rejected",
                "complete_authoritative_no_scratch_observation_is_valid"):
        if adjudicator.get(key) is not True:
            errors.append(f"adjudicator_contract:{key}")
    if adjudicator.get("stable_error_code") != "rules_facts_cue_scratch_evidence_invalid":
        errors.append("adjudicator_error_code")

    proof = value.get("runtime_proof", {})
    for key in ("uses_update_physics", "successful_physics_tick", "collector_finalized_at_shot_end",
                "automation_readback_includes_origin_completeness_tick_and_event_identity"):
        if proof.get(key) is not True:
            errors.append(f"runtime_proof:{key}")
    if proof.get("capture_count") != 1 or proof.get("cue_ball_state_pocketed_after_replacement") is not False or \
            proof.get("foul_reason") != "cue_ball_scratch" or proof.get("ball_in_hand_awarded") is not True or proof.get("core_g_offer_staged") is not True:
        errors.append("runtime_outcome")

    fixtures = value.get("negative_fixtures")
    expected_fixtures = ["FAILED_STEP_CAPTURE_ROLLS_BACK", "PARTIAL_CAPTURE_SEQUENCE_ZERO_FAILS_CLOSED",
                         "CONFLICTING_DUPLICATE_CAPTURE_IDENTITY_FAILS_CLOSED", "EXACT_DUPLICATE_CAPTURE_DEDUPES_ONCE",
                         "COLLECTOR_BEGIN_RESETS_PRIOR_CAPTURE", "DIRECT_OR_SYNTHETIC_SCRATCH_REJECTED",
                         "AUTHORITATIVE_ORIGIN_WITHOUT_COMPLETENESS_REJECTED", "AUTOMATION_UNATTESTED_SCRATCH_REJECTED"]
    if fixtures != expected_fixtures:
        errors.append("negative_fixtures")

    results = value.get("cpu_results")
    if not isinstance(results, list) or len(results) != 6 or any(
            row.get("result") != "PASS" or row.get("gpu") is not False for row in results if isinstance(row, dict)) or \
            len([row for row in results if isinstance(row, dict)]) != 6:
        errors.append("cpu_results")

    nonscope = value.get("non_scope")
    expected_nonscope = {
        "object_ball_off_table_changed": False,
        "wpa_ii_12_3_vs_17_e_status": "POLICY_PENDING",
        "cue_ball_replacement_physics_changed": False,
        "ball_in_hand_physics_changed": False,
        "global_rules_decision_ui_product_exposed": False,
        "cmake_source_changed": False,
        "phase4_or_phase5_changed": False,
        "gpu_cocoa_opengl_readiness_or_authorization_executed": False,
    }
    if nonscope != expected_nonscope:
        errors.append("non_scope")

    claims = value.get("claims")
    expected_claims = {
        "wpa_ii_17_a_cue_scratch_physical_collection_complete": True,
        "wpa_ii_17_a_cue_scratch_product_bih_subflow_reachable": True,
        "wpa_ii_17_complete": False,
        "wpa_ii_17_a_cue_ball_off_table_complete": False,
        "wpa_ii_17_e_object_ball_off_table_complete": False,
        "object_ball_off_table_policy_resolved": False,
        "audit_row_promoted": False,
        "gpu_executed": False,
    }
    if claims != expected_claims:
        errors.append("claims")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--receipt", type=Path, default=DEFAULT)
    args = parser.parse_args()
    try:
        value = json.loads(args.receipt.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        print(f"FAIL_WPA_II_17_A_CUE_SCRATCH_TELEMETRY:{exc}")
        return 1
    errors = validate(value, check_files=True)
    if errors:
        print("FAIL_WPA_II_17_A_CUE_SCRATCH_TELEMETRY:" + ",".join(errors))
        return 1
    print("PASS_WPA_II_17_A_AUTHORITATIVE_CUE_SCRATCH_TELEMETRY")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
