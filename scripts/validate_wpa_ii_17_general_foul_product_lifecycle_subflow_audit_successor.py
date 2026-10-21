#!/usr/bin/env python3
"""Validate the fail-closed WPA II §17 classified-foul product subflow audit."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/rules/wpa-ii-17-general-foul-product-lifecycle-subflow-audit-successor-v1.json"
PDF_SHA = "2b92dd5088ea630337326e26b7a2a7a4499a9801f2c5da7e5cc40421d5424a24"
AUDIT_SHA = "2811958f1205d9998b40060b83c577f91359d2164e1ac0eaa9ccc4cac2ad787a"
SOURCE_SHA = {
    "src/Billiards/rules/chinese_eight_ball_rules.h": "86b6bf332fe5ca182580c57f82c74ddda075fda28b659019c709bc459e785793",
    "src/Billiards/rules/chinese_eight_ball_rules.cpp": "b984639bdf239814e0a1e0a14a64d6b73de56841273f71d7671c20e86bf475a4",
    "src/Billiards/rules/shot_fact_collector.h": "88dfc28370b4b56936ac88ac560623a8600d89a295b1f98efff65471de17d615",
    "src/Billiards/rules/shot_fact_collector.cpp": "c901ba8591ebd301ccbd4a2ee4fe2ab02f390436cf80aed0596074f65e0ec4c4",
    "src/Billiards/game_runtime.h": "61453e15d88d5581fa439fa77eb6b834fc0a22a5366b470b182b17b8d4d9f725",
    "src/Billiards/game_runtime.cpp": "fca6d4f8ffc54b5d68f3aa24928f71416b003ac8ae37ef7ab4d376aa78684ca4",
    "src/Billiards/physics.cpp": "fc497d79650bf2ff9cd8570c320653c8742e4201b8a2428a7892bfcb0124de77",
    "src/Billiards/rules/rules_presentation.cpp": "383a4d4488f406b0e7cac336aef4ac50ae3206527e17f79f0e69df82f3100044",
    "src/Billiards/render/modern/modern_product_session.h": "bc7862aac74640bd9fb2aa5929eddc3601b89d5e17b5cd047c8fd8e5578f003d",
    "src/Billiards/render/modern/modern_product_session.cpp": "d7a3d4e88e3ec8723d78b7ed37808463fbfe9f0e005e71b409c82d4a3e7cb5e9",
    "tests/chinese_eight_ball_rules_tests.cpp": "4c4079159e1db19545a82c1ad43b1cdfbee8ea4462b9c66c92a8be13322f3b8e",
    "tests/shot_fact_collector_tests.cpp": "4a5a4fee924cf62c29551844af718a4b73e6b5917d1b1323dcac29b20d43683c",
    "tests/rules_runtime_integration_tests.cpp": "aab289bcb7e01d037e6022d41676145e09027d2d90317b9cf9e2e017335f0698",
    "tests/rules_decision_session_integration_tests.cpp": "71e7fe78f078c2287814f9b836ff9588275ee532da32fb274611f1d2fde970f2",
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
        return ["audit_not_object"]
    if value.get("schema") != "wpa_ii_17_general_foul_product_lifecycle_subflow_audit_successor_v1" or \
            value.get("status") != "CLASSIFIED_SUBFLOW_PRODUCT_REACHABLE_FULL_SECTION_PARTIAL_NO_PRODUCTION_MUTATION" or \
            value.get("scope") != ["WPA-II-17", "WPA-II-12-3-CONFLICT-BOUNDARY"]:
        errors.append("identity_status_scope")

    official = value.get("official_source", {})
    conflict = official.get("policy_conflict", {}) if isinstance(official, dict) else {}
    section17 = official.get("section_17", {}) if isinstance(official, dict) else {}
    section123 = official.get("section_12_3", {}) if isinstance(official, dict) else {}
    if not isinstance(official, dict) or official.get("path") != "tmp/pdfs/250816-Rules-of-Heyball.pdf" or \
            official.get("sha256") != PDF_SHA or official.get("visually_reviewed_pdf_pages") != [9, 11] or \
            section17.get("clause") != "17(a)-(l)" or section17.get("pdf_page") != 11 or \
            section123.get("clause") != "12.3" or section123.get("pdf_page") != 9 or \
            conflict.get("status") != "POLICY_PENDING" or conflict.get("machine_resolution") is not None or \
            conflict.get("current_code_is_not_authorized_as_the_official_conflict_resolution") is not True:
        errors.append("official_and_conflict_binding")
    elif check_files:
        path = workspace_path(official.get("path"))
        if path is None or not path.is_file() or sha256(path) != PDF_SHA:
            errors.append("official_pdf_drift")

    audit = value.get("frozen_audit", {})
    expected_rows = [
        {"id": "WPA-II-12-3", "status": "PARTIAL", "product_reachable": True, "policy_decision_required": True},
        {"id": "WPA-II-17", "status": "PARTIAL", "product_reachable": True, "policy_decision_required": True},
    ]
    if not isinstance(audit, dict) or audit.get("path") != "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v5.json" or \
            audit.get("sha256") != AUDIT_SHA or audit.get("rows") != expected_rows or \
            audit.get("row_promotion_authorized") is not False or audit.get("complete_section_claim_authorized") is not False:
        errors.append("frozen_audit_binding")
    elif check_files:
        path = workspace_path(audit.get("path"))
        if path is None or not path.is_file() or sha256(path) != AUDIT_SHA:
            errors.append("frozen_audit_drift")
        else:
            parsed = json.loads(path.read_text(encoding="utf-8"))
            rows = {row.get("id"): row for row in parsed.get("matrix", [])}
            for expected in expected_rows:
                row = rows.get(expected["id"], {})
                if any(row.get(key) != expected[key] for key in ("status", "product_reachable", "policy_decision_required")):
                    errors.append(f"frozen_audit_row_drift:{expected['id']}")

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

    facts = value.get("current_fact_authority", {})
    expected_reasons = ["illegal_break", "no_object_ball_contact", "wrong_first_contact",
                        "no_rail_after_contact", "cue_ball_scratch", "cue_ball_off_table",
                        "object_ball_off_table", "illegal_eight_ball"]
    scratch = facts.get("cue_ball_scratch_gap", {}) if isinstance(facts, dict) else {}
    if not isinstance(facts, dict) or facts.get("represented_foul_reasons") != expected_reasons or \
            len(facts.get("collector_success_path_observes", [])) != 4 or len(facts.get("collector_does_not_observe", [])) != 8 or \
            scratch.get("shot_facts_field_exists") is not True or scratch.get("production_physics_sets_cue_ball_pocket_event") is not True or \
            scratch.get("production_physics_sets_cue_ball_pocketed_state") is not False or \
            scratch.get("collector_detects_only_false_to_true_pocketed_state_transition") is not True or \
            scratch.get("physical_authoritative_collection_complete") is not False or \
            facts.get("general_foul_origin_and_completeness_attestation_exists") is not False or \
            facts.get("direct_or_automation_injected_shot_facts_are_not_physical_telemetry_proof") is not True:
        errors.append("fact_authority_boundary")

    matrix = value.get("clause_matrix")
    expected_clauses = [f"17({letter})" for letter in "abcdefghijkl"]
    if not isinstance(matrix, list) or len(matrix) != 12 or [row.get("clause") for row in matrix if isinstance(row, dict)] != expected_clauses:
        errors.append("clause_matrix_identity")
    else:
        by_clause = {row["clause"]: row for row in matrix}
        if by_clause["17(a)"].get("pending") != "PHYSICAL_ORIGIN_AND_COMPLETENESS" or \
                by_clause["17(b)"].get("telemetry") != "RUNTIME_COLLECTED_ON_SUCCEEDED_SHOT_WITHOUT_GENERAL_COMPLETENESS_ATTESTATION" or \
                by_clause["17(c)"].get("classification") != "NOT_REPRESENTED_AS_FOUL" or \
                by_clause["17(e)"].get("classification") != "CURRENT_CODE_FOUL_NOT_POLICY_AUTHORIZED" or \
                "12-3-VS-17-E" not in by_clause["17(e)"].get("pending", "") or \
                by_clause["17(g)"].get("classification") != "ABSENT" or by_clause["17(h)"].get("classification") != "ABSENT" or \
                by_clause["17(i)"].get("telemetry") != "PLACEMENT_GEOMETRY_ONLY" or \
                by_clause["17(l)"].get("pending") != "REFEREE_INPUT_REPEAT_HISTORY_SCOPE_AND_LOSS_OF_RACK_TRANSACTION":
            errors.append("clause_matrix_boundary")

    subflow = value.get("classified_foul_bih_product_subflow", {})
    if not isinstance(subflow, dict) or len(subflow.get("adjudication", [])) != 3 or \
            len(subflow.get("runtime_commit", [])) != 4 or len(subflow.get("core_g", [])) != 4 or \
            len(subflow.get("modern_product_session", [])) != 4 or \
            subflow.get("presentation_reads_current_match_state_ball_in_hand") is not True or \
            subflow.get("last_adjudication_ball_in_hand_is_historical_verdict_not_live_entitlement") is not True or \
            subflow.get("global_rules_decision_ui_product_exposed") is not False or \
            subflow.get("lifecycle_gap_found_for_already_classified_subset") is not False or \
            subflow.get("production_fix_required") is not False:
        errors.append("classified_subflow_contract")

    future = value.get("future_closure_contract", {})
    if not isinstance(future, dict) or len(future.get("required_telemetry", [])) != 6 or \
            len(future.get("atomic_classification", [])) != 4 or len(future.get("required_policy_gates", [])) != 4 or \
            "without a second entitlement" not in future.get("exact_retry", "") or \
            "zero mutation" not in future.get("conflicting_retry", "") or future.get("partial_commit_allowed") is not False:
        errors.append("future_closure_contract")

    forbidden = value.get("forbidden_scope")
    required_forbidden = {
        "promoting WPA-II-17 above PARTIAL",
        "treating ShotFacts schema presence or automation injection as physical telemetry",
        "treating product prevention of a shot while balls move as a committed foul",
        "resolving WPA-II-12-3 versus WPA-II-17(e) without an approved policy ruling",
        "enabling global rulesDecisionUiProductExposed",
        "production or CMake changes",
        "Phase4 or Phase5 renderer changes",
        "GPU, Cocoa, OpenGL, readiness, or authorization execution",
    }
    if not isinstance(forbidden, list) or len(forbidden) != len(required_forbidden) or set(forbidden) != required_forbidden:
        errors.append("forbidden_scope")

    claims = value.get("claims")
    expected_claims = {
        "classified_non_break_foul_bih_product_subflow_reachable": True,
        "classified_subflow_atomic_and_idempotent": True,
        "full_wpa_ii_17_telemetry_complete": False,
        "full_wpa_ii_17_classification_complete": False,
        "all_listed_fouls_product_reachable": False,
        "cue_ball_scratch_physical_collection_complete": False,
        "object_ball_off_table_policy_resolved": False,
        "global_rules_decision_ui_product_exposed": False,
        "production_source_changed": False,
        "audit_row_promoted": False,
        "gpu_executed": False,
    }
    if claims != expected_claims:
        errors.append("claims")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--audit", type=Path, default=DEFAULT)
    args = parser.parse_args()
    try:
        value = json.loads(args.audit.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        print(f"FAIL_WPA_II_17_SUBFLOW_AUDIT:{exc}")
        return 1
    errors = validate(value, check_files=True)
    if errors:
        print("FAIL_WPA_II_17_SUBFLOW_AUDIT:" + ",".join(errors))
        return 1
    print("PASS_WPA_II_17_CLASSIFIED_FOUL_PRODUCT_SUBFLOW_AUDIT")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
