#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/rules/m4-d6-a-c08-authoritative-analytic-lineage-allowlist-correction-gate-v1.json"
SCHEMA = "m4_d6_a_c08_authoritative_analytic_lineage_allowlist_correction_gate_v1"
STATUS = "FINAL_DESIGN_ONLY_ROOT_REVALIDATION_REQUIRED_NO_PRODUCTION_AUTHORIZATION"
BINDINGS = [
    ("parent_same_tick_checklist", "docs/rules/m4-d6-a-legacy-d6-same-tick-composition-tests-first-acceptance-checklist-v1.json", "4b9abbce5e010194af942996dd963dcf25cc7f32c0e32dba5625cf2bd2683fea"),
    ("c09_correction_gate", "docs/rules/m4-d6-a-c09-final-validation-unreachable-path-correction-gate-v1.json", "9089cd6778764be0c0ff6b0fa8a3600e5fa3b14a31ab1bb265c8c55b9f2f88be"),
    ("composition_candidate_at_c08_blocker", "src/Billiards/physics.cpp", "8bdb0a11013700116c3cc3d86c242b0d82218de7303f40913a6dfb08605c7530"),
    ("d6_header_before_lineage", "src/Billiards/table_departure_geometry.h", "6ad680503270f70583d4f9417bb98147b3bf1e5a584a4a6c453c33f72331e781"),
    ("d6_implementation_before_lineage", "src/Billiards/table_departure_geometry.cpp", "82a969ea2f96393634564fad72839a1546493136db8a83cdceb2c140d895008d"),
    ("focused_test_at_c08_blocker", "tests/table_departure_geometry_tests.cpp", "70ce07f5db93d21a1bcda4392737674e7a982ce1ad5a4c797e9198630a35b801"),
    ("accepted_cmake_lineage", "CMakeLists.txt", "a227883fb85290a9cde3fff1ab33ab17d4a6d096e1d7fe7e726de64aba5408bc"),
]
FIELDS = ["ballIndex", "local segmentOrdinal", "wholeTickStartOffsetSeconds local to the D6 call", "durationSeconds", "startPositionCm", "endPositionCm", "startVelocityCmS", "constantAccelerationCmS2", "motionState", "analyticPathComplete"]
MUTATIONS = ["drop a no-event segment", "duplicate a segment", "insert a gap", "insert an overlap", "change ball identity", "change local start offset", "change duration", "change start position", "change end position", "change start velocity", "change acceleration", "mark analyticPathComplete false", "collapse moving and stationary supported pieces", "omit one cross-contact participant boundary", "rebase a recursive tail twice", "retain lineage on failure", "recompute a segment endpoint in physics.cpp", "publish a private D6 event as canonical contact or solver evidence"]
PROD = ["src/Billiards/physics.cpp", "src/Billiards/table_departure_geometry.h", "src/Billiards/table_departure_geometry.cpp"]
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
    require(value.get("decision") == "D6_MUTATOR_OWNS_PRIVATE_ANALYTIC_SEGMENTS_PHYSICS_ONLY_REBASES_AND_PUBLISHES_CANONICAL_LINEAGE", "decision")
    expected = [{"role": r, "path": p, "sha256": h} for r, p, h in BINDINGS]
    require(value.get("frozen_bindings") == expected, "bindings")
    if file_checks:
        for role, target, digest in BINDINGS:
            candidate = ROOT / target
            require(candidate.is_file() and sha(candidate) == digest, "binding_drift:" + role)

    blocker = value.get("blocker", {})
    require("two prefix analytic segments" in blocker.get("observed", "") and "incomplete attestation" in blocker.get("observed", ""), "blocker_observed")
    require("lacks per-interval" in blocker.get("why_physics_reconstruction_is_forbidden", "") and "duplicate D6" in blocker.get("why_physics_reconstruction_is_forbidden", ""), "blocker_reason")
    require("same mutation" in blocker.get("required_owner", ""), "required_owner")

    private = value.get("private_result_contract", {})
    require(private.get("field") == "TableDepartureStepResult::analyticSegments" and private.get("type") == "std::vector<SurfaceMotionAnalyticSegment>", "private_field")
    require(private.get("required_fields_exact") == FIELDS, "fields")
    require("not a canonical event" in private.get("scope", "") and "serialized public schema" in private.get("scope", ""), "private_scope")

    recording = value.get("authoritative_recording_contract", {})
    expected_keys = {"single_integrator", "interval_partition", "ballistic", "supported_piecewise", "event_boundaries", "cross_domain_boundary", "failure_discard", "determinism"}
    require(set(recording) == expected_keys, "recording_keys")
    for key, text in recording.items():
        require(isinstance(text, str) and len(text) > 100, "recording:" + key)
    require("no replay" in recording.get("single_integrator", "") and "no gaps or overlaps" in recording.get("interval_partition", ""), "single_partition")
    require("exact stop time" in recording.get("supported_piecewise", "") and "stationary interval" in recording.get("supported_piecewise", ""), "piecewise")
    require("both participants" in recording.get("cross_domain_boundary", "") and "rebased" in recording.get("cross_domain_boundary", ""), "cross")
    require("no analytic segments" in recording.get("failure_discard", "") and "exact input snapshot" in recording.get("failure_discard", ""), "failure")

    consumer = value.get("composition_consumer_contract", {})
    operations = consumer.get("physics_cpp_only_operations", [])
    require(len(operations) == 5 and "prefixDuration once" in operations[1] and "without recomputing" in operations[2] and "global segmentOrdinal" in operations[3], "consumer_ops")
    require("exactly once" in consumer.get("canonical_continuity", "") and "coveredSegmentCount equals" in consumer.get("canonical_continuity", ""), "continuity")
    require("remain absent" in consumer.get("publication_boundary", "") and "ShotFacts" in consumer.get("publication_boundary", ""), "publication")
    require("fails the whole composed tick" in consumer.get("fail_closed", "") and "empty canonical telemetry" in consumer.get("fail_closed", ""), "fail_closed")

    require(value.get("required_tests_first_mutations") == MUTATIONS, "mutations")
    allow = value.get("corrected_allowlist_after_root_revalidation", {})
    require(allow.get("production_files_exact") == PROD and allow.get("test_files_exact") == TESTS, "allowlist")
    for key in ("surface_motion_header_edit_allowed", "physics_header_edit_allowed", "canonical_telemetry_schema_edit_allowed", "solver_profile_threshold_or_formula_edit_allowed", "cmake_edit_allowed", "public_d6_event_or_consumer_edit_allowed"):
        require(allow.get(key) is False, "allowlist_false:" + key)
    boundary = value.get("acceptance_boundary", {})
    require(boundary.get("design_correction_complete") is True and boundary.get("root_revalidation_required") is True, "boundary_true")
    require(all(flag is False for key, flag in boundary.items() if key not in {"design_correction_complete", "root_revalidation_required"}), "boundary_false")
    return sorted(set(errors))


if __name__ == "__main__":
    target = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT
    result = validate(target)
    print(json.dumps({"valid": not result, "errors": result}, sort_keys=True))
    raise SystemExit(1 if result else 0)
