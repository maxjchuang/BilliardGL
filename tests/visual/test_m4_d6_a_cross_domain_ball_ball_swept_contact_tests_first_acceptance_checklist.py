#!/usr/bin/env python3
from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import validate_m4_d6_a_cross_domain_ball_ball_swept_contact_tests_first_acceptance_checklist as validator


class CrossDomainChecklistTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(validator.DEFAULT.read_text(encoding="utf-8"))

    def reject(self, mutation) -> None:
        value = copy.deepcopy(self.value)
        mutation(value)
        with tempfile.TemporaryDirectory(prefix="d6a-cross-domain-") as directory:
            path = Path(directory) / "gate.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            self.assertTrue(validator.validate(path, False))

    def test_canonical(self) -> None:
        self.assertEqual(validator.validate(), [])

    def test_mutations(self) -> None:
        mutations = [
            lambda d: d.update(status="IMPLEMENTED"),
            lambda d: d["frozen_bindings"][0].update(sha256="0" * 64),
            lambda d: d["root_pm_decision"].update(silent_split_or_pass_through_allowed=True),
            lambda d: d["root_pm_decision"].update(discrete_end_overlap_only_allowed=True),
            lambda d: d["continuous_candidate_contract"].update(geometry="endpoint overlap"),
            lambda d: d["continuous_candidate_contract"].update(root_selection="first sample"),
            lambda d: d["continuous_candidate_contract"].update(broad_phase_or_coordinate_whitelist_may_decide_contact=True),
            lambda d: d["resolution_and_atomicity_contract"].update(resolver="D6 bounce constant"),
            lambda d: d["resolution_and_atomicity_contract"].update(angular_rule="reset spin"),
            lambda d: d["resolution_and_atomicity_contract"].update(failed_tick="partial commit"),
            lambda d: d["resolution_and_atomicity_contract"].update(private_only="publish telemetry"),
            lambda d: d["required_tests_first_red_fixtures_exact"].pop(),
            lambda d: d["required_tests_first_red_fixtures_exact"][0].update(id="X02_DUPLICATE"),
            lambda d: d["required_tests_first_red_fixtures_exact"][7].update(requirement="final overlap is enough"),
            lambda d: d["tests_first_and_implementation_sequence"].pop(0),
            lambda d: d["future_correction_allowlist_after_root_revalidation"]["production_files_exact"].append("src/Billiards/physics_telemetry.cpp"),
            lambda d: d["future_correction_allowlist_after_root_revalidation"].update(cmake_edit_allowed=True),
            lambda d: d["future_correction_allowlist_after_root_revalidation"].update(ball_ball_contact_or_contact_solver_source_edit_allowed=True),
            lambda d: d["acceptance_boundary"].update(tests_first_red_authorized_by_this_artifact=True),
            lambda d: d["acceptance_boundary"].update(production_edit_authorized_by_this_artifact=True),
            lambda d: d["acceptance_boundary"].update(d6_a_source_stable=True),
            lambda d: d["acceptance_boundary"].update(gpu_readiness_capture_or_authorization=True),
        ]
        for mutation in mutations:
            with self.subTest(mutation=mutation):
                self.reject(mutation)


if __name__ == "__main__":
    unittest.main()
