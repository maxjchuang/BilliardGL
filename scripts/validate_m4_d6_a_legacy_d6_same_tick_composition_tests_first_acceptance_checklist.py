#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/rules/m4-d6-a-legacy-d6-same-tick-composition-tests-first-acceptance-checklist-v1.json"
SCHEMA = "m4_d6_a_legacy_d6_same_tick_composition_tests_first_acceptance_checklist_v1"
STATUS = "FINAL_DESIGN_ONLY_ROOT_REVALIDATION_REQUIRED_TESTS_FIRST_NOT_AUTHORIZED_NO_PRODUCTION_CHANGE"
BINDINGS = [
    ("d6_a_parent_design_gate", "docs/rules/m4-d6-a-3d-table-departure-physics-foundation-implementation-design-gate-v1.json", "676b70795fc45e4e3cc4f5c8ffa3a236c1e1e83aea9371e422ff9b54e87264d1"),
    ("d6_a_cross_domain_tests_first_checklist", "docs/rules/m4-d6-a-cross-domain-ball-ball-swept-contact-tests-first-acceptance-checklist-v1.json", "18e32ac086c6985d82dc8297f78758bb0c7734cbe60bbb66f432b6605dc303c1"),
    ("focused_d6_a_cpu_test_source", "tests/table_departure_geometry_tests.cpp", "f5998396e9b402dda6feec4db4b11e21f3d50fb0d5cf087c8e6dfd1074ff9478"),
    ("focused_d6_a_implementation", "src/Billiards/table_departure_geometry.cpp", "82a969ea2f96393634564fad72839a1546493136db8a83cdceb2c140d895008d"),
    ("legacy_composition_source_before_correction", "src/Billiards/physics.cpp", "bad3d1ac32c398463ac3048b76245b4b988b4c45ff6ae05435e4a3c70148576f"),
    ("interactive_120hz_control_source", "tests/interactive_physics_tests.cpp", "795ed009981b9ba9e0ba327d73d1b3f06e8e143b5739ffb10382058b2e087396"),
    ("accepted_parent_cmake_lineage", "CMakeLists.txt", "a227883fb85290a9cde3fff1ab33ab17d4a6d096e1d7fe7e726de64aba5408bc"),
]
FIXTURES = [f"C{i:02d}_" for i in range(1, 13)]
PROD = ["src/Billiards/physics.cpp"]
TESTS = ["tests/table_departure_geometry_tests.cpp", "tests/interactive_physics_tests.cpp"]


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate(path: Path = DEFAULT, file_checks: bool = True) -> list[str]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except Exception as error:
        return [f"json_invalid:{error}"]
    errors: list[str] = []

    def require(condition: bool, message: str) -> None:
        if not condition:
            errors.append(message)

    require(value.get("schema") == SCHEMA and value.get("schema_version") == 1, "schema")
    require(value.get("status") == STATUS, "status")
    scope = value.get("scope", "")
    for term in ("exact contact TOI", "unconsumed same-tick tail", "does not weaken", "does not", "public-schema"):
        require(term in scope, "scope:" + term)
    expected = [{"role": role, "path": target, "sha256": digest} for role, target, digest in BINDINGS]
    require(value.get("frozen_bindings") == expected, "bindings")
    if file_checks:
        for role, target, digest in BINDINGS:
            candidate = ROOT / target
            require(candidate.is_file() and sha(candidate) == digest, "binding_drift:" + role)

    blocker = value.get("reproducible_blocker", {})
    require(blocker.get("first_failure_tick") == 180 and blocker.get("ball_index") == 15, "blocker_identity")
    require(blocker.get("entry_domain") == "Airborne" and blocker.get("mapped_failure") == "InvalidControls from private D6-A AmbiguousInitialPenetration", "blocker_failure")
    require(blocker.get("entry_position_cm") == [11.5624, 89.3074, 83.5078] and blocker.get("entry_velocity_cm_s") == [10.1418, -8.17221, 16.1194], "blocker_state")
    require("late in tick 179" in blocker.get("root_cause", "") and "Tick 180 correctly rejects" in blocker.get("root_cause", ""), "blocker_cause")

    contract = value.get("selected_composition_contract", {})
    require(contract.get("model") == "EVENT_TOI_HANDOFF_OF_UNCONSUMED_TAIL", "model")
    for key in ("handoff_trigger", "handoff_state", "tail_duration", "candidate_order", "single_integration", "no_end_state_repair", "initial_guard", "implementation_shape"):
        require(isinstance(contract.get(key), str) and len(contract[key]) > 50, "contract:" + key)
    require("minus the exact handoff TOI" in contract.get("tail_duration", "") and "never the original whole-tick" in contract.get("tail_duration", ""), "tail_duration")
    require("exactly once" in contract.get("single_integration", "") and "may not replay" in contract.get("single_integration", ""), "single_integration")
    for forbidden in ("tick-end clamp", "velocity-y zeroing", "next-tick recovery"):
        require(forbidden in contract.get("no_end_state_repair", ""), "repair_negative:" + forbidden)

    atomic = value.get("atomicity_and_telemetry_contract", {})
    for key in ("staging", "success_commit", "failure_rollback", "legacy_contact_telemetry", "d6_private", "surface_and_plane_lineage", "no_handoff_control", "energy"):
        require(isinstance(atomic.get(key), str) and len(atomic[key]) > 50, "atomic:" + key)
    require("exact pre-tick" in atomic.get("failure_rollback", "") and "no partial success telemetry" in atomic.get("failure_rollback", ""), "rollback")
    require("exactly once" in atomic.get("legacy_contact_telemetry", "") and "independently executed prefix" in atomic.get("legacy_contact_telemetry", ""), "telemetry_prefix")
    require("not appended to canonical PhysicsStepTelemetry" in atomic.get("d6_private", ""), "private")

    fixtures = value.get("required_tests_first_red_fixtures_exact", [])
    ids = [row.get("id", "") for row in fixtures]
    require(len(fixtures) == 12 and len(set(ids)) == 12 and all(ids[i].startswith(FIXTURES[i]) for i in range(12)), "fixtures")
    require(all(isinstance(row.get("requirement"), str) and len(row["requirement"]) > 80 for row in fixtures), "fixture_substance")
    joined = " ".join(row.get("requirement", "") for row in fixtures)
    for term in ("negative vertical velocity", "positive vertical velocity", "byte-equivalent", "whole-tick", "same absolute TOI", "EventBudget", "field-for-field", "segment ordinals", "every staged GameState field", "clamp y", "20000 ticks", "renderer"):
        require(term in joined, "fixture_term:" + term)

    sequence = value.get("tests_first_sequence", [])
    require(len(sequence) == 5 and "honest RED" in sequence[0] and "before production authorization" in sequence[1] and "physics.cpp" in sequence[2] and "O0/O2" in sequence[3] and "separate independent acceptance" in sequence[4], "sequence")
    allow = value.get("future_correction_allowlist_after_root_revalidation", {})
    require(allow.get("production_files_exact") == PROD and allow.get("test_files_exact") == TESTS, "allowlist")
    for key in ("table_departure_geometry_edit_allowed", "physics_header_or_public_api_edit_allowed", "ball_ball_contact_or_solver_edit_allowed", "cmake_edit_allowed", "profile_threshold_or_formula_edit_allowed"):
        require(allow.get(key) is False, "allowlist_boundary:" + key)
    boundary = value.get("acceptance_boundary", {})
    require(boundary.get("design_checklist_complete") is True and boundary.get("root_revalidation_required") is True, "boundary_true")
    require(all(flag is False for key, flag in boundary.items() if key not in {"design_checklist_complete", "root_revalidation_required"}), "boundary_false")
    return sorted(set(errors))


if __name__ == "__main__":
    target = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT
    result = validate(target)
    print(json.dumps({"valid": not result, "errors": result}, sort_keys=True))
    raise SystemExit(1 if result else 0)
