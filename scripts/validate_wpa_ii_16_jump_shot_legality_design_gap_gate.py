#!/usr/bin/env python3
"""Validate the fail-closed WPA II §16 jump-shot design gate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GATE = ROOT / "docs/rules/wpa-ii-16-jump-shot-legality-design-gap-gate-v1.json"
PDF_SHA = "2b92dd5088ea630337326e26b7a2a7a4499a9801f2c5da7e5cc40421d5424a24"
AUDIT_SHA = "2811958f1205d9998b40060b83c577f91359d2164e1ac0eaa9ccc4cac2ad787a"
SOURCE_SHA = {
    "src/Billiards/shot_control_model.h": "4afd2b710a8ab7e74dc9d35bf418f00bc3cfc39a999bac8022317e4a9a12ed1f",
    "src/Billiards/shot_control_model.cpp": "9aab55bddf904c9d8caebb7d5c1107ce28729c40392cfc52ac34b767264b0d57",
    "src/Billiards/cue_impact.h": "b8d34ee8204596366eb55ade70bb2db1177212adeff6d5aa25ba77b1b52c0000",
    "src/Billiards/cue_contact.h": "8a04660ae3927f76544a46a2676fe30e17c88856f4b2f9f53c274dd849be9a9c",
    "src/Billiards/cue_contact.cpp": "371eeea9353fe2759c0edf25101cdaa2a1298d6688b537690a65957c47565d27",
    "src/Billiards/coupled_cue_contact.h": "c4e24dbfcf3b5f3a3eb4ab4ef4d639bc31a56fb194ae046b44aea5ea2c408664",
    "src/Billiards/coupled_cue_contact.cpp": "e5034d0697ba86e3ed2341d2555d9da6fefbd6f084b0fb74bdf45b386b75d278",
    "src/Billiards/surface_motion.h": "76b282b8173acfe6ef87421232b97eeddc5f537178a5ee7b9c5d9eadf71da248",
    "src/Billiards/surface_motion.cpp": "66803444f1ecdcbf8c0b6ae8b37095dc61a8babf4b7b0bcd2fbe4a50098cca4e",
    "src/Billiards/physics.h": "962c06be2ac99b93484de948b51669dbc25da1c72b3009fe38a0d9d0f8883f42",
    "src/Billiards/physics.cpp": "fc497d79650bf2ff9cd8570c320653c8742e4201b8a2428a7892bfcb0124de77",
    "src/Billiards/physics_telemetry.h": "d1fa08cb8d1c2878d7a662dd267959ffb8b505fb6afcc22351b1056a5a941178",
    "src/Billiards/physics_telemetry.cpp": "b2e781bbc461e28688d8bb9eeca9b2bfb067317099adf0d431c3fe2761592612",
    "src/Billiards/continuous_collision.h": "53465297f0dcdb821cc9adce3db9dd7cc714ee3f15c7d5474d4f4d800e3cb792",
    "src/Billiards/continuous_collision.cpp": "2ff6ef3f37a963f795a38c8182a6aaad1cda7003971df5ccb57380d206c9bbf5",
    "src/Billiards/ball_ball_contact.h": "6b9c1dfd56056e24fe98226b2f8364ab29304e0042203a5744e3c7b13468fb2e",
    "src/Billiards/ball_ball_contact.cpp": "604470b189f170e998e4ad775ee170c7961c4d5c5f9d14689f11eedb322722e3",
    "src/Billiards/rules/chinese_eight_ball_rules.h": "86b6bf332fe5ca182580c57f82c74ddda075fda28b659019c709bc459e785793",
    "src/Billiards/rules/chinese_eight_ball_rules.cpp": "b984639bdf239814e0a1e0a14a64d6b73de56841273f71d7671c20e86bf475a4",
    "src/Billiards/rules/shot_fact_collector.h": "88dfc28370b4b56936ac88ac560623a8600d89a295b1f98efff65471de17d615",
    "src/Billiards/rules/shot_fact_collector.cpp": "c901ba8591ebd301ccbd4a2ee4fe2ab02f390436cf80aed0596074f65e0ec4c4",
    "src/Billiards/game_runtime.h": "61453e15d88d5581fa439fa77eb6b834fc0a22a5366b470b182b17b8d4d9f725",
    "src/Billiards/game_runtime.cpp": "fca6d4f8ffc54b5d68f3aa24928f71416b003ac8ae37ef7ab4d376aa78684ca4",
    "tests/cue_contact_tests.cpp": "93a6d368f985d6c990537d12eef5ea89ea9166e0029b6836b425a0328139755d",
    "tests/coupled_cue_contact_tests.cpp": "b439f5a698ac7ee665f16cc1ab0c35df96fe577882d8c57fbb17adc1ffbfd777",
    "tests/physics_tests.cpp": "f127de3a46b22c042c97c5a348e96b7edd55fe081f646c68f6390ad8674e20ac",
    "tests/physics_telemetry_tests.cpp": "cbbb2eb4e5f1c1eb22c5525d0c1859e19aebf2a0d975b94321f6daf741ced0c8",
    "tests/shot_fact_collector_tests.cpp": "4a5a4fee924cf62c29551844af718a4b73e6b5917d1b1323dcac29b20d43683c",
    "tests/chinese_eight_ball_rules_tests.cpp": "4c4079159e1db19545a82c1ad43b1cdfbee8ea4462b9c66c92a8be13322f3b8e",
    "tests/rules_runtime_integration_tests.cpp": "aab289bcb7e01d037e6022d41676145e09027d2d90317b9cf9e2e017335f0698",
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


def exact_list(value: Any, expected: set[str]) -> bool:
    return isinstance(value, list) and len(value) == len(expected) and set(value) == expected


def validate(value: Any, *, check_files: bool = True) -> list[str]:
    errors: list[str] = []
    if not isinstance(value, dict):
        return ["gate_not_object"]
    if value.get("schema") != "wpa_ii_16_jump_shot_legality_design_gap_gate_v1" or \
            value.get("status") != "POLICY_AND_TELEMETRY_PENDING_NO_PRODUCTION_MUTATION" or \
            value.get("scope") != ["WPA-II-16"]:
        errors.append("identity_status_scope")

    official = value.get("official_source")
    resolved = {
        "a jump intended to pass over an obstacle must use cue-tip contact on the upper half of the cue ball",
        "a lower-half strike that causes the cue ball to pass over an obstacle ball is illegal",
        "a miscue or other cause that makes the cue ball pass over an obstacle ball is illegal",
        "a lower-half strike, miscue, or other cause is not automatically an illegal jump when the cue ball does not pass over an obstacle and the shot otherwise legally hits the player's object ball",
        "an illegal jump-shot violation awards the opponent ball in hand",
    }
    pending = {
        "whether a contact point exactly on the equator belongs to the upper or lower half",
        "the contact-point sampling time, deformation model, or numerical hemisphere tolerance",
        "the minimum airborne height or duration that constitutes a jump",
        "the swept-volume clearance and tolerance that constitutes passing over an obstacle ball",
        "whether any grazing contact disqualifies passing over or requires a referee classification",
        "a standalone scoop predicate distinct from lower-half impact, miscue, and other reasons",
        "how intent is established when classifying an attempted jump",
        "the exhaustive machine classification for other reasons",
    }
    section = official.get("section", {}) if isinstance(official, dict) else {}
    if not isinstance(official, dict) or official.get("path") != "tmp/pdfs/250816-Rules-of-Heyball.pdf" or \
            official.get("sha256") != PDF_SHA or official.get("visually_reviewed_pdf_pages") != [10, 11] or \
            section.get("chapter") != "II" or section.get("clause") != "16.1-16.3" or \
            section.get("pdf_pages") != [10, 11] or section.get("locator") != "Jump Shot":
        errors.append("official_binding")
    else:
        if not exact_list(official.get("uniquely_determined_rules"), resolved):
            errors.append("official_resolved_rules")
        if not exact_list(official.get("text_does_not_operationally_define"), pending):
            errors.append("official_pending_rules")
        if check_files:
            path = workspace_path(official.get("path"))
            if path is None or not path.is_file() or sha256(path) != PDF_SHA:
                errors.append("official_pdf_drift")

    audit = value.get("frozen_audit")
    expected_audit = {
        "path": "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v5.json",
        "sha256": AUDIT_SHA,
        "row": {"id": "WPA-II-16", "status": "MISSING", "product_reachable": True},
        "promotion_authorized": False,
    }
    if audit != expected_audit:
        errors.append("audit_binding")
    elif check_files:
        path = workspace_path(audit["path"])
        if path is None or not path.is_file() or sha256(path) != AUDIT_SHA:
            errors.append("audit_drift")
        else:
            parsed = json.loads(path.read_text(encoding="utf-8"))
            rows = [x for x in parsed.get("matrix", []) if x.get("id") == "WPA-II-16"]
            if len(rows) != 1 or rows[0].get("status") != "MISSING" or rows[0].get("product_reachable") is not True:
                errors.append("audit_row_drift")

    bindings = value.get("source_bindings")
    actual = {x.get("path"): x.get("sha256") for x in bindings.values()
              if isinstance(x, dict)} if isinstance(bindings, dict) else {}
    if actual != SOURCE_SHA or len(bindings or {}) != len(SOURCE_SHA):
        errors.append("source_bindings")
    elif check_files:
        for raw, digest in SOURCE_SHA.items():
            path = workspace_path(raw)
            if path is None or not path.is_file() or sha256(path) != digest:
                errors.append(f"source_drift:{raw}")

    mapping = value.get("current_system_mapping")
    expected_mapping = {
        "shot_control_has_requested_cue_elevation_field": True,
        "runtime_cue_elevation_capability_enabled": False,
        "nonzero_elevation_rejected_by_control_conversion_without_capability": True,
        "regular_cue_contact_accepts_nonzero_elevation": False,
        "coupled_cue_contact_accepts_nonzero_elevation": False,
        "tip_offset_y_is_requested_input_not_complete_actual_impact_provenance": True,
        "regular_contact_result_has_contact_arm_and_normal": True,
        "miscue_result_applies_ball_impulse": False,
        "miscue_induced_jump_observable": False,
        "surface_motion_integrates_vertical_position_or_velocity": False,
        "surface_motion_applies_airborne_gravity": False,
        "table_plane_departure_and_landing_events_exist": False,
        "physics_frame_stores_xyz_samples": True,
        "xyz_storage_is_authoritative_ballistic_trajectory": False,
        "obstacle_overflight_or_clearance_fact_exists": False,
        "shot_facts_has_jump_tip_trajectory_obstacle_or_miscue_fields": False,
        "rules_model_has_illegal_jump_foul_reason": False,
        "global_rules_decision_ui_product_exposed": False,
    }
    if mapping != expected_mapping:
        errors.append("current_mapping")

    boundary = value.get("policy_independent_fact_boundary")
    if not isinstance(boundary, dict) or len(boundary.get("safe_existing_observations", [])) != 5 or \
            len(boundary.get("forbidden_inferences", [])) != 8 or \
            boundary.get("production_extension_safe_now") is not False or \
            "no vertical integration" not in boundary.get("reason", ""):
        errors.append("fact_boundary")

    telemetry = value.get("required_future_telemetry_contract")
    expected_lengths = {"impact": 9, "airborne_interval": 9,
                        "obstacle_overflight": 8, "shot_lifecycle": 6}
    if not isinstance(telemetry, dict) or telemetry.get("status") != "TELEMETRY_PENDING" or \
            telemetry.get("facts_are_measurements_not_legality_decisions") is not True or \
            any(len(telemetry.get(k, [])) != count for k, count in expected_lengths.items()):
        errors.append("future_telemetry")

    policy = value.get("pending_classification_contract")
    impact = policy.get("impact_hemisphere", {}) if isinstance(policy, dict) else {}
    jump = policy.get("jump_interval", {}) if isinstance(policy, dict) else {}
    obstacle = policy.get("passes_over_obstacle", {}) if isinstance(policy, dict) else {}
    miscue = policy.get("miscue_and_other_reason", {}) if isinstance(policy, dict) else {}
    if not isinstance(policy, dict) or policy.get("status") != "POLICY_AND_TELEMETRY_PENDING" or \
            impact.get("equator_and_numeric_tolerance") != "POLICY_PENDING" or impact.get("requested_offset_may_not_substitute_for_actual_contact") is not True or impact.get("forbidden_guess") is not True or \
            jump.get("machine_predicate") is not None or jump.get("status") != "POLICY_AND_TELEMETRY_PENDING" or jump.get("forbidden_guess") is not True or len(jump.get("required_dimensions", [])) != 6 or \
            obstacle.get("machine_predicate") is not None or obstacle.get("status") != "POLICY_AND_TELEMETRY_PENDING" or obstacle.get("forbidden_guess") is not True or len(obstacle.get("required_dimensions", [])) != 6 or \
            miscue.get("current_miscue_has_no_jump_trajectory") is not True or miscue.get("intent_or_referee_input_may_be_required") is not True or miscue.get("scoop_is_not_independently_defined_by_clause_16") is not True or miscue.get("status") != "POLICY_OR_REFEREE_INPUT_PENDING" or miscue.get("forbidden_guess") is not True:
        errors.append("pending_classification")

    atomic = value.get("future_atomic_contract")
    if not isinstance(atomic, dict) or len(atomic.get("launch_commit", [])) != 3 or \
            len(atomic.get("trajectory_commit", [])) != 4 or len(atomic.get("adjudication_commit", [])) != 4 or \
            "byte-equivalent" not in atomic.get("failure_rollback", "") or \
            "without a second launch" not in atomic.get("exact_retry", "") or \
            "zero mutation" not in atomic.get("conflicting_retry", "") or atomic.get("partial_commit_allowed") is not False:
        errors.append("future_atomic_contract")

    fixtures = value.get("required_future_fixtures")
    if not isinstance(fixtures, dict) or len(fixtures.get("telemetry_positive", [])) != 7 or \
            len(fixtures.get("telemetry_negative", [])) != 10 or \
            len(fixtures.get("classification_after_policy", [])) != 7 or \
            any(len(x) != len(set(x)) for x in fixtures.values() if isinstance(x, list)):
        errors.append("future_fixtures")

    allowlist = value.get("future_implementation_allowlist")
    production = allowlist.get("production_files", []) if isinstance(allowlist, dict) else []
    if not isinstance(allowlist, dict) or allowlist.get("allowlist_is_dormant_until_3d_physics_and_policy_gate_is_approved") is not True or \
            allowlist.get("cmake_change_requires_separate_coordination") is not True or \
            "src/Billiards/render/modern/modern_renderer.cpp" in production or "CMakeLists.txt" in production:
        errors.append("future_allowlist")

    forbidden = {
        "production source edits in this design-gate task",
        "enabling cue elevation before a supported three-dimensional contact and trajectory model exists",
        "treating requested tip offset as actual impact hemisphere",
        "treating inert XYZ storage or renderer arcs as authoritative jump telemetry",
        "guessing equator, airborne, clearance, grazing, scoop, intent or other-reason predicates",
        "adjudicating from incomplete impact, trajectory or obstacle evidence",
        "promoting WPA-II-16 above MISSING",
        "enabling global rulesDecisionUiProductExposed",
        "CMake changes", "Phase4 or Phase5 renderer changes",
        "GPU, Cocoa, or OpenGL execution",
        "readiness generation or authorization issuance",
    }
    if not exact_list(value.get("forbidden_scope"), forbidden):
        errors.append("forbidden_scope")

    claims = {
        "design_gap_gate_complete": True,
        "official_pages_visually_reviewed": True,
        "current_contact_and_trajectory_mapped": True,
        "product_reachable_jump_shot_available": False,
        "actual_impact_hemisphere_telemetry_complete": False,
        "airborne_trajectory_telemetry_complete": False,
        "obstacle_overflight_telemetry_complete": False,
        "jump_legality_implemented": False,
        "production_implementation_complete": False,
        "audit_row_promoted": False,
        "gpu_executed": False,
    }
    if value.get("claims") != claims:
        errors.append("claims")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--gate", type=Path, default=DEFAULT_GATE)
    args = parser.parse_args()
    try:
        value = json.loads(args.gate.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        print(json.dumps({"status": "FAIL", "errors": [f"gate_read:{error}"]}, sort_keys=True))
        return 1
    errors = validate(value, check_files=True)
    print(json.dumps({"status": "PASS" if not errors else "FAIL", "gate": str(args.gate.resolve()), "errors": errors}, sort_keys=True))
    if errors:
        return 1
    print("PASS_WPA_II_16_JUMP_SHOT_POLICY_TELEMETRY_PENDING_GATE")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
