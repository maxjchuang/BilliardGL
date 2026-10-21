#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/rules/m4-d6-a-c09-final-validation-unreachable-path-correction-gate-v1.json"
SCHEMA = "m4_d6_a_c09_final_validation_unreachable_path_correction_gate_v1"
STATUS = "FINAL_DESIGN_ONLY_ROOT_REVALIDATION_REQUIRED_NO_PRODUCTION_AUTHORIZATION"
BINDINGS = [
    ("parent_same_tick_checklist", "docs/rules/m4-d6-a-legacy-d6-same-tick-composition-tests-first-acceptance-checklist-v1.json", "4b9abbce5e010194af942996dd963dcf25cc7f32c0e32dba5625cf2bd2683fea"),
    ("parent_d6_a_gate", "docs/rules/m4-d6-a-3d-table-departure-physics-foundation-implementation-design-gate-v1.json", "676b70795fc45e4e3cc4f5c8ffa3a236c1e1e83aea9371e422ff9b54e87264d1"),
    ("rejected_gravity_ke_red", "tests/table_departure_geometry_tests.cpp", "92a7056825b959e7d8d94e24f3457a4e31db77efd35ac19006bdc03b3d04a73b"),
    ("production_before_correction", "src/Billiards/physics.cpp", "bad3d1ac32c398463ac3048b76245b4b988b4c45ff6ae05435e4a3c70148576f"),
    ("d6_implementation_before_correction", "src/Billiards/table_departure_geometry.cpp", "82a969ea2f96393634564fad72839a1546493136db8a83cdceb2c140d895008d"),
    ("accepted_cmake_lineage", "CMakeLists.txt", "a227883fb85290a9cde3fff1ab33ab17d4a6d096e1d7fe7e726de64aba5408bc"),
]
MUTATIONS = [
    "delete final validator call", "move final validator after state commit",
    "move final validator before tail merge", "delete snapshot restore",
    "retain any canonical telemetry ledger on failure",
    "replace gravity-aware accounting with kinetic-only comparison",
    "multiply tolerance by a private D6 event count",
    "add a production test hook or conditional bypass",
]


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
    require(value.get("decision") == "STATIC_FINAL_GATE_CONTRACT_PLUS_SOURCE_MUTATIONS_AND_GRAVITY_MECHANICAL_ENERGY_CONTROL", "decision")
    expected = [{"role": r, "path": p, "sha256": h} for r, p, h in BINDINGS]
    require(value.get("frozen_bindings") == expected, "bindings")
    if file_checks:
        for role, target, digest in BINDINGS:
            candidate = ROOT / target
            require(candidate.is_file() and sha(candidate) == digest, "binding_drift:" + role)

    proof = value.get("proof_of_unreachability", {})
    require("passed its finite-state" in proof.get("child_invariants", "") and "every BallState field" in proof.get("child_invariants", ""), "child_invariants")
    require("at most 16" in proof.get("merge_invariant", "") and "double cannot overflow" in proof.get("merge_invariant", ""), "merge_invariant")
    require("no honest reachable fixture" in proof.get("conclusion", "") and "production-resident injection seam" in proof.get("conclusion", ""), "conclusion")

    rejected = value.get("rejected_alternatives", {})
    require(set(rejected) == {"production_test_hook", "kinetic_only_gravity_failure", "mismatched_subrequest", "threshold_change"}, "rejected_keys")
    for key, text in rejected.items():
        require(isinstance(text, str) and text.startswith("Forbidden:") and len(text) > 90, "rejected:" + key)
    require("gravitational potential" in rejected.get("kinetic_only_gravity_failure", ""), "gravity_rejection")

    corrected = value.get("corrected_c09_acceptance", {})
    require("real public executions" in corrected.get("runtime_failure_stages", "") and "byte-exact" in corrected.get("runtime_failure_stages", ""), "runtime_stages")
    static = corrected.get("final_gate_static_contract", "")
    for term in ("extract the complete", "exactly one", "after successful", "before the only state commit", "restores the snapshot", "attestation ledger"):
        require(term in static, "static:" + term)
    require(corrected.get("source_mutations_exact") == MUTATIONS, "source_mutations")
    gravity = corrected.get("gravity_runtime_control", "")
    for term in ("K plus gravitational potential", "U=-m*g_y*y", "SI conversion", "unchanged public passive-energy tolerance", "kinetic energy is required to increase", "must succeed"):
        require(term in gravity, "gravity:" + term)
    same = corrected.get("same_request_success_control", "")
    for term in ("same initial state", "whole duration", "ProductionTable", "legacy prefix", "exact remaining D6 tail", "final validation/commit"):
        require(term in same, "same_request:" + term)

    allow = value.get("future_tests_first_allowlist_after_root_revalidation", {})
    require(allow.get("production_files_exact") == ["src/Billiards/physics.cpp"], "prod_allowlist")
    require(allow.get("test_files_exact") == ["tests/table_departure_geometry_tests.cpp", "tests/interactive_physics_tests.cpp"], "test_allowlist")
    require(all(allow.get(key) is False for key in (
        "production_injection_hook_allowed", "physics_header_or_public_api_edit_allowed",
        "table_departure_geometry_edit_allowed", "cmake_edit_allowed",
        "profile_or_threshold_edit_allowed")), "allowlist_false")
    boundary = value.get("acceptance_boundary", {})
    require(boundary.get("design_correction_complete") is True and boundary.get("root_revalidation_required") is True, "boundary_true")
    require(all(flag is False for key, flag in boundary.items() if key not in {"design_correction_complete", "root_revalidation_required"}), "boundary_false")
    return sorted(set(errors))


if __name__ == "__main__":
    target = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT
    result = validate(target)
    print(json.dumps({"valid": not result, "errors": result}, sort_keys=True))
    raise SystemExit(1 if result else 0)
