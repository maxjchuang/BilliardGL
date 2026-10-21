#!/usr/bin/env python3
"""Validate the fail-closed WPA II 6(d) classification design gate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GATE = ROOT / "docs/rules/wpa-ii-06-d-head-string-half-ball-classification-design-gate-v1.json"
PDF_SHA = "2b92dd5088ea630337326e26b7a2a7a4499a9801f2c5da7e5cc40421d5424a24"
AUDIT_SHA = "d7ab2104fac9fd85fe8d7336ae658cc9baaaa6b4fa866cd4e7815c51404ad033"

EXPECTED_SOURCES: dict[str, dict[str, Any]] = {
    "runtime": {
        "path": "src/Billiards/game_runtime.cpp",
        "sha256": "3292df4f4546d9fd1639de8d5db33b7dab526f13b129f496d1420a489253fb9a",
        "relevant_lines": [[904, 922], [1419, 1454], [1500, 1614]],
    },
    "runtime_header": {
        "path": "src/Billiards/game_runtime.h",
        "sha256": "deacceaa4ffff8b0e5080963d958f5fc49a77a197c744c2693be6f2d82b96ec3",
    },
    "placement_authority": {
        "path": "src/Billiards/rules/ball_in_hand_authority.h",
        "sha256": "026fd6196263b1c6dab180fa7f94d29baf277ff56bdff450ca189e7d8ba795d6",
    },
    "shot_model": {
        "path": "src/Billiards/rules/chinese_eight_ball_rules.h",
        "sha256": "5ab7a045dbae177697a72aed97326d56ee14ae31c0434191066f011a8f52145b",
    },
    "product_session": {
        "path": "src/Billiards/render/modern/modern_product_session.cpp",
        "sha256": "8e962dfc76149a4461e011941231e5e72860aed9f8cb6f006169e269455cd174",
        "relevant_lines": [[283, 312], [315, 353]],
    },
    "ui_model": {
        "path": "src/Billiards/rules/rules_decision_ui_model.cpp",
        "sha256": "5d4942db12f43069069d68e00e8aecf7f38106bac4f1330f9d41f2c51e830cb8",
    },
    "runtime_tests": {
        "path": "tests/rules_runtime_integration_tests.cpp",
        "sha256": "2abb5a981251e05b179c03fb82adc2bd3da1076f291a4c8e0a174743baef8a55",
        "relevant_lines": [[2055, 2105], [2129, 2196]],
    },
    "product_tests": {
        "path": "tests/rules_decision_session_integration_tests.cpp",
        "sha256": "6082e1d44054a1adc8b3743c2eb31698472a3842626bf25da17db82dadd3220d",
    },
}

EXPECTED_TIMING = {
    "status": "POLICY_PENDING",
    "selected": None,
    "allowed_candidates": [
        "successful_break_placement_commit",
        "accepted_break_shot_release_using_frozen_committed_position",
        "single_post_break_adjudication_using_frozen_launch_position",
    ],
    "forbidden_guess": True,
    "reason": "Clause 6(d) does not identify the authoritative event at which the cue-ball position is sampled.",
}

EXPECTED_RESET = {
    "status": "POLICY_PENDING",
    "selected": None,
    "allowed_candidates": ["rack", "match", "set", "event"],
    "forbidden_guess": True,
    "reason": "Clause 6(d) defines first and later offense consequences but does not define the lifetime of that history.",
}

EXPECTED_POSITIVE = [
    "GEO_HEAD_SIDE_NO_OFFENSE",
    "GEO_EXACT_HALF_NO_OFFENSE",
    "GEO_FOOT_SIDE_VIOLATION_CANDIDATE",
    "PLAYER0_FIRST_REMINDER_ONLY",
    "PLAYER0_LATER_FOUL_ONCE",
    "PLAYER1_FIRST_INDEPENDENT_REMINDER",
    "SELECTED_BOUNDARY_COUNTS_ONCE",
    "ATOMIC_SUCCESS_ONE_REVISION_ONE_RECEIPT",
]

EXPECTED_NEGATIVE = [
    "DRAG_PREVIEW_NEVER_COUNTS",
    "REPEATED_PREVIEW_NEVER_COUNTS",
    "INVALID_PREVIEW_NEVER_COUNTS",
    "REJECTED_CONFIRM_NEVER_COUNTS",
    "WRONG_ACTOR_NEVER_COUNTS",
    "WRONG_ACTION_NEVER_COUNTS",
    "WRONG_FLOW_NEVER_COUNTS",
    "STALE_DECISION_OR_REVISION_NEVER_COUNTS",
    "STATE_HASH_DRIFT_NEVER_COUNTS",
    "NONFINITE_POSITION_NEVER_COUNTS",
    "GENERIC_SECTION8_PLACEMENT_NEVER_COUNTS",
    "LIVE_POST_LAUNCH_POSITION_NEVER_COUNTS",
    "IDEMPOTENT_RETRY_NEVER_DOUBLE_COUNTS",
    "CONFLICTING_RETRY_FAILS_WITHOUT_MUTATION",
    "FAILURE_ROLLS_BACK_BOTH_PLAYER_HISTORIES",
    "UNSELECTED_RESET_SCOPE_CANNOT_BE_IMPLEMENTED",
]

EXPECTED_FORBIDDEN = [
    "production source edits in this design-gate task",
    "CMake changes",
    "physics solver interpretation of the rule",
    "counting preview or rejected UI attempts",
    "global rulesDecisionUiProductExposed enablement",
    "Phase4 renderer or trajectory changes",
    "Phase5 exact-shape UI changes",
    "visual or evidence threshold changes",
    "GPU, Cocoa, or OpenGL execution",
    "readiness generation",
    "authorization issuance",
    "promotion of WPA-II-06-D above PARTIAL",
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

    if value.get("schema") != "wpa_ii_06_d_head_string_half_ball_classification_design_gate_v1" or \
            value.get("status") != "POLICY_CLASSIFICATION_PENDING_NO_PRODUCTION_MUTATION" or \
            value.get("scope") != ["WPA-II-06-D"]:
        errors.append("gate identity/status/scope differs")

    official = value.get("official_source")
    if not isinstance(official, dict):
        errors.append("official source binding missing")
    else:
        if official.get("path") != "tmp/pdfs/250816-Rules-of-Heyball.pdf" or \
                official.get("sha256") != PDF_SHA:
            errors.append("official PDF binding differs")
        definition = official.get("head_string_definition")
        if not isinstance(definition, dict) or \
                definition.get("chapter") != "II" or \
                definition.get("clause") != "3(e)" or \
                definition.get("pdf_page") != 6 or \
                definition.get("locator") != "Head String definition" or \
                definition.get("controlled_paraphrase") != \
                "The Head String is parallel to the bottom cushion, 635 mm from that cushion, and intersects both side cushions.":
            errors.append("official Head String definition differs")
        offense = official.get("offense_clause")
        if not isinstance(offense, dict) or \
                offense.get("chapter") != "II" or \
                offense.get("clause") != "6(d)" or \
                offense.get("pdf_page") != 7 or \
                offense.get("locator") != "Cue-ball placement behind Head String" or \
                offense.get("controlled_paraphrase") != \
                "The cue ball is placed behind the Head String. More than half crossing produces a reminder for the first offense and a foul for a later offense after the break.":
            errors.append("official offense clause differs")
        omissions = official.get("text_does_not_specify")
        required_omissions = {
            "whether the authoritative measurement is at placement commit, shot release, or post-break adjudication",
            "whether preview or rejected placement attempts are offenses",
            "whether the per-player offense history resets per rack, match, set, or event",
            "the numerical tolerance or quantization policy at the exact-half boundary",
        }
        if not isinstance(omissions, list) or set(omissions) != required_omissions or len(omissions) != 4:
            errors.append("official-text ambiguity set differs")
        pdf_path = workspace_path(official.get("path"))
        if check_files and (pdf_path is None or not pdf_path.is_file() or sha256(pdf_path) != PDF_SHA):
            errors.append("official PDF bytes/path changed")

    audit = value.get("frozen_audit")
    expected_audit = {
        "path": "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v3.json",
        "sha256": AUDIT_SHA,
        "row_id": "WPA-II-06-D",
        "row_status": "PARTIAL",
        "product_reachable": True,
        "gap": "No half-ball crossing measurement or reminder/offense escalation exists.",
    }
    if audit != expected_audit:
        errors.append("frozen audit-v3 binding/classification differs")
    elif check_files:
        audit_path = workspace_path(audit["path"])
        if audit_path is None or not audit_path.is_file() or sha256(audit_path) != AUDIT_SHA:
            errors.append("frozen audit-v3 bytes/path changed")
        else:
            audit_value = json.loads(audit_path.read_text(encoding="utf-8"))
            rows = [row for row in audit_value.get("matrix", []) if row.get("id") == "WPA-II-06-D"]
            if len(rows) != 1 or rows[0].get("status") != "PARTIAL" or \
                    rows[0].get("product_reachable") is not True:
                errors.append("frozen audit row no longer matches PARTIAL/product-reachable boundary")

    sources = value.get("source_bindings")
    if sources != EXPECTED_SOURCES:
        errors.append("source bindings differ")
    elif check_files:
        for name, binding in EXPECTED_SOURCES.items():
            path = workspace_path(binding["path"])
            if path is None or not path.is_file() or sha256(path) != binding["sha256"]:
                errors.append(f"source binding bytes/path changed: {name}")

    geometry = value.get("geometry_contract")
    if not isinstance(geometry, dict) or \
            geometry.get("units") != "centimetres" or \
            geometry.get("world_axis") != "The table longitudinal axis is world Z; -Z is the head/bottom-cushion side and +Z is the foot/top-cushion side." or \
            geometry.get("production_table_length_cm") != 254.0 or \
            geometry.get("head_string_world_z_formula") != "-playfield_length_cm / 2 + 63.5" or \
            geometry.get("head_string_world_z_cm") != -63.5 or \
            geometry.get("cue_ball_radius_cm") != 2.8575 or \
            geometry.get("measurement_object") != "the cue-ball circular footprint projected onto the table plane" or \
            geometry.get("signed_crossing_cm_formula") != "cue_ball_center_world_z_cm - head_string_world_z_cm" or \
            geometry.get("offense_geometry_predicate") != "signed_crossing_cm > 0" or \
            geometry.get("mathematical_equivalence") != "For a circle bisected by a zero-width line, more than half of its footprint is on the foot side if and only if its center is on the foot side." or \
            geometry.get("exact_half_boundary") != "signed_crossing_cm == 0 is exactly half and is not an offense under the words more than half" or \
            geometry.get("head_side_example") != "signed_crossing_cm < 0 is not an offense" or \
            geometry.get("foot_side_example") != "signed_crossing_cm > 0 is a geometric violation candidate" or \
            geometry.get("x_coordinate_changes_result") is not False or \
            geometry.get("finite_inputs_required") is not True or \
            geometry.get("implementation_tolerance_policy_selected") is not False:
        errors.append("geometry contract differs")

    decisions = value.get("classification_decisions")
    if not isinstance(decisions, dict) or \
            decisions.get("authoritative_measurement_time") != EXPECTED_TIMING or \
            decisions.get("per_player_history_reset_scope") != EXPECTED_RESET or \
            decisions.get("exact_boundary_numeric_policy") != {
                "status": "POLICY_PENDING",
                "selected": None,
                "allowed_candidates": [
                    "exact_authoritative_double",
                    "documented_deterministic_quantization",
                ],
                "forbidden_guess": True,
            } or \
            decisions.get("production_implementation_authorized") is not False or \
            decisions.get("audit_row_may_be_promoted") is not False:
        errors.append("classification-pending decision differs")

    core = value.get("existing_core_g_mapping")
    if not isinstance(core, dict) or \
            core.get("head_string_plane_available") is not True or \
            core.get("head_string_world_z_cm") != -63.5 or \
            core.get("current_authority_clause") != "WPA-II-08" or \
            core.get("current_flow") != "ball_in_hand_behind_head_string" or \
            core.get("current_action") != "rules.ball_in_hand.confirm_behind_head_string" or \
            core.get("current_validation") != "A Section 8 placement center must be strictly behind the Head String; the line itself and foot-side centers are rejected." or \
            core.get("preview_is_read_only") is not True or \
            core.get("submit_requires_exact_context_actor_revision_action_and_state_hash") is not True or \
            core.get("accepted_submit_is_idempotent_and_increments_one_authority_revision") is not True or \
            core.get("shot_facts_has_wpa_ii_06_d_signal") is not False or \
            core.get("section_8_authority_is_not_wpa_ii_06_d_break_authority") is not True or \
            core.get("reuse_without_distinct_break_context_forbidden") is not True:
        errors.append("existing Core G boundary differs")

    product = value.get("product_submission_contract")
    must_not_count = product.get("must_not_count") if isinstance(product, dict) else None
    required_noncounts = {
        "drag preview samples, including repeated samples",
        "UI previews rejected by geometry validation",
        "confirm attempts rejected before an authoritative ACK",
        "wrong actor, action, flow, decision ID, revision, state hash, or idempotency conflicts",
        "accepted Section 8 ball-in-hand placement outside a distinct Section 6(d) break context",
        "raw live cue-ball positions after physics has started",
        "replayed idempotent ACKs",
    }
    required_preconditions = {
        "the classification gate selects exactly one authoritative measurement time",
        "the request or frozen launch fact is in a distinct WPA-II-06-D break context",
        "actor/player identity and authority revision are exact",
        "the sampled position is finite and comes from committed or frozen authoritative state",
        "the geometry predicate is true under the selected deterministic numeric policy",
        "the mutation key has not already been consumed",
    }
    preconditions = product.get("future_count_preconditions") if isinstance(product, dict) else None
    if not isinstance(product, dict) or \
            not isinstance(must_not_count, list) or set(must_not_count) != required_noncounts or len(must_not_count) != 7 or \
            not isinstance(preconditions, list) or set(preconditions) != required_preconditions or len(preconditions) != 6 or \
            product.get("placement_commit_candidate") != "Count only after a successful authoritative Section 6(d) placement ACK; preview and failed submission remain read-only." or \
            product.get("shot_release_candidate") != "Count only when the break shot is accepted for launch, using the last committed cue-ball position frozen before motion." or \
            product.get("post_break_candidate") != "Count exactly once during break adjudication using the launch position frozen before motion, never the simulated post-shot cue-ball position." or \
            product.get("no_candidate_is_selected_by_this_gate") is not True:
        errors.append("product submission/non-counting contract differs")

    history = value.get("per_player_history_contract")
    expected_history = {
        "required": True,
        "key": "actor_player_id",
        "valid_players": [0, 1],
        "state_shape": "one offense count per player within the later-selected reset scope",
        "first_geometric_offense_for_player": "atomically increment that player's count from 0 to 1 and emit one reminder; do not mark a foul",
        "later_geometric_offense_for_same_player": "atomically increment that player's count and emit one foul classification exactly once",
        "other_player_history_is_independent": True,
        "reset_scope_selected": False,
        "invalid_actor_mutates_history": False,
    }
    if history != expected_history:
        errors.append("per-player history contract differs")

    atomic = value.get("atomic_mutation_contract")
    expected_atomic = {
        "transaction_order": [
            "validate unique authority event and exact break context",
            "freeze and validate actor, revision, position, and policy scope",
            "evaluate the strict geometric predicate",
            "read only the actor's per-player history",
            "derive reminder or foul without mutating",
            "commit history, authority revision, event/action, adjudication effect, receipt, and state hash as one transaction",
        ],
        "failure_rollback": "Any validation, classification, persistence, or downstream commit failure restores cue state, match state, both players' histories, authority revision, events/actions, and receipts byte-for-byte.",
        "idempotent_retry": "An exact retry returns the original ACK or adjudication receipt with no second history increment, reminder, foul, event, action, or revision.",
        "conflicting_retry": "A reused key with different actor, position, context, revision, or action fails closed with no mutation.",
        "partial_commit_allowed": False,
    }
    if atomic != expected_atomic:
        errors.append("atomic mutation/rollback contract differs")

    fixtures = value.get("required_future_fixtures")
    if fixtures != {"positive": EXPECTED_POSITIVE, "negative": EXPECTED_NEGATIVE}:
        errors.append("required positive/negative fixture matrix differs")

    allowlist = value.get("future_implementation_allowlist")
    expected_production = [
        "src/Billiards/rules/break_head_string_offense.h",
        "src/Billiards/rules/break_head_string_offense.cpp",
        "src/Billiards/game_runtime.h",
        "src/Billiards/game_runtime.cpp",
        "src/Billiards/rules/chinese_eight_ball_rules.h",
        "src/Billiards/rules/chinese_eight_ball_rules.cpp",
        "src/Billiards/render/modern/modern_product_session.cpp",
    ]
    expected_tests = [
        "tests/break_head_string_offense_tests.cpp",
        "tests/rules_runtime_integration_tests.cpp",
        "tests/rules_decision_session_integration_tests.cpp",
    ]
    if not isinstance(allowlist, dict) or \
            allowlist.get("production_files") != expected_production or \
            allowlist.get("test_files") != expected_tests or \
            allowlist.get("allowlist_is_dormant_until_policy_resolved") is not True or \
            allowlist.get("cmake_change_requires_separate_coordination") is not True:
        errors.append("future implementation allowlist differs")

    if value.get("forbidden_scope") != EXPECTED_FORBIDDEN:
        errors.append("forbidden scope differs")

    claims = value.get("claims")
    if claims != {
        "design_gap_gate_complete": True,
        "official_geometry_interpretation_frozen": True,
        "measurement_time_resolved": False,
        "history_reset_scope_resolved": False,
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
        "status": "PASS_WPA_II_06_D_CLASSIFICATION_DESIGN_GATE" if not errors else "FAIL",
        "errors": errors,
        "production_edited": False,
        "gpu_executed": False,
    }, sort_keys=True))
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
