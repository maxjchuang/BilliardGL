#!/usr/bin/env python3
"""Validate the fail-closed WPA II 19 group-confusion design gate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GATE = ROOT / "docs/rules/wpa-ii-19-ball-group-confusion-discovery-rerack-design-gap-gate-v1.json"
PDF_SHA = "2b92dd5088ea630337326e26b7a2a7a4499a9801f2c5da7e5cc40421d5424a24"
AUDIT_SHA = "2811958f1205d9998b40060b83c577f91359d2164e1ac0eaa9ccc4cac2ad787a"
SOURCE_SHA = {
    "src/Billiards/rules/chinese_eight_ball_rules.h": "7408e24a58c827e3de27b766cd91bdb5ce5831919cea97f733f8191baae59f7f",
    "src/Billiards/rules/chinese_eight_ball_rules.cpp": "a591b919e99580a8e859e98ebfda9de1a0f2ec89e521d47f67ef180c6145f930",
    "src/Billiards/rules/group_choice_authority.h": "9ebf1e938b7248e34245f3ce40205d5438e06d6d3463a2d7cf986063e55101d3",
    "src/Billiards/rules/shot_fact_collector.h": "13453f30d0eeb7ce4231b1f5802eaf3317e085f4187e3e16abaff34cb171986c",
    "src/Billiards/rules/shot_fact_collector.cpp": "92b205a1a0025b21a47fbd86e755beceeb4f89208b5db28945ddfcdd1f7e5f7d",
    "src/Billiards/game_runtime.h": "61453e15d88d5581fa439fa77eb6b834fc0a22a5366b470b182b17b8d4d9f725",
    "src/Billiards/game_runtime.cpp": "fca6d4f8ffc54b5d68f3aa24928f71416b003ac8ae37ef7ab4d376aa78684ca4",
    "src/Billiards/rules/rack_executor.h": "d42dc8ae486ad9df8911e4f82cb3d657c0e51e76b35082edb3fd909ee37ef242",
    "src/Billiards/rules/rack_executor.cpp": "d1805c8e197f6b1a8f712f80ba8be807217e875d56def3c7d00186293cd9fb17",
    "src/Billiards/replay/replay_recorder.h": "90ac051be4dda3294433e545fc1326152711cc44733c0207c2bcd924a6b38c7a",
    "src/Billiards/replay/replay_recorder.cpp": "0864c2d7af54230eb1adc7851466ffd6201e600d21574fc6de450cbcfd1569ed",
    "src/Billiards/game_state.h": "6363b8c2481df6702bb22c4b00b44a546eb594241a7744684c2c94c96adaed4d",
    "src/Billiards/game_state.cpp": "a91185ba5fad8c8571aa1b0efd36de967d0fc3c307fef01b2903c2f910de88c3",
    "src/Billiards/render/modern/modern_product_session.h": "bc7862aac74640bd9fb2aa5929eddc3601b89d5e17b5cd047c8fd8e5578f003d",
    "src/Billiards/render/modern/modern_product_session.cpp": "d7a3d4e88e3ec8723d78b7ed37808463fbfe9f0e005e71b409c82d4a3e7cb5e9",
    "tests/chinese_eight_ball_rules_tests.cpp": "950a98501feca064cbaab8c4b1025b3b421dfcbc8b59a8f82739042069d7997c",
    "tests/shot_fact_collector_tests.cpp": "2b592431fdee204a6c7cfb976125d8697770a477a0749e7e7fcc332c66bbd401",
    "tests/rules_runtime_integration_tests.cpp": "f4f67a8f2b34b7c35ce8c555d12d7a4b65922c4f5710d21c8efe66408476152c",
    "tests/replay_recorder_tests.cpp": "40e47f08df57c8437482d05050589e16433ed93e6cea96b4543e805283578b02",
    "tests/rules_decision_session_integration_tests.cpp": "318561223599fae85fa628d03b104f01cd7dedeb169fd6975deac6aac12f3379",
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def resolve(raw: str) -> Path | None:
    path = Path(raw)
    if path.is_absolute() or ".." in path.parts:
        return None
    result = (ROOT / path).resolve()
    return result if result.is_relative_to(ROOT.resolve()) else None


def exact_list(value: Any, expected: set[str]) -> bool:
    return isinstance(value, list) and len(value) == len(expected) and set(value) == expected


def validate(value: Any, check_files: bool = True) -> list[str]:
    errors: list[str] = []
    if not isinstance(value, dict):
        return ["root_not_object"]
    if value.get("schema") != "wpa_ii_19_ball_group_confusion_discovery_rerack_design_gap_gate_v1":
        errors.append("schema")
    if value.get("status") != "REFEREE_OR_PLAYER_INPUT_AND_POLICY_PENDING_NO_PRODUCTION_MUTATION":
        errors.append("status")
    if value.get("scope") != ["WPA-II-19"]:
        errors.append("scope")
    if value.get("program_priority") != {
            "current_task": "M4_REQUIRED", "frozen_audit_v5_m4_required": False,
            "priority_override_changes_audit_status_or_evidence": False}:
        errors.append("program_priority")

    official = value.get("official_source", {})
    section = official.get("section", {}) if isinstance(official, dict) else {}
    resolved = {
        "the clause applies after the table is closed rather than while groups remain open",
        "wrongly hitting the other group is a foul",
        "the foul-pronouncement deadline is before the next shot",
        "a player or referee may be the party who realizes that groups were wrongly played",
        "a valid group-confusion realization stops the rack immediately",
        "the prescribed rack-level remedy is rerack",
    }
    pending = {
        "whether every ordinary wrong-first-ball foul is also a group-confusion rerack event",
        "whether groups wrongly played means one wrong-group stroke, a mistaken group assignment, or a longer sequence of play under confused assignments",
        "the authoritative evidence or declaration payload by which a player or referee realizes the confusion",
        "whether discovery after the next shot has no remedy, a different remedy, or a referee remedy",
        "whether next shot means cue-strike acceptance, completed stroke, or another boundary",
        "whether the ordinary ball-in-hand foul entitlement is staged, suppressed, or superseded when a timely rerack declaration occurs",
        "who breaks the rerack",
        "whether the original breaker and original shooting order are restored",
        "the numbered rack seed or any non-official implementation completion beyond official rack geometry",
    }
    if not isinstance(official, dict) or official.get("path") != "tmp/pdfs/250816-Rules-of-Heyball.pdf" or \
            official.get("sha256") != PDF_SHA or official.get("visually_reviewed_pdf_pages") != [11] or \
            section.get("chapter") != "II" or section.get("clause") != "19" or section.get("pdf_page") != 11 or \
            section.get("locator") != "Ball Group Confusion" or \
            not exact_list(official.get("uniquely_determined_rules"), resolved) or \
            not exact_list(official.get("text_does_not_uniquely_determine"), pending) or \
            official.get("automatic_wrong_first_to_rerack_mapping_authorized") is not False:
        errors.append("official_binding")
    elif check_files:
        path = resolve(official["path"])
        if path is None or not path.is_file() or sha256(path) != PDF_SHA:
            errors.append("official_pdf_drift")

    audit = value.get("frozen_audit", {})
    expected_audit = {
        "path": "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v5.json",
        "sha256": AUDIT_SHA,
        "row": {"id": "WPA-II-19", "status": "MISSING", "m4_required": False,
                "policy_decision_required": True,
                "assessment": "No wrong-group discovery window, stop-rack action or corrective rerack transaction exists."},
        "promotion_authorized": False,
    }
    if audit != expected_audit:
        errors.append("audit_binding")
    elif check_files:
        path = resolve(audit["path"])
        if path is None or not path.is_file() or sha256(path) != AUDIT_SHA:
            errors.append("audit_drift")

    bindings = value.get("source_bindings", {})
    actual = {entry.get("path"): entry.get("sha256") for entry in bindings.values()
              if isinstance(entry, dict)} if isinstance(bindings, dict) else {}
    if actual != SOURCE_SHA or len(bindings) != len(SOURCE_SHA):
        errors.append("source_bindings")
    elif check_files:
        for raw, digest in SOURCE_SHA.items():
            path = resolve(raw)
            if path is None or not path.is_file() or sha256(path) != digest:
                errors.append(f"source_drift:{raw}")

    expected_mapping = {
        "groups_assigned_phase_exists": True, "authoritative_player_group_assignment_exists": True,
        "first_contact_ball_identity_exists": True, "simultaneous_first_contact_set_exists": True,
        "wrong_first_contact_foul_exists": True, "wrong_first_contact_awards_ordinary_ball_in_hand": True,
        "wrong_first_contact_is_section_19_discovery_event": False,
        "group_confusion_discovery_actor_or_input_exists": False,
        "offending_shot_identity_persists_for_discovery_window": False,
        "before_next_shot_window_state_exists": False, "rack_stop_action_exists": False,
        "section_19_parent_authority_exists": False,
        "rack_foundation_can_stage_official_rerack_for_explicit_breaker": True,
        "rack_foundation_requires_caller_to_select_breaker": True,
        "section_19_breaker_or_order_policy_exists": False,
        "applied_shoot_journal_exists": True,
        "applied_shoot_journal_is_section_19_discovery_authority": False,
        "replay_shot_count_exists": True, "replay_shot_count_is_section_19_deadline_authority": False,
        "dedicated_group_confusion_product_seam_exists": False,
        "global_rules_decision_ui_product_exposed": False,
    }
    if value.get("current_system_mapping") != expected_mapping:
        errors.append("current_mapping")

    boundary = value.get("ordinary_foul_boundary", {})
    ordinary = boundary.get("section_17_wrong_first_contact", {}) if isinstance(boundary, dict) else {}
    confusion = boundary.get("section_19_group_confusion", {}) if isinstance(boundary, dict) else {}
    if ordinary.get("current_effect") != "ordinary foul, opponent ball in hand and turn change" or ordinary.get("remains_independent") is not True or \
            confusion.get("fact") is not None or confusion.get("requires_discovery_input") is not True or \
            confusion.get("timely_window_required") is not True or confusion.get("rack_level_effect") != "stop rack and rerack" or \
            confusion.get("may_not_be_inferred_from_wrong_first_contact_alone") is not True or \
            boundary.get("no_optimistic_entitlement_or_rerack_mutation") is not True:
        errors.append("ordinary_foul_boundary")

    blocked = value.get("why_implementation_is_blocked", {})
    if not isinstance(blocked, dict) or set(blocked) != {
            "classification", "input", "deadline", "effect_interaction", "rerack",
            "production_extension_safe_now"} or blocked.get("production_extension_safe_now") is not False or \
            "does not uniquely say" not in blocked.get("classification", "") or \
            "No player/referee realization" not in blocked.get("input", "") or \
            "does not state who breaks" not in blocked.get("rerack", ""):
        errors.append("implementation_block")

    policy = value.get("pending_policy_and_input_contract", {})
    classification = policy.get("classification", {}) if isinstance(policy, dict) else {}
    actor = policy.get("actor", {}) if isinstance(policy, dict) else {}
    deadline = policy.get("deadline", {}) if isinstance(policy, dict) else {}
    rerack = policy.get("rerack", {}) if isinstance(policy, dict) else {}
    if policy.get("status") != "REFEREE_OR_PLAYER_INPUT_AND_POLICY_PENDING" or \
            any(classification.get(key) is not None for key in ("ordinary_wrong_first_always_section_19", "mistaken_assignment_or_sequence_required", "authoritative_realization_evidence_schema")) or classification.get("forbidden_guess") is not True or \
            actor.get("allowed_textual_classes") != ["player", "referee"] or any(actor.get(key) is not None for key in ("authenticated_player_request_schema", "authenticated_referee_request_schema", "conflict_resolution_between_claims")) or actor.get("forbidden_guess") is not True or \
            any(deadline.get(key) is not None for key in ("next_shot_machine_boundary", "late_discovery_effect", "clock_or_pause_behavior")) or deadline.get("forbidden_guess") is not True or \
            any(rerack.get(key) is not None for key in ("breaker_player_id", "shooting_order", "ordinary_ball_in_hand_disposition")) or rerack.get("forbidden_guess") is not True:
        errors.append("pending_policy_input")

    evidence = value.get("required_future_evidence", {})
    expected_lengths = {"rack": 5, "offending_shot": 7, "discovery": 8, "window": 4}
    if not isinstance(evidence, dict) or evidence.get("all_fields_in_rules_authority_hash") is not True or \
            any(len(evidence.get(key, [])) != count for key, count in expected_lengths.items()):
        errors.append("future_evidence")

    machine = value.get("future_state_machine", {})
    if not isinstance(machine, dict) or machine.get("states") != [
            "INACTIVE", "TIMELY_DISCOVERY_WINDOW_OPEN", "DISCOVERY_VALIDATING", "RACK_STOPPED",
            "RERACK_COMMITTING", "RERACK_COMMITTED", "WINDOW_EXPIRED"] or \
            "future policy" not in machine.get("window_open", "") or \
            "before stopping play" not in machine.get("discovery_submit", "") or \
            "never close it" not in machine.get("next_shot_close", "") or \
            "policy supplies the breaker/order" not in machine.get("rerack_commit", "") or \
            machine.get("product_exposed") is not False:
        errors.append("future_state_machine")

    atomic = value.get("future_atomic_idempotency_contract", {})
    if not isinstance(atomic, dict) or "before mutation" not in atomic.get("precommit", "") or \
            "same atomic parent transaction" not in atomic.get("stop_and_rerack", "") or \
            "Section 17 foul" not in atomic.get("ordinary_foul_isolation", "") or \
            "byte-equivalent" not in atomic.get("failure_rollback", "") or \
            "without a second stop" not in atomic.get("exact_retry", "") or \
            "zero mutation" not in atomic.get("conflicting_retry", "") or atomic.get("partial_commit_allowed") is not False:
        errors.append("atomic_idempotency_contract")

    fixtures = value.get("required_future_fixtures", {})
    if not isinstance(fixtures, dict) or len(fixtures.get("positive_after_policy", [])) != 5 or \
            len(fixtures.get("negative", [])) != 15 or \
            any(len(items) != len(set(items)) for items in fixtures.values() if isinstance(items, list)):
        errors.append("future_fixtures")

    allowlist = value.get("future_implementation_allowlist", {})
    production = allowlist.get("production_files", []) if isinstance(allowlist, dict) else []
    if not isinstance(allowlist, dict) or \
            allowlist.get("allowlist_is_dormant_until_discovery_input_classification_deadline_and_rerack_policy_are_approved") is not True or \
            allowlist.get("product_session_files_allowed") is not False or \
            allowlist.get("cmake_change_requires_separate_coordination") is not True or \
            "CMakeLists.txt" in production or "src/Billiards/render/modern/modern_product_session.cpp" in production:
        errors.append("future_allowlist")

    forbidden = {
        "production source edits in this design-gap task",
        "mapping every wrong-first-ball foul directly to a Section 19 rerack",
        "guessing whether group confusion means one stroke or mistaken assignment play",
        "inventing a player or referee discovery event from physics facts alone",
        "guessing the next-shot deadline boundary or late-discovery remedy",
        "guessing the rerack breaker or shooting order",
        "changing Section 17 wrong-first-contact foul semantics",
        "exposing a group-confusion product seam or global Rules UI",
        "promoting WPA-II-19 above MISSING", "CMake changes",
        "Phase4 or Phase5 renderer changes", "GPU, Cocoa, or OpenGL execution",
        "readiness generation or authorization issuance",
    }
    if not exact_list(value.get("forbidden_scope"), forbidden):
        errors.append("forbidden_scope")

    expected_claims = {
        "design_gap_gate_complete": True, "official_page_visually_reviewed": True,
        "group_assignment_contact_journal_and_rack_paths_mapped": True,
        "ordinary_wrong_first_contact_implemented": True,
        "section_19_discovery_input_available": False, "section_19_classification_resolved": False,
        "next_shot_deadline_policy_resolved": False,
        "rerack_breaker_and_order_policy_resolved": False,
        "section_19_internal_authority_implemented": False,
        "dedicated_product_seam_available": False, "production_implementation_complete": False,
        "audit_row_promoted": False, "global_ui_exposed": False, "cmake_modified": False,
        "gpu_executed": False, "readiness_or_authorization_created": False,
    }
    if value.get("claims") != expected_claims:
        errors.append("claims")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("gate", nargs="?", default=str(DEFAULT_GATE))
    parser.add_argument("--no-file-checks", action="store_true")
    args = parser.parse_args()
    try:
        value = json.loads(Path(args.gate).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        print(f"INVALID: {exc}")
        return 1
    errors = validate(value, check_files=not args.no_file_checks)
    if errors:
        print("INVALID: " + ", ".join(errors))
        return 1
    print("PASS_WPA_II_19_BALL_GROUP_CONFUSION_DISCOVERY_RERACK_DESIGN_GAP_GATE")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
