#!/usr/bin/env python3
"""Validate the WPA II §§8/9 product-placement subflow audit successor."""

from __future__ import annotations

from collections import Counter
import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_AUDIT = ROOT / "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v5.json"
V4_PATH = ROOT / "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v4.json"
EXPECTED_V4_SHA = "5252add5a0364cdc6bf596c622bbf2f40c838650f703d8b155f23f3c73c3456f"
EXPECTED_SUMMARY = {"total": 40, "implemented": 11, "partial": 9,
                    "missing": 13, "procedural_only": 7}
EXPECTED_SOURCES = {
    "src/Billiards/rules/ball_in_hand_authority.h": "026fd6196263b1c6dab180fa7f94d29baf277ff56bdff450ca189e7d8ba795d6",
    "src/Billiards/game_runtime.h": "61453e15d88d5581fa439fa77eb6b834fc0a22a5366b470b182b17b8d4d9f725",
    "src/Billiards/game_runtime.cpp": "fca6d4f8ffc54b5d68f3aa24928f71416b003ac8ae37ef7ab4d376aa78684ca4",
    "src/Billiards/render/modern/modern_product_session.h": "bc7862aac74640bd9fb2aa5929eddc3601b89d5e17b5cd047c8fd8e5578f003d",
    "src/Billiards/render/modern/modern_product_session.cpp": "d7a3d4e88e3ec8723d78b7ed37808463fbfe9f0e005e71b409c82d4a3e7cb5e9",
    "tests/rules_runtime_integration_tests.cpp": "aab289bcb7e01d037e6022d41676145e09027d2d90317b9cf9e2e017335f0698",
    "tests/rules_decision_session_integration_tests.cpp": "71e7fe78f078c2287814f9b836ff9588275ee532da32fb274611f1d2fde970f2",
}
EXPECTED_CPU = {
    "BilliardsRulesRuntimeIntegrationTests": "67ee35915a4c8306a016deda672ad7cd17f5f26a670a77f1eb2fc28106320afe",
    "BilliardsRulesDecisionSessionIntegrationTests": "a6f94813c4584fa76553c955c7153fb7050aac371a1b692d4ed00f9152a44092",
    "BilliardsAutomationControllerTests": "9529ab7afc6dbbf5441285d2c3c5622cce592d8dea0f30f619e1ab6bceb3b68a",
}
TARGETS = {"WPA-II-08", "WPA-II-09"}


