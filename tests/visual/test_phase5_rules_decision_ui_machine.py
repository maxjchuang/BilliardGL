from __future__ import annotations

import copy
import json
import unittest
from pathlib import Path

from scripts.generate_phase5_rules_decision_ui_evidence_template import build
from scripts.validate_phase5_rules_decision_ui_machine import validate


ROOT = Path(__file__).resolve().parents[2]
REPORT = ROOT / "docs/evidence/phase5/rules-decision-ui-machine-v1.json"
TEMPLATE = ROOT / "docs/evidence/phase5/rules-decision-ui-gpu-manual-template-v1.json"


class Phase5RulesDecisionUiMachineTests(unittest.TestCase):
    def test_committed_machine_report_is_exhaustive_and_fail_closed(self):
        value = json.loads(REPORT.read_text(encoding="utf-8"))
        self.assertEqual(validate(value), [])
        self.assertEqual(value["totals"]["machine_passed"], 540)
        self.assertEqual(value["totals"]["actual_gpu_completed"], 0)
        self.assertEqual(value["totals"]["manual_completed"], 0)
        self.assertEqual(value["totals"]["beta_completed"], 0)

    def test_machine_report_cannot_claim_external_evidence_or_policy_resolution(self):
        value = json.loads(REPORT.read_text(encoding="utf-8"))
        value["totals"]["actual_gpu_completed"] = 60
        value["evidence_boundaries"]["actual_gpu_claimed"] = True
        value["off_table_12_3_vs_17_e_status"] = "RESOLVED"
        errors = validate(value)
        self.assertTrue(any("totals differ" in error for error in errors))
        self.assertTrue(any("forbidden GPU" in error for error in errors))
        self.assertTrue(any("unresolved policy" in error for error in errors))

    def test_missing_or_duplicate_matrix_case_is_rejected(self):
        value = json.loads(REPORT.read_text(encoding="utf-8"))
        value["cases"][-1] = copy.deepcopy(value["cases"][0])
        errors = validate(value)
        self.assertTrue(any("unique" in error for error in errors))
        self.assertTrue(any("cross product differs" in error for error in errors))

    def test_gpu_manual_template_is_exactly_60_pending_rows(self):
        value = json.loads(TEMPLATE.read_text(encoding="utf-8"))
        self.assertEqual(value, build())
        self.assertEqual(value["case_count"], 60)
        self.assertFalse(value["actual_gpu_claimed"])
        self.assertFalse(value["manual_review_claimed"])
        self.assertTrue(all(case["production_core41_after_tonemap_png"] is None
                            and case["independent_manual_review"] ==
                            "PENDING_REAL_REVIEWER"
                            for case in value["cases"]))


if __name__ == "__main__":
    unittest.main()
