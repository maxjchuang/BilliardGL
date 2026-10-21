from __future__ import annotations

import copy
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from scripts.validate_phase5_rules_decision_ui_v2_gpu_evidence import (  # noqa: E402
    DEFAULT_RECEIPT,
    load_inputs,
    validate,
    validate_structures,
)


class Phase5RulesDecisionUiV2GpuEvidenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.receipt, cls.plan, cls.rows = load_inputs(DEFAULT_RECEIPT)

    def assert_rejected(self, receipt=None, plan=None, rows=None) -> None:
        with self.assertRaises(RuntimeError):
            validate_structures(
                receipt if receipt is not None else self.receipt,
                plan if plan is not None else self.plan,
                rows if rows is not None else self.rows)

    def test_committed_evidence_is_fully_valid(self) -> None:
        validate(DEFAULT_RECEIPT)

    def test_duplicate_capture_id_rejects(self) -> None:
        rows = copy.deepcopy(self.rows)
        rows[1]["capture_id"] = rows[0]["capture_id"]
        self.assert_rejected(rows=rows)

    def test_plan_order_drift_rejects(self) -> None:
        plan = copy.deepcopy(self.plan)
        plan[0], plan[1] = plan[1], plan[0]
        self.assert_rejected(plan=plan)

    def test_gpu_or_after_tonemap_false_rejects(self) -> None:
        for field in ("actual_gpu_capture", "rules_ui_after_tone_map"):
            rows = copy.deepcopy(self.rows)
            rows[0][field] = False
            self.assert_rejected(rows=rows)

    def test_canonical_or_draw_mismatch_rejects(self) -> None:
        rows = copy.deepcopy(self.rows)
        rows[0]["gpu_canonical_sha256"] = "0" * 64
        self.assert_rejected(rows=rows)
        rows = copy.deepcopy(self.rows)
        rows[0]["gpu_draw_calls"] = 2
        self.assert_rejected(rows=rows)

    def test_render_pass_order_rejects(self) -> None:
        rows = copy.deepcopy(self.rows)
        rows[0]["render_passes"][-2:] = reversed(rows[0]["render_passes"][-2:])
        self.assert_rejected(rows=rows)

    def test_premature_claim_rejects(self) -> None:
        receipt = copy.deepcopy(self.receipt)
        receipt["unclaimed"]["product_exposed"] = True
        self.assert_rejected(receipt=receipt)

    def test_manual_count_or_status_promotion_rejects(self) -> None:
        receipt = copy.deepcopy(self.receipt)
        receipt["counts"]["independent_manual_reviews_completed"] = 60
        self.assert_rejected(receipt=receipt)
        receipt = copy.deepcopy(self.receipt)
        receipt["status"] = "PASS"
        self.assert_rejected(receipt=receipt)


if __name__ == "__main__":
    unittest.main()