def file_sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


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
        return ["audit_not_object"]
    if value.get("schema") != "phase5_wpa_heyball_as_built_reconciliation_audit_v5" or \
            value.get("schema_version") != 5 or \
            value.get("audit_scope") != "targeted_product_subflow_wpa_ii_08_09":
        errors.append("schema_or_scope")
    if value.get("frozen_v4_predecessor") != {
        "path": "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v4.json",
        "sha256": EXPECTED_V4_SHA,
    }:
        errors.append("frozen_v4_predecessor")

    matrix = value.get("matrix")
    if not isinstance(matrix, list) or len(matrix) != 40:
        return errors + ["matrix"]
    rows = {row.get("id"): row for row in matrix if isinstance(row, dict)}
    if len(rows) != 40 or not TARGETS.issubset(rows):
        return errors + ["matrix_ids"]
    expected_gaps = {
        "WPA-II-08": [
            "Implement and product-authorize the all-legal-object-balls-behind-string re-spot transaction.",
            "Close the post-placement first-contact restriction lifecycle without inferring missing policy.",
        ],
        "WPA-II-09": [
            "Define and implement the authoritative cue-tip-touch classification while placement remains active.",
            "Close the product lifecycle for continued cue-ball adjustment until the legal shot boundary.",
        ],
    }
    expected_evidence = {
        "WPA-II-08": {
            "src/Billiards/rules/ball_in_hand_authority.h",
            "src/Billiards/game_runtime.cpp",
            "src/Billiards/render/modern/modern_product_session.cpp",
            "tests/rules_runtime_integration_tests.cpp",
            "tests/rules_decision_session_integration_tests.cpp",
        },
        "WPA-II-09": {
            "src/Billiards/rules/ball_in_hand_authority.h",
            "src/Billiards/game_runtime.cpp",
            "src/Billiards/render/modern/modern_product_session.cpp",
            "tests/rules_runtime_integration_tests.cpp",
            "tests/rules_decision_session_integration_tests.cpp",
        },
    }
    for row_id in sorted(TARGETS):
        row = rows[row_id]
        if row.get("status") != "PARTIAL" or \
                row.get("product_reachable") is not False or \
                row.get("m4_required") is not True or \
                row.get("product_action_required") is not True or \
                row.get("gaps") != expected_gaps[row_id]:
            errors.append(f"target_boundary:{row_id}")
        paths = {item.get("path") for item in row.get("evidence", [])
                 if isinstance(item, dict)}
        if paths != expected_evidence[row_id]:
            errors.append(f"target_evidence:{row_id}")

    contract = value.get("wpa_ii_08_09_product_placement_subflow", {})
    required_true = [
        "requires_match_screen", "requires_current_live_offer",
        "requires_local_offered_actor", "requires_current_legal_preview",
        "ruleset_capability_flow_decision_revision_state_hash_bound",
        "position_payload_is_authoritatively_revalidated",
        "exact_retry_returns_original_ack",
        "conflicting_retry_fails_without_mutation", "product_subflow_reachable",
    ]
    required_false = [
        "preview_authority_mutation",
        "foundation_or_other_rules_actions_product_exposed",
        "rules_decision_ui_product_exposed", "complete_clause_product_reachable",
        "gpu_executed",
    ]
    if contract.get("rows") != ["WPA-II-08", "WPA-II-09"] or \
            contract.get("allowed_actions") != {
                "BehindHeadString": "rules.ball_in_hand.confirm_behind_head_string",
                "Anywhere": "rules.ball_in_hand.confirm_full_table_placement",
            } or contract.get("row_status_promotions") != [] or \
            any(contract.get(key) is not True for key in required_true) or \
            any(contract.get(key) is not False for key in required_false):
        errors.append("subflow_contract")
    unresolved = contract.get("unresolved_clause_semantics", {})
    if unresolved != {
        "WPA-II-08": [
            "all_legal_object_balls_behind_string_respot_product_transaction",
            "post_placement_first_contact_product_lifecycle",
        ],
        "WPA-II-09": [
            "cue_tip_touch_during_placement_classification",
            "continued_move_until_legal_shot_product_lifecycle",
        ],
    }:
        errors.append("unresolved_semantics")

    try:
        predecessor = json.loads(V4_PATH.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        predecessor = None
        errors.append("predecessor_load")
    if isinstance(predecessor, dict):
        old_rows = {row["id"]: row for row in predecessor["matrix"]}
        invariant_fields = ("source", "requirement", "status", "m4_required",
                            "policy_decision_required", "product_action_required",
                            "product_reachable", "core_increments")
        for row_id, row in rows.items():
            old = old_rows[row_id]
            for field in invariant_fields:
                if row.get(field) != old.get(field):
                    errors.append(f"predecessor_semantic_drift:{row_id}:{field}")
            if row_id not in TARGETS:
                for field in ("assessment", "gaps"):
                    if row.get(field) != old.get(field):
                        errors.append(f"non_target_drift:{row_id}:{field}")

    counts = Counter(row.get("status") for row in rows.values())
    computed = {"total": len(rows), "implemented": counts["IMPLEMENTED"],
                "partial": counts["PARTIAL"], "missing": counts["MISSING"],
                "procedural_only": counts["PROCEDURAL_ONLY"]}
    if value.get("summary") != EXPECTED_SUMMARY or computed != EXPECTED_SUMMARY:
        errors.append("summary")
    expected_unclosed = [row["id"] for row in matrix
                         if row.get("m4_required") and
                         row.get("status") != "IMPLEMENTED"]
    if value.get("m4_required_unclosed") != expected_unclosed or \
            not TARGETS.issubset(expected_unclosed):
        errors.append("m4_unclosed")
    increments = value.get("completed_product_subincrements")
    if increments != [{
        "priority": "P0",
        "rows": ["WPA-II-08", "WPA-II-09"],
        "result": "Core-G BehindHeadString/Anywhere placement preview and confirm are product reachable through a fail-closed narrow seam.",
        "rows_remain_partial": True,
    }]:
        errors.append("subincrement_boundary")
    target_plans = [plan for plan in
                    value.get("next_deterministic_implementation_plan", [])
                    if any(row in TARGETS for row in plan.get("rows", []))]
    if target_plans != [{
        "priority": "P1",
        "rows": ["WPA-II-08", "WPA-II-09"],
        "depends_on": [
            "all_behind_string_respot_authority",
            "post_placement_first_contact_lifecycle",
            "cue_tip_touch_placement_classification",
            "until_legal_shot_adjustment_lifecycle",
        ],
        "deliverable": "Close the remaining official clause semantics; do not infer them from the completed Core-G placement subflow.",
    }]:
        errors.append("target_followup_plan")

    bindings = value.get("wpa_ii_08_09_implementation_bindings", {})
    if bindings.get("bound_files") != [
            {"path": path, "sha256": digest}
            for path, digest in EXPECTED_SOURCES.items()]:
        errors.append("source_bindings")
    if bindings.get("cpu_results") != [
            {"target": target, "sha256": digest, "exit_code": 0}
            for target, digest in EXPECTED_CPU.items()] or \
            bindings.get("cmake_changed_by_increment") is not False or \
            bindings.get("gpu_cocoa_or_opengl_executed") is not False or \
            bindings.get("readiness_generated") is not False or \
            bindings.get("authorization_issued") is not False:
        errors.append("cpu_or_forbidden_bindings")

    if check_files:
        if not V4_PATH.is_file() or file_sha(V4_PATH) != EXPECTED_V4_SHA:
            errors.append("predecessor_drift")
        for path_text, digest in EXPECTED_SOURCES.items():
            path = safe_path(path_text)
            if path is None or not path.is_file() or file_sha(path) != digest:
                errors.append(f"source_drift:{path_text}")
        build_root = ROOT / "build-wpa-08-09-product-placement-cpu-v1/fresh"
        for target, digest in EXPECTED_CPU.items():
            path = build_root / target
            if not path.is_file() or file_sha(path) != digest:
                errors.append(f"cpu_drift:{target}")
        for row in matrix:
            for item in row.get("evidence", []):
                path = safe_path(item.get("path"))
                if path is None or not path.is_file() or \
                        file_sha(path) != item.get("sha256"):
                    errors.append(
                        f"evidence_drift:{row.get('id')}:{item.get('path')}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--audit", type=Path, default=DEFAULT_AUDIT)
    parser.add_argument("--no-file-checks", action="store_true")
    args = parser.parse_args()
    try:
        value = json.loads(args.audit.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        print(json.dumps({"status": "FAIL", "errors": [f"load:{error}"]}))
        return 1
    errors = validate(value, check_files=not args.no_file_checks)
    print(json.dumps({"status": "PASS" if not errors else "FAIL",
                      "errors": errors, "audit": str(args.audit)},
                     sort_keys=True))
    if errors:
        return 1
    print("PASS_PHASE5_WPA_HEYBALL_AS_BUILT_AUDIT_V5_WPA_II_08_09_SUBFLOW")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
