#!/usr/bin/env python3
"""Fail-closed tests for the v14 independent failure addendum."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import validate_phase4_v14_cap_mask_premise_independent_ta_failure_addendum as validator

ROOT = Path(__file__).resolve().parents[1]
ADDENDUM = ROOT / "docs/art/phase4-v14-cap-mask-premise-independent-ta-failure-addendum-v1.json"


class V14IndependentFailureAddendumTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(ADDENDUM.read_text(encoding="utf-8"))

    def errors_for(self, value: dict) -> list[str]:
        with tempfile.TemporaryDirectory(prefix="v14-ta-addendum-") as directory:
            path = Path(directory) / "addendum.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            return validator.validate(path)

    def assert_rejected(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(self.errors_for(value))

    def test_committed_addendum_passes(self) -> None:
        self.assertEqual(validator.validate(ADDENDUM), [])

    def test_rejects_receipt_binding_drift(self) -> None:
        self.assert_rejected(lambda value:
                             value["frozen_bindings"][0].update(sha256="0" * 64))

    def test_rejects_production_not_restored(self) -> None:
        self.assert_rejected(lambda value: value["restored_production_bindings"]
                             ["modern_renderer_cpp"].update(sha256="0" * 64))

    def test_rejects_false_v14_pass(self) -> None:
        self.assert_rejected(lambda value: value["independent_failure_finding"]
                             .update(v14_gate_result="PASS"))

    def test_rejects_predicted_core_count_mutation(self) -> None:
        self.assert_rejected(lambda value: value["pixel_and_control_flow_root_cause"]
                             .update(final_prior_mask_pixels_owned_by_predicted_core=80))

    def test_rejects_inner_count_mutation(self) -> None:
        self.assert_rejected(lambda value: value["pixel_and_control_flow_root_cause"]
                             .update(final_prior_mask_pixels_owned_by_untagged_predicted_inner_body=8))

    def test_rejects_tick_promoted_to_route_body(self) -> None:
        self.assert_rejected(lambda value: value["independent_failure_finding"]
                             ["unchanged_failed_result_per_palette"].update(
                                 actual_route_body_final_pixels=1))

    def test_rejects_v14_resume(self) -> None:
        self.assert_rejected(lambda value:
                             value["v14_disposition"].update(v14_gpu_allowed=True))

    def test_rejects_direct_visual_authorization(self) -> None:
        self.assert_rejected(lambda value: value["successor_design_decision"]
                             .update(visual_repair_authorized_now=True))

    def test_rejects_missing_gap_coordinate(self) -> None:
        self.assert_rejected(lambda value: value["v15_diagnostic_only_gate"]
                             ["exact_case"]["blocking_gap_coordinates"].pop())

    def test_rejects_missing_repair_probe(self) -> None:
        self.assert_rejected(lambda value: value["v15_diagnostic_only_gate"]
                             ["exact_repair_probe_ids"]["predicted_contrast"].pop())

    def test_rejects_missing_core_base_class(self) -> None:
        self.assert_rejected(lambda value: value["v15_diagnostic_only_gate"]
                             ["required_predicted_fragment_classes"].remove(
                                 "core_base_body"))

    def test_rejects_modern_renderer_scope_expansion(self) -> None:
        self.assert_rejected(lambda value: value["v15_diagnostic_only_gate"]
                             .update(modern_renderer_edit_allowed=True))

    def test_rejects_predicted_base_core_suppression(self) -> None:
        self.assert_rejected(lambda value: value["future_narrow_candidate_eligibility"]
                             ["always_protected_never_suppress"].remove(
                                 "core_base_body"))

    def test_rejects_lateral_annulus_suppression(self) -> None:
        self.assert_rejected(lambda value: value["future_narrow_candidate_eligibility"]
                             ["always_protected_never_suppress"].remove(
                                 "inner_lateral_annulus"))

    def test_rejects_gpu_or_ta_claim(self) -> None:
        def mutate(value: dict) -> None:
            value["proof_boundary"]["gpu_run"] = True
            value["proof_boundary"]["ta_pass"] = True
        self.assert_rejected(mutate)


if __name__ == "__main__":
    unittest.main()
