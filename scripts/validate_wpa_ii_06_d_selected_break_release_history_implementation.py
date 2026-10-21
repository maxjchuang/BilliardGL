#!/usr/bin/env python3
"""Validate the CPU-only WPA-II-06-D selected release/history receipt."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_RECEIPT = ROOT / "docs/rules/wpa-ii-06-d-selected-break-release-history-implementation-receipt-v1.json"
EXPECTED_TOP_LEVEL = {
    "schema", "status", "selection_binding", "predecessor_bindings",
    "implementation_contract", "atomic_and_failure_contract",
    "presentation_and_readback_contract", "source_bindings",
    "cpu_verification", "evidence_invalidation", "claims",
}
DIGESTS = {
    "selection_binding": "018a6a3f13252510dd5e85c96d5f41b2575568333766e92d7c0d8eb58ce7607c",
    "predecessor_bindings": "789cc2a6e087856088870879375110b4cb7a64b28403026648a5d70051192653",
    "implementation_contract": "c127f0a936227bcc3c242abd5b4dca70264b85139a5aaf305d645d860a1ae917",
    "atomic_and_failure_contract": "40be4705aed559a87535757faeb393caf46252ef7db115e1b30d1471795c85ba",
    "presentation_and_readback_contract": "423141c2dfdb2e73ff5d8aed6c08d7b026c48db6c8ffca239f4f599d724c3474",
    "source_bindings": "afa86adc20c43b056a9b56421b9d21a59689eb07d9e899c51fcb2f44f5c19b44",
    "cpu_verification": "1b0d03605c0882e94a5df7114957218cabd1e43f0baf9e07385dbb60bf605b75",
    "evidence_invalidation": "820987cd083626a8452fca4db7616668ad1663e329ad5d87e2fff9fd2d12206b",
    "claims": "57c3beb5128b75854316c714b13a74ce900c62293e6ba55687652cc1f496ba09",
}


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def canonical_digest(value: Any) -> str:
    return sha256(json.dumps(value, sort_keys=True, separators=(",", ":"),
                             ensure_ascii=False).encode("utf-8"))


def resolve(raw: Any) -> Path | None:
    if not isinstance(raw, str):
        return None
    path = Path(raw)
    if path.is_absolute() or ".." in path.parts:
        return None
    result = (ROOT / path).resolve()
    return result if result.is_relative_to(ROOT.resolve()) else None


def validate(value: Any, check_files: bool = True) -> list[str]:
    if not isinstance(value, dict):
        return ["root_not_object"]
    errors: list[str] = []
    if set(value) != EXPECTED_TOP_LEVEL:
        errors.append("top_level_keys")
    if value.get("schema") != "wpa_ii_06_d_selected_break_release_history_implementation_receipt_v1":
        errors.append("schema")
    if value.get("status") != "SELECTED_RELEASE_HISTORY_IMPLEMENTED_CPU_VERIFIED_ROW_REMAINS_OPEN":
        errors.append("status")
    for key, digest in DIGESTS.items():
        if key not in value or canonical_digest(value.get(key)) != digest:
            errors.append(f"contract_digest:{key}")

    selection = value.get("selection_binding")
    if not isinstance(selection, dict) or \
            selection.get("decision_id") != "M4-POL-06D-HALF-BALL-HISTORY" or \
            selection.get("selected_option_id") != "FROZEN_POSITION_AT_ACCEPTED_BREAK_RELEASE_MATCH_SCOPE" or \
            selection.get("authorized_stage") != "B1_TESTS_FIRST_CPU_DIRECT_CODE":
        errors.append("selection_contract")

    contract = value.get("implementation_contract")
    if not isinstance(contract, dict) or contract.get("row_id") != "WPA-II-06-D" or \
            contract.get("sample_event") != "ACCEPTED_BREAK_SHOT_RELEASE" or \
            contract.get("position_source") != "FROZEN_COMMITTED_CUE_BALL_CENTER" or \
            contract.get("foot_side_relation") != "STRICTLY_GREATER_THAN_HEAD_STRING_AFTER_QUANTIZATION" or \
            contract.get("quantization_grid_world_units") != 0.000001 or \
            contract.get("exact_half_is_offense") is not False or \
            contract.get("first_offense_result") != "REMINDER" or \
            contract.get("subsequent_same_player_result") != "FOUL" or \
            contract.get("history_partition") != "PER_PLAYER" or \
            contract.get("history_scope") != "MATCH" or \
            contract.get("official_rerack_clears_history") is not False or \
            contract.get("product_or_automation_reclassifies_geometry") is not False:
        errors.append("implementation_contract")

    atomic = value.get("atomic_and_failure_contract")
    required_true = (
        "successful_cue_application_stages_exactly_one_frozen_sample",
        "history_commit_waits_for_completed_authoritative_physics_shot",
        "other_player_history_is_independent",
        "exact_shot_identity_retry_is_idempotent",
        "conflicting_shot_identity_retry_is_rejected_without_mutation",
        "invalid_actor_or_nonfinite_position_is_rejected_without_mutation",
        "successful_history_and_adjudication_commit_share_one_authority_revision",
    )
    required_false = (
        "failed_physics_commits_history",
        "drag_preview_or_repeated_state_edit_commits_history",
        "rejected_contact_launch_commits_history",
        "balls_moving_rejected_launch_commits_history",
    )
    if not isinstance(atomic, dict) or any(atomic.get(key) is not True for key in required_true) or \
            any(atomic.get(key) is not False for key in required_false):
        errors.append("atomic_contract")

    presentation = value.get("presentation_and_readback_contract")
    if not isinstance(presentation, dict) or presentation.get("automation_command") != "get_rules_state" or \
            presentation.get("visible_only_for_reminder_or_foul") is not True or \
            presentation.get("ui_draw_code_modified") is not False or \
            presentation.get("fresh_gpu_ta_required_before_visual_claim") is not True:
        errors.append("presentation_boundary")

    sources = value.get("source_bindings")
    if not isinstance(sources, dict):
        errors.append("source_bindings")
    elif check_files:
        for key, binding in sources.items():
            if not isinstance(binding, dict):
                errors.append(f"source_binding:{key}")
                continue
            path = resolve(binding.get("path"))
            if path is None or not path.is_file() or sha256(path.read_bytes()) != binding.get("sha256"):
                errors.append(f"source_drift:{key}")

    cpu = value.get("cpu_verification")
    binaries = cpu.get("binaries") if isinstance(cpu, dict) else None
    if not isinstance(binaries, list) or len(binaries) != 8 or \
            any(not isinstance(item, dict) or item.get("exit_code") != 0 for item in binaries):
        errors.append("cpu_results")
    if not isinstance(cpu, dict) or cpu.get("ctest_subset") != {"passed": 8, "failed": 0} or \
            cpu.get("gpu_cocoa_or_opengl_executed") is not False or \
            cpu.get("cmake_source_modified") is not False:
        errors.append("cpu_boundary")
    elif check_files and isinstance(binaries, list):
        build_root = resolve(cpu.get("build_root"))
        if build_root is None:
            errors.append("build_root")
        else:
            for item in binaries:
                if not isinstance(item, dict):
                    continue
                binary = build_root / str(item.get("target", ""))
                if not binary.is_file() or sha256(binary.read_bytes()) != item.get("sha256"):
                    errors.append(f"binary_drift:{item.get('target', 'unknown')}")

    claims = value.get("claims")
    for key in ("selected_policy_cpu_implemented", "accepted_release_sample_authoritative",
                "match_scope_history_cpu_verified", "automation_readback_cpu_verified"):
        if not isinstance(claims, dict) or claims.get(key) is not True:
            errors.append(f"positive_claim:{key}")
    for key in ("wpa_ii_06_d_row_closed", "audit_v5_modified_or_promoted", "profile_complete",
                "m4_complete", "product_visual_evidence_complete", "ui_modified", "cmake_modified",
                "phase4_or_phase5_renderer_modified", "gpu_executed",
                "readiness_or_authorization_created"):
        if not isinstance(claims, dict) or claims.get(key) is not False:
            errors.append(f"false_completion:{key}")

    if check_files and isinstance(selection, dict):
        try:
            path = resolve(selection.get("path"))
            if path is None or sha256(path.read_bytes()) != selection.get("sha256"):
                errors.append("selection_drift")
            else:
                selected = json.loads(path.read_text(encoding="utf-8"))
                row = next(item for item in selected["decisions"] if item["row_id"] == "WPA-II-06-D")
                if row["selected_option_id"] != selection["selected_option_id"] or \
                        row["root_selected"] is not True or \
                        row["authorized_stage"] != selection["authorized_stage"]:
                    errors.append("selection_row_drift")
        except (OSError, KeyError, StopIteration, TypeError, json.JSONDecodeError):
            errors.append("selection_parse")
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
    print("PASS_WPA_II_06_D_SELECTED_BREAK_RELEASE_HISTORY_IMPLEMENTATION")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
