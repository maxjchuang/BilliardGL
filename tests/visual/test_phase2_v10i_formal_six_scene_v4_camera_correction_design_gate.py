from __future__ import annotations

import copy
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from scripts.validate_phase2_v10i_formal_six_scene_v4_camera_correction_design_gate import (  # noqa: E402
    DEFAULT_GATE,
    load_json,
    validate,
    validate_frozen_failure,
    validate_gate_structure,
)


class Phase2V10iFormalSixSceneV4CameraCorrectionDesignGateTests(
        unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.gate = load_json(DEFAULT_GATE)

    def assert_structure_rejected(self, gate) -> None:
        with self.assertRaises(RuntimeError):
            validate_gate_structure(gate)

    def test_committed_gate_passes_without_gpu_or_production_edit(self) -> None:
        result = validate(DEFAULT_GATE)
        self.assertEqual(
            result["status"],
            "PASS_V4_CAMERA_CORRECTION_DESIGN_GATE_NO_GPU")
        self.assertEqual(result["classification"],
                         "REAL_CAMERA_COMPOSITION_STATE_FAILURE")
        self.assertFalse(result["gpu_executed"])
        self.assertFalse(result["production_modified"])
        self.assertFalse(result["capture_authorized"])

    def test_frozen_failure_receipt_or_ppm_binding_drift_rejects(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["frozen_v3_failure"]["receipt_sha256"] = "0" * 64
        with self.assertRaises(RuntimeError):
            validate_frozen_failure(gate)
        gate = copy.deepcopy(self.gate)
        gate["frozen_v3_failure"]["low_aim_ppm"]["sha256"] = "0" * 64
        with self.assertRaises(RuntimeError):
            validate_frozen_failure(gate)

    def test_camera_attribution_cannot_be_relabelled_roi_or_lighting(self) -> None:
        gate = copy.deepcopy(self.gate)
        finding = gate["independent_technical_art_attribution"]
        finding["classification"] = "INTEREST_REGION_MISCONFIGURATION"
        finding["interest_region_misconfiguration"] = True
        self.assert_structure_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["independent_technical_art_attribution"][
            "asset_or_lighting_failure_primary"] = True
        self.assert_structure_rejected(gate)

    def test_static_occluder_math_drift_rejects(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["independent_technical_art_attribution"]["static_geometry_cm"][
            "v3_low_aim_transition_start_eye_y"] = 200.0
        self.assert_structure_rejected(gate)

    def test_every_scene_needs_explicit_instant_semantic_preset(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["v4_minimum_correction_contract"]["every_scene_camera_rule"][
            "required_parameter"]["instant"] = False
        self.assert_structure_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["v4_minimum_correction_contract"]["semantic_preset_map"].pop(
            "post_shot_observe")
        self.assert_structure_rejected(gate)

    def test_low_aim_relative_camera_or_unfinished_transition_rejects(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["v4_minimum_correction_contract"]["low_aim_required_recipe"].insert(
            1, {"command": "orbit_camera", "params": {"pitch_delta": 0.4}})
        self.assert_structure_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["v4_minimum_correction_contract"][
            "low_aim_pre_capture_presentation_state"]["transitioning"] = True
        self.assert_structure_rejected(gate)

    def test_instant_camera_may_not_advance_physics_or_change_product(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["v4_minimum_correction_contract"]["instant_semantics"][
            "must_not_advance_physics_tick"] = False
        self.assert_structure_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["v4_minimum_correction_contract"]["instant_semantics"][
            "interactive_product_transition_behavior_unchanged"] = False
        self.assert_structure_rejected(gate)

    def test_renderer_asset_or_acceptance_relaxation_rejects(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["allowed_implementation_layers"].append(
            "modern renderer implementation")
        self.assert_structure_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["immutable_visual_acceptance"][
            "low_aim_interest_region"]["rect_normalized"] = [0, 0, 1, 1]
        self.assert_structure_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["immutable_visual_acceptance"][
            "visible_pixel_channel_rule"] = "max_rgb_channel_gt_1"
        self.assert_structure_rejected(gate)

    def test_cpu_preflight_must_cover_break_and_low_aim_fail_closed(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["cpu_static_preflight_gate"]["required_cases"] = ["low_aim"]
        self.assert_structure_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["cpu_static_preflight_gate"][
            "report_must_fail_closed_if_any_assertion_missing_or_false"] = False
        self.assert_structure_rejected(gate)

    def test_targeted_gpu_scope_authorization_or_promotion_rejects(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["future_targeted_actual_gpu_gate"][
            "required_scene_ids_in_order"] = ["low_aim"]
        self.assert_structure_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["future_targeted_actual_gpu_gate"][
            "capture_authorized_by_this_gate"] = True
        self.assert_structure_rejected(gate)
        gate = copy.deepcopy(self.gate)
        gate["evidence_boundary"]["formal_pass"] = True
        self.assert_structure_rejected(gate)


if __name__ == "__main__":
    unittest.main()
