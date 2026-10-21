#!/usr/bin/env python3
"""Fail-closed mutations for the Phase4 V18 signed-normal correction gate."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import validate_phase4_v18_signed_normal_axis_cpu_diagnostic_interpretation_correction_gate as validator


ROOT = Path(__file__).resolve().parents[1]
GATE = ROOT / "docs/art/phase4-v18-signed-normal-axis-cpu-diagnostic-interpretation-correction-gate-v1.json"


class V18SignedNormalAxisCorrectionGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(GATE.read_text(encoding="utf-8"))

    def errors_for(self, value: dict) -> list[str]:
        with tempfile.TemporaryDirectory(prefix="v18-signed-normal-correction-") as directory:
            path = Path(directory) / "gate.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            return validator.validate(path)

    def reject(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(self.errors_for(value))

    def test_committed_gate_passes(self) -> None:
        self.assertEqual(validator.validate(GATE), [])

    def test_rejects_root_statement_drift(self) -> None:
        self.reject(lambda v: v["root_clarification_binding"].update(statement="drift"))

    def test_rejects_original_gate_binding_drift(self) -> None:
        self.reject(lambda v: v["supersession"].update(corrected_gate_sha256="0" * 64))

    def test_rejects_positive_side_only(self) -> None:
        self.reject(lambda v: v["signed_axis_generation_contract"].update(required_signed_sides_in_order=[1]))

    def test_rejects_negative_side_only(self) -> None:
        self.reject(lambda v: v["signed_axis_generation_contract"].update(required_signed_sides_in_order=[-1]))

    def test_rejects_silent_opposite_side_drop(self) -> None:
        self.reject(lambda v: v["signed_axis_generation_contract"].update(opposite_side_candidates_may_be_silently_dropped=True))

    def test_rejects_coordinate_side_choice(self) -> None:
        self.reject(lambda v: v["signed_axis_generation_contract"].update(signed_side_may_depend_on_coordinates_probes_ids_palette_or_color=True))

    def test_rejects_coordinate_tie_break(self) -> None:
        self.reject(lambda v: v["selection_and_ambiguity_contract"].update(coordinate_or_id_tie_break_allowed=True))

    def test_rejects_cross_side_equal_minimum_selection(self) -> None:
        self.reject(lambda v: v["selection_and_ambiguity_contract"].update(cross_side_equal_minimum_fails_closed=False))

    def test_rejects_signed_side_as_preference(self) -> None:
        self.reject(lambda v: v["selection_and_ambiguity_contract"].update(signed_side_recording="Prefer positive side"))

    def test_rejects_missing_all_candidate_digest(self) -> None:
        self.reject(lambda v: v["exact_case_and_regression_contract"].update(all_candidate_inventory_digest_required_before_matcher=False))

    def test_rejects_pre_generation_probe_use(self) -> None:
        self.reject(lambda v: v["exact_case_and_regression_contract"].update(coordinates_probes_and_repair_ids_are_output_only=False))

    def test_rejects_case_count_drift(self) -> None:
        self.reject(lambda v: v["exact_case_and_regression_contract"].update(required_case_count=12))

    def test_rejects_source_digest_drift(self) -> None:
        self.reject(lambda v: v["exact_case_and_regression_contract"].update(ordered_contrast_probe_digest="0" * 64))

    def test_rejects_missing_midpoint_false_negative_fixture(self) -> None:
        def mutate(value: dict) -> None:
            value["required_fail_closed_fixtures"] = [
                item for item in value["required_fail_closed_fixtures"] if "old fixed-midpoint" not in item
            ]
        self.reject(mutate)

    def test_rejects_production_file_in_allowlist(self) -> None:
        self.reject(lambda v: v["allowed_corrected_standalone_cpu_diagnostic_files"].append("src/Billiards/training/trajectory_overlay_renderer.cpp"))

    def test_rejects_gpu_or_product_authorization(self) -> None:
        def mutate(value: dict) -> None:
            value["authorization_boundary"]["gpu_authorized"] = True
            value["authorization_boundary"]["product_implementation_authorized"] = True
        self.reject(mutate)

    def test_rejects_false_feasibility_result(self) -> None:
        self.reject(lambda v: v["proof_boundary"].update(feasibility_result_exists=True))


if __name__ == "__main__":
    unittest.main()
