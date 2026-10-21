#!/usr/bin/env python3
"""Fail-closed tests for the Phase4 V19 orientation correction gate."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import validate_phase4_v19_landscape_orientation_layout_interpretation_correction_gate as validator


ROOT = Path(__file__).resolve().parents[1]
GATE = ROOT / "docs/art/phase4-v19-landscape-orientation-layout-interpretation-correction-gate-v1.json"


class V19LandscapeOrientationCorrectionGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(GATE.read_text(encoding="utf-8"))

    def errors_for(self, value: dict) -> list[str]:
        with tempfile.TemporaryDirectory(prefix="v19-orientation-correction-") as directory:
            path = Path(directory) / "gate.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            return validator.validate(path)

    def reject(self, mutation) -> None:
        value = copy.deepcopy(self.value)
        mutation(value)
        self.assertTrue(self.errors_for(value))

    def test_committed_gate_passes(self) -> None:
        self.assertEqual(validator.validate(GATE), [])

    def test_rejects_original_gate_drift(self) -> None:
        self.reject(lambda v: v["frozen_original_gate"].update(sha256="0" * 64))

    def test_rejects_contradiction_as_frozen_pass(self) -> None:
        self.reject(lambda v: v["nonfinal_contradiction_binding"].update(implementation_result_inferred=True))

    def test_rejects_root_choice_drift(self) -> None:
        self.reject(lambda v: v["root_orientation_decision_binding"].update(option="PORTRAIT"))

    def test_rejects_original_gate_without_correction(self) -> None:
        self.reject(lambda v: v["supersession_contract"].update(original_gate_may_be_used_without_this_correction=True))

    def test_rejects_head_bottom(self) -> None:
        self.reject(lambda v: v["corrected_orientation_contract"].update(head_rail_screen_side="BOTTOM"))

    def test_rejects_foot_left(self) -> None:
        self.reject(lambda v: v["corrected_orientation_contract"].update(foot_rail_screen_side="LEFT"))

    def test_rejects_auto_flip(self) -> None:
        self.reject(lambda v: v["corrected_orientation_contract"].update(mirror_rotate_or_auto_flip_by_player_side_allowed=True))

    def test_rejects_anisotropic_stretch(self) -> None:
        self.reject(lambda v: v["corrected_orientation_contract"].update(anisotropic_stretch_allowed=True))

    def test_rejects_stretched_table_rect(self) -> None:
        self.reject(lambda v: v["reference_gutter_contract"].update(reference_canonical_table_rect_within_plot_px=[12, 12, 392, 184]))

    def test_rejects_clearance_reduction(self) -> None:
        self.reject(lambda v: v["reference_gutter_contract"].update(minimum_silhouette_clearance_px_at_reference=8))

    def test_rejects_monolingual_label(self) -> None:
        self.reject(lambda v: v["orientation_labels_contract"].update(left_label="HEAD"))

    def test_rejects_labels_in_denominator(self) -> None:
        self.reject(lambda v: v["orientation_labels_contract"].update(labels_are_outside_trajectory_denominator=False))

    def test_rejects_96_case_change(self) -> None:
        self.reject(lambda v: v["cp1_cpu_acceptance_delta_not_executed"].update(existing_exact_96_case_matrix_unchanged=False))

    def test_rejects_false_cpu_pass(self) -> None:
        self.reject(lambda v: v["cp1_cpu_acceptance_delta_not_executed"].update(cpu_checkpoint_pass=True))

    def test_rejects_production_or_gpu_authorization(self) -> None:
        def mutation(value: dict) -> None:
            value["authorization_boundary"].update(production_edit_authorized=True, gpu_authorized=True)
        self.reject(mutation)


if __name__ == "__main__":
    unittest.main()
