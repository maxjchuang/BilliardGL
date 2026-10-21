#!/usr/bin/env python3
"""Validate the fail-closed WPA II 6(f) eight-on-break design-gap gate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GATE = ROOT / "docs/rules/wpa-ii-06-f-eight-on-break-choice-design-gap-gate-v1.json"
EXPECTED_STATUS = (
    "CORE_F_PRECOMMIT_BIH_DEFECT_AND_FULL_PRODUCT_CHOICE_GATE_CLOSED_"
    "UPSTREAM_FOUL_CLASSIFICATION_PENDING_NO_PRODUCTION_MUTATION"
)
EXPECTED_CONTRACT_SHA = "dd55f95a16cb28e4d0185e35dd7bf80de0b7b95500b2798eb5972589327286e4"

EXPECTED_SOURCES: dict[str, dict[str, str]] = {
    "official_pdf": {"path": "tmp/pdfs/250816-Rules-of-Heyball.pdf", "sha256": "2b92dd5088ea630337326e26b7a2a7a4499a9801f2c5da7e5cc40421d5424a24"},
    "frozen_audit_v3": {"path": "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v3.json", "sha256": "d7ab2104fac9fd85fe8d7336ae658cc9baaaa6b4fa866cd4e7815c51404ad033"},
    "rule_core": {"path": "src/Billiards/rules/chinese_eight_ball_rules.cpp", "sha256": "5a298f2bd492dfd6f57b8bb478cabd1a3041716e4411c93642e1aeb85b8727b3"},
    "rule_model": {"path": "src/Billiards/rules/chinese_eight_ball_rules.h", "sha256": "5ab7a045dbae177697a72aed97326d56ee14ae31c0434191066f011a8f52145b"},
    "core_f_parent": {"path": "src/Billiards/rules/eight_on_break_authority.h", "sha256": "dacb83140b687b285f1dc67bcbb38d571349a60caa01a45664f0bbdcc2981b2e"},
    "core_g_child": {"path": "src/Billiards/rules/ball_in_hand_authority.h", "sha256": "026fd6196263b1c6dab180fa7f94d29baf277ff56bdff450ca189e7d8ba795d6"},
    "core_e_foundation": {"path": "src/Billiards/rules/rack_executor.cpp", "sha256": "d1805c8e197f6b1a8f712f80ba8be807217e875d56def3c7d00186293cd9fb17"},
    "core_e_contract": {"path": "src/Billiards/rules/rack_executor.h", "sha256": "d42dc8ae486ad9df8911e4f82cb3d657c0e51e76b35082edb3fd909ee37ef242"},
    "core_h_sibling": {"path": "src/Billiards/rules/break_foul_authority.h", "sha256": "5b4bf09f9950fd61d3d4add4fd70a9304d5c82ab05254f5878c208a6dd92ad9e"},
    "runtime": {"path": "src/Billiards/game_runtime.cpp", "sha256": "3292df4f4546d9fd1639de8d5db33b7dab526f13b129f496d1420a489253fb9a"},
    "runtime_header": {"path": "src/Billiards/game_runtime.h", "sha256": "deacceaa4ffff8b0e5080963d958f5fc49a77a197c744c2693be6f2d82b96ec3"},
    "internal_ui": {"path": "src/Billiards/rules/rules_decision_ui_model.cpp", "sha256": "5d4942db12f43069069d68e00e8aecf7f38106bac4f1330f9d41f2c51e830cb8"},
    "product_session": {"path": "src/Billiards/render/modern/modern_product_session.cpp", "sha256": "8e962dfc76149a4461e011941231e5e72860aed9f8cb6f006169e269455cd174"},
    "product_session_header": {"path": "src/Billiards/render/modern/modern_product_session.h", "sha256": "f9b5ecef2223943bc8cb278b081b8f7f7fb886e7b5a0cc4db4c1fe2bac8528c4"},
    "core_tests": {"path": "tests/chinese_eight_ball_rules_tests.cpp", "sha256": "e587bb2bbc20f7dc682e0ea07e4e30df4484d3ae2ac9622a3d029c286c997a6c"},
    "runtime_tests": {"path": "tests/rules_runtime_integration_tests.cpp", "sha256": "2abb5a981251e05b179c03fb82adc2bd3da1076f291a4c8e0a174743baef8a55"},
    "ui_tests": {"path": "tests/rules_decision_ui_tests.cpp", "sha256": "4bde82aa9bb233ca542c6d4c4ebc440b341c29478aab473ec60f95b3b9208834"},
    "product_tests": {"path": "tests/rules_decision_session_integration_tests.cpp", "sha256": "6082e1d44054a1adc8b3743c2eb31698472a3842626bf25da17db82dadd3220d"},
}


def canonical_sha(value: Any) -> str:
    encoded = json.dumps(
        value, sort_keys=True, separators=(",", ":"), ensure_ascii=False
    ).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def file_sha(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def safe_workspace_path(raw: Any) -> Path | None:
    if not isinstance(raw, str) or not raw or Path(raw).is_absolute():
        return None
    parts = Path(raw).parts
    if ".." in parts:
        return None
    resolved = (ROOT / raw).resolve()
    try:
        resolved.relative_to(ROOT.resolve())
    except ValueError:
        return None
    return resolved


def validate(value: Any, *, check_files: bool = True) -> list[str]:
    errors: list[str] = []
    if not isinstance(value, dict):
        return ["gate_not_object"]

    expected_keys = {
        "schema", "status", "scope", "source_bindings",
        "frozen_contract_sha256", "frozen_contract",
    }
    if set(value) != expected_keys:
        errors.append("top_level_keys")
    if value.get("schema") != "wpa_ii_06_f_eight_on_break_choice_design_gap_gate_v1":
        errors.append("schema")
    if value.get("status") != EXPECTED_STATUS:
        errors.append("status")
    if value.get("scope") != ["WPA-II-06-F"]:
        errors.append("scope")

    contract = value.get("frozen_contract")
    if not isinstance(contract, dict):
        errors.append("frozen_contract")
    else:
        calculated = canonical_sha(contract)
        if value.get("frozen_contract_sha256") != EXPECTED_CONTRACT_SHA:
            errors.append("frozen_contract_declared_sha")
        if calculated != EXPECTED_CONTRACT_SHA:
            errors.append("frozen_contract_content_sha")

        official = contract.get("official_interpretation", {})
        resolved = official.get("resolved_actor_and_action_semantics", {})
        if resolved.get("actor_policy_ambiguous") is not False or \
                resolved.get("breaker_assignment_policy_ambiguous") is not False:
            errors.append("resolved_actor_policy")
        gap = contract.get("observed_authority_gap", {})
        if gap.get("certainty") != "CONFIRMED_BY_STATIC_SOURCE" or \
                gap.get("audit_or_product_promotion_blocked_until_corrected_and_tested") is not True:
            errors.append("precommit_bih_gap")
        classification = contract.get("classification_and_sibling_boundary", {})
        if classification.get("legal_vs_foul_branch", {}).get("status") != \
                "UPSTREAM_BREAK_FOUL_CLASSIFICATION_PENDING" or \
                classification.get("simultaneous_core_f_and_core_h_parent_allowed") is not False:
            errors.append("classification_or_sibling_boundary")
        reach = contract.get("as_built_product_reachability", {})
        if reach.get("partial_spot_parent_product_seam_exists", False):
            errors.append("invented_reachability_key")
        if reach.get("complete_clause_6f_choice_set_product_reachable") is not False or \
                reach.get("either_rerack_action_product_reachable") is not False or \
                reach.get("rules_decision_ui_product_exposed") is not False:
            errors.append("product_reachability")
        claims = contract.get("claims", {})
        if claims.get("core_f_precommit_bih_semantics_correct") is not False or \
                claims.get("complete_clause_6f_product_reachable") is not False or \
                claims.get("audit_promoted") is not False or \
                claims.get("product_behavior_changed") is not False or \
                claims.get("gpu_executed") is not False:
            errors.append("claims")

    sources = value.get("source_bindings")
    if sources != EXPECTED_SOURCES:
        errors.append("source_bindings")
    if isinstance(sources, dict):
        for name, binding in sources.items():
            if not isinstance(binding, dict):
                errors.append(f"source_{name}_not_object")
                continue
            path = safe_workspace_path(binding.get("path"))
            if path is None:
                errors.append(f"source_{name}_unsafe_path")
                continue
            digest = binding.get("sha256")
            if not isinstance(digest, str) or len(digest) != 64:
                errors.append(f"source_{name}_bad_sha")
                continue
            if check_files:
                if not path.is_file():
                    errors.append(f"source_{name}_missing")
                elif file_sha(path) != digest:
                    errors.append(f"source_{name}_drift")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--gate", type=Path, default=DEFAULT_GATE)
    parser.add_argument("--skip-file-checks", action="store_true")
    args = parser.parse_args()
    try:
        value = json.loads(args.gate.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        print(json.dumps({"status": "FAIL", "errors": [f"load:{error}"]}))
        return 1
    errors = validate(value, check_files=not args.skip_file_checks)
    print(json.dumps({
        "status": "PASS" if not errors else "FAIL",
        "errors": errors,
        "gate": str(args.gate),
    }, sort_keys=True))
    if errors:
        return 1
    print("PASS_WPA_II_06_F_DESIGN_GAP_GATE")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
