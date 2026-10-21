#!/usr/bin/env python3
"""Validate the CPU-only WPA-II-06-F full Core-F product consumer receipt."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_RECEIPT = ROOT / "docs/rules/wpa-ii-06-f-selected-full-choice-product-consumer-implementation-receipt-v1.json"
TOP_LEVEL_KEYS = {
    "schema", "status", "selection_binding", "predecessor_bindings",
    "exact_core_f_product_contract", "effects_and_atomicity_contract",
    "source_bindings", "cpu_verification", "evidence_invalidation", "claims",
}
DIGESTS = {
    "selection_binding": "83f8b79bfe7af59969c077b54e200d899f4f110d4af4169dd87ba1ed7f253f03",
    "predecessor_bindings": "53ae51ec39438775a8ff5bf77013d470ea8218f2355aaa71f378a40c9bea0bd4",
    "exact_core_f_product_contract": "0b513d46053a2445ab837379813f3bb86b6d2c28e9219ec76cfb02e74a20a8d5",
    "effects_and_atomicity_contract": "23ee3a6323640309c8f3436b73e98f4ee25152bbeda9d06a3a7205ffc5c18ab8",
    "source_bindings": "e2dec36ede73210e8ff25291344a26b2ee0db5cf227cc6080b8ae242c402ebe4",
    "cpu_verification": "9c677d4df14d0f32c51f84f697c4bae6422753b1146b89b5d5302aa8c967a17c",
    "evidence_invalidation": "ef2a0a96ad7050977101204b5de0223c7c04430e216f21786e331c30eb58e170",
    "claims": "bf49528a0f9b7804f0982337f73972299cd265b19539e1d7bdf00a476f06da3c",
}
LEGAL_ACTIONS = [
    "rules.eight_on_break.spot_continue",
    "rules.eight_on_break.rerack_original_breaker",
]
FOUL_ACTIONS = [
    "rules.eight_on_break.spot_ball_in_hand_head_string",
    "rules.eight_on_break.rerack_opponent_breaks",
    "rules.eight_on_break.rerack_original_breaker",
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
    if value.get("schema") != "wpa_ii_06_f_selected_full_choice_product_consumer_implementation_receipt_v1":
        errors.append("schema")
    if value.get("status") != "SELECTED_FULL_CORE_F_PRODUCT_CONSUMER_IMPLEMENTED_CPU_VERIFIED_ROW_REMAINS_OPEN":
        errors.append("status")
    for key, digest in DIGESTS.items():
        if key not in value or canonical_digest(value.get(key)) != digest:
            errors.append(f"contract_digest:{key}")

    selection = value.get("selection_binding")
    if not isinstance(selection, dict) or \
            selection.get("row_id") != "WPA-II-06-F" or \
            selection.get("decision_id") != "M4-PROD-06F-EIGHT-ON-BREAK-FULL-CHOICES" or \
            selection.get("selected_option_id") != "EXPAND_NARROW_CORE_F_TO_EXACT_RERACK_ACTIONS" or \
            selection.get("authorized_stage") != "B1_TESTS_FIRST_CPU_DIRECT_CODE" or \
            selection.get("root_selected") is not True or \
            selection.get("selection_alone_closes_row") is not False:
        errors.append("selection_contract")

    product = value.get("exact_core_f_product_contract")
    legal = product.get("legal_branch") if isinstance(product, dict) else None
    foul = product.get("foul_branch") if isinstance(product, dict) else None
    if not isinstance(product, dict) or not isinstance(legal, dict) or not isinstance(foul, dict) or \
            product.get("product_entrypoint") != "ModernProductSession::submitProductAuthorizedEightOnBreakRespot" or \
            product.get("product_screen") != "Match" or product.get("branch_action_lane_count") != 5 or \
            product.get("canonical_payload_text") != "{}" or \
            product.get("idempotency_key_length") != {"minimum": 1, "maximum": 128}:
        errors.append("product_contract")
    if not isinstance(legal, dict) or legal.get("branch") != "LegalBreak" or \
            legal.get("actor_relation") != "FROZEN_ORIGINAL_BREAKER" or \
            legal.get("frozen_next_player_relation") != "SAME_AS_DECISION_ACTOR" or \
            legal.get("frozen_continue_turn") is not True or \
            legal.get("ordered_action_ids") != LEGAL_ACTIONS:
        errors.append("legal_branch")
    if not isinstance(foul, dict) or foul.get("branch") != "FoulBreak" or \
            foul.get("actor_relation") != "OPPONENT_OF_FROZEN_ORIGINAL_BREAKER" or \
            foul.get("frozen_next_player_relation") != "SAME_AS_DECISION_ACTOR" or \
            foul.get("frozen_continue_turn") is not False or \
            foul.get("ordered_action_ids") != FOUL_ACTIONS:
        errors.append("foul_branch")
    for key in ("local_actor_must_equal_offer_actor", "request_binds_current_decision_revision",
                "request_binds_current_authority_state_hash", "ack_verified_before_request_retention",
                "authority_sync_and_ai_refresh_only_after_verified_ack"):
        if not isinstance(product, dict) or product.get(key) is not True:
            errors.append(f"product_required_true:{key}")
    for key in ("product_reclassifies_break_facts_or_branch",
                "core_h_or_direct_core_e_action_ids_product_reachable",
                "global_rules_decision_ui_exposed"):
        if not isinstance(product, dict) or product.get(key) is not False:
            errors.append(f"product_required_false:{key}")

    effects = value.get("effects_and_atomicity_contract")
    required_true = (
        "legal_spot_continues_original_breaker_turn_without_bih",
        "legal_rerack_selects_original_breaker_without_bih", "foul_parent_stages_without_bih",
        "foul_spot_parent_commit_grants_behind_head_string_bih",
        "foul_spot_parent_commit_stages_exactly_one_core_g_child",
        "foul_spot_child_requires_separate_authoritative_confirm",
        "foul_rerack_opponent_action_selects_decision_actor",
        "foul_rerack_original_action_selects_frozen_original_breaker",
        "exact_retry_returns_original_ack_without_duplicate_effect",
    )
    required_false = (
        "any_rerack_grants_or_preserves_bih", "same_key_different_action_or_payload_mutates_authority",
        "reset_stale_wrong_actor_wrong_action_wrong_payload_or_hash_drift_mutates_authority",
        "failed_core_e_respot_mutates_parent_bih_child_revision_or_action",
        "rejected_child_preview_weakens_committed_parent_entitlement",
        "partial_parent_child_or_rerack_commit_allowed",
    )
    if not isinstance(effects, dict) or any(effects.get(key) is not True for key in required_true) or \
            any(effects.get(key) is not False for key in required_false):
        errors.append("effects_atomicity")

    sources = value.get("source_bindings")
    if not isinstance(sources, dict) or len(sources) != 11:
        errors.append("source_bindings")
    elif check_files:
        for key, binding in sources.items():
            if not file_binding_ok(binding):
                errors.append(f"source_drift:{key}")

    cpu = value.get("cpu_verification")
    binaries = cpu.get("binaries") if isinstance(cpu, dict) else None
    if not isinstance(cpu, dict) or cpu.get("build_root") != "build-product-session" or \
            cpu.get("ctest_subset") != {"passed": 10, "failed": 0} or \
            cpu.get("tests_first_old_spot_only_api_failure_reproduced_before_production_patch") is not True or \
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
            evidence.get("prior_spot_only_product_session_evidence") != "HISTORICAL_ONLY_AFTER_THIS_SOURCE_CHANGE" or \
            evidence.get("fresh_release_binary_bindings_required_before_future_readiness") is not True or \
            evidence.get("fresh_targeted_gpu_ta_required_before_visible_full_choice_claim") is not True or \
            evidence.get("audit_v5_is_not_rewritten_or_promoted_here") is not True or \
            evidence.get("gpu_or_readiness_authorized_here") is not False:
        errors.append("evidence_boundary")

    claims = value.get("claims")
    for key in ("selected_full_core_f_consumer_cpu_implemented",
                "all_five_branch_action_lanes_cpu_verified", "current_frozen_core_f_offer_only",
                "ack_then_sync_and_ai_refresh_cpu_verified", "parent_child_and_rerack_atomicity_cpu_verified",
                "wpa_ii_06_f_current_frozen_offer_product_seam_reachable"):
        if not isinstance(claims, dict) or claims.get(key) is not True:
            errors.append(f"positive_claim:{key}")
    for key in ("wpa_ii_06_f_row_closed", "upstream_break_foul_classification_complete",
                "audit_v5_modified_or_promoted", "global_rules_decision_ui_exposed",
                "core_h_or_direct_core_e_product_reachable", "product_visual_evidence_complete",
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
                selected = json.loads(resolve(selection["path"]).read_text(encoding="utf-8"))
                row = next(item for item in selected["decisions"] if item["row_id"] == "WPA-II-06-F")
                if row["decision_id"] != selection["decision_id"] or \
                        row["selected_option_id"] != selection["selected_option_id"] or \
                        row["root_selected"] is not True or row["authorized_stage"] != selection["authorized_stage"] or \
                        row["row_closed_by_selection"] is not False:
                    errors.append("selection_row_drift")
        except (OSError, KeyError, StopIteration, TypeError, json.JSONDecodeError):
            errors.append("selection_parse")

        predecessors = value.get("predecessor_bindings")
        try:
            candidate = predecessors["candidate"]
            gate = predecessors["design_gate"]
            timing = predecessors["entitlement_timing_implementation"]
            if not file_binding_ok(candidate):
                errors.append("candidate_drift")
            else:
                candidate_value = json.loads(resolve(candidate["path"]).read_text(encoding="utf-8"))
                row = next(item for item in candidate_value["decisions"] if item["row_id"] == "WPA-II-06-F")
                if row["proposed_option_id"] != "EXPAND_NARROW_CORE_F_TO_EXACT_RERACK_ACTIONS":
                    errors.append("candidate_row_drift")
            if not file_binding_ok(gate):
                errors.append("design_gate_drift")
            if not file_binding_ok(timing):
                errors.append("entitlement_timing_drift")
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
    print("PASS_WPA_II_06_F_SELECTED_FULL_CHOICE_PRODUCT_CONSUMER_IMPLEMENTATION")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
