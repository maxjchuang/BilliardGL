#!/usr/bin/env python3
"""Fail-closed tests for the Phase4 V19 dedicated inset design gate."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import validate_phase4_v19_dedicated_trajectory_inset_minimap_product_decision_and_implementation_design_gate as validator


ROOT = Path(__file__).resolve().parents[1]
GATE = ROOT / "docs/art/phase4-v19-dedicated-trajectory-inset-minimap-product-decision-and-implementation-design-gate-v1.json"


class V19DedicatedInsetDesignGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(GATE.read_text(encoding="utf-8"))

    def errors_for(self, value: dict) -> list[str]:
        with tempfile.TemporaryDirectory(prefix="phase4-v19-inset-gate-") as directory:
            path = Path(directory) / "gate.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            return validator.validate(path)

    def reject(self, mutation) -> None:
        value = copy.deepcopy(self.value)
        mutation(value)
        self.assertTrue(self.errors_for(value))

    def test_committed_gate_passes(self) -> None:
        self.assertEqual(validator.validate(GATE), [])

    def test_rejects_root_decision_drift(self) -> None:
        self.reject(lambda v: v["root_product_decision_binding"].update(option="A"))

    def test_rejects_unselected_option(self) -> None:
        self.reject(lambda v: v["option_disposition"].update(A_SCREEN_SPACE_LANE_SEPARATION="SELECTED"))

    def test_rejects_binding_drift(self) -> None:
        self.reject(lambda v: v["frozen_bindings"][0].update(sha256="0" * 64))

    def test_rejects_world_default_on(self) -> None:
        self.reject(lambda v: v["product_semantics"].update(default_world_overlay_enabled=True))

    def test_rejects_inherited_world_pass(self) -> None:
        self.reject(lambda v: v["product_semantics"].update(prior_world_machine_ta_or_visual_pass_inherited_by_inset=True))

    def test_rejects_layout_minimum_drift(self) -> None:
        self.reject(lambda v: v["default_layout_and_ui_contract"].update(minimum_outer_panel_px=[240, 120]))

    def test_rejects_missing_bilingual_legend(self) -> None:
        self.reject(lambda v: v["default_layout_and_ui_contract"]["legend_entries_in_order"].__setitem__(0, "ACTUAL"))

    def test_rejects_camera_driven_inset(self) -> None:
        self.reject(lambda v: v["inset_visual_grammar"].update(projection="Camera matrices drive inset geometry."))

    def test_rejects_single_actual_rail(self) -> None:
        self.reject(lambda v: v["inset_visual_grammar"].update(actual_track="One cyan rail."))

    def test_rejects_predicted_centerline_removal(self) -> None:
        self.reject(lambda v: v["inset_visual_grammar"].update(predicted_track="Predicted route omitted."))

    def test_rejects_main_world_token_reuse(self) -> None:
        self.reject(lambda v: v["independent_semantic_plane_contract"].update(token_namespace="Reuse world tokens."))

    def test_rejects_combined_denominator(self) -> None:
        self.reject(lambda v: v["independent_semantic_plane_contract"].update(main_world_and_inset_denominators_may_be_combined=True))

    def test_rejects_historical_81_inheritance(self) -> None:
        self.reject(lambda v: v["independent_semantic_plane_contract"].update(historical_81_probe_pass_inherited=True))

    def test_rejects_direct_hdr_architecture(self) -> None:
        self.reject(lambda v: v["render_resource_and_pass_contract"].update(architecture_decision="DIRECT_HDR"))

    def test_rejects_pass_order_drift(self) -> None:
        self.reject(lambda v: v["render_resource_and_pass_contract"]["exact_pass_order"].reverse())

    def test_rejects_attachment_reuse(self) -> None:
        self.reject(lambda v: v["render_resource_and_pass_contract"].update(no_main_depth_or_world_semantic_attachment_reuse=False))

    def test_rejects_sample_count_downgrade(self) -> None:
        self.reject(lambda v: v["render_resource_and_pass_contract"].update(sample_counts_supported_exactly=[1, 4]))

    def test_rejects_stale_fail_open(self) -> None:
        self.reject(lambda v: v["gl_fail_closed_contract"].update(stale_texture_or_semantic_fail_open_allowed=True))

    def test_rejects_world_failure_fallback(self) -> None:
        self.reject(lambda v: v["gl_fail_closed_contract"].update(world_overlay_fallback_allowed=True))

    def test_rejects_cpu_case_count_drift(self) -> None:
        self.reject(lambda v: v["future_cpu_static_acceptance_not_executed"].update(exact_total_case_count=72))

    def test_rejects_missing_broadcast_fixture(self) -> None:
        self.reject(lambda v: v["future_cpu_static_acceptance_not_executed"]["camera_presets_in_order"].remove("broadcast"))

    def test_rejects_false_cpu_pass(self) -> None:
        self.reject(lambda v: v["future_cpu_static_acceptance_not_executed"].update(cpu_machine_pass=True))

    def test_rejects_false_gpu_ta_human_pass(self) -> None:
        def mutation(value: dict) -> None:
            value["future_targeted_gpu_ta_and_human_gates_not_authorized"].update(gpu_pass=True, ta_pass=True, human_comprehension_pass=True)
        self.reject(mutation)

    def test_rejects_production_or_cmake_authorization(self) -> None:
        def mutation(value: dict) -> None:
            value["authorization_boundary"].update(production_edit_authorized=True, cmake_edit_authorized=True, implementation_authorized=True)
        self.reject(mutation)


if __name__ == "__main__":
    unittest.main()
