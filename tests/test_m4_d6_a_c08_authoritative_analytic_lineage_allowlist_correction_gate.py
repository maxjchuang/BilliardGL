from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
VALIDATOR = ROOT / "scripts/validate_m4_d6_a_c08_authoritative_analytic_lineage_allowlist_correction_gate.py"
GATE = ROOT / "docs/rules/m4-d6-a-c08-authoritative-analytic-lineage-allowlist-correction-gate-v1.json"

spec = importlib.util.spec_from_file_location("c08_lineage_validator", VALIDATOR)
assert spec and spec.loader
validator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(validator)


class C08LineageGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.canonical = json.loads(GATE.read_text(encoding="utf-8"))

    def validate_mutation(self, mutation: dict) -> list[str]:
        candidate = ROOT / "tmp/c08-lineage-gate-mutation.json"
        candidate.parent.mkdir(exist_ok=True)
        candidate.write_text(json.dumps(mutation), encoding="utf-8")
        try:
            return validator.validate(candidate, file_checks=False)
        finally:
            candidate.unlink(missing_ok=True)

    def test_canonical(self) -> None:
        self.assertEqual(validator.validate(GATE), [])

    def test_fail_closed_mutations(self) -> None:
        mutations = []

        def add(edit) -> None:
            value = copy.deepcopy(self.canonical)
            edit(value)
            mutations.append(value)

        add(lambda v: v.__setitem__("schema", "wrong"))
        add(lambda v: v.__setitem__("status", "IMPLEMENTED"))
        add(lambda v: v.__setitem__("decision", "PHYSICS_RECONSTRUCTS"))
        add(lambda v: v["frozen_bindings"].pop())
        add(lambda v: v["frozen_bindings"][2].__setitem__("sha256", "0" * 64))
        add(lambda v: v["blocker"].__setitem__("observed", "weak"))
        add(lambda v: v["blocker"].__setitem__("why_physics_reconstruction_is_forbidden", "allowed"))
        add(lambda v: v["private_result_contract"].__setitem__("field", "PhysicsStepTelemetry::events"))
        add(lambda v: v["private_result_contract"]["required_fields_exact"].pop())
        for key in ("single_integrator", "interval_partition", "ballistic", "supported_piecewise", "event_boundaries", "cross_domain_boundary", "failure_discard", "determinism"):
            add(lambda v, key=key: v["authoritative_recording_contract"].__setitem__(key, "weak"))
        for index in range(5):
            add(lambda v, index=index: v["composition_consumer_contract"]["physics_cpp_only_operations"].pop(index))
        add(lambda v: v["composition_consumer_contract"].__setitem__("canonical_continuity", "gaps allowed"))
        add(lambda v: v["composition_consumer_contract"].__setitem__("publication_boundary", "publish D6 events"))
        add(lambda v: v["composition_consumer_contract"].__setitem__("fail_closed", "best effort"))
        for index in range(18):
            add(lambda v, index=index: v["required_tests_first_mutations"].pop(index))
        add(lambda v: v["corrected_allowlist_after_root_revalidation"]["production_files_exact"].append("src/Billiards/physics.h"))
        add(lambda v: v["corrected_allowlist_after_root_revalidation"]["test_files_exact"].pop())
        for key in ("surface_motion_header_edit_allowed", "physics_header_edit_allowed", "canonical_telemetry_schema_edit_allowed", "solver_profile_threshold_or_formula_edit_allowed", "cmake_edit_allowed", "public_d6_event_or_consumer_edit_allowed"):
            add(lambda v, key=key: v["corrected_allowlist_after_root_revalidation"].__setitem__(key, True))
        add(lambda v: v["acceptance_boundary"].__setitem__("tests_first_authorized_by_this_artifact", True))
        add(lambda v: v["acceptance_boundary"].__setitem__("production_edit_authorized_by_this_artifact", True))
        add(lambda v: v["acceptance_boundary"].__setitem__("gpu_readiness_capture_or_authorization", True))

        for index, mutation in enumerate(mutations):
            with self.subTest(index=index):
                self.assertTrue(self.validate_mutation(mutation))


if __name__ == "__main__":
    unittest.main()
