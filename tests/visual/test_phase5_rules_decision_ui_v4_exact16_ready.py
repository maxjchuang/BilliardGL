from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import prepare_phase5_rules_decision_ui_v4_exact16_ready as prep
from scripts import validate_phase5_rules_decision_ui_v4_exact16_ready as validator


class Phase5RulesDecisionUiV4Exact16ReadyTests(unittest.TestCase):
    def test_plan_is_exact_four_repairs_plus_twelve_controls(self) -> None:
        ids = prep.case_ids()
        gate = json.loads(prep.machine.GATE.read_text(encoding="utf-8"))[
            "future_targeted_actual_gpu_gate"]
        self.assertEqual(ids[:4], gate["repair_case_ids"])
        self.assertEqual(ids[4:], gate["regression_control_case_ids"])
        self.assertEqual(len(prep.plan_rows()), 16)

    def test_plan_profile_scale_coverage_is_4_8_4(self) -> None:
        scales = [row.split("\t")[10] for row in prep.plan_rows()]
        self.assertEqual(scales.count("0.85"), 4)
        self.assertEqual(scales.count("1.0"), 8)
        self.assertEqual(scales.count("1.35"), 4)

    def test_generated_readiness_if_present_is_valid(self) -> None:
        if not prep.READY_ROOT.exists():
            self.skipTest("fresh v4 readiness not generated yet")
        result = validator.validate(prep.READY_ROOT)
        self.assertEqual(result["case_count"], 16)
        self.assertFalse(result["gpu_authorized"])

    def test_validator_rejects_auth_promotion_if_present(self) -> None:
        if not prep.READY_ROOT.exists():
            self.skipTest("fresh v4 readiness not generated yet")
        with tempfile.TemporaryDirectory() as directory:
            copied = Path(directory) / "ready"
            copied.mkdir()
            for source in prep.READY_ROOT.iterdir():
                (copied / source.name).write_bytes(source.read_bytes())
            auth_path = copied / "rules-decision-ui-v4-authorization-template-not-issued.json"
            auth = json.loads(auth_path.read_text(encoding="utf-8"))
            auth["authorized"] = True
            auth_path.write_text(json.dumps(auth), encoding="utf-8")
            with self.assertRaises(RuntimeError):
                validator.validate(copied)

    def test_auth_template_is_explicitly_non_executable_if_present(self) -> None:
        if not prep.READY_ROOT.exists():
            self.skipTest("fresh v4 readiness not generated yet")
        auth = json.loads((prep.READY_ROOT /
            "rules-decision-ui-v4-authorization-template-not-issued.json").read_text())
        self.assertEqual(auth["status"], "NOT_ISSUED")
        self.assertFalse(auth["authorized"])
        self.assertFalse(auth["exact_shape_gpu_authorized"])
        self.assertFalse(auth["single_use_dispatch_implemented"])


if __name__ == "__main__":
    unittest.main()
