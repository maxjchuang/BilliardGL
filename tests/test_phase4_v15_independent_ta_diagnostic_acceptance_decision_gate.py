#!/usr/bin/env python3
"""Fail-closed tests for the independent V15 TA decision gate."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import validate_phase4_v15_independent_ta_diagnostic_acceptance_decision_gate as validator


ROOT = Path(__file__).resolve().parents[1]
GATE = ROOT / "docs/art/phase4-v15-independent-ta-diagnostic-acceptance-decision-gate-v1.json"


class Phase4V15IndependentTaDecisionGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(GATE.read_text(encoding="utf-8"))

    def errors_for(self, value: dict) -> list[str]:
        with tempfile.TemporaryDirectory(prefix="phase4-v15-ta-decision-") as directory:
            path = Path(directory) / "gate.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            return validator.validate(path)

    def assert_rejected(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(self.errors_for(value))

    def test_committed_gate_passes(self) -> None:
        self.assertEqual(validator.validate(GATE), [])

    def test_rejects_v15_receipt_binding_drift(self) -> None:
        self.assert_rejected(lambda value:
            value["frozen_bindings"][0].update(sha256="0" * 64))

    def test_rejects_v14_addendum_or_v2_binding_drift(self) -> None:
        for index in (4, 5):
            self.assert_rejected(lambda value, index=index:
                value["frozen_bindings"][index].update(sha256="0" * 64))

    def test_rejects_same_stem_core_halo_misclassified_as_cross(self) -> None:
        self.assert_rejected(lambda value:
            value["corrected_classification_acceptance"].update(
                same_stem_core_and_halo_is_cross=True))

    def test_rejects_missing_different_arc_positive_fixture(self) -> None:
        self.assert_rejected(lambda value:
            value["corrected_classification_acceptance"].update(
                different_arc_is_cross=False))

    def test_rejects_source_id_removal_or_reorder(self) -> None:
        self.assert_rejected(lambda value:
            value["source_inventory_contract"]["ordered_source_probe_ids"].pop())
        self.assert_rejected(lambda value:
            value["source_inventory_contract"]["ordered_source_probe_ids"].reverse())

    def test_rejects_source_digest_mutation(self) -> None:
        self.assert_rejected(lambda value:
            value["source_inventory_contract"].update(
                ordered_source_probe_digest="0" * 64))

    def test_rejects_any_repair_probe_substitution(self) -> None:
        for index in range(3):
            self.assert_rejected(lambda value, index=index:
                value["source_inventory_contract"]
                ["exact_repair_probe_ids_in_order"].__setitem__(index, "0" * 64))

    def test_rejects_892_false_feasible_metric(self) -> None:
        self.assert_rejected(lambda value:
            value["independent_recomputed_diagnostic"]
            ["per_palette_three_gap_results_in_order"][1].update(
                eligible_samples=2, eligible_largest8=2,
                protected_or_ambiguous_samples=94, meets_requirement=True))

    def test_rejects_940_false_eligible_or_feasible_metric(self) -> None:
        self.assert_rejected(lambda value:
            value["independent_recomputed_diagnostic"]
            ["per_palette_three_gap_results_in_order"][2].update(
                eligible_samples=2, eligible_largest8=2,
                protected_or_ambiguous_samples=86, meets_requirement=True))

    def test_rejects_940_fragment_or_interval_partition_drift(self) -> None:
        self.assert_rejected(lambda value:
            value["independent_recomputed_diagnostic"]
            ["blocking_940_525_fragment_partition"].update(
                core_base_body=47))
        self.assert_rejected(lambda value:
            value["independent_recomputed_diagnostic"]
            ["blocking_940_525_interval_partition"].update(
                cross_segment_or_cross_source_overlap=82))

    def test_rejects_feasible_or_visual_acceptance_claim(self) -> None:
        self.assert_rejected(lambda value:
            value["independent_ta_decision"].update(
                classification="FEASIBLE", visual_acceptance=True))

    def test_rejects_automatic_scope_or_implementation_authority(self) -> None:
        self.assert_rejected(lambda value:
            value["independent_ta_decision"].update(
                automatic_scope_expansion=True))
        self.assert_rejected(lambda value:
            value["independent_ta_decision"].update(
                implementation_or_visual_change_authorized=True))

    def test_rejects_candidate_priority_reorder_or_authorization(self) -> None:
        self.assert_rejected(lambda value:
            value["product_decision_candidates_in_priority_order"].reverse())
        self.assert_rejected(lambda value:
            value["product_decision_candidates_in_priority_order"][0].update(
                authorized_by_this_gate=True))
        self.assert_rejected(lambda value:
            value["product_decision_candidates_in_priority_order"][0].update(
                recommended=False))
        self.assert_rejected(lambda value:
            value["product_decision_candidates_in_priority_order"][1].update(
                mutually_exclusive_with=""))
        self.assert_rejected(lambda value:
            value["product_decision_candidates_in_priority_order"][0]
            ["forbidden_if_selected"].pop())

    def test_rejects_bypassing_successor_gate_or_cpu_preflight(self) -> None:
        self.assert_rejected(lambda value:
            value["required_successor_gate_after_product_decision"].update(
                required=False))
        self.assert_rejected(lambda value:
            value["required_successor_gate_after_product_decision"].update(
                must_require_CPU_static_before_any_future_GPU_or_TA=False))

    def test_rejects_threshold_or_gpu_forbidden_clause_removal(self) -> None:
        self.assert_rejected(lambda value:
            value["forbidden"].__setitem__(1, "threshold changes allowed"))
        self.assert_rejected(lambda value:
            value["forbidden"].__setitem__(6, "GPU allowed"))

    def test_rejects_product_decision_or_proof_claim(self) -> None:
        self.assert_rejected(lambda value:
            value["proof_boundary"].update(product_decision_selected=True))
        self.assert_rejected(lambda value:
            value["proof_boundary"].update(ta_visual_pass=True,
                                            product_pass=True))


if __name__ == "__main__":
    unittest.main()
