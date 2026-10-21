#!/usr/bin/env python3
"""Validate the fail-closed WPA II 6(e) opponent-choice design-gap gate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GATE = ROOT / "docs/rules/wpa-ii-06-e-break-foul-opponent-choice-design-gap-gate-v1.json"
PDF_SHA = "2b92dd5088ea630337326e26b7a2a7a4499a9801f2c5da7e5cc40421d5424a24"
AUDIT_SHA = "d7ab2104fac9fd85fe8d7336ae658cc9baaaa6b4fa866cd4e7815c51404ad033"

EXPECTED_SOURCES: dict[str, dict[str, Any]] = {
    "rule_core": {"path": "src/Billiards/rules/chinese_eight_ball_rules.cpp", "sha256": "5a298f2bd492dfd6f57b8bb478cabd1a3041716e4411c93642e1aeb85b8727b3", "relevant_lines": [[175, 226]]},
    "rule_model": {"path": "src/Billiards/rules/chinese_eight_ball_rules.h", "sha256": "5ab7a045dbae177697a72aed97326d56ee14ae31c0434191066f011a8f52145b"},
    "core_h_parent": {"path": "src/Billiards/rules/break_foul_authority.h", "sha256": "5b4bf09f9950fd61d3d4add4fd70a9304d5c82ab05254f5878c208a6dd92ad9e"},
    "core_g_child": {"path": "src/Billiards/rules/ball_in_hand_authority.h", "sha256": "026fd6196263b1c6dab180fa7f94d29baf277ff56bdff450ca189e7d8ba795d6"},
    "core_e_foundation": {"path": "src/Billiards/rules/rack_executor.cpp", "sha256": "d1805c8e197f6b1a8f712f80ba8be807217e875d56def3c7d00186293cd9fb17"},
    "core_e_contract": {"path": "src/Billiards/rules/rack_executor.h", "sha256": "d42dc8ae486ad9df8911e4f82cb3d657c0e51e76b35082edb3fd909ee37ef242"},
    "core_f_sibling": {"path": "src/Billiards/rules/eight_on_break_authority.h", "sha256": "dacb83140b687b285f1dc67bcbb38d571349a60caa01a45664f0bbdcc2981b2e"},
    "runtime": {"path": "src/Billiards/game_runtime.cpp", "sha256": "3292df4f4546d9fd1639de8d5db33b7dab526f13b129f496d1420a489253fb9a", "relevant_lines": [[1012, 1134], [1622, 1760]]},
    "runtime_header": {"path": "src/Billiards/game_runtime.h", "sha256": "deacceaa4ffff8b0e5080963d958f5fc49a77a197c744c2693be6f2d82b96ec3"},
    "internal_ui": {"path": "src/Billiards/rules/rules_decision_ui_model.cpp", "sha256": "5d4942db12f43069069d68e00e8aecf7f38106bac4f1330f9d41f2c51e830cb8", "relevant_lines": [[149, 190], [544, 562]]},
    "product_session_header": {"path": "src/Billiards/render/modern/modern_product_session.h", "sha256": "f9b5ecef2223943bc8cb278b081b8f7f7fb886e7b5a0cc4db4c1fe2bac8528c4"},
    "product_session": {"path": "src/Billiards/render/modern/modern_product_session.cpp", "sha256": "8e962dfc76149a4461e011941231e5e72860aed9f8cb6f006169e269455cd174", "relevant_lines": [[171, 263], [315, 386], [691, 718]]},
    "core_tests": {"path": "tests/chinese_eight_ball_rules_tests.cpp", "sha256": "e587bb2bbc20f7dc682e0ea07e4e30df4484d3ae2ac9622a3d029c286c997a6c"},
    "runtime_tests": {"path": "tests/rules_runtime_integration_tests.cpp", "sha256": "2abb5a981251e05b179c03fb82adc2bd3da1076f291a4c8e0a174743baef8a55", "relevant_lines": [[1030, 1280], [2040, 2196]]},
    "ui_tests": {"path": "tests/rules_decision_ui_tests.cpp", "sha256": "4bde82aa9bb233ca542c6d4c4ebc440b341c29478aab473ec60f95b3b9208834"},
    "product_tests": {"path": "tests/rules_decision_session_integration_tests.cpp", "sha256": "6082e1d44054a1adc8b3743c2eb31698472a3842626bf25da17db82dadd3220d"},
}

EXPECTED_PARENT_ACTIONS = [
    "rules.break_foul.ball_in_hand_behind_head_string",
    "rules.break_foul.rerack_opponent_breaks",
    "rules.break_foul.rerack_original_breaker",
]

EXPECTED_POSITIVE = [
    "CLASSIFIED_SCRATCH_STAGES_EXACT_6E_PARENT", "CLASSIFIED_CUE_OFF_TABLE_STAGES_EXACT_6E_PARENT",
    "PARENT_ACTOR_IS_OPPONENT_OF_ORIGINAL_BREAKER", "BIH_PARENT_COMMIT_CREATES_ONE_BEHIND_HEAD_STRING_CHILD",
    "BIH_PARENT_DOES_NOT_PHYSICALLY_PLACE_CUE_BALL", "BEHIND_HEAD_CHILD_EXACT_COMMIT_PLACES_CUE_BALL_ONCE",
    "RERACK_SELF_BREAK_SETS_DECISION_ACTOR_AS_BREAKER", "RERACK_OPPONENT_BREAK_SETS_ORIGINAL_BREAKER",
    "EACH_PARENT_SUCCESS_COMMITS_ONE_REVISION_EVENT_ACTION_RECEIPT", "EXACT_PARENT_RETRY_RETURNS_ORIGINAL_ACK",
]

EXPECTED_NEGATIVE = [
    "BIH_NOT_GRANTED_BEFORE_PARENT_COMMIT", "PLACEMENT_CHILD_NOT_STAGED_BEFORE_PARENT_COMMIT",
    "CHILD_PREVIEW_NEVER_MUTATES", "REJECTED_CHILD_CONFIRM_NEVER_MUTATES",
    "ACCEPT_TABLE_ACTION_REJECTED_ON_6E_BRANCH", "ILLEGAL_BREAK_6C_OFFER_REJECTED_BY_6E_PRODUCT_SEAM",
    "EIGHT_ON_BREAK_6F_OFFER_REJECTED_BY_6E_PRODUCT_SEAM", "NO_RACK_CONTACT_6G_NEVER_RECEIVES_BIH_ACTION",
    "RAW_PRODUCT_FACTS_CANNOT_CREATE_OR_RECLASSIFY_PARENT", "OUTSIDE_MATCH_REJECTED_WITHOUT_MUTATION",
    "WRONG_LOCAL_ACTOR_REJECTED_WITHOUT_MUTATION", "WRONG_BRANCH_SOURCE_OR_PAGE_REJECTED_WITHOUT_MUTATION",
    "WRONG_ACTION_VECTOR_REJECTED_WITHOUT_MUTATION", "WRONG_RULESET_CAPABILITY_OR_FLOW_REJECTED_WITHOUT_MUTATION",
    "STALE_DECISION_OR_REVISION_REJECTED_WITHOUT_MUTATION", "AUTHORITY_HASH_DRIFT_REJECTED_WITHOUT_MUTATION",
    "INVALID_OR_REUSED_CONFLICTING_KEY_REJECTED_WITHOUT_MUTATION", "INVALID_PAYLOAD_REJECTED_WITHOUT_MUTATION",
    "MOVING_BALLS_OR_ACTIVE_COLLECTOR_REJECTED_WITHOUT_MUTATION", "RERACK_PLAN_FAILURE_ROLLS_BACK_ALL_PREACK_STATE",
    "EXACT_RETRY_NEVER_REGRANTS_OR_REBUILDS", "POST_ACK_REFRESH_NEVER_REVERSES_COMMITTED_AUTHORITY",
    "FOUNDATION_ACTION_IDS_REMAIN_PRODUCT_UNREACHABLE", "GLOBAL_RULES_UI_REMAINS_UNEXPOSED",
]

EXPECTED_FORBIDDEN = [
    "production source edits in this design-gap task", "CMake changes", "product-side break-foul classification",
    "early ball-in-hand grant before parent commit", "physical cue placement in the parent transaction",
    "global rulesDecisionUiProductExposed enablement", "direct product exposure of Core E foundation actions",
    "changes to Core F sibling semantics", "Phase4 changes", "Phase5 presentation or exact-shape changes",
    "visual or evidence threshold changes", "GPU, Cocoa, or OpenGL execution", "readiness generation",
    "authorization issuance", "promotion of WPA-II-06-E above PARTIAL",
]


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def workspace_path(path_text: object) -> Path | None:
    if not isinstance(path_text, str) or not path_text or Path(path_text).is_absolute():
        return None
    candidate = (ROOT / path_text).resolve()
    try:
        candidate.relative_to(ROOT.resolve())
    except ValueError:
        return None
    return candidate


def validate(value: object, *, check_files: bool = True) -> list[str]:
    errors: list[str] = []
    if not isinstance(value, dict):
        return ["gate root must be an object"]
    if value.get("schema") != "wpa_ii_06_e_break_foul_opponent_choice_design_gap_gate_v1" or \
            value.get("status") != "UPSTREAM_CLASSIFICATION_PENDING_PRODUCT_GATE_CLOSED_NO_PRODUCTION_MUTATION" or \
            value.get("scope") != ["WPA-II-06-E"]:
        errors.append("gate identity/status/scope differs")

    official = value.get("official_source")
    if not isinstance(official, dict) or official.get("path") != "tmp/pdfs/250816-Rules-of-Heyball.pdf" or \
            official.get("sha256") != PDF_SHA:
        errors.append("official PDF binding differs")
    else:
        primary = official.get("primary_clause")
        if primary != {
            "chapter": "II", "clause": "6(e)", "pdf_page": 7,
            "locator": "Break-shot foul opponent choices",
            "controlled_paraphrase": "After a break-shot foul, the opposing player chooses ball in hand behind the Head String, a rerack followed by that opposing player breaking, or a rerack followed by the original breaker breaking.",
        }:
            errors.append("official primary clause differs")
        if official.get("dependent_clauses") != [
            {"clause": "6(f)", "pdf_page": 8, "role": "8-ball-on-break is a distinct spot-or-rerack parent, including its foul branch"},
            {"clause": "6(g)", "pdf_page": 8, "role": "unintentional no-rack-contact miscue allows only the two rerack choices and explicitly no ball in hand"},
            {"clause": "8", "pdf_page": 8, "role": "defines execution of the BehindHeadString ball-in-hand entitlement after the parent choice"},
        ]:
            errors.append("official dependent-clause routing differs")
        if official.get("resolved_choice_semantics") != {
            "decision_actor": "the player opposing the original breaker",
            "ball_in_hand_choice": "grant that actor a BehindHeadString placement entitlement",
            "rerack_self_break_choice": "rerack and set that actor as breaker",
            "rerack_opponent_break_choice": "rerack and set the original breaker as breaker",
            "choice_count": 3,
            "actor_policy_ambiguous": False,
            "breaker_assignment_policy_ambiguous": False,
        }:
            errors.append("official actor/action semantics differ")
        if not isinstance(official.get("unresolved_classification_boundary"), str) or \
                "does not itself enumerate every fact" not in official["unresolved_classification_boundary"]:
            errors.append("official classification boundary missing")
        pdf = workspace_path(official.get("path"))
        if check_files and (pdf is None or not pdf.is_file() or sha256(pdf) != PDF_SHA):
            errors.append("official PDF bytes/path changed")

    audit = value.get("frozen_audit")
    expected_audit = {
        "path": "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v3.json", "sha256": AUDIT_SHA,
        "row_id": "WPA-II-06-E", "row_status": "PARTIAL", "product_action_required": True,
        "product_reachable": False,
        "assessment": "Core H offers the three official opponent choices and Core G handles head-string placement, but product actions remain gated off.",
    }
    if audit != expected_audit:
        errors.append("frozen audit-v3 binding differs")
    elif check_files:
        audit_path = workspace_path(audit["path"])
        if audit_path is None or not audit_path.is_file() or sha256(audit_path) != AUDIT_SHA:
            errors.append("frozen audit-v3 bytes/path changed")
        else:
            parsed = json.loads(audit_path.read_text(encoding="utf-8"))
            rows = [row for row in parsed.get("matrix", []) if row.get("id") == "WPA-II-06-E"]
            if len(rows) != 1 or rows[0].get("status") != "PARTIAL" or rows[0].get("product_reachable") is not False:
                errors.append("frozen WPA-II-06-E row differs")

    if value.get("source_bindings") != EXPECTED_SOURCES:
        errors.append("source bindings differ")
    elif check_files:
        for name, binding in EXPECTED_SOURCES.items():
            path = workspace_path(binding["path"])
            if path is None or not path.is_file() or sha256(path) != binding["sha256"]:
                errors.append(f"source binding bytes/path changed: {name}")

    parent = value.get("as_built_parent_offer")
    expected_parent = {
        "capability": "break_foul_opponent_choices", "flow": "break_foul_opponent_choice",
        "branch": "BreakShotFoul", "source_clause": "WPA-II-06-E", "source_pdf_page": 7,
        "actor_player_id": "1 - original_breaker_player_id", "frozen_next_player": "the opposing actor",
        "offered_actions": EXPECTED_PARENT_ACTIONS, "accept_table_action_offered": False,
        "ball_in_hand_before_parent_commit": False, "placement_child_before_parent_commit": False,
        "pending_parent_blocks_shot_cue_adjudication_and_foundation_bypass": True,
    }
    if parent != expected_parent:
        errors.append("as-built parent offer differs")

    actions = value.get("exact_action_semantics")
    expected_actions = {
        EXPECTED_PARENT_ACTIONS[0]: {
            "parent_effect": "grant the opposing actor a BehindHeadString entitlement and atomically stage one Core G child offer",
            "physical_cue_ball_placement_occurs_in_parent": False, "child_actor": "same opposing actor",
            "child_flow": "ball_in_hand_behind_head_string", "child_action": "rules.ball_in_hand.confirm_behind_head_string",
            "child_source_clause": "WPA-II-08", "child_source_pdf_page": 8, "parent_revision_increment": 1,
            "child_requires_separate_exact_authority_commit": True,
        },
        EXPECTED_PARENT_ACTIONS[1]: {
            "name_reference": "opponent of the original breaker", "selected_breaker": "the opposing decision actor",
            "foundation": "one verified Core E official rerack", "ball_in_hand": False, "child_decision_created": False,
        },
        EXPECTED_PARENT_ACTIONS[2]: {
            "selected_breaker": "the frozen original breaker", "foundation": "one verified Core E official rerack",
            "ball_in_hand": False, "child_decision_created": False,
        },
    }
    if actions != expected_actions:
        errors.append("exact action/child/rerack semantics differ")

    routing = value.get("sibling_and_exception_routing")
    expected_routing = {
        "illegal_break_without_foul": "WPA-II-06-C IllegalBreak parent; never use the 6(e) action set",
        "eight_ball_pocketed_on_break": "WPA-II-06-F Core F parent; no simultaneous Core H 6(e) offer",
        "unintentional_no_rack_contact_miscue": "WPA-II-06-G two-rerack-only exception; no ball-in-hand action",
        "current_no_rack_contact_behavior": "fail closed with rules_break_no_rack_contact_classification_pending and no offer",
        "raw_product_input_may_select_route": False, "only_runtime_frozen_exact_offer_may_select_route": True,
    }
    if routing != expected_routing:
        errors.append("sibling/exception routing differs")

    policy = value.get("classification_and_policy")
    if not isinstance(policy, dict) or \
            policy.get("actor_policy") != {"status": "RESOLVED_BY_OFFICIAL_TEXT", "value": "opponent_of_original_breaker"} or \
            policy.get("rerack_breaker_assignment_policy") != {"status": "RESOLVED_BY_OFFICIAL_TEXT", "self_break": "opposing decision actor", "opponent_break": "original breaker"} or \
            policy.get("behind_head_string_child_policy") != {"status": "RESOLVED_BY_OFFICIAL_TEXT_AND_SECTION_8", "entitlement_only_after_parent_commit": True, "physical_placement_only_after_child_commit": True} or \
            policy.get("break_shot_foul_classification_coverage", {}).get("status") != "UPSTREAM_CLASSIFICATION_PENDING" or \
            policy.get("break_shot_foul_classification_coverage", {}).get("product_may_reclassify_raw_facts") is not False or \
            policy.get("break_shot_foul_classification_coverage", {}).get("product_may_consume_runtime_frozen_BreakShotFoul_offer") is not True or \
            policy.get("new_wpa_ii_06_e_policy_decision_required") is not False or \
            policy.get("product_parent_seam_authorized") is not False or policy.get("audit_row_may_be_promoted") is not False:
        errors.append("classification/policy boundary differs")

    integrity = value.get("authority_integrity_contract")
    required_request = ["ruleset_id", "capability_id", "flow_id", "decision_id", "expected_authority_state_revision", "idempotency_key", "actor_player_id", "offered_action_id", "payload_canonical_empty_object", "unchanged_authority_state_hash", "balls_stationary_and_no_active_shot_collector"]
    expected_integrity = {
        "request_requires": required_request, "rerack_plan_built_and_validated_before_authority_mutation": True,
        "successful_parent_commit": "exactly one authority revision, one event, one authority action, and one receipt",
        "exact_retry": "return original ACK with no duplicate effect",
        "conflicting_retry": "fail with rules_decision_idempotency_conflict and no mutation",
        "stale_or_drifted_request": "fail closed and preserve parent offer and all authoritative state",
        "foundation_action_direct_submission_allowed": False,
    }
    if integrity != expected_integrity:
        errors.append("authority integrity contract differs")

    transaction = value.get("transaction_and_rollback_contract")
    expected_transaction = {
        "precommit": "All product/offer/request/hash checks and any Core E rerack plan must complete before the first authoritative mutation.",
        "parent_commit": "GameRuntime is the single authority; it commits either the entitlement-plus-child or one rerack, plus revision/event/action/receipt, as one logical transaction.",
        "child_commit": "BehindHeadString placement is a separate exact transaction; preview and rejected child submissions never mutate the parent receipt, entitlement, position, or revisions.",
        "pre_ack_failure": "Any failure before verified parent ACK leaves runtime, rack, cue ball, match, parent/child offers, shell, AI gate, UI, revisions, events/actions, and receipts byte-for-byte unchanged.",
        "post_ack_refresh_boundary": "A committed authority ACK must never be reversed by product code. UI/AI refresh must be non-authoritative and recover by deterministic resync from the committed receipt/state.",
        "idempotent_parent_retry": "Return the original parent ACK without regranting BIH, recreating the child, rebuilding the rack, or incrementing revision.",
        "partial_authority_commit_allowed": False,
    }
    if transaction != expected_transaction:
        errors.append("transaction/rollback boundary differs")

    product = value.get("as_built_product_reachability")
    if product != {
        "internal_ui_understands_exact_6e_offer": True, "internal_adapter_builds_exact_parent_request": True,
        "rules_decision_ui_implementation_enabled_by_default": False, "rules_decision_ui_product_exposed": False,
        "existing_narrow_product_seam": "only Core F 8-on-break spot actions",
        "wpa_ii_06_e_narrow_product_parent_seam_exists": False, "product_reachable": False,
        "global_rules_ui_enablement_allowed": False,
    }:
        errors.append("product reachability boundary differs")

    future = value.get("future_minimal_product_parent_seam")
    expected_preconditions = [
        "ProductShell is on Match", "a live BreakFoulDecisionOffer exists", "offer branch is BreakShotFoul",
        "offer source is WPA-II-06-E page 7",
        "offer actor equals local product player and equals the opponent of frozen original breaker",
        "offer action vector exactly equals BIH plus the two rerack actions", "requested action is in that exact vector",
        "runtime, not product, has already classified and frozen the offer",
    ]
    if not isinstance(future, dict) or future.get("entry_point") != "submitProductAuthorizedBreakShotFoulChoice(action_id, idempotency_key)" or \
            future.get("preconditions") != expected_preconditions or \
            future.get("request_payload") != "exact offer identity/revision/actor/action/idempotency with canonical {} payload" or \
            future.get("call_only") != "GameRuntime::submitBreakFoulDecision" or \
            future.get("sync_rules_and_ai_only_after_verified_ack") is not True or \
            future.get("pre_ack_rejection_has_zero_product_or_runtime_mutation") is not True or \
            future.get("rules_decision_ui_product_exposed_remains_false") is not True or \
            future.get("foundation_actions_remain_product_unreachable") is not True or future.get("core_f_or_core_c_offer_rejected") is not True:
        errors.append("future narrow product parent seam differs")

    if value.get("required_future_fixtures") != {"positive": EXPECTED_POSITIVE, "negative": EXPECTED_NEGATIVE}:
        errors.append("required fixtures differ")
    allowlist = value.get("future_implementation_allowlist")
    if allowlist != {
        "production_files": ["src/Billiards/render/modern/modern_product_session.h", "src/Billiards/render/modern/modern_product_session.cpp"],
        "test_files": ["tests/rules_decision_session_integration_tests.cpp", "tests/rules_runtime_integration_tests.cpp"],
        "runtime_or_rule_changes_allowed": False, "core_e_or_core_f_changes_allowed": False,
        "allowlist_is_dormant_until_separate_implementation_approval": True, "cmake_change_requires_separate_coordination": True,
    }:
        errors.append("future implementation allowlist differs")
    if value.get("forbidden_scope") != EXPECTED_FORBIDDEN:
        errors.append("forbidden scope differs")
    if value.get("claims") != {
        "design_gap_gate_complete": True, "official_actor_and_three_choices_resolved": True,
        "upstream_break_foul_classification_complete": False, "core_h_parent_mapped": True,
        "core_g_child_mapped": True, "core_e_rerack_mapped": True, "core_f_sibling_separated": True,
        "product_reachable": False, "implementation_complete": False, "product_behavior_changed": False,
        "gpu_executed": False,
    }:
        errors.append("claims exceed design-only evidence")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--gate", type=Path, default=DEFAULT_GATE)
    parser.add_argument("--no-file-checks", action="store_true")
    args = parser.parse_args()
    try:
        value = json.loads(args.gate.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        print(json.dumps({"status": "FAIL", "errors": [str(error)]}, sort_keys=True))
        return 1
    errors = validate(value, check_files=not args.no_file_checks)
    print(json.dumps({
        "status": "PASS_WPA_II_06_E_DESIGN_GAP_GATE" if not errors else "FAIL",
        "errors": errors, "production_edited": False, "gpu_executed": False,
    }, sort_keys=True))
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
