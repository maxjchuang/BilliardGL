from __future__ import annotations

import copy
import json
from pathlib import Path
import unittest

from scripts.validate_wpa_ii_04_official_rack_product_migration_gate import validate


ROOT = Path(__file__).resolve().parents[2]
GATE = ROOT / "docs/rules/wpa-ii-04-official-rack-product-migration-gate-v1.json"


class OfficialRackProductMigrationGateTests(unittest.TestCase):
    def setUp(self) -> None:
        self.value = json.loads(GATE.read_text(encoding="utf-8"))

    def test_frozen_gate_passes(self) -> None:
        self.assertEqual([], validate(self.value))

    def test_rejects_false_equivalence(self) -> None:
        bad = copy.deepcopy(self.value)
        bad["comparison"]["exact_position_mismatch_count"] = 0
        self.assertTrue(validate(bad, check_files=False))

    def test_rejects_same_group_corner_promotion(self) -> None:
        bad = copy.deepcopy(self.value)
        bad["comparison"]["current_base_corners_are_opposite_groups"] = True
        self.assertTrue(validate(bad, check_files=False))

    def test_rejects_number_order_normative_claim(self) -> None:
        bad = copy.deepcopy(self.value)
        bad["comparison"]["numbered_ball_order_is_official"] = True
        self.assertTrue(validate(bad, check_files=False))

    def test_rejects_historical_evidence_inheritance(self) -> None:
        for phase in (2, 4, 5):
            with self.subTest(phase=phase):
                bad = copy.deepcopy(self.value)
                bad["historical_visual_evidence"][f"phase{phase}_inheritable"] = True
                self.assertTrue(validate(bad, check_files=False))

    def test_rejects_ui_or_gpu_scope_expansion(self) -> None:
        bad = copy.deepcopy(self.value)
        bad["decision"]["rules_decision_ui_product_exposed"] = True
        self.assertTrue(validate(bad, check_files=False))
        bad = copy.deepcopy(self.value)
        bad["implementation_boundaries"]["forbidden"].remove("GPU execution")
        self.assertTrue(validate(bad, check_files=False))


if __name__ == "__main__":
    unittest.main()
