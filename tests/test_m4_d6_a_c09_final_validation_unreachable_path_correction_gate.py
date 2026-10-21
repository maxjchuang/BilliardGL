from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
VALIDATOR = ROOT / "scripts/validate_m4_d6_a_c09_final_validation_unreachable_path_correction_gate.py"
GATE = ROOT / "docs/rules/m4-d6-a-c09-final-validation-unreachable-path-correction-gate-v1.json"

spec = importlib.util.spec_from_file_location("c09_correction_validator", VALIDATOR)
assert spec and spec.loader
validator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(validator)


class C09CorrectionGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.canonical = json.loads(GATE.read_text(encoding="utf-8"))

    def validate_mutation(self, mutation: dict) -> list[str]:
        candidate = ROOT / "tmp/c09-correction-gate-mutation.json"
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
        add(lambda v: v.__setitem__("decision", "PRODUCTION_TEST_HOOK"))
        add(lambda v: v["frozen_bindings"].pop())
        add(lambda v: v["frozen_bindings"][2].__setitem__("sha256", "0" * 64))
        add(lambda v: v["proof_of_unreachability"].__setitem__("child_invariants", "weak"))
        add(lambda v: v["proof_of_unreachability"].__setitem__("merge_invariant", "weak"))
        add(lambda v: v["proof_of_unreachability"].__setitem__("conclusion", "inject"))
        for key in ("production_test_hook", "kinetic_only_gravity_failure", "mismatched_subrequest", "threshold_change"):
            add(lambda v, key=key: v["rejected_alternatives"].__setitem__(key, "Allowed"))
        add(lambda v: v["corrected_c09_acceptance"].__setitem__("runtime_failure_stages", "self reported"))
        add(lambda v: v["corrected_c09_acceptance"].__setitem__("final_gate_static_contract", "substring"))
        for index in range(8):
            add(lambda v, index=index: v["corrected_c09_acceptance"]["source_mutations_exact"].pop(index))
        add(lambda v: v["corrected_c09_acceptance"].__setitem__("gravity_runtime_control", "kinetic only"))
        add(lambda v: v["corrected_c09_acceptance"].__setitem__("same_request_success_control", "different request"))
        add(lambda v: v["future_tests_first_allowlist_after_root_revalidation"]["production_files_exact"].append("src/Billiards/physics.h"))
        add(lambda v: v["future_tests_first_allowlist_after_root_revalidation"]["test_files_exact"].pop())
        for key in ("production_injection_hook_allowed", "physics_header_or_public_api_edit_allowed", "table_departure_geometry_edit_allowed", "cmake_edit_allowed", "profile_or_threshold_edit_allowed"):
            add(lambda v, key=key: v["future_tests_first_allowlist_after_root_revalidation"].__setitem__(key, True))
        add(lambda v: v["acceptance_boundary"].__setitem__("tests_first_authorized_by_this_artifact", True))
        add(lambda v: v["acceptance_boundary"].__setitem__("production_edit_authorized_by_this_artifact", True))
        add(lambda v: v["acceptance_boundary"].__setitem__("gpu_readiness_capture_or_authorization", True))

        for index, mutation in enumerate(mutations):
            with self.subTest(index=index):
                self.assertTrue(self.validate_mutation(mutation))


if __name__ == "__main__":
    unittest.main()
