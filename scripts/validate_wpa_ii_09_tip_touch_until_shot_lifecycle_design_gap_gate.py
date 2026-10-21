#!/usr/bin/env python3
"""Validate the fail-closed WPA II §9 tip/lifecycle design gate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GATE = ROOT / "docs/rules/wpa-ii-09-tip-touch-until-shot-lifecycle-design-gap-gate-v1.json"
PDF_SHA = "2b92dd5088ea630337326e26b7a2a7a4499a9801f2c5da7e5cc40421d5424a24"
AUDIT_SHA = "2811958f1205d9998b40060b83c577f91359d2164e1ac0eaa9ccc4cac2ad787a"
EXPECTED_SOURCE_SHA = {
    "src/Billiards/rules/ball_in_hand_authority.h": "026fd6196263b1c6dab180fa7f94d29baf277ff56bdff450ca189e7d8ba795d6",
    "src/Billiards/game_runtime.h": "61453e15d88d5581fa439fa77eb6b834fc0a22a5366b470b182b17b8d4d9f725",
    "src/Billiards/game_runtime.cpp": "fca6d4f8ffc54b5d68f3aa24928f71416b003ac8ae37ef7ab4d376aa78684ca4",
    "src/Billiards/render/modern/modern_product_session.h": "bc7862aac74640bd9fb2aa5929eddc3601b89d5e17b5cd047c8fd8e5578f003d",
    "src/Billiards/render/modern/modern_product_session.cpp": "d7a3d4e88e3ec8723d78b7ed37808463fbfe9f0e005e71b409c82d4a3e7cb5e9",
    "src/Billiards/product_input_controller.h": "bd0d10bb8eb44e15db6fdef45262d57130f284e639c45cbb0f67b42377ae3f7d",
    "src/Billiards/product_input_controller.cpp": "1e76dfdd0b5c646f0bcd6e5f4d9f8516dd3a3903d4be74b9aad5cd872e79e9b9",
    "src/Billiards/render/modern/product_input_adapter.h": "f26df1a2773b4ecb9433e16e4c159b79693780ad21ca7644024f93852dbde5d0",
    "src/Billiards/render/modern/product_input_adapter.cpp": "b672e9231d17807a59652878dbd4b70b7a949f6e6cfe2fc7694a5126de0c167d",
    "src/Billiards/cue_impact.h": "b8d34ee8204596366eb55ade70bb2db1177212adeff6d5aa25ba77b1b52c0000",
    "src/Billiards/cue_contact.h": "8a04660ae3927f76544a46a2676fe30e17c88856f4b2f9f53c274dd849be9a9c",
    "src/Billiards/cue_contact.cpp": "371eeea9353fe2759c0edf25101cdaa2a1298d6688b537690a65957c47565d27",
    "src/Billiards/coupled_cue_contact.h": "c4e24dbfcf3b5f3a3eb4ab4ef4d639bc31a56fb194ae046b44aea5ea2c408664",
    "src/Billiards/coupled_cue_contact.cpp": "e5034d0697ba86e3ed2341d2555d9da6fefbd6f084b0fb74bdf45b386b75d278",
    "src/Billiards/physics_telemetry.h": "d1fa08cb8d1c2878d7a662dd267959ffb8b505fb6afcc22351b1056a5a941178",
    "src/Billiards/shot_control_model.h": "4afd2b710a8ab7e74dc9d35bf418f00bc3cfc39a999bac8022317e4a9a12ed1f",
    "src/Billiards/shot_control_model.cpp": "9aab55bddf904c9d8caebb7d5c1107ce28729c40392cfc52ac34b767264b0d57",
    "tests/rules_decision_session_integration_tests.cpp": "71e7fe78f078c2287814f9b836ff9588275ee532da32fb274611f1d2fde970f2",
    "tests/product_input_controller_tests.cpp": "0137d40ab14fb09163ca3cd245d26904959369c3e8a5edea906407a820c3873b",
    "tests/cue_contact_tests.cpp": "93a6d368f985d6c990537d12eef5ea89ea9166e0029b6836b425a0328139755d",
    "tests/coupled_cue_contact_tests.cpp": "b439f5a698ac7ee665f16cc1ab0c35df96fe577882d8c57fbb17adc1ffbfd777",
    "tests/rules_runtime_integration_tests.cpp": "aab289bcb7e01d037e6022d41676145e09027d2d90317b9cf9e2e017335f0698",
}
EXPECTED_REQUIRED_TELEMETRY = [
    "placement_entitlement_id_actor_scope_and_revision",
    "cue_part_identity_tip_or_non_tip",
    "cue_part_contact_point_and_normal",
    "cue_motion_direction_distance_duration_velocity_and_acceleration",
    "forward_stroke_classifier_version_and_result",
    "event_origin_sample_time_and_completeness",
    "shot_launch_request_validation_result",
    "cue_contact_regime_and_impulse_applied",
    "entitlement_consumption_reason",
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
    if value.get("schema") != "wpa_ii_09_tip_touch_until_shot_lifecycle_design_gap_gate_v1" or \
            value.get("status") != "POLICY_AND_TELEMETRY_PENDING_NO_PRODUCTION_MUTATION" or \
            value.get("scope") != ["WPA-II-09"]:
        errors.append("identity_status_scope")

    official = value.get("official_source")
    if not isinstance(official, dict) or \
            official.get("path") != "tmp/pdfs/250816-Rules-of-Heyball.pdf" or \
            official.get("sha256") != PDF_SHA or \
            official.get("visually_reviewed_pdf_pages") != [8]:
        errors.append("official_binding")
    else:
        section = official.get("section_9", {})
        if section.get("chapter") != "II" or section.get("clause") != "9" or \
                section.get("pdf_page") != 8 or section.get("locator") != "Ball in Hand":
            errors.append("official_section")
        resolved = official.get("text_uniquely_determines")
        required_resolved = {
            "a valid placement preview or movement is not itself the shot that ends ball-in-hand",
            "the ball-in-hand entitlement persists through repeated cue-ball movement until a shot is executed",
            "a rejected preview, rejected placement commit, or rejected technical shot request cannot consume the entitlement",
            "break-shot ball in hand remains spatially restricted behind the Head String",
            "ball in hand awarded for a non-break foul permits full-table placement",
            "for the explicit non-break-foul context, touching the cue ball with the cue tip while moving it is a foul",
        }
        omissions = official.get("text_does_not_specify")
        required_omissions = {
            "the cue-tip kinematic, intent, direction, distance, duration, velocity, or acceleration predicate that constitutes a forward stroke",
            "the numerical contact tolerance and authoritative sample time for cue-part identity",
            "whether a technically accepted miscue or zero-impulse cue contact counts as executing a shot for entitlement consumption",
            "how to classify a cue action whose launch is accepted but physics applies no cue-ball impulse",
        }
        contextual = official.get("contextual_tip_reading", {})
        if not isinstance(resolved, list) or set(resolved) != required_resolved or \
                len(resolved) != len(required_resolved):
            errors.append("official_resolved_contract")
        if not isinstance(omissions, list) or set(omissions) != required_omissions or \
                len(omissions) != len(required_omissions):
            errors.append("official_ambiguities")
        if contextual.get("specific_non_break_sentence_overrides_general_tip_permission") is not True or \
                contextual.get("forward_stroke_definition_available") is not False:
            errors.append("contextual_tip_reading")
        if check_files:
            path = workspace_path(official.get("path"))
            if path is None or not path.is_file() or sha256(path) != PDF_SHA:
                errors.append("official_pdf_drift")

    expected_audit = {
        "path": "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v5.json",
        "sha256": AUDIT_SHA,
        "row_id": "WPA-II-09",
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
                    if row.get("id") == "WPA-II-09"]
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

    mapping = value.get("existing_mapping")
    expected_mapping = {
        "core_g_preview_read_only": True,
        "core_g_confirm_moves_cue_ball_atomically": True,
        "core_g_confirm_currently_clears_ball_in_hand": True,
        "core_g_confirm_currently_clears_placement_offer": True,
        "repeated_move_after_confirm_currently_product_reachable": False,
        "apply_cue_impact_currently_rejects_while_placement_offer_pending": True,
        "product_coordinate_preview_or_confirm_has_cue_part_identity": False,
        "cue_impact_input_has_cue_part_identity": False,
        "cue_impact_input_has_forward_stroke_classification": False,
        "cue_contact_result_has_pre_shot_placement_touch_provenance": False,
        "physics_telemetry_has_placement_tip_touch_event": False,
        "global_rules_decision_ui_product_exposed": False,
    }
    if mapping != expected_mapping:
        errors.append("existing_mapping")

    lifecycle = value.get("resolved_lifecycle_contract")
    if not isinstance(lifecycle, dict) or \
            lifecycle.get("entitlement_starts") != "only after the authoritative foul or parent decision grants ball in hand to its exact actor" or \
            len(lifecycle.get("entitlement_persists_through", [])) != 4 or \
            lifecycle.get("entitlement_consumption_candidate") != "one authority-accepted shot execution classified under the future shot-execution policy" or \
            len(lifecycle.get("must_not_consume", [])) != 6 or \
            len(lifecycle.get("reset_boundaries", [])) != 4 or \
            lifecycle.get("unexpected_turn_change_while_entitlement_active") != "fail closed and invalidate the stale actor-bound entitlement; do not transfer it implicitly" or \
            lifecycle.get("failed_or_miscue_shot_consumes_entitlement") != "POLICY_PENDING" or \
            lifecycle.get("placement_commit_may_end_entitlement") is not False:
        errors.append("resolved_lifecycle_contract")

    classification = value.get("tip_touch_and_shot_classification")
    policies = classification.get("policy_decisions", {}) if isinstance(classification, dict) else {}
    forward = policies.get("forward_stroke_predicate", {})
    shot = policies.get("shot_execution_boundary", {})
    numeric = policies.get("tip_contact_numeric_policy", {})
    if not isinstance(classification, dict) or \
            classification.get("status") != "POLICY_AND_TELEMETRY_PENDING" or \
            classification.get("required_future_telemetry") != EXPECTED_REQUIRED_TELEMETRY or \
            classification.get("physics_solver_may_report_contact_kinematics_but_not_decide_foul") is not True or \
            classification.get("coordinate_drag_policy") != "The existing product coordinate preview/confirm path is an explicit placement tool, not a simulated cue-part touch; it remains governed by Core-G geometry and authority validation." or \
            classification.get("production_implementation_authorized") is not False or \
            forward.get("status") != "POLICY_PENDING" or forward.get("selected") is not None or \
            forward.get("forbidden_guess") is not True or len(forward.get("required_dimensions", [])) != 6 or \
            shot != {
                "status": "POLICY_PENDING", "selected": None,
                "allowed_candidates": ["validated_launch_request", "cue_contact_regime_classified_as_shot_even_without_impulse", "positive_authoritative_cue_ball_impulse"],
                "must_distinguish": ["rejected_launch", "zero_speed", "miscue", "unsupported_contact", "successful_impulse"],
                "forbidden_guess": True,
            } or numeric != {
                "status": "POLICY_PENDING", "selected": None,
                "allowed_candidates": ["exact_geometry", "documented_deterministic_quantization"],
                "forbidden_guess": True,
            }:
        errors.append("policy_telemetry_pending_boundary")

    atomic = value.get("future_atomic_product_contract")
    if not isinstance(atomic, dict) or \
            len(atomic.get("placement_move_commit", [])) != 4 or \
            len(atomic.get("shot_release_commit_after_policy", [])) != 4 or \
            "preserve" not in atomic.get("rejected_or_unclassified_shot_release", "") or \
            "no behavior is selected" not in atomic.get("failed_or_miscue_release", "") or \
            "without moving again" not in atomic.get("exact_retry", "") or \
            "fails closed" not in atomic.get("conflicting_retry", "") or \
            "exact pre-call state" not in atomic.get("failure_rollback", "") or \
            atomic.get("foundation_actions_product_exposed") is not False or \
            atomic.get("other_rules_actions_product_exposed") is not False or \
            atomic.get("global_rules_decision_ui_product_exposed") is not False or \
            atomic.get("partial_commit_allowed") is not False:
        errors.append("future_atomic_contract")

    fixtures = value.get("required_future_fixtures")
    if not isinstance(fixtures, dict) or \
            len(fixtures.get("lifecycle_positive", [])) != 6 or \
            len(fixtures.get("lifecycle_negative", [])) != 7 or \
            len(fixtures.get("tip_and_shot_after_policy", [])) != 9 or \
            len(set(fixtures.get("lifecycle_positive", []))) != 6 or \
            len(set(fixtures.get("lifecycle_negative", []))) != 7 or \
            len(set(fixtures.get("tip_and_shot_after_policy", []))) != 9:
        errors.append("future_fixtures")

    allowlist = value.get("future_implementation_allowlist")
    production = allowlist.get("production_files", []) if isinstance(allowlist, dict) else []
    if not isinstance(allowlist, dict) or \
            allowlist.get("allowlist_is_dormant_until_tip_and_shot_policies_resolved") is not True or \
            allowlist.get("cmake_change_requires_separate_coordination") is not True or \
            "src/Billiards/render/modern/modern_renderer.cpp" in production or \
            "CMakeLists.txt" in production:
        errors.append("future_allowlist")

    required_forbidden = {
        "production source edits in this design-gate task",
        "guessing a forward-stroke, tip-contact, miscue, zero-speed, or shot-execution threshold",
        "treating coordinate preview or drag as a cue-tip touch",
        "consuming ball in hand on preview, rejected confirm, or rejected shot dispatch",
        "promotion of WPA-II-09 above PARTIAL",
        "changes to WPA-II-08 re-spot or first-contact semantics",
        "global rulesDecisionUiProductExposed enablement",
        "CMake changes",
        "Phase4 or Phase5 renderer changes",
        "GPU, Cocoa, or OpenGL execution",
        "readiness generation or authorization issuance",
    }
    forbidden = value.get("forbidden_scope")
    if not isinstance(forbidden, list) or set(forbidden) != required_forbidden or \
            len(forbidden) != len(required_forbidden):
        errors.append("forbidden_scope")
    expected_claims = {
        "design_gap_gate_complete": True,
        "entitlement_persistence_text_resolved": True,
        "reset_and_rejected_attempt_boundaries_resolved": True,
        "forward_stroke_policy_resolved": False,
        "shot_execution_boundary_resolved": False,
        "tip_touch_telemetry_complete": False,
        "production_implementation_complete": False,
        "product_behavior_changed": False,
        "audit_row_promoted": False,
        "gpu_executed": False,
    }
    if value.get("claims") != expected_claims:
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
    print("PASS_WPA_II_09_TIP_TOUCH_UNTIL_SHOT_POLICY_TELEMETRY_PENDING_GATE")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
