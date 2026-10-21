#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/rules/m4-d6-a-cross-domain-ball-ball-swept-contact-tests-first-acceptance-checklist-v1.json"
SCHEMA = "m4_d6_a_cross_domain_ball_ball_swept_contact_tests_first_acceptance_checklist_v1"
STATUS = "FINAL_DESIGN_ONLY_ROOT_REVALIDATION_REQUIRED_TESTS_FIRST_NOT_AUTHORIZED_NO_PRODUCTION_CHANGE"
BINDINGS = [
    ("d6_a_parent_design_gate", "docs/rules/m4-d6-a-3d-table-departure-physics-foundation-implementation-design-gate-v1.json", "676b70795fc45e4e3cc4f5c8ffa3a236c1e1e83aea9371e422ff9b54e87264d1"),
    ("d5_completed_rigid_contact_stage", "docs/rules/m4-d5-cue-contact-stage-completion-successor-v1.json", "c977989b6d1045218caca8a609dd4df0b39114a81fdc1992d53ea5f79da65d15"),
]
FIXTURES = [f"X{i:02d}_" for i in range(1, 11)]
PROD = ["src/Billiards/table_departure_geometry.h", "src/Billiards/table_departure_geometry.cpp", "src/Billiards/physics.cpp"]
TESTS = ["tests/table_departure_geometry_tests.cpp"]


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

    require(value.get("schema") == SCHEMA and value.get("schema_version") == 1 and value.get("status") == STATUS, "header")
    expected = [{"role": role, "path": target, "sha256": digest} for role, target, digest in BINDINGS]
    require(value.get("frozen_bindings") == expected, "bindings")
    if file_checks:
        for role, target, digest in BINDINGS:
            candidate = ROOT / target
            require(candidate.is_file() and sha(candidate) == digest, "binding_drift:" + role)

    decision = value.get("root_pm_decision", {})
    require(decision.get("selected") == "CROSS_DOMAIN_BALL_BALL_IS_REQUIRED_IN_D6_A", "decision")
    require(decision.get("silent_split_or_pass_through_allowed") is False and decision.get("discrete_end_overlap_only_allowed") is False, "decision_boundary")
    candidate = value.get("continuous_candidate_contract", {})
    for key in ("candidate_pair", "geometry", "time_basis", "motion_inputs", "root_selection", "candidate_order", "stable_identity", "same_toi_ambiguity", "initial_penetration"):
        require(isinstance(candidate.get(key), str) and candidate.get(key), "candidate:" + key)
    require("sum of radii" in candidate.get("geometry", "") and "absolute whole-tick" in candidate.get("time_basis", ""), "continuous_geometry")
    require("approach derivative is strictly negative" in candidate.get("root_selection", ""), "approach_root")
    require(candidate.get("broad_phase_or_coordinate_whitelist_may_decide_contact") is False, "coordinate_whitelist")

    resolution = value.get("resolution_and_atomicity_contract", {})
    require("existing rigid ball-ball" in resolution.get("resolver", ""), "resolver")
    require("angularVelocity" in resolution.get("linear_and_angular_state", "") and "physical domain" in resolution.get("linear_and_angular_state", ""), "state_inventory")
    require("may change angular state only" in resolution.get("angular_rule", "") and "may not reset" in resolution.get("angular_rule", ""), "angular")
    require("momentum" in resolution.get("momentum_and_energy", "") and "passive-energy" in resolution.get("momentum_and_energy", ""), "energy")
    require("restore the exact pre-tick GameState" in resolution.get("failed_tick", ""), "rollback")
    require("not appended to canonical PhysicsStepTelemetry" in resolution.get("private_only", ""), "private")

    fixtures = value.get("required_tests_first_red_fixtures_exact", [])
    ids = [row.get("id", "") for row in fixtures]
    require(len(fixtures) == 10 and len(set(ids)) == 10 and all(ids[i].startswith(FIXTURES[i]) for i in range(10)), "fixtures")
    require(all(isinstance(row.get("requirement"), str) and len(row["requirement"]) > 50 for row in fixtures), "fixture_substance")
    joined = " ".join(row.get("requirement", "") for row in fixtures)
    for term in ("cloth-supported", "rail-top-supported", "outside-landing-supported", "angular", "whole-tick", "rollback", "near-miss", "final overlap", "plane-observation", "renderer"):
        require(term in joined, "fixture_term:" + term)

    sequence = value.get("tests_first_and_implementation_sequence", [])
    require(len(sequence) == 5 and "honest RED" in sequence[0] and "before any production edit" in sequence[1] and "O0 and O2" in sequence[3] and "SOURCE_STABLE" in sequence[4], "sequence")
    allow = value.get("future_correction_allowlist_after_root_revalidation", {})
    require(allow.get("production_files_exact") == PROD and allow.get("test_files_exact") == TESTS, "allowlist")
    require(allow.get("cmake_edit_allowed") is False and allow.get("ball_ball_contact_or_contact_solver_source_edit_allowed") is False and allow.get("reuse_existing_contact_api_required") is True, "allowlist_boundary")
    boundary = value.get("acceptance_boundary", {})
    require(boundary.get("design_checklist_complete") is True and boundary.get("root_revalidation_required") is True, "design_boundary")
    require(all(flag is False for key, flag in boundary.items() if key not in {"design_checklist_complete", "root_revalidation_required"}), "false_claim")
    return sorted(set(errors))


if __name__ == "__main__":
    target = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT
    result = validate(target)
    print(json.dumps({"valid": not result, "errors": result}, sort_keys=True))
    raise SystemExit(1 if result else 0)
