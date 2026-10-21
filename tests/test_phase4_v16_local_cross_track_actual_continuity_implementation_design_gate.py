#!/usr/bin/env python3
"""Fail-closed tests for the Phase4 V16 selected implementation gate."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import validate_phase4_v16_local_cross_track_actual_continuity_implementation_design_gate as validator


ROOT = Path(__file__).resolve().parents[1]
GATE = ROOT / "docs/art/phase4-v16-local-cross-track-actual-continuity-implementation-design-gate-v1.json"


class Phase4V16ImplementationDesignGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(GATE.read_text(encoding="utf-8"))

    def errors_for(self, value: dict) -> list[str]:
        with tempfile.TemporaryDirectory(prefix="phase4-v16-gate-") as directory:
            path = Path(directory) / "gate.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            return validator.validate(path)

    def assert_rejected(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(self.errors_for(value))

    def test_committed_gate_passes(self) -> None:
        self.assertEqual(validator.validate(GATE), [])

    def test_rejects_selected_decision_gate_binding_drift(self) -> None:
        self.assert_rejected(lambda value:
            value["frozen_bindings"][0].update(sha256="0" * 64))

    def test_rejects_wrong_or_alternative_product_decision(self) -> None:
        self.assert_rejected(lambda value:
            value["root_product_decision_binding"].update(
                selected_decision_id="EXPLICIT_NON_OCCLUDING_CROSSING_GRAMMAR"))
        self.assert_rejected(lambda value:
            value["root_product_decision_binding"].update(
                mutually_exclusive_alternative_rejected=""))

    def test_rejects_tampered_root_selection_statement(self) -> None:
        self.assert_rejected(lambda value:
            value["root_product_decision_binding"].update(
                selection_statement="different selection"))

    def test_rejects_source_count_digest_or_repair_id_drift(self) -> None:
        self.assert_rejected(lambda value:
            value["exact_source_and_repair_contract"].update(
                ordered_source_probe_count=80))
        self.assert_rejected(lambda value:
            value["exact_source_and_repair_contract"].update(
                ordered_source_probe_digest="0" * 64))
        self.assert_rejected(lambda value:
            value["exact_source_and_repair_contract"]
            ["exact_repair_probe_ids_in_order"].pop())

    def test_rejects_same_stem_core_halo_as_cross(self) -> None:
        self.assert_rejected(lambda value:
            value["candidate_eligibility_contract"].update(
                same_stem_core_halo_is_cross=True))

    def test_rejects_color_or_non_distinct_provenance_selection(self) -> None:
        self.assert_rejected(lambda value:
            value["candidate_eligibility_contract"].update(
                color_selects_eligibility=True))
        self.assert_rejected(lambda value:
            value["candidate_eligibility_contract"].update(
                different_arc_is_cross=False))

    def test_rejects_coordinate_whitelist(self) -> None:
        self.assert_rejected(lambda value:
            value["minimum_connected_selection_algorithm"].update(
                coordinate_whitelist_allowed=True))

    def test_rejects_non_minimum_or_disconnected_tie_contract(self) -> None:
        self.assert_rejected(lambda value:
            value["minimum_connected_selection_algorithm"].update(
                objective="select every crossing sample"))
        self.assert_rejected(lambda value:
            value["minimum_connected_selection_algorithm"]
            ["deterministic_tie_break"].pop())

    def test_rejects_783_unnecessary_selection(self) -> None:
        self.assert_rejected(lambda value:
            value["frozen_fixture_expected_selector_result_per_palette"][0]
            .update(selected_true_crossing_count=1))

    def test_rejects_892_missing_or_extra_selection(self) -> None:
        self.assert_rejected(lambda value:
            value["frozen_fixture_expected_selector_result_per_palette"][1]
            .update(selected_true_crossing_count=0,
                    final_actual_largest8=1))
        self.assert_rejected(lambda value:
            value["frozen_fixture_expected_selector_result_per_palette"][1]
            .update(selected_true_crossing_count=2))

    def test_rejects_940_missing_or_extra_selection(self) -> None:
        self.assert_rejected(lambda value:
            value["frozen_fixture_expected_selector_result_per_palette"][2]
            .update(selected_true_crossing_count=1,
                    canonical_selected_output_only_not_input=[[932, 517]],
                    final_actual_largest8=1))
        self.assert_rejected(lambda value:
            value["frozen_fixture_expected_selector_result_per_palette"][2]
            .update(selected_true_crossing_count=3))

    def test_rejects_canonical_output_coordinate_as_input_substitution(self) -> None:
        self.assert_rejected(lambda value:
            value["frozen_fixture_expected_selector_result_per_palette"][1]
            .update(canonical_selected_output_only_not_input=[[892, 491]]))

    def test_rejects_predicted_visual_regression_contract_removal(self) -> None:
        self.assert_rejected(lambda value:
            value["implementation_contract"]["unchanged_visual_contract"]
            .__setitem__(0, "predicted may change everywhere"))
        self.assert_rejected(lambda value:
            value["implementation_contract"]["unchanged_visual_contract"]
            .__setitem__(2, "annulus may disappear"))

    def test_rejects_allowlist_expansion(self) -> None:
        self.assert_rejected(lambda value:
            value["allowed_files"].append({
                "path": "src/Billiards/game_runtime.cpp", "allowed": "anything"}))

    def test_rejects_cpu_fixture_count_or_not_run_drift(self) -> None:
        self.assert_rejected(lambda value:
            value["cpu_static_acceptance_required_before_any_future_gpu"]
            .update(required_fixture_count=15))
        self.assert_rejected(lambda value:
            value["cpu_static_acceptance_required_before_any_future_gpu"]
            .update(status="PASS"))

    def test_rejects_threshold_or_global_priority_forbidden_removal(self) -> None:
        self.assert_rejected(lambda value:
            value["forbidden_changes"].__setitem__(1,
                "global actual priority allowed"))
        self.assert_rejected(lambda value:
            value["forbidden_changes"].__setitem__(3,
                "threshold changes allowed"))

    def test_rejects_gpu_readiness_or_ta_authority(self) -> None:
        self.assert_rejected(lambda value:
            value["authorization_boundary"].update(gpu_authorized=True))
        self.assert_rejected(lambda value:
            value["authorization_boundary"].update(readiness_authorized=True))
        self.assert_rejected(lambda value:
            value["authorization_boundary"].update(machine_or_ta_acceptance=True))

    def test_rejects_cpu_only_evidence_as_future_gpu_or_ta(self) -> None:
        self.assert_rejected(lambda value:
            value["future_gpu_and_ta_boundary"].update(
                status="AUTHORIZED"))
        self.assert_rejected(lambda value:
            value["future_gpu_and_ta_boundary"].update(
                existing_targeted5_template_cannot_be_marked_from_cpu_only=False))

    def test_rejects_premature_execution_or_product_claim(self) -> None:
        self.assert_rejected(lambda value:
            value["proof_boundary"].update(
                cpu_static_implementation_executed_by_this_gate=True,
                cpu_static_pass=True))
        self.assert_rejected(lambda value:
            value["proof_boundary"].update(product_pass=True))


if __name__ == "__main__":
    unittest.main()
