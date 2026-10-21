#!/usr/bin/env python3
"""Validate the fail-closed WPA II 21 stalemate design gap gate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GATE = ROOT / "docs/rules/wpa-ii-21-stalemate-state-machine-design-gap-gate-v1.json"
PDF_SHA = "2b92dd5088ea630337326e26b7a2a7a4499a9801f2c5da7e5cc40421d5424a24"
AUDIT_SHA = "2811958f1205d9998b40060b83c577f91359d2164e1ac0eaa9ccc4cac2ad787a"
SOURCE_SHA = {
    "src/Billiards/rules/chinese_eight_ball_rules.h": "7408e24a58c827e3de27b766cd91bdb5ce5831919cea97f733f8191baae59f7f",
    "src/Billiards/rules/chinese_eight_ball_rules.cpp": "a591b919e99580a8e859e98ebfda9de1a0f2ec89e521d47f67ef180c6145f930",
    "src/Billiards/game_runtime.h": "61453e15d88d5581fa439fa77eb6b834fc0a22a5366b470b182b17b8d4d9f725",
    "src/Billiards/game_runtime.cpp": "fca6d4f8ffc54b5d68f3aa24928f71416b003ac8ae37ef7ab4d376aa78684ca4",
    "src/Billiards/rules/rack_executor.h": "d42dc8ae486ad9df8911e4f82cb3d657c0e51e76b35082edb3fd909ee37ef242",
    "src/Billiards/rules/rack_executor.cpp": "d1805c8e197f6b1a8f712f80ba8be807217e875d56def3c7d00186293cd9fb17",
    "src/Billiards/replay/replay_recorder.h": "90ac051be4dda3294433e545fc1326152711cc44733c0207c2bcd924a6b38c7a",
    "src/Billiards/replay/replay_recorder.cpp": "0864c2d7af54230eb1adc7851466ffd6201e600d21574fc6de450cbcfd1569ed",
    "src/Billiards/replay/replay_v1.h": "85ba7442355d52b7a329b6008aa0b5d8c6118fc9cedde2cbbbd6f5843ade47c5",
    "src/Billiards/replay/replay_v1.cpp": "4bbac73fa19881f158d8c9c184e8eac0463e35fb225218534fe531ed0c1a8b51",
    "src/Billiards/game_state.h": "6363b8c2481df6702bb22c4b00b44a546eb594241a7744684c2c94c96adaed4d",
    "src/Billiards/game_state.cpp": "a91185ba5fad8c8571aa1b0efd36de967d0fc3c307fef01b2903c2f910de88c3",
    "src/Billiards/rules/rules_presentation.cpp": "383a4d4488f406b0e7cac336aef4ac50ae3206527e17f79f0e69df82f3100044",
    "src/Billiards/render/modern/modern_product_session.h": "bc7862aac74640bd9fb2aa5929eddc3601b89d5e17b5cd047c8fd8e5578f003d",
    "src/Billiards/render/modern/modern_product_session.cpp": "d7a3d4e88e3ec8723d78b7ed37808463fbfe9f0e005e71b409c82d4a3e7cb5e9",
    "tests/chinese_eight_ball_rules_tests.cpp": "950a98501feca064cbaab8c4b1025b3b421dfcbc8b59a8f82739042069d7997c",
    "tests/rules_runtime_integration_tests.cpp": "f4f67a8f2b34b7c35ce8c555d12d7a4b65922c4f5710d21c8efe66408476152c",
    "tests/replay_recorder_tests.cpp": "40e47f08df57c8437482d05050589e16433ed93e6cea96b4543e805283578b02",
    "tests/rules_decision_session_integration_tests.cpp": "318561223599fae85fa628d03b104f01cd7dedeb169fd6975deac6aac12f3379",
    "tests/modern_product_ui_runtime_tests.cpp": "9054f03ec9d41341fb5afb7a5d0b42af33a675b98eb91dc279b28d2173b8107b",
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
    if value.get("schema") != "wpa_ii_21_stalemate_state_machine_design_gap_gate_v1":
        errors.append("schema")
    if value.get("status") != "REFEREE_INPUT_AND_POLICY_PENDING_NO_PRODUCTION_MUTATION":
        errors.append("status")
    if value.get("scope") != ["WPA-II-21"]:
        errors.append("scope")

    official = value.get("official_source", {})
    section = official.get("section", {}) if isinstance(official, dict) else {}
    resolved = {
        "the initial proposal is triggered by referee judgment, not by an automatic shot predicate",
        "the proposal is to rerack and break",
        "a refusal by any player causes play to continue for a referee-controlled period",
        "after that continuation, a remaining stalemate requires a referee declaration before rerack",
        "the final rerack assigns the break to the original breaking player",
        "the original shooting order remains unchanged",
    }
    pending = {
        "a machine predicate for stalemate occurred or likely to occur",
        "the signed referee identity and transport in each product mode",
        "whether acceptance requires explicit responses from both players or an authority timeout",
        "the exact continuation duration",
        "whether generally three more shots each is a default, recommendation, maximum, or replaceable allocation",
        "the complete set of another judgment-based decisions",
        "the ordering and accounting of the generally three shots when turns continue after legal pockets",
        "which events prove that the stalemate was broken",
        "the exact time at which the referee reassesses and issues the final declaration",
    }
    if not isinstance(official, dict) or official.get("path") != "tmp/pdfs/250816-Rules-of-Heyball.pdf" or \
            official.get("sha256") != PDF_SHA or official.get("visually_reviewed_pdf_pages") != [12] or \
            section.get("chapter") != "II" or section.get("clause") != "21" or section.get("pdf_page") != 12 or \
            section.get("locator") != "Stalemate" or not exact_list(official.get("uniquely_determined_rules"), resolved) or \
            not exact_list(official.get("text_does_not_uniquely_determine"), pending) or \
            official.get("fixed_three_each_machine_policy_authorized") is not False:
        errors.append("official_binding")
    elif check_files:
        path = resolve(official["path"])
        if path is None or not path.is_file() or sha256(path) != PDF_SHA:
            errors.append("official_pdf_drift")

    audit = value.get("frozen_audit", {})
    expected_audit = {
        "path": "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v5.json",
        "sha256": AUDIT_SHA,
        "row": {"id": "WPA-II-21", "status": "MISSING", "m4_required": True,
                "assessment": "No stalemate proposal/refusal state, three-shot counter or original-breaker rerack workflow exists."},
        "promotion_authorized": False,
    }
    if audit != expected_audit:
        errors.append("audit_binding")
    elif check_files:
        path = resolve(audit["path"])
        if path is None or not path.is_file() or sha256(path) != AUDIT_SHA:
            errors.append("audit_drift")

    lineage = value.get("existing_pending_ui_lineage", {})
    if not isinstance(lineage, dict) or lineage.get("actions_manifest") != {
            "path": "docs/art/phase5-rules-decision-ui-actions-v1.pending.json",
            "sha256": "7b3de569f57d1a8e589a6d053bf3959f04a5b68bbfb204046bfe8b63428383f1"} or \
            lineage.get("design_gate") != {
                "path": "docs/art/phase5-rules-decision-ui-design-gate-v1.json",
                "sha256": "26567d0547163561ed77e6e41b32753c4cc55efcd693c9ce22faf847d43f5a08"} or \
            lineage.get("capability") != "stalemate_decision" or lineage.get("product_exposed") is not False or \
            lineage.get("unresolved_policy_ids") != ["stalemate_declaration_authority_policy", "stalemate_continuation_window_policy"] or \
            lineage.get("placeholder_is_not_authority_or_implementation") is not True:
        errors.append("pending_ui_lineage")
    elif check_files:
        for entry in (lineage["actions_manifest"], lineage["design_gate"]):
            path = resolve(entry["path"])
            if path is None or not path.is_file() or sha256(path) != entry["sha256"]:
                errors.append("pending_ui_lineage_drift")

    bindings = value.get("source_bindings", {})
    actual = {item.get("path"): item.get("sha256") for item in bindings.values()
              if isinstance(item, dict)} if isinstance(bindings, dict) else {}
    if actual != SOURCE_SHA or len(bindings) != len(SOURCE_SHA):
        errors.append("source_bindings")
    elif check_files:
        for raw, digest in SOURCE_SHA.items():
            path = resolve(raw)
            if path is None or not path.is_file() or sha256(path) != digest:
                errors.append(f"source_drift:{raw}")

    expected_mapping = {
        "match_state_has_stalemate_phase": False, "match_state_has_rack_identity": False,
        "match_state_persists_original_breaker": False, "match_state_persists_original_shooting_order": False,
        "referee_proposal_input_exists": False, "referee_final_declaration_input_exists": False,
        "player_refusal_authority_exists": False, "continuation_window_policy_exists": False,
        "per_player_stalemate_shot_counters_exist": False, "stalemate_broken_predicate_exists": False,
        "rack_foundation_can_stage_official_rerack_for_explicit_breaker": True,
        "rack_foundation_commit_is_revision_hash_and_idempotency_bound": True,
        "rack_foundation_by_itself_proves_section_21_parent_authority": False,
        "applied_shoot_journal_exists": True,
        "applied_shoot_journal_is_authoritative_per_rack_per_player_stalemate_counter": False,
        "replay_global_shot_count_exists": True, "replay_global_shot_count_is_rules_authority": False,
        "pending_ui_placeholder_exists": True, "dedicated_stalemate_product_seam_exists": False,
        "global_rules_decision_ui_product_exposed": False,
    }
    if value.get("current_system_mapping") != expected_mapping:
        errors.append("current_mapping")

    blocked = value.get("why_implementation_is_blocked", {})
    if not isinstance(blocked, dict) or set(blocked) != {
            "referee_input", "continuation_policy", "state_identity", "counter_boundary",
            "rerack_boundary", "production_extension_safe_now"} or \
            blocked.get("production_extension_safe_now") is not False or \
            "generally three shots each" not in blocked.get("continuation_policy", "") or \
            "not revision-bound" not in blocked.get("counter_boundary", ""):
        errors.append("implementation_block")

    policy = value.get("pending_policy_contract", {})
    referee = policy.get("referee_authority", {}) if isinstance(policy, dict) else {}
    response = policy.get("player_response", {}) if isinstance(policy, dict) else {}
    window = policy.get("continuation_window", {}) if isinstance(policy, dict) else {}
    if policy.get("status") != "REFEREE_INPUT_AND_POLICY_PENDING" or \
            any(referee.get(k) is not None for k in ("initial_proposal_issuer", "final_declaration_issuer", "authentication_and_replay_protection")) or referee.get("forbidden_guess") is not True or \
            response.get("any_player_may_refuse") is not True or response.get("acceptance_quorum_or_timeout") is not None or response.get("response_order") is not None or response.get("forbidden_guess") is not True or \
            any(window.get(k) is not None for k in ("fixed_three_shots_each", "default_allocation", "alternative_referee_directive_schema", "stalemate_broken_predicate", "final_reassessment_boundary")) or window.get("forbidden_guess") is not True:
        errors.append("pending_policy_contract")

    state = value.get("required_future_state", {})
    expected_lengths = {"rack_identity": 3, "original_order": 3, "proposal": 6,
                        "responses": 4, "continuation": 6, "final_declaration": 5}
    if not isinstance(state, dict) or state.get("all_fields_in_rules_authority_hash") is not True or \
            any(len(state.get(key, [])) != count for key, count in expected_lengths.items()):
        errors.append("future_state")

    counting = value.get("shot_counting_boundary", {})
    if not isinstance(counting, dict) or len(counting.get("count_only", [])) != 2 or \
            len(counting.get("never_count", [])) != 6 or \
            counting.get("turn_continuation_after_legal_pocket_requires_policy") is not True or \
            counting.get("hardcoded_three_each_forbidden") is not True:
        errors.append("shot_counting_boundary")

    machine = value.get("future_state_machine", {})
    if not isinstance(machine, dict) or machine.get("states") != [
            "INACTIVE", "REFEREE_PROPOSAL_OPEN", "AWAITING_PLAYER_RESPONSES",
            "CONTINUATION_DIRECTIVE_ACTIVE", "AWAITING_REFEREE_REASSESSMENT",
            "FINAL_DECLARATION_COMMITTING", "RERACK_COMMITTED"] or \
            "signed referee" not in machine.get("proposal_transition", "") or \
            "right to refuse" not in machine.get("accept_transition", "") or \
            "Any valid player refusal" not in machine.get("refusal_transition", "") or \
            "never itself declares" not in machine.get("progress_transition", "") or \
            "fresh signed referee declaration" not in machine.get("final_transition", "") or \
            machine.get("product_exposed") is not False:
        errors.append("future_state_machine")

    reset = value.get("reset_contract", {})
    if not isinstance(reset, dict) or len(reset.get("clear_all_stalemate_state_on", [])) != 6 or \
            reset.get("turn_change_alone_resets_window") is not False or \
            reset.get("legal_pocket_alone_resets_window") is not False or \
            reset.get("failed_or_rejected_request_resets_or_mutates") is not False:
        errors.append("reset_contract")

    atomic = value.get("future_atomic_idempotency_contract", {})
    if not isinstance(atomic, dict) or "before mutation" not in atomic.get("precommit", "") or \
            "before the first parent mutation" not in atomic.get("rerack_staging", "") or \
            "immutable rack-start snapshot" not in atomic.get("original_breaker_and_order", "") or \
            "byte-equivalent" not in atomic.get("failure_rollback", "") or \
            "without a second proposal" not in atomic.get("exact_retry", "") or \
            "zero mutation" not in atomic.get("conflicting_retry", "") or \
            atomic.get("partial_commit_allowed") is not False:
        errors.append("atomic_idempotency_contract")

    fixtures = value.get("required_future_fixtures", {})
    if not isinstance(fixtures, dict) or len(fixtures.get("positive_after_policy", [])) != 7 or \
            len(fixtures.get("negative", [])) != 15 or \
            any(len(items) != len(set(items)) for items in fixtures.values() if isinstance(items, list)):
        errors.append("future_fixtures")

    allowlist = value.get("future_implementation_allowlist", {})
    production = allowlist.get("production_files", []) if isinstance(allowlist, dict) else []
    if not isinstance(allowlist, dict) or \
            allowlist.get("allowlist_is_dormant_until_referee_input_and_continuation_policy_are_approved") is not True or \
            allowlist.get("product_session_files_allowed") is not False or \
            allowlist.get("cmake_change_requires_separate_coordination") is not True or \
            "CMakeLists.txt" in production or "src/Billiards/render/modern/modern_product_session.cpp" in production:
        errors.append("future_allowlist")

    forbidden = {
        "production source edits in this design-gap task",
        "hardcoding generally three shots each as mandatory policy",
        "automatically detecting or finally declaring stalemate without referee input",
        "using global replay shotCount as a rules-authoritative per-player continuation counter",
        "inferring original breaker or order from currentPlayer at proposal time",
        "calling the rack foundation directly as proof of a Section 21 parent transaction",
        "exposing the pending stalemate UI or product seam",
        "promoting WPA-II-21 above MISSING", "enabling global rulesDecisionUiProductExposed",
        "CMake changes", "Phase4 or Phase5 renderer changes", "GPU, Cocoa, or OpenGL execution",
        "readiness generation or authorization issuance",
    }
    if not exact_list(value.get("forbidden_scope"), forbidden):
        errors.append("forbidden_scope")

    expected_claims = {
        "design_gap_gate_complete": True, "official_page_visually_reviewed": True,
        "match_rack_replay_and_counter_paths_mapped": True, "referee_input_available": False,
        "continuation_policy_resolved": False, "fixed_three_each_authorized": False,
        "stalemate_authority_implemented": False, "dedicated_product_seam_available": False,
        "production_implementation_complete": False, "audit_row_promoted": False,
        "global_ui_exposed": False, "cmake_modified": False, "gpu_executed": False,
        "readiness_or_authorization_created": False,
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
    print("PASS_WPA_II_21_STALEMATE_STATE_MACHINE_DESIGN_GAP_GATE")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
