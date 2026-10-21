#!/usr/bin/env python3
"""Validate the fail-closed WPA II §8 re-spot/first-contact design gate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GATE = ROOT / "docs/rules/wpa-ii-08-all-behind-respot-first-contact-design-gap-gate-v1.json"
PDF_SHA = "2b92dd5088ea630337326e26b7a2a7a4499a9801f2c5da7e5cc40421d5424a24"
AUDIT_SHA = "2811958f1205d9998b40060b83c577f91359d2164e1ac0eaa9ccc4cac2ad787a"
EXPECTED_SOURCE_SHA = {
    "src/Billiards/rules/ball_in_hand_authority.h": "026fd6196263b1c6dab180fa7f94d29baf277ff56bdff450ca189e7d8ba795d6",
    "src/Billiards/rules/rack_executor.h": "d42dc8ae486ad9df8911e4f82cb3d657c0e51e76b35082edb3fd909ee37ef242",
    "src/Billiards/rules/rack_executor.cpp": "d1805c8e197f6b1a8f712f80ba8be807217e875d56def3c7d00186293cd9fb17",
    "src/Billiards/rules/chinese_eight_ball_rules.h": "86b6bf332fe5ca182580c57f82c74ddda075fda28b659019c709bc459e785793",
    "src/Billiards/rules/chinese_eight_ball_rules.cpp": "b984639bdf239814e0a1e0a14a64d6b73de56841273f71d7671c20e86bf475a4",
    "src/Billiards/rules/shot_fact_collector.h": "88dfc28370b4b56936ac88ac560623a8600d89a295b1f98efff65471de17d615",
    "src/Billiards/rules/shot_fact_collector.cpp": "c901ba8591ebd301ccbd4a2ee4fe2ab02f390436cf80aed0596074f65e0ec4c4",
    "src/Billiards/game_runtime.h": "61453e15d88d5581fa439fa77eb6b834fc0a22a5366b470b182b17b8d4d9f725",
    "src/Billiards/game_runtime.cpp": "fca6d4f8ffc54b5d68f3aa24928f71416b003ac8ae37ef7ab4d376aa78684ca4",
    "src/Billiards/render/modern/modern_product_session.h": "bc7862aac74640bd9fb2aa5929eddc3601b89d5e17b5cd047c8fd8e5578f003d",
    "src/Billiards/render/modern/modern_product_session.cpp": "d7a3d4e88e3ec8723d78b7ed37808463fbfe9f0e005e71b409c82d4a3e7cb5e9",
    "tests/rules_runtime_integration_tests.cpp": "aab289bcb7e01d037e6022d41676145e09027d2d90317b9cf9e2e017335f0698",
    "tests/shot_fact_collector_tests.cpp": "4a5a4fee924cf62c29551844af718a4b73e6b5917d1b1323dcac29b20d43683c",
    "tests/chinese_eight_ball_rules_tests.cpp": "4c4079159e1db19545a82c1ad43b1cdfbee8ea4462b9c66c92a8be13322f3b8e",
    "tests/rules_decision_session_integration_tests.cpp": "71e7fe78f078c2287814f9b836ff9588275ee532da32fb274611f1d2fde970f2",
}
EXPECTED_ACTIONS = [
    "rules.ball_in_hand.request_nearest_legal_respot",
    "rules.ball_in_hand.choose_equidistant_respot_ball",
]
EXPECTED_MISSING_FACTS = [
    "behind_head_string_first_contact_restriction_active",
    "head_string_world_z_cm",
    "cue_ball_crossed_head_string_before_first_contact",
    "crossing_event_physics_tick_and_time_of_impact",
    "crossing_evidence_origin_and_completeness",
]


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def workspace_path(raw: Any) -> Path | None:
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
        return ["gate_not_object"]
    if value.get("schema") != "wpa_ii_08_all_behind_respot_first_contact_design_gap_gate_v1" or \
            value.get("status") != "POLICY_AND_TELEMETRY_PENDING_NO_PRODUCTION_MUTATION" or \
            value.get("scope") != ["WPA-II-08"]:
        errors.append("identity_status_scope")

    official = value.get("official_source")
    if not isinstance(official, dict) or \
            official.get("path") != "tmp/pdfs/250816-Rules-of-Heyball.pdf" or \
            official.get("sha256") != PDF_SHA or \
            official.get("visually_reviewed_pdf_pages") != [8]:
        errors.append("official_binding")
    else:
        section = official.get("section_8", {})
        if section.get("chapter") != "II" or section.get("clause") != "8" or \
                section.get("pdf_page") != 8 or \
                section.get("locator") != "Ball in Hand Behind the Head String":
            errors.append("official_section")
        resolved = official.get("text_uniquely_determines")
        required_resolved = {
            "the re-spot request belongs to the player holding the BehindHeadString entitlement",
            "at least one current legal object ball must exist and every such ball must be strictly behind the Head String",
            "a ball exactly on the Head String is not behind it and prevents the all-behind predicate when legal",
            "the candidate is the legal object ball with the smallest center-to-line distance",
            "the entitled player chooses only when two or more eligible candidates are exactly equidistant",
            "the selected object ball is re-spotted at the foot spot under the Section 7 obstruction procedure",
        }
        omissions = official.get("text_does_not_specify")
        required_omissions = {
            "whether cue-ball passage past the Head String is measured by center, leading edge, or the entire ball",
            "the numerical tolerance or quantization policy for a crossing event",
            "how to order a continuous crossing against first contact when both occur within one physics step",
        }
        if not isinstance(resolved, list) or set(resolved) != required_resolved or \
                len(resolved) != len(required_resolved):
            errors.append("official_resolved_contract")
        if not isinstance(omissions, list) or set(omissions) != required_omissions or \
                len(omissions) != len(required_omissions):
            errors.append("official_ambiguities")
        if check_files:
            path = workspace_path(official.get("path"))
            if path is None or not path.is_file() or sha256(path) != PDF_SHA:
                errors.append("official_pdf_drift")

    expected_audit = {
        "path": "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v5.json",
        "sha256": AUDIT_SHA,
        "row_id": "WPA-II-08",
        "row_status": "PARTIAL",
        "product_reachable": False,
        "product_placement_subflow_reachable": True,
    }
    audit = value.get("frozen_audit")
    if audit != expected_audit:
        errors.append("audit_binding")
    elif check_files:
        path = workspace_path(audit["path"])
        if path is None or not path.is_file() or sha256(path) != AUDIT_SHA:
            errors.append("audit_drift")
        else:
            audit_value = json.loads(path.read_text(encoding="utf-8"))
            rows = [row for row in audit_value.get("matrix", [])
                    if row.get("id") == "WPA-II-08"]
            if len(rows) != 1 or rows[0].get("status") != "PARTIAL" or \
                    rows[0].get("product_reachable") is not False:
                errors.append("audit_row_boundary")

    bindings = value.get("source_bindings")
    if not isinstance(bindings, dict) or len(bindings) != len(EXPECTED_SOURCE_SHA):
        errors.append("source_binding_inventory")
    else:
        actual = {item.get("path"): item.get("sha256")
                  for item in bindings.values() if isinstance(item, dict)}
        if actual != EXPECTED_SOURCE_SHA:
            errors.append("source_bindings")
        if check_files:
            for path_text, digest in EXPECTED_SOURCE_SHA.items():
                path = workspace_path(path_text)
                if path is None or not path.is_file() or sha256(path) != digest:
                    errors.append(f"source_drift:{path_text}")

    respot = value.get("resolved_respot_contract")
    sample = respot.get("sample_time", {}) if isinstance(respot, dict) else {}
    legal = respot.get("legal_object_predicate", {}) if isinstance(respot, dict) else {}
    if not isinstance(respot, dict) or \
            respot.get("authority_context") != "current live ball_in_hand_behind_head_string Core-G offer only" or \
            respot.get("actor") != "exact actor_player_id from that offer" or \
            respot.get("request_is_optional") is not True or \
            sample.get("status") != "RESOLVED_IMPLEMENTATION_BOUNDARY" or \
            sample.get("selected") != "request_stage_with_atomic_commit_revalidation" or \
            legal.get("must_match_existing_first_contact_predicate") is not True or \
            respot.get("behind_predicate") != "finite active object-ball center world_z_cm < head_string_world_z_cm" or \
            respot.get("trigger") != "eligible set is non-empty and every eligible ball satisfies behind_predicate" or \
            respot.get("candidate") != "eligible ball with maximum world_z_cm, equivalently minimum positive center distance behind the Head String" or \
            respot.get("foundation_direct_product_access") is not False or \
            respot.get("preview_mutates_authority") is not False:
        errors.append("resolved_respot_contract")

    contact = value.get("first_contact_classification")
    decisions = contact.get("policy_decisions", {}) if isinstance(contact, dict) else {}
    geometry = decisions.get("cue_ball_past_line_geometry", {})
    numeric = decisions.get("crossing_numeric_policy", {})
    if not isinstance(contact, dict) or \
            contact.get("status") != "POLICY_AND_TELEMETRY_PENDING" or \
            contact.get("missing_shot_facts") != EXPECTED_MISSING_FACTS or \
            contact.get("physics_solver_may_not_decide_rule_legality") is not True or \
            contact.get("production_implementation_authorized") is not False or \
            geometry != {
                "status": "POLICY_PENDING", "selected": None,
                "allowed_candidates": ["center_crosses_line", "leading_edge_crosses_line", "entire_ball_crosses_line"],
                "forbidden_guess": True,
            } or numeric != {
                "status": "POLICY_PENDING", "selected": None,
                "allowed_candidates": ["exact_continuous_event", "documented_deterministic_quantization"],
                "forbidden_guess": True,
            }:
        errors.append("first_contact_pending_boundary")

    transaction = value.get("future_parent_child_transaction")
    if not isinstance(transaction, dict) or \
            transaction.get("allowed_product_actions") != EXPECTED_ACTIONS or \
            transaction.get("foundation_action_product_exposed") is not False or \
            transaction.get("other_rules_actions_product_exposed") is not False or \
            transaction.get("global_rules_decision_ui_product_exposed") is not False or \
            transaction.get("partial_commit_allowed") is not False or \
            len(transaction.get("unique_candidate_path", [])) != 5 or \
            len(transaction.get("tie_path", [])) != 3 or \
            "zero additional mutation" not in transaction.get("idempotent_retry", "") or \
            "fails closed" not in transaction.get("conflicting_retry", "") or \
            "exact pre-call state" not in transaction.get("failure_rollback", ""):
        errors.append("future_atomic_transaction")

    fixtures = value.get("required_future_fixtures")
    if not isinstance(fixtures, dict) or \
            len(fixtures.get("respot_positive", [])) != 8 or \
            len(fixtures.get("respot_negative", [])) != 11 or \
            len(fixtures.get("first_contact_after_policy", [])) != 8 or \
            len(set(fixtures.get("respot_positive", []))) != 8 or \
            len(set(fixtures.get("respot_negative", []))) != 11 or \
            len(set(fixtures.get("first_contact_after_policy", []))) != 8:
        errors.append("future_fixtures")

    allowlist = value.get("future_implementation_allowlist")
    production = allowlist.get("production_files", []) if isinstance(allowlist, dict) else []
    if not isinstance(allowlist, dict) or \
            allowlist.get("allowlist_is_dormant_until_crossing_policy_resolved") is not True or \
            allowlist.get("cmake_change_requires_separate_coordination") is not True or \
            "src/Billiards/render/modern/modern_renderer.cpp" in production or \
            "CMakeLists.txt" in production:
        errors.append("future_allowlist")

    forbidden = value.get("forbidden_scope")
    required_forbidden = {
        "production source edits in this design-gate task",
        "implementation of an unselected cue-ball crossing geometry or tolerance",
        "direct product exposure of rules.foundation.respot",
        "promotion of WPA-II-08 above PARTIAL",
        "changes to WPA-II-09 tip-touch or until-legal-shot semantics",
        "global rulesDecisionUiProductExposed enablement",
        "CMake changes",
        "Phase4 or Phase5 renderer changes",
        "GPU, Cocoa, or OpenGL execution",
        "readiness generation or authorization issuance",
    }
    if not isinstance(forbidden, list) or set(forbidden) != required_forbidden or \
            len(forbidden) != len(required_forbidden):
        errors.append("forbidden_scope")
    claims = value.get("claims")
    expected_claims = {
        "design_gap_gate_complete": True,
        "respot_candidate_and_actor_resolved": True,
        "respot_sample_time_resolved": True,
        "first_contact_crossing_policy_resolved": False,
        "first_contact_telemetry_complete": False,
        "production_implementation_complete": False,
        "product_behavior_changed": False,
        "audit_row_promoted": False,
        "gpu_executed": False,
    }
    if claims != expected_claims:
        errors.append("claims")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--gate", type=Path, default=DEFAULT_GATE)
    parser.add_argument("--no-file-checks", action="store_true")
    args = parser.parse_args()
    try:
        value = json.loads(args.gate.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        print(json.dumps({"status": "FAIL", "errors": [f"load:{error}"]}))
        return 1
    errors = validate(value, check_files=not args.no_file_checks)
    print(json.dumps({"status": "PASS" if not errors else "FAIL",
                      "errors": errors, "gate": str(args.gate)}, sort_keys=True))
    if errors:
        return 1
    print("PASS_WPA_II_08_RESPOT_FIRST_CONTACT_POLICY_TELEMETRY_PENDING_GATE")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
