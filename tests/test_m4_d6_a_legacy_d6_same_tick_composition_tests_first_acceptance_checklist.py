from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
VALIDATOR = ROOT / "scripts/validate_m4_d6_a_legacy_d6_same_tick_composition_tests_first_acceptance_checklist.py"
GATE = ROOT / "docs/rules/m4-d6-a-legacy-d6-same-tick-composition-tests-first-acceptance-checklist-v1.json"

spec = importlib.util.spec_from_file_location("d6_composition_validator", VALIDATOR)
assert spec and spec.loader
validator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(validator)


class D6CompositionGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.canonical = json.loads(GATE.read_text(encoding="utf-8"))

    def validate_mutation(self, mutation: dict) -> list[str]:
        candidate = ROOT / "tmp" / "d6-composition-gate-mutation.json"
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
        add(lambda v: v["frozen_bindings"].pop())
        add(lambda v: v["frozen_bindings"][4].__setitem__("sha256", "0" * 64))
        add(lambda v: v["reproducible_blocker"].__setitem__("first_failure_tick", 179))
        add(lambda v: v["reproducible_blocker"].__setitem__("ball_index", 0))
        add(lambda v: v["reproducible_blocker"].__setitem__("entry_domain", "ClothSupported"))
        add(lambda v: v["reproducible_blocker"].__setitem__("entry_position_cm", [0, 0, 0]))
        add(lambda v: v["selected_composition_contract"].__setitem__("model", "TICK_END_CLAMP"))
        add(lambda v: v["selected_composition_contract"].__setitem__("tail_duration", "replay dt"))
        add(lambda v: v["selected_composition_contract"].__setitem__("single_integration", "double integrate"))
        add(lambda v: v["selected_composition_contract"].__setitem__("no_end_state_repair", "allow clamp"))
        add(lambda v: v["selected_composition_contract"].__setitem__("initial_guard", "weaken"))
        add(lambda v: v["atomicity_and_telemetry_contract"].__setitem__("failure_rollback", "partial"))
        add(lambda v: v["atomicity_and_telemetry_contract"].__setitem__("legacy_contact_telemetry", "duplicate"))
        add(lambda v: v["atomicity_and_telemetry_contract"].__setitem__("d6_private", "publish D6"))
        add(lambda v: v["required_tests_first_red_fixtures_exact"].pop())
        add(lambda v: v["required_tests_first_red_fixtures_exact"][0].__setitem__("id", "C02_DUPLICATE"))
        add(lambda v: v["required_tests_first_red_fixtures_exact"][0].__setitem__("requirement", "weak"))
        add(lambda v: v["required_tests_first_red_fixtures_exact"][6].__setitem__("requirement", "x" * 100))
        add(lambda v: v["tests_first_sequence"].pop())
        add(lambda v: v["tests_first_sequence"].__setitem__(2, ""))
        add(lambda v: v["future_correction_allowlist_after_root_revalidation"]["production_files_exact"].append("src/Billiards/physics.h"))
        add(lambda v: v["future_correction_allowlist_after_root_revalidation"]["test_files_exact"].pop())
        add(lambda v: v["future_correction_allowlist_after_root_revalidation"].__setitem__("cmake_edit_allowed", True))
        add(lambda v: v["future_correction_allowlist_after_root_revalidation"].__setitem__("table_departure_geometry_edit_allowed", True))
        add(lambda v: v["future_correction_allowlist_after_root_revalidation"].__setitem__("profile_threshold_or_formula_edit_allowed", True))
        add(lambda v: v["acceptance_boundary"].__setitem__("production_edit_authorized_by_this_artifact", True))
        add(lambda v: v["acceptance_boundary"].__setitem__("gpu_readiness_capture_or_authorization", True))
        add(lambda v: v["acceptance_boundary"].__setitem__("d6_a_source_stable", True))

        for index, mutation in enumerate(mutations):
            with self.subTest(index=index):
                self.assertTrue(self.validate_mutation(mutation))


if __name__ == "__main__":
    unittest.main()
