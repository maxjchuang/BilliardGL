#!/usr/bin/env python3
"""Fail-closed mutation tests for the WPA rack visual rebaseline gate."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import validate_wpa_official_fixed_seed_rack_visual_rebaseline_design_gate as validator


ROOT = Path(__file__).resolve().parents[2]
GATE = ROOT / "docs/art/wpa-official-fixed-seed-rack-visual-rebaseline-design-gate-v1.json"


class OfficialRackVisualRebaselineGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(GATE.read_text(encoding="utf-8"))

    def errors_for(self, value: dict) -> list[str]:
        with tempfile.TemporaryDirectory(prefix="rack-rebaseline-gate-") as directory:
            path = Path(directory) / "gate.json"
            path.write_text(json.dumps(value, ensure_ascii=False), encoding="utf-8")
            return validator.validate(path)

    def assert_rejected(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(self.errors_for(value))

    def test_committed_gate_passes(self) -> None:
        self.assertEqual(validator.validate(GATE), [])

    def test_rejects_migration_gate_sha_drift(self) -> None:
        self.assert_rejected(lambda value: value["frozen_bindings"]
                             ["migration_gate"].update(sha256="0" * 64))

    def test_rejects_production_sha_drift(self) -> None:
        self.assert_rejected(lambda value: value["post_migration_production_bindings"]
                             ["product_match_commit"].update(sha256="0" * 64))

    def test_rejects_missing_ball(self) -> None:
        self.assert_rejected(lambda value:
                             value["ball_id_position_comparison"].pop())

    def test_rejects_wrong_fixed_seed_position(self) -> None:
        def mutate(value: dict) -> None:
            value["ball_id_position_comparison"][7]["official_position_cm"][0] = 0.25
        self.assert_rejected(mutate)

    def test_rejects_seeded_order_promoted_to_official(self) -> None:
        self.assert_rejected(lambda value: value["official_contract_semantics"]
                             .update(numbered_ball_order_is_official=True))

    def test_rejects_same_group_corner_claim(self) -> None:
        self.assert_rejected(lambda value: value["coordinate_contract"]
                             ["comparison_summary"].update(
                                 official_base_corners_are_opposite_groups=False))

    def test_rejects_historical_inheritance(self) -> None:
        self.assert_rejected(lambda value:
                             value["evidence_inheritance"].update(phase2=True))

    def test_rejects_phase2_case_omission(self) -> None:
        self.assert_rejected(lambda value: value["affected_case_inventory"]
                             ["phase2_full_six"]["case_ids_in_order"].pop())

    def test_rejects_phase4_false_direct_rack_claim(self) -> None:
        self.assert_rejected(lambda value: value["affected_case_inventory"]
                             ["phase4_targeted5"].update(
                                 direct_rack_texture_or_composition_impact=True))

    def test_rejects_phase5_case_omission(self) -> None:
        self.assert_rejected(lambda value: value["affected_case_inventory"]
                             ["phase5_exact16"]["directly_affected_case_ids"].pop())

    def test_rejects_reduced_capture_minimum(self) -> None:
        self.assert_rejected(lambda value: value["minimum_fresh_shared_build_rebaseline"]
                             .update(minimum_total_capture_count=26))

    def test_rejects_missing_rack_axis(self) -> None:
        self.assert_rejected(lambda value: value["original_resolution_ta_contract"]
                             ["canonical_rack_axes_in_order"].pop())

    def test_rejects_threshold_workaround(self) -> None:
        self.assert_rejected(lambda value: value["unchanged_machine_boundaries"]
                             .update(roi_thresholds_may_change=True))

    def test_rejects_prefilled_future_receipt(self) -> None:
        def mutate(value: dict) -> None:
            receipt = value["future_evidence"]["phase2_machine_receipt"]
            receipt.update(status="PASS", path="future.json", sha256="1" * 64)
        self.assert_rejected(mutate)

    def test_rejects_gpu_or_acceptance_claim(self) -> None:
        def mutate(value: dict) -> None:
            value["evidence_boundary"]["gpu_executed"] = True
            value["evidence_boundary"]["ta_acceptance"] = True
        self.assert_rejected(mutate)


if __name__ == "__main__":
    unittest.main()
