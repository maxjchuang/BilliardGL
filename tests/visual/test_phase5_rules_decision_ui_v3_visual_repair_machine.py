from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import run_phase5_rules_decision_ui_v3_visual_repair_machine as runner
from scripts import validate_phase5_rules_decision_ui_v3_visual_repair_machine as validator


class Phase5RulesDecisionUiV3VisualRepairMachineTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.temp = tempfile.TemporaryDirectory()
        cls.report = Path(cls.temp.name) / "report.json"
        runner.run(cls.report)
        cls.value = json.loads(cls.report.read_text(encoding="utf-8"))

    @classmethod
    def tearDownClass(cls) -> None:
        cls.temp.cleanup()

    def reject(self, mutation) -> None:
        value = copy.deepcopy(self.value)
        mutation(value)
        with self.assertRaises(RuntimeError):
            validator.validate(value)

    def test_540_case_report_passes(self) -> None:
        result = validator.validate(self.value)
        self.assertEqual(result["case_count"], 540)
        self.assertEqual(result["repair_state_case_count"], 180)

    def test_two_clean_runs_are_byte_reproducible(self) -> None:
        second = Path(self.temp.name) / "report-second.json"
        runner.run(second)
        self.assertEqual(self.report.read_bytes(), second.read_bytes())

    def test_gate_and_authority_drift_fail_closed(self) -> None:
        self.reject(lambda value: value.update(
            {"visual_repair_design_gate_sha256": "0" * 64}))
        self.reject(lambda value: value["v3_forbidden_authority_bindings"].pop(
            "src/Billiards/rules/group_choice_authority.h"))

    def test_machine_case_or_repair_coverage_drift_fails(self) -> None:
        self.reject(lambda value: value["cases"].pop())
        self.reject(lambda value: value["cases"][0].update(
            {"visual_repair_contract_passed": False}))

    def test_gpu_product_ta_or_promotion_claim_fails(self) -> None:
        for field in ("actual_gpu_authorized", "actual_gpu_claimed",
                      "golden_promotion", "m2_promotion", "product_exposed",
                      "technical_art_review_claimed"):
            with self.subTest(field=field):
                self.reject(lambda value, key=field:
                    value["v3_evidence_boundary"].update({key: True}))

    def test_hard_checks_cannot_be_removed(self) -> None:
        self.reject(lambda value: value["v3_visual_repair_checks"].pop())


if __name__ == "__main__":
    unittest.main()
