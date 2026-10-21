from __future__ import annotations

import unittest

from scripts import validate_phase5_rules_decision_ui_v4_exact16_capture_producer_implementation as validator


class Exact16CaptureProducerImplementationTests(unittest.TestCase):
    def test_live_static_implementation_passes(self) -> None:
        self.assertEqual(validator.validate()["case_count"], 16)

    def reject_plan(self, lines: list[str]) -> None:
        with self.assertRaises(RuntimeError):
            validator.validate_plan_lines(lines)

    def test_plan_15_rejects(self) -> None:
        self.reject_plan(validator.PLAN.read_text().splitlines()[:15])

    def test_plan_17_rejects(self) -> None:
        lines = validator.PLAN.read_text().splitlines()
        self.reject_plan(lines + [lines[-1]])

    def test_duplicate_rejects(self) -> None:
        lines = validator.PLAN.read_text().splitlines()
        self.reject_plan(lines[:-1] + [lines[0]])

    def test_reordered_rejects(self) -> None:
        lines = validator.PLAN.read_text().splitlines()
        lines[0], lines[1] = lines[1], lines[0]
        self.reject_plan(lines)

    def test_wrong_id_rejects(self) -> None:
        lines = validator.PLAN.read_text().splitlines()
        fields = lines[0].split("\t"); fields[0] = "wrong"
        lines[0] = "\t".join(fields)
        self.reject_plan(lines)

    def test_malformed_fields_rejects(self) -> None:
        lines = validator.PLAN.read_text().splitlines()
        lines[0] = "\t".join(lines[0].split("\t")[:-1])
        self.reject_plan(lines)

    def test_v3_and_default_contracts_remain_present(self) -> None:
        source = validator.RUNNER.read_text()
        self.assertIn("phase5_rules_ui_v3_plan_requires_exactly_28_cases", source)
        self.assertIn("phase5_rules_ui_plan_requires_60_cases", source)


if __name__ == "__main__":
    unittest.main()
