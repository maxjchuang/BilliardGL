#!/usr/bin/env python3
"""Validate the fail-closed WPA II 17(a)/17(e) off-table design gap gate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GATE = ROOT / "docs/rules/wpa-ii-17-a-e-off-table-physical-telemetry-design-gap-gate-v1.json"
PDF_SHA = "2b92dd5088ea630337326e26b7a2a7a4499a9801f2c5da7e5cc40421d5424a24"
AUDIT_SHA = "2811958f1205d9998b40060b83c577f91359d2164e1ac0eaa9ccc4cac2ad787a"
SCRATCH_SHA = "38d4c067d0f3c77603ef5149ea5160e859f8fc8d02c52d04a116761b8106aef5"
SOURCE_SHA = {
    "src/Billiards/surface_motion.h": "76b282b8173acfe6ef87421232b97eeddc5f537178a5ee7b9c5d9eadf71da248",
    "src/Billiards/surface_motion.cpp": "66803444f1ecdcbf8c0b6ae8b37095dc61a8babf4b7b0bcd2fbe4a50098cca4e",
    "src/Billiards/physics.h": "962c06be2ac99b93484de948b51669dbc25da1c72b3009fe38a0d9d0f8883f42",
    "src/Billiards/physics.cpp": "fc497d79650bf2ff9cd8570c320653c8742e4201b8a2428a7892bfcb0124de77",
    "src/Billiards/physics_telemetry.h": "d1fa08cb8d1c2878d7a662dd267959ffb8b505fb6afcc22351b1056a5a941178",
    "src/Billiards/physics_telemetry.cpp": "b2e781bbc461e28688d8bb9eeca9b2bfb067317099adf0d431c3fe2761592612",
    "src/Billiards/pocket_boundary.h": "07e3e69930b460e9dfed1952e97f88a4f29a1a099e00cab0ad64a18341380440",
    "src/Billiards/pocket_boundary.cpp": "0ca06e54b565053bbd7027318aea8a84e0c8d96712ff5bca93eac00a52f770b0",
    "src/Billiards/physics_boundary_mode.h": "f4052fa8d31ffd261aa66987ba87b37e73727396d78689681df2af46d0060ddf",
    "src/Billiards/game_state.h": "6363b8c2481df6702bb22c4b00b44a546eb594241a7744684c2c94c96adaed4d",
    "src/Billiards/game_state.cpp": "a91185ba5fad8c8571aa1b0efd36de967d0fc3c307fef01b2903c2f910de88c3",
    "src/Billiards/table_specs.h": "9e2b64b317d99f6e8e1877522d9cd06049b529de54caebc89742c76af19ba592",
    "src/Billiards/table_specs.cpp": "751b4f4629b07416590b8bc46744e8d086c262fb488cc0cf8ea124845ca0dce6",
    "src/Billiards/rules/chinese_eight_ball_rules.h": "7408e24a58c827e3de27b766cd91bdb5ce5831919cea97f733f8191baae59f7f",
    "src/Billiards/rules/chinese_eight_ball_rules.cpp": "a591b919e99580a8e859e98ebfda9de1a0f2ec89e521d47f67ef180c6145f930",
    "src/Billiards/rules/shot_fact_collector.h": "13453f30d0eeb7ce4231b1f5802eaf3317e085f4187e3e16abaff34cb171986c",
    "src/Billiards/rules/shot_fact_collector.cpp": "92b205a1a0025b21a47fbd86e755beceeb4f89208b5db28945ddfcdd1f7e5f7d",
    "tests/physics_tests.cpp": "f127de3a46b22c042c97c5a348e96b7edd55fe081f646c68f6390ad8674e20ac",
    "tests/surface_motion_tests.cpp": "c5a05e0d73bbadbe0c9b5443a6aef03df7049c1059ab17f5f2ea45da38f404ec",
    "tests/physics_telemetry_tests.cpp": "cbbb2eb4e5f1c1eb22c5525d0c1859e19aebf2a0d975b94321f6daf741ced0c8",
    "tests/shot_fact_collector_tests.cpp": "2b592431fdee204a6c7cfb976125d8697770a477a0749e7e7fcc332c66bbd401",
    "tests/chinese_eight_ball_rules_tests.cpp": "950a98501feca064cbaab8c4b1025b3b421dfcbc8b59a8f82739042069d7997c",
    "tests/rules_runtime_integration_tests.cpp": "f4f67a8f2b34b7c35ce8c555d12d7a4b65922c4f5710d21c8efe66408476152c",
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
    if value.get("schema") != "wpa_ii_17_a_e_off_table_physical_telemetry_design_gap_gate_v1":
        errors.append("schema")
    if value.get("status") != "TELEMETRY_AND_PHYSICS_PENDING_NO_PRODUCTION_MUTATION":
        errors.append("status")
    if value.get("scope") != ["WPA-II-17(a)", "WPA-II-17(e)"]:
        errors.append("scope")

    correction = value.get("requested_scope_locator_correction", {})
    if correction != {
        "request_named": "WPA-II-17(b)/(e) off-table physical telemetry",
        "official_cue_off_table_locator": "17(a)",
        "official_17_b_locator": "cue ball first contacts the wrong object ball",
        "silent_relabel_forbidden": True,
    }:
        errors.append("locator_correction")

    official = value.get("official_source", {})
    clauses = official.get("clauses", []) if isinstance(official, dict) else []
    conflict = official.get("object_ball_policy_conflict", {}) if isinstance(official, dict) else {}
    if not isinstance(official, dict) or official.get("path") != "tmp/pdfs/250816-Rules-of-Heyball.pdf" or \
            official.get("sha256") != PDF_SHA or official.get("visually_reviewed_pdf_pages") != [9, 11] or \
            not isinstance(clauses, list) or [x.get("clause") for x in clauses] != ["12.3", "17(a)", "17(e)"] or \
            [x.get("pdf_page") for x in clauses] != [9, 11, 11] or \
            conflict != {"clauses": ["12.3", "17(e)"], "status": "POLICY_PENDING",
                         "resolved_by_this_gate": False, "classification_or_adjudication_authorized": False}:
        errors.append("official_binding")
    elif check_files:
        path = resolve(official["path"])
        if path is None or not path.is_file() or sha256(path) != PDF_SHA:
            errors.append("official_pdf_drift")

    audit = value.get("frozen_audit", {})
    if not isinstance(audit, dict) or audit.get("path") != "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v5.json" or \
            audit.get("sha256") != AUDIT_SHA or audit.get("promotion_authorized") is not False or \
            audit.get("rows") != [
                {"id": "WPA-II-12-3", "status": "PARTIAL", "policy_decision_required": True},
                {"id": "WPA-II-17", "status": "PARTIAL", "policy_decision_required": True},
            ] or "must not be read as proof" not in audit.get("physical_telemetry_claim_supersession", ""):
        errors.append("audit_binding")
    elif check_files:
        path = resolve(audit["path"])
        if path is None or not path.is_file() or sha256(path) != AUDIT_SHA:
            errors.append("audit_drift")

    scratch = value.get("independent_cue_scratch_receipt", {})
    if scratch != {
        "path": "docs/rules/wpa-ii-17-a-cue-scratch-policy-independent-telemetry-implementation-receipt-v1.json",
        "sha256": SCRATCH_SHA,
        "scope": "real cue-ball pocket capture only",
        "off_table_extension_claimed": False,
        "must_remain_independent_and_unchanged": True,
    }:
        errors.append("scratch_receipt_binding")
    elif check_files:
        path = resolve(scratch["path"])
        if path is None or not path.is_file() or sha256(path) != SCRATCH_SHA:
            errors.append("scratch_receipt_drift")

    bindings = value.get("source_bindings", {})
    actual = {x.get("path"): x.get("sha256") for x in bindings.values()
              if isinstance(x, dict)} if isinstance(bindings, dict) else {}
    if actual != SOURCE_SHA or len(bindings) != len(SOURCE_SHA):
        errors.append("source_bindings")
    elif check_files:
        for raw, digest in SOURCE_SHA.items():
            path = resolve(raw)
            if path is None or not path.is_file() or sha256(path) != digest:
                errors.append(f"source_drift:{raw}")

    expected_mapping = {
        "production_motion_dimensions": "PLANAR_XZ_ONLY",
        "position_y_integrated": False, "velocity_y_integrated": False,
        "gravity_or_airborne_phase_exists": False, "rail_top_collision_volume_exists": False,
        "outside_table_landing_surface_exists": False, "leave_table_event_exists": False,
        "rail_top_return_event_exists": False, "off_table_rest_event_exists": False,
        "straight_rail_resolution_clamps_center_to_boundary": True,
        "pocket_mouth_is_only_production_passable_boundary_path": True,
        "pocket_topology_is_mouth_throat_capture": True,
        "pocket_capture_is_distinct_from_off_table": True,
        "object_capture_storage_coordinates_are_physical_off_table_evidence": False,
        "unbounded_mode_is_product_off_table_physics": False,
        "physics_frame_xyz_storage_proves_vertical_dynamics": False,
        "shot_facts_schema_has_injectable_off_table_fields": True,
        "shot_fact_collector_derives_cue_off_table": False,
        "shot_fact_collector_derives_object_off_table": False,
        "physical_off_table_observable_now": False,
    }
    if value.get("current_physics_mapping") != expected_mapping:
        errors.append("current_physics_mapping")

    boundary = value.get("capture_and_exclusion_boundary", {})
    expected_forbidden = {
        "manually placing a ball outside the x/z playfield rectangle",
        "using PhysicsBoundaryMode::Unbounded to let a planar ball wander beyond a boundary",
        "treating the cue-ball reset position after pocket capture as an off-table lifecycle",
        "treating an object's post-pocket storage coordinates as an off-table rest",
        "injecting cueBallOffTable or objectBallsOffTable through automation or a unit-test ShotFacts object",
        "treating inert position.y or velocity.y storage as airborne motion",
        "inferring departure from absence of a rail contact",
        "inferring off-table rest without a landing surface and terminal rest event",
    }
    if not isinstance(boundary, dict) or len(boundary.get("accepted_existing_physical_events", [])) != 5 or \
            not exact_list(boundary.get("forbidden_substitutes"), expected_forbidden) or \
            boundary.get("production_extension_safe_now") is not False or \
            "no three-dimensional departure" not in boundary.get("reason", ""):
        errors.append("capture_exclusion_boundary")

    physics = value.get("required_future_physics_contract", {})
    if not isinstance(physics, dict) or physics.get("status") != "PHYSICS_PENDING" or \
            len(physics.get("capabilities", [])) != 7 or physics.get("no_coordinate_only_shortcut") is not True:
        errors.append("future_physics_contract")

    telemetry = value.get("required_future_telemetry_contract", {})
    expected_lengths = {"event_identity": 6, "departure": 6, "rail_top_return": 6,
                        "outside_rest": 6, "exclusions": 5, "attestation": 6}
    if not isinstance(telemetry, dict) or telemetry.get("status") != "TELEMETRY_PENDING" or \
            telemetry.get("facts_are_measurements_not_legality_decisions") is not True or \
            any(len(telemetry.get(key, [])) != count for key, count in expected_lengths.items()):
        errors.append("future_telemetry_contract")

    pending = value.get("pending_classification_contract", {})
    cue = pending.get("cue_ball", {}) if isinstance(pending, dict) else {}
    obj = pending.get("object_ball", {}) if isinstance(pending, dict) else {}
    eight = pending.get("eight_ball", {}) if isinstance(pending, dict) else {}
    if cue != {"official_locator": "17(a)", "classification_requires_complete_physical_evidence": True,
               "pocket_scratch_remains_separate": True, "status": "TELEMETRY_AND_PHYSICS_PENDING"} or \
            obj != {"official_locators": ["12.3", "17(e)"], "classification_predicate": None,
                    "policy_conflict_status": "POLICY_PENDING", "telemetry_may_not_choose_the_policy": True,
                    "status": "POLICY_TELEMETRY_AND_PHYSICS_PENDING"} or \
            eight != {"existing_rules_path_unchanged": True, "no_new_loss_or_foul_semantics_authorized": True}:
        errors.append("pending_classification_contract")

    atomic = value.get("future_atomic_contract", {})
    if not isinstance(atomic, dict) or atomic.get("append_only_on_succeeded_physics_steps") is not True or \
            atomic.get("dedupe_key") != ["shot_id", "ball_id", "event_ordinal", "physics_tick", "solver_event_id"] or \
            "No departure" not in atomic.get("failed_step", "") or "same canonical evidence" not in atomic.get("exact_retry", "") or \
            "zero state" not in atomic.get("conflicting_retry", "") or atomic.get("partial_commit_allowed") is not False:
        errors.append("future_atomic_contract")

    fixtures = value.get("required_future_fixtures", {})
    if not isinstance(fixtures, dict) or len(fixtures.get("physics_positive", [])) != 4 or \
            len(fixtures.get("physics_negative", [])) != 10 or len(fixtures.get("classification_after_policy", [])) != 3 or \
            any(len(items) != len(set(items)) for items in fixtures.values() if isinstance(items, list)):
        errors.append("future_fixtures")

    allowlist = value.get("future_implementation_allowlist", {})
    production = allowlist.get("production_files", []) if isinstance(allowlist, dict) else []
    if not isinstance(allowlist, dict) or \
            allowlist.get("allowlist_is_dormant_until_physics_telemetry_and_object_policy_gates_are_approved") is not True or \
            allowlist.get("cmake_change_requires_separate_coordination") is not True or \
            "CMakeLists.txt" in production or "src/Billiards/render/modern/modern_renderer.cpp" in production:
        errors.append("future_allowlist")

    forbidden = {
        "production source edits in this design-gap task",
        "using injected or out-of-bounds coordinates as physical off-table evidence",
        "using unbounded test mode as product off-table physics",
        "using pocket capture or post-pocket storage coordinates as off-table evidence",
        "classifying incomplete departure without landing and terminal rest",
        "resolving the 12.3 versus 17(e) object-ball policy conflict",
        "changing cue-scratch receipt or pocket-capture semantics",
        "promoting WPA-II-12-3 or WPA-II-17 audit rows",
        "enabling global rulesDecisionUiProductExposed", "CMake changes",
        "Phase4 or Phase5 renderer changes", "GPU, Cocoa, or OpenGL execution",
        "readiness generation or authorization issuance",
    }
    if not exact_list(value.get("forbidden_scope"), forbidden):
        errors.append("forbidden_scope")

    expected_claims = {
        "design_gap_gate_complete": True, "official_pages_visually_reviewed": True,
        "production_boundary_and_pocket_paths_mapped": True,
        "physical_off_table_telemetry_available": False,
        "cue_off_table_fact_collector_complete": False,
        "object_off_table_fact_collector_complete": False,
        "rail_top_return_observable": False, "object_policy_conflict_resolved": False,
        "production_implementation_complete": False, "audit_row_promoted": False,
        "cue_scratch_receipt_modified": False, "cmake_modified": False,
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
    print("PASS_WPA_II_17_A_E_OFF_TABLE_PHYSICAL_TELEMETRY_DESIGN_GAP_GATE")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
