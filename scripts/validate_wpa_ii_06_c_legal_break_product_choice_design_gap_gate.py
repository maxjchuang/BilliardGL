#!/usr/bin/env python3
"""Validate the fail-closed WPA II 6(c) legal-break/product-choice gap gate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GATE = ROOT / "docs/rules/wpa-ii-06-c-legal-break-product-choice-design-gap-gate-v1.json"
PDF_SHA = "2b92dd5088ea630337326e26b7a2a7a4499a9801f2c5da7e5cc40421d5424a24"
AUDIT_SHA = "d7ab2104fac9fd85fe8d7336ae658cc9baaaa6b4fa866cd4e7815c51404ad033"

EXPECTED_SOURCES: dict[str, dict[str, Any]] = {
    "rule_core": {"path": "src/Billiards/rules/chinese_eight_ball_rules.cpp", "sha256": "5a298f2bd492dfd6f57b8bb478cabd1a3041716e4411c93642e1aeb85b8727b3", "relevant_lines": [[175, 226]]},
    "rule_model": {"path": "src/Billiards/rules/chinese_eight_ball_rules.h", "sha256": "5ab7a045dbae177697a72aed97326d56ee14ae31c0434191066f011a8f52145b", "relevant_lines": [[20, 71]]},
    "shot_collector": {"path": "src/Billiards/rules/shot_fact_collector.cpp", "sha256": "94fcb372a4a3c9fe16f1db12723a29de9572d8fe260ea8163fc547012ac11498", "relevant_lines": [[32, 90]]},
    "shot_collector_header": {"path": "src/Billiards/rules/shot_fact_collector.h", "sha256": "938134962a5a9e4c1c0ebc8a27b9213f8e242318dff73771dbbe633d65de42d1"},
    "physics_telemetry": {"path": "src/Billiards/physics_telemetry.h", "sha256": "d1fa08cb8d1c2878d7a662dd267959ffb8b505fb6afcc22351b1056a5a941178", "relevant_lines": [[35, 101], [143, 153]]},
    "physics_producer": {"path": "src/Billiards/physics.cpp", "sha256": "fc497d79650bf2ff9cd8570c320653c8742e4201b8a2428a7892bfcb0124de77", "relevant_lines": [[1115, 1210]]},
    "parent_authority": {"path": "src/Billiards/rules/break_foul_authority.h", "sha256": "5b4bf09f9950fd61d3d4add4fd70a9304d5c82ab05254f5878c208a6dd92ad9e"},
    "runtime": {"path": "src/Billiards/game_runtime.cpp", "sha256": "3292df4f4546d9fd1639de8d5db33b7dab526f13b129f496d1420a489253fb9a", "relevant_lines": [[561, 590], [1012, 1097], [1622, 1760]]},
    "runtime_header": {"path": "src/Billiards/game_runtime.h", "sha256": "deacceaa4ffff8b0e5080963d958f5fc49a77a197c744c2693be6f2d82b96ec3"},
    "ui_model": {"path": "src/Billiards/rules/rules_decision_ui_model.cpp", "sha256": "5d4942db12f43069069d68e00e8aecf7f38106bac4f1330f9d41f2c51e830cb8", "relevant_lines": [[166, 190], [544, 562]]},
    "product_session": {"path": "src/Billiards/render/modern/modern_product_session.cpp", "sha256": "8e962dfc76149a4461e011941231e5e72860aed9f8cb6f006169e269455cd174", "relevant_lines": [[171, 217], [315, 386], [691, 718]]},
    "collector_tests": {"path": "tests/shot_fact_collector_tests.cpp", "sha256": "0c4920d317ac1ce7edf6a866a431f2164ee417b32be999b82834039417fec3f9"},
    "core_tests": {"path": "tests/chinese_eight_ball_rules_tests.cpp", "sha256": "e587bb2bbc20f7dc682e0ea07e4e30df4484d3ae2ac9622a3d029c286c997a6c"},
    "runtime_tests": {"path": "tests/rules_runtime_integration_tests.cpp", "sha256": "2abb5a981251e05b179c03fb82adc2bd3da1076f291a4c8e0a174743baef8a55"},
    "product_tests": {"path": "tests/rules_decision_session_integration_tests.cpp", "sha256": "6082e1d44054a1adc8b3743c2eb31698472a3842626bf25da17db82dadd3220d"},
}

EXPECTED_POSITIVE = [
    "FOUR_DISTINCT_VALID_CUSHION_OBJECTS_LEGAL",
    "FIVE_DISTINCT_VALID_CUSHION_OBJECTS_LEGAL",
    "THREE_CUSHION_OBJECTS_PLUS_ONE_OBJECT_POCKET_LEGAL",
    "ZERO_CUSHION_OBJECTS_PLUS_OBJECT_POCKET_LEGAL",
    "EIGHT_BALL_POCKET_SATISFIES_THRESHOLD_THEN_USES_EIGHT_ON_BREAK_PARENT",
    "THREE_CUSHION_OBJECTS_NO_POCKET_STAGES_EXACT_ILLEGAL_BREAK_OFFER",
    "OPPONENT_ACCEPT_TABLE_ONE_ATOMIC_COMMIT",
    "OPPONENT_RERACKS_AND_BREAKS_ONE_ATOMIC_COMMIT",
    "ORIGINAL_BREAKER_RERACKS_AND_BREAKS_ONE_ATOMIC_COMMIT",
    "EXACT_RETRY_RETURNS_ORIGINAL_ACK",
]

EXPECTED_NEGATIVE = [
    "REPEATED_CONTACT_BY_ONE_OBJECT_COUNTS_ONCE",
    "CUE_BALL_CUSHION_CONTACT_NEVER_COUNTS",
    "BALL_BALL_CONTACT_NEVER_COUNTS_AS_CUSHION",
    "FAILED_PHYSICS_STEP_NEVER_CONTRIBUTES",
    "POCKET_JAW_CONTACT_BLOCKED_WHILE_POLICY_PENDING",
    "CORRECTION_ONLY_RAIL_RECORD_BLOCKED_WHILE_POLICY_PENDING",
    "POCKET_MOUTH_OR_THROAT_WITHOUT_CAPTURE_IS_NOT_POCKETED",
    "CUE_BALL_POCKET_SCOPE_BLOCKED_WHILE_POLICY_PENDING_AND_FOUL_REMAINS_SEPARATE",
    "SYNTHETIC_SCALAR_WITHOUT_PRODUCT_PROVENANCE_CANNOT_OPEN_PRODUCT_CHOICE",
    "INCOMPLETE_BREAK_EVIDENCE_FAILS_CLOSED",
    "WRONG_PRODUCT_SCREEN_REJECTED_WITHOUT_MUTATION",
    "WRONG_LOCAL_ACTOR_REJECTED_WITHOUT_MUTATION",
    "WRONG_BRANCH_OR_SOURCE_REJECTED_WITHOUT_MUTATION",
    "BALL_IN_HAND_ACTION_NOT_OFFERED_ON_ILLEGAL_BREAK",
    "WRONG_ACTION_REJECTED_WITHOUT_MUTATION",
    "STALE_DECISION_OR_REVISION_REJECTED_WITHOUT_MUTATION",
    "AUTHORITY_STATE_HASH_DRIFT_REJECTED_WITHOUT_MUTATION",
    "INVALID_PAYLOAD_REJECTED_WITHOUT_MUTATION",
    "RERACK_PLAN_FAILURE_ROLLS_BACK_ALL_STATE",
    "IDEMPOTENCY_CONFLICT_REJECTED_WITHOUT_MUTATION",
    "ACK_OR_PRODUCT_REFRESH_FAILURE_ROLLS_BACK_ALL_STATE",
    "GLOBAL_RULES_UI_REMAINS_UNEXPOSED",
]

EXPECTED_FORBIDDEN = [
    "production source edits in this design-gap task",
    "CMake changes",
    "guessing pocket-jaw cushion classification",
    "guessing solver-contact validity threshold",
    "global rulesDecisionUiProductExposed enablement",
    "direct product exposure of foundation actions",
    "Phase4 changes",
    "Phase5 presentation or exact-shape changes",
    "visual or evidence threshold changes",
    "GPU, Cocoa, or OpenGL execution",
    "readiness generation",
    "authorization issuance",
    "promotion of WPA-II-06-C above PARTIAL",
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
    if value.get("schema") != "wpa_ii_06_c_legal_break_product_choice_design_gap_gate_v1" or \
            value.get("status") != "POLICY_TELEMETRY_PENDING_PRODUCT_GATE_CLOSED_NO_PRODUCTION_MUTATION" or \
            value.get("scope") != ["WPA-II-06-C"]:
        errors.append("gate identity/status/scope differs")

    official = value.get("official_source")
    if not isinstance(official, dict) or \
            official.get("path") != "tmp/pdfs/250816-Rules-of-Heyball.pdf" or \
            official.get("sha256") != PDF_SHA or \
            official.get("chapter") != "II" or official.get("clause") != "6(c)" or \
            official.get("pdf_page") != 7 or \
            official.get("locator") != "Objective illegal-break threshold and opponent choice" or \
            official.get("controlled_paraphrase") != "A break with fewer than four object balls contacting cushions and no ball pocketed is illegal. The opponent may accept the layout and continue, or require a rerack with either player breaking, without ball in hand behind the Head String.":
        errors.append("official clause binding/paraphrase differs")
    else:
        semantics = official.get("resolved_semantics")
        if semantics != {
            "rail_branch": "at least four distinct object balls contact cushions",
            "pocket_branch_literal": "at least one ball is pocketed",
            "current_core_interpretation": "at least one object ball is pocketed",
            "legal_break_threshold": "rail_branch OR policy-classified pocket_branch",
            "illegal_break_threshold": "fewer than four distinct object-ball cushion contacts AND no policy-classified pocket",
            "opponent_actions": ["accept_table_and_continue", "rerack_opponent_breaks", "rerack_original_breaker"],
            "ball_in_hand_awarded": False,
        }:
            errors.append("official threshold/action semantics differ")
        omissions = official.get("text_does_not_specify")
        if omissions != [
            "whether a pocket-jaw contact is a cushion contact for the four-object-ball threshold",
            "which numerical physics-contact predicate distinguishes a rules contact from a solver candidate or correction-only contact",
            "whether the words one ball being pocketed include the cue ball for the legal-break threshold, independently of the cue-ball foul",
        ]:
            errors.append("official ambiguity boundary differs")
        pdf_path = workspace_path(official.get("path"))
        if check_files and (pdf_path is None or not pdf_path.is_file() or sha256(pdf_path) != PDF_SHA):
            errors.append("official PDF bytes/path changed")

    audit = value.get("frozen_audit")
    expected_audit = {
        "path": "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v3.json",
        "sha256": AUDIT_SHA,
        "row_id": "WPA-II-06-C",
        "row_status": "PARTIAL",
        "product_action_required": True,
        "product_reachable": False,
        "assessment": "Objective legal-break adjudication and opponent accept/rerack authority exist, but the product decision gate remains closed.",
    }
    if audit != expected_audit:
        errors.append("frozen audit-v3 binding differs")
    elif check_files:
        audit_path = workspace_path(audit["path"])
        if audit_path is None or not audit_path.is_file() or sha256(audit_path) != AUDIT_SHA:
            errors.append("frozen audit-v3 bytes/path changed")
        else:
            parsed = json.loads(audit_path.read_text(encoding="utf-8"))
            rows = [row for row in parsed.get("matrix", []) if row.get("id") == "WPA-II-06-C"]
            if len(rows) != 1 or rows[0].get("status") != "PARTIAL" or \
                    rows[0].get("product_action_required") is not True or \
                    rows[0].get("product_reachable") is not False:
                errors.append("frozen WPA-II-06-C audit row differs")

    sources = value.get("source_bindings")
    if sources != EXPECTED_SOURCES:
        errors.append("source bindings differ")
    elif check_files:
        for name, binding in EXPECTED_SOURCES.items():
            path = workspace_path(binding["path"])
            if path is None or not path.is_file() or sha256(path) != binding["sha256"]:
                errors.append(f"source binding bytes/path changed: {name}")

    core = value.get("as_built_rule_core")
    if core != {
        "minimum_distinct_object_balls_to_rail": 4,
        "pocketed_object_ball_makes_legal_break": True,
        "object_ball_ids": "1 through 15, including the 8-ball",
        "three_distinct_rails_without_object_pocket": "IllegalBreak",
        "four_distinct_rails_without_object_pocket": "legal_threshold_satisfied",
        "zero_distinct_rails_with_object_pocket": "legal_threshold_satisfied",
        "illegal_break_is_foul": False,
        "illegal_break_rerack_allowed": True,
        "illegal_break_decision_options": ["accept_table", "rerack_opponent_breaks", "rerack_original_breaker"],
        "direct_shotfacts_injection_is_product_authoritative": False,
    }:
        errors.append("as-built rule-core mapping differs")

    telemetry = value.get("as_built_telemetry")
    expected_telemetry = {
        "product_shot_collection_starts_after_accepted_cue_impact": True,
        "runtime_consumes_only_succeeded_physics_steps": True,
        "collector_deduplicates_object_ball_ids_internally": True,
        "collector_output_preserves_only_distinct_rail_count": True,
        "collector_output_preserves_rail_ball_ids": False,
        "collector_output_preserves_boundary_kind": False,
        "collector_output_preserves_impulse_or_contact_validity": False,
        "all_PhysicsContactKind_Rail_records_currently_count": True,
        "jaw_candidates_can_be_PhysicsContactKind_Rail": True,
        "rail_record_exposes_pocket_boundary_event_and_impulse": True,
        "pocket_capture_uses_authoritative_state_transition_and_capture_sequence": True,
        "pocketed_ball_ids_and_order_are_preserved": True,
        "telemetry_sufficient_for_product_four_cushion_claim": False,
        "pocket_evidence_sufficient_for_product_object_pocket_branch": True,
    }
    if telemetry != expected_telemetry:
        errors.append("as-built telemetry sufficiency mapping differs")

    missing = value.get("missing_authoritative_fields")
    expected_missing = {
        "required_canonical_fact": "break_object_cushion_contacts",
        "entry_fields": ["object_ball_id", "boundary_kind_straight_or_jaw", "velocity_impulse_applied", "normal_impulse_ns", "solver_event_id", "physics_tick"],
        "collection_contract": "canonical sorted unique evidence per object ball, derived only from succeeded authoritative product physics steps",
        "required_origin_field": "break_evidence_origin",
        "allowed_product_origin": "authoritative_shot_fact_collector",
        "required_completeness_field": "break_evidence_complete",
        "synthetic_or_direct_fact_origin_may_remain_for_cpu_tests": True,
        "synthetic_or_direct_fact_origin_may_open_product_choice": False,
        "existing_pocketedBallsInOrder_reusable": True,
    }
    if missing != expected_missing:
        errors.append("missing authoritative field contract differs")

    pending = value.get("pending_classification")
    expected_surface = {
        "status": "POLICY_PENDING",
        "selected": None,
        "allowed_candidates": ["straight_cushion_only", "straight_cushion_and_pocket_jaw"],
        "forbidden_guess": True,
        "reason": "The local official PDF uses cushion without defining whether a pocket jaw counts.",
    }
    expected_validity = {
        "status": "TELEMETRY_POLICY_PENDING",
        "selected": None,
        "allowed_candidates": ["positive_normal_impulse", "velocity_impulse_applied", "documented_solver_contact_predicate"],
        "forbidden_guess": True,
        "reason": "The current collector counts every Rail record, including records that may represent jaw or correction-only candidates.",
    }
    expected_pocket_scope = {
        "status": "POLICY_PENDING",
        "selected": None,
        "allowed_candidates": ["object_ball_only", "any_ball_for_threshold_with_cue_ball_foul_classified_separately"],
        "forbidden_guess": True,
        "reason": "Clause 6(c) changes from object balls for cushion contacts to one ball for the pocket alternative.",
    }
    if not isinstance(pending, dict) or \
            pending.get("cushion_surface_policy") != expected_surface or \
            pending.get("physics_contact_validity_policy") != expected_validity or \
            pending.get("pocketed_ball_scope_policy") != expected_pocket_scope or \
            pending.get("production_telemetry_closure_authorized") is not False or \
            pending.get("product_choice_exposure_authorized") is not False or \
            pending.get("audit_row_may_be_promoted") is not False:
        errors.append("policy/telemetry pending boundary differs")

    authority = value.get("as_built_parent_authority")
    expected_authority = {
        "capability": "break_foul_opponent_choices",
        "flow": "break_foul_opponent_choice",
        "illegal_break_branch": "IllegalBreak",
        "source_clause": "WPA-II-06-C",
        "source_pdf_page": 7,
        "actor": "opponent of the frozen original breaker",
        "offered_actions": ["rules.break_foul.accept_table", "rules.break_foul.rerack_opponent_breaks", "rules.break_foul.rerack_original_breaker"],
        "ball_in_hand_action_offered": False,
        "pending_offer_blocks_shot_and_competing_authority": True,
        "request_requires_exact_ruleset_capability_flow_decision_revision_actor_action_payload": True,
        "request_requires_frozen_authority_state_hash": True,
        "rerack_plan_built_and_validated_before_mutation": True,
        "accepted_action_commits_one_revision_event_action_and_receipt": True,
        "exact_retry_returns_original_ack_without_mutation": True,
        "conflicting_retry_fails_without_mutation": True,
        "accept_effect": "opponent continues on the accepted layout, OpenTable, no ball in hand",
        "rerack_effect": "official rack rebuild with the selected frozen breaker, Break phase, no ball in hand",
    }
    if authority != expected_authority:
        errors.append("as-built parent authority mapping differs")

    product = value.get("as_built_product_reachability")
    if product != {
        "internal_ui_model_understands_exact_illegal_break_offer": True,
        "internal_ui_adapter_can_submit_exact_parent_request": True,
        "rules_decision_ui_implementation_enabled_by_default": False,
        "rules_decision_ui_product_exposed": False,
        "narrow_product_authorized_wpa_ii_06_c_parent_seam_exists": False,
        "product_reachable": False,
        "global_rules_ui_enablement_allowed": False,
    }:
        errors.append("product reachability gate differs")

    future = value.get("future_minimal_parent_product_contract")
    expected_future = {
        "blocked_until_telemetry_and_policy_close": True,
        "entry_point": "submitProductAuthorizedIllegalBreakChoice(action_id, idempotency_key)",
        "preconditions": [
            "ProductShell is on Match",
            "the live offer exists and is IllegalBreak / WPA-II-06-C / page 7",
            "offer actor equals the local product player",
            "offer actions exactly equal accept plus the two rerack actions",
            "action is one of those exact offered actions",
            "offer is backed by complete authoritative product break evidence under the selected cushion/contact policies",
        ],
        "request_bindings": ["ruleset_id", "capability_id", "flow_id", "decision_id", "authority_state_revision", "actor_player_id", "action_id", "idempotency_key", "payload_canonical_empty_object"],
        "runtime_commit_is_single_authority": True,
        "product_refresh_and_ai_gate_only_after_verified_ack": True,
        "any_rejection_preserves_runtime_shell_ai_ui_and_offer_state": True,
        "rules_decision_ui_product_exposed_remains_false": True,
        "foundation_actions_directly_product_reachable": False,
    }
    if future != expected_future:
        errors.append("future minimal parent product contract differs")

    atomic = value.get("atomic_mutation_contract")
    expected_atomic = {
        "validation_phase": "Validate evidence completeness, exact branch/source/actor/action/revision/state hash/idempotency and prepare any rerack plan before authoritative mutation.",
        "accept_commit": "Atomically preserve the layout, set the frozen opponent as current player in OpenTable, clear the offer, increment one revision, and append one event/action/receipt; no ball in hand.",
        "rerack_commit": "Atomically apply one verified official rerack with the selected frozen breaker, clear the offer, increment one revision, and append one event/action/receipt; no ball in hand.",
        "rollback": "Any telemetry, policy, product, request, plan, persistence, ACK, or refresh failure leaves runtime, rack, match, shell, AI gate, UI, authority revision, offer, events/actions, and receipts byte-for-byte unchanged.",
        "idempotent_retry": "Exact retry returns the original ACK and does not rebuild, advance revision, or duplicate event/action/receipt.",
        "conflicting_retry": "Reused key with changed action, actor, decision, revision, evidence, or payload fails closed without mutation.",
        "partial_commit_allowed": False,
    }
    if atomic != expected_atomic:
        errors.append("atomic rollback/idempotency contract differs")

    fixtures = value.get("required_future_fixtures")
    if fixtures != {"positive": EXPECTED_POSITIVE, "negative": EXPECTED_NEGATIVE}:
        errors.append("required positive/negative fixtures differ")

    allowlist = value.get("future_implementation_allowlist")
    expected_production = [
        "src/Billiards/rules/chinese_eight_ball_rules.h", "src/Billiards/rules/chinese_eight_ball_rules.cpp",
        "src/Billiards/rules/shot_fact_collector.h", "src/Billiards/rules/shot_fact_collector.cpp",
        "src/Billiards/game_runtime.h", "src/Billiards/game_runtime.cpp",
        "src/Billiards/render/modern/modern_product_session.h", "src/Billiards/render/modern/modern_product_session.cpp",
    ]
    expected_tests = ["tests/shot_fact_collector_tests.cpp", "tests/chinese_eight_ball_rules_tests.cpp", "tests/rules_runtime_integration_tests.cpp", "tests/rules_decision_session_integration_tests.cpp"]
    if not isinstance(allowlist, dict) or allowlist.get("production_files") != expected_production or \
            allowlist.get("test_files") != expected_tests or \
            allowlist.get("physics_cpp_change_allowed") is not False or \
            allowlist.get("allowlist_is_dormant_until_policy_resolved") is not True or \
            allowlist.get("cmake_change_requires_separate_coordination") is not True:
        errors.append("future implementation allowlist differs")

    if value.get("forbidden_scope") != EXPECTED_FORBIDDEN:
        errors.append("forbidden scope differs")
    if value.get("claims") != {
        "design_gap_gate_complete": True,
        "official_threshold_and_parent_choices_mapped": True,
        "authoritative_cushion_telemetry_complete": False,
        "cushion_classification_policy_resolved": False,
        "pocket_capture_path_mapped": True,
        "product_reachable": False,
        "implementation_complete": False,
        "product_behavior_changed": False,
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
        "status": "PASS_WPA_II_06_C_DESIGN_GAP_GATE" if not errors else "FAIL",
        "errors": errors,
        "production_edited": False,
        "gpu_executed": False,
    }, sort_keys=True))
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
