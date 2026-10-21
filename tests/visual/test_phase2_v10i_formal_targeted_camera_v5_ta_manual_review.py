from __future__ import annotations

import copy
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from scripts.validate_phase2_v10i_formal_targeted_camera_v5_ta_manual_review import (  # noqa: E402
    DEFAULT_REVIEW,
    load_json,
    validate,
    validate_bindings,
    validate_manifest_and_pixels,
    validate_review_structure,
)


class Phase2V10iFormalTargetedCameraV5TaManualReviewTests(
        unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.review = load_json(DEFAULT_REVIEW)
        cls.loaded = validate_bindings(cls.review)

    def assert_structure_rejected(self, review) -> None:
        with self.assertRaises(RuntimeError):
            validate_review_structure(review)

    def test_committed_independent_review_passes_with_narrow_boundary(self) -> None:
        result = validate(DEFAULT_REVIEW)
        self.assertEqual(
            result["status"],
            "PASS_INDEPENDENT_TARGETED_CAMERA_V5_TA_MANUAL_REVIEW")
        self.assertEqual(result["scene_results"],
                         {"break": "PASS", "low_aim": "PASS"})
        self.assertEqual(result["blocking_axes"], [])
        self.assertFalse(result["full_six_scene_evidence"])
        self.assertFalse(result["m2_promotion"])
        self.assertFalse(result["golden_promotion"])
        self.assertFalse(result["product_pass"])

    def test_readiness_manifest_gate_or_ppm_binding_drift_rejects(self) -> None:
        for role in (
            "v5_readiness_manifest", "v5_targeted_capture_manifest",
            "v4_ta_camera_correction_design_gate", "break_ppm", "low_aim_ppm",
        ):
            review = copy.deepcopy(self.review)
            review["frozen_bindings"][role]["sha256"] = "0" * 64
            with self.assertRaises(RuntimeError, msg=role):
                validate_bindings(review)

    def test_original_resolution_or_no_gpu_review_method_drift_rejects(self) -> None:
        review = copy.deepcopy(self.review)
        review["review_method"][
            "each_source_reviewed_at_original_resolution"] = False
        self.assert_structure_rejected(review)
        review = copy.deepcopy(self.review)
        review["review_method"][
            "gpu_or_graphics_context_used_for_review"] = True
        self.assert_structure_rejected(review)

    def test_camera_state_preset_or_transition_drift_rejects(self) -> None:
        for scene_id, field, replacement in (
            ("break", "state", "aim_player"),
            ("low_aim", "preset", "elevated"),
            ("low_aim", "transitioning", True),
        ):
            review = copy.deepcopy(self.review)
            review["machine_facts_reverified_read_only"]["scenes"][scene_id][
                "semantic_camera"][field] = replacement
            with self.assertRaises(RuntimeError):
                validate_manifest_and_pixels(review, self.loaded)

    def test_roi_or_visible_pixel_facts_cannot_be_relaxed(self) -> None:
        review = copy.deepcopy(self.review)
        review["machine_facts_reverified_read_only"]["scenes"]["low_aim"][
            "interest_region"]["rect_normalized"] = [0, 0, 1, 1]
        with self.assertRaises(RuntimeError):
            validate_manifest_and_pixels(review, self.loaded)
        review = copy.deepcopy(self.review)
        review["machine_facts_reverified_read_only"][
            "visible_pixel_channel_rule"] = "max_rgb_channel_gt_1"
        with self.assertRaises(RuntimeError):
            validate_manifest_and_pixels(review, self.loaded)

    def test_only_pass_or_fail_axis_status_is_allowed(self) -> None:
        review = copy.deepcopy(self.review)
        review["cases"][1]["axis_findings"][
            "ball_readability"]["status"] = "N/A"
        self.assert_structure_rejected(review)

    def test_failure_requires_exact_blocking_axis_and_consistent_counts(self) -> None:
        review = copy.deepcopy(self.review)
        case = review["cases"][1]
        case["manual_result"] = "FAIL"
        case["axis_findings"]["cue_and_aim_axis_readability"][
            "status"] = "FAIL"
        # Missing blocking axis must fail closed before the committed-PASS rule.
        self.assert_structure_rejected(review)
        case["blocking_axes"] = ["cue_and_aim_axis_readability"]
        self.assert_structure_rejected(review)

    def test_low_aim_cue_pocket_or_lamp_axis_omission_rejects(self) -> None:
        for axis in (
            "cue_and_aim_axis_readability", "rail_and_pocket_readability",
            "lamp_occlusion_clearance",
        ):
            review = copy.deepcopy(self.review)
            del review["cases"][1]["axis_findings"][axis]
            self.assert_structure_rejected(review)

    def test_scope_cannot_expand_to_full_six(self) -> None:
        review = copy.deepcopy(self.review)
        review["review_scope"]["full_six_scene_review"] = True
        self.assert_structure_rejected(review)
        review = copy.deepcopy(self.review)
        review["review_scope"]["scene_ids_in_order"].append("ball_closeup")
        self.assert_structure_rejected(review)

    def test_m2_golden_product_or_capture_claim_rejects(self) -> None:
        for field in (
            "full_six_scene_formal_pass", "m2_promotion", "golden_promotion",
            "product_review_performed", "product_pass", "beta_pass",
            "review_gpu_executed", "capture_authorized_by_review",
        ):
            review = copy.deepcopy(self.review)
            review["evidence_boundary"][field] = True
            self.assert_structure_rejected(review)


if __name__ == "__main__":
    unittest.main()
