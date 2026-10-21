#!/usr/bin/env python3
"""Validate the CPU-only WPA-II-06-E narrow product consumer receipt."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_RECEIPT = ROOT / "docs/rules/wpa-ii-06-e-narrow-frozen-offer-consumer-implementation-receipt-v1.json"
TOP_LEVEL_KEYS = {
    "schema", "status", "selection_binding", "predecessor_bindings",
    "narrow_product_contract", "transaction_and_failure_contract",
    "source_bindings", "cpu_verification", "evidence_invalidation", "claims",
}
DIGESTS = {
    "selection_binding": "6e8d38d7daaed749653da011d41bc4b2c5f4d9075b31d036a72f7b47dc607b3c",
    "predecessor_bindings": "8704463e3818a70afc293626a941ebe54cd21ede5dc4a13b7020ab911cb89b06",
    "narrow_product_contract": "2f3832df5dbb53fad534d528d156a8c110ae916423611c4a1d83aa39421eca36",
    "transaction_and_failure_contract": "c2115449dc5e86948d6dad558d64234138cdbc9c52eb859c6f06616bafdbee74",
    "source_bindings": "9601af295b2be413fbc7c946c977fb2be5ace8dae788aa523c5de49be467de4b",
    "cpu_verification": "6a03d45f9a3562f8f6fbb6882318a8bfc2386afd8887b9b91813b20bbae37813",
    "evidence_invalidation": "1d4e60ced7f7d0cdc8474ad1e678019e587db8d86293e6290f60f3b43a0ec07a",
    "claims": "2fd3467f8ada7bdb39e4588047855388ec5c3010554b5994f1633183472a8f10",
}
EXACT_ACTIONS = [
    "rules.break_foul.ball_in_hand_behind_head_string",
    "rules.break_foul.rerack_opponent_breaks",
    "rules.break_foul.rerack_original_breaker",
]


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def canonical_digest(value: Any) -> str:
    raw = json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    return sha256(raw)


def resolve(raw: Any) -> Path | None:
    if not isinstance(raw, str):
        return None
    path = Path(raw)
    if path.is_absolute() or ".." in path.parts:
        return None
    result = (ROOT / path).resolve()
    return result if result.is_relative_to(ROOT.resolve()) else None


def file_binding_ok(binding: Any) -> bool:
    if not isinstance(binding, dict):
        return False
    path = resolve(binding.get("path"))
    digest = binding.get("sha256")
    return path is not None and path.is_file() and isinstance(digest, str) and sha256(path.read_bytes()) == digest


def validate(value: Any, check_files: bool = True) -> list[str]:
    if not isinstance(value, dict):
        return ["root_not_object"]
    errors: list[str] = []
    if set(value) != TOP_LEVEL_KEYS:
        errors.append("top_level_keys")
    if value.get("schema") != "wpa_ii_06_e_narrow_frozen_offer_consumer_implementation_receipt_v1":
        errors.append("schema")
    if value.get("status") != "NARROW_FROZEN_OFFER_CONSUMER_IMPLEMENTED_CPU_VERIFIED_ROW_REMAINS_OPEN":
        errors.append("status")
    for key, digest in DIGESTS.items():
        if key not in value or canonical_digest(value.get(key)) != digest:
            errors.append(f"contract_digest:{key}")

    selection = value.get("selection_binding")
    if not isinstance(selection, dict) or \
            selection.get("row_id") != "WPA-II-06-E" or \
            selection.get("decision_id") != "M4-PROD-06E-BREAK-FOUL-CHOICES" or \
            selection.get("selected_option_id") != "NARROW_FROZEN_OFFER_CONSUMER" or \
            selection.get("authorized_stage") != "B1_TESTS_FIRST_CPU_DIRECT_CODE" or \
            selection.get("root_selected") is not True or \
            selection.get("selection_alone_closes_row") is not False:
        errors.append("selection_contract")

    product = value.get("narrow_product_contract")
    offer = product.get("required_runtime_offer") if isinstance(product, dict) else None
    if not isinstance(product, dict) or not isinstance(offer, dict) or \
            product.get("product_entrypoint") != "ModernProductSession::submitProductAuthorizedBreakFoulChoice" or \
            product.get("product_screen") != "Match" or \
            offer.get("branch") != "BreakShotFoul" or \
            offer.get("source_clause") != "WPA-II-06-E" or \
            offer.get("actor_relation") != "OPPONENT_OF_FROZEN_ORIGINAL_BREAKER" or \
            offer.get("frozen_next_player_relation") != "SAME_AS_DECISION_ACTOR" or \
            offer.get("ordered_action_ids") != EXACT_ACTIONS or \
            product.get("canonical_payload_text") != "{}" or \
            product.get("idempotency_key_length") != {"minimum": 1, "maximum": 128}:
        errors.append("narrow_product_contract")
    for key in ("local_actor_must_equal_offer_actor", "request_binds_current_decision_revision",
                "request_binds_current_authority_state_hash", "ack_verified_before_request_retention",
                "authority_sync_and_ai_refresh_only_after_verified_ack"):
        if not isinstance(product, dict) or product.get(key) is not True:
            errors.append(f"product_required_true:{key}")
    for key in ("product_reclassifies_break_facts", "sibling_or_foundation_action_ids_product_reachable",
                "global_rules_decision_ui_exposed"):
        if not isinstance(product, dict) or product.get(key) is not False:
            errors.append(f"product_required_false:{key}")

    tx = value.get("transaction_and_failure_contract")
    required_true = (
        "ball_in_hand_parent_commit_stages_exactly_one_core_g_child",
        "child_requires_separate_authoritative_confirm",
        "invalid_child_preview_or_confirm_preserves_parent_entitlement",
        "rerack_opponent_breaks_selects_decision_actor",
        "rerack_original_breaker_selects_frozen_original_breaker",
        "exact_retry_returns_original_ack_without_duplicate_effect",
        "same_key_different_action_or_payload_conflicts_without_mutation",
    )
    required_false = (
        "ball_in_hand_before_parent_commit", "placement_child_before_parent_commit",
        "rerack_branches_grant_ball_in_hand",
        "reset_stale_wrong_actor_wrong_action_wrong_payload_or_hash_drift_mutates_authority",
        "pre_ack_product_failure_refreshes_shell_or_ai", "foundation_or_sibling_bypass_allowed",
        "partial_parent_child_or_rerack_commit_allowed",
    )
    if not isinstance(tx, dict) or tx.get("ball_in_hand_parent_commit_scope") != "BehindHeadString" or \
            any(tx.get(key) is not True for key in required_true) or \
            any(tx.get(key) is not False for key in required_false):
        errors.append("transaction_contract")

    sources = value.get("source_bindings")
    if not isinstance(sources, dict) or len(sources) != 10:
        errors.append("source_bindings")
    elif check_files:
        for key, binding in sources.items():
            if not file_binding_ok(binding):
                errors.append(f"source_drift:{key}")

    cpu = value.get("cpu_verification")
    binaries = cpu.get("binaries") if isinstance(cpu, dict) else None
    if not isinstance(cpu, dict) or cpu.get("build_root") != "build-product-session" or \
            cpu.get("ctest_subset") != {"passed": 10, "failed": 0} or \
            cpu.get("tests_first_missing_entrypoint_reproduced_before_production_patch") is not True or \
            cpu.get("gpu_cocoa_or_opengl_executed") is not False or \
            cpu.get("cmake_source_modified_in_task") is not False or \
            not isinstance(binaries, list) or len(binaries) != 10 or \
            any(not isinstance(item, dict) or item.get("exit_code") != 0 for item in (binaries or [])):
        errors.append("cpu_contract")
    elif check_files:
        build_root = resolve(cpu.get("build_root"))
        if build_root is None:
            errors.append("build_root")
        else:
            for item in binaries:
                binary = build_root / str(item.get("target", ""))
                if not binary.is_file() or sha256(binary.read_bytes()) != item.get("sha256"):
                    errors.append(f"binary_drift:{item.get('target', 'unknown')}")

    evidence = value.get("evidence_invalidation")
    if not isinstance(evidence, dict) or \
            evidence.get("prior_source_bound_product_session_evidence") != "HISTORICAL_ONLY_AFTER_THIS_SOURCE_CHANGE" or \
            evidence.get("fresh_release_binary_bindings_required_before_future_readiness") is not True or \
            evidence.get("fresh_targeted_gpu_ta_required_before_visible_product_action_claim") is not True or \
            evidence.get("audit_v5_is_not_rewritten_or_promoted_here") is not True or \
            evidence.get("gpu_or_readiness_authorized_here") is not False:
        errors.append("evidence_boundary")

    claims = value.get("claims")
    for key in ("selected_narrow_consumer_cpu_implemented", "current_frozen_core_h_offer_only",
                "ack_then_sync_and_ai_refresh_cpu_verified", "parent_child_and_rerack_atomicity_cpu_verified",
                "wpa_ii_06_e_narrow_product_seam_reachable"):
        if not isinstance(claims, dict) or claims.get(key) is not True:
            errors.append(f"positive_claim:{key}")
    for key in ("wpa_ii_06_e_row_closed", "upstream_break_foul_classification_complete",
                "audit_v5_modified_or_promoted", "global_rules_decision_ui_exposed",
                "foundation_or_sibling_product_reachable", "product_visual_evidence_complete",
                "profile_complete", "m4_complete", "ui_visual_modified", "cmake_modified",
                "phase4_or_phase5_renderer_modified", "gpu_executed",
                "readiness_or_authorization_created"):
        if not isinstance(claims, dict) or claims.get(key) is not False:
            errors.append(f"false_completion:{key}")

    if check_files and isinstance(selection, dict):
        try:
            if not file_binding_ok(selection):
                errors.append("selection_drift")
            else:
                selected_path = resolve(selection.get("path"))
                selected = json.loads(selected_path.read_text(encoding="utf-8"))
                row = next(item for item in selected["decisions"] if item["row_id"] == "WPA-II-06-E")
                if row["decision_id"] != selection["decision_id"] or \
                        row["selected_option_id"] != selection["selected_option_id"] or \
                        row["root_selected"] is not True or \
                        row["authorized_stage"] != selection["authorized_stage"] or \
                        row["row_closed_by_selection"] is not False:
                    errors.append("selection_row_drift")
        except (OSError, KeyError, StopIteration, TypeError, json.JSONDecodeError):
            errors.append("selection_parse")

        predecessors = value.get("predecessor_bindings")
        try:
            candidate = predecessors["candidate"]
            gate = predecessors["design_gate"]
            if not file_binding_ok(candidate):
                errors.append("candidate_drift")
            else:
                candidate_value = json.loads(resolve(candidate["path"]).read_text(encoding="utf-8"))
                row = next(item for item in candidate_value["decisions"] if item["row_id"] == "WPA-II-06-E")
                if row["proposed_option_id"] != "NARROW_FROZEN_OFFER_CONSUMER" or \
                        row["candidate_state"] != "PROPOSED_RECOMMENDED_NOT_ROOT_SELECTED":
                    errors.append("candidate_row_drift")
            if not file_binding_ok(gate):
                errors.append("design_gate_drift")
        except (OSError, KeyError, StopIteration, TypeError, json.JSONDecodeError):
            errors.append("predecessor_parse")
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
    print("PASS_WPA_II_06_E_NARROW_FROZEN_OFFER_CONSUMER_IMPLEMENTATION")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
