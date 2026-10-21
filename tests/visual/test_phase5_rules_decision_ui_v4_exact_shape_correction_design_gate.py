from __future__ import annotations

import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from scripts.validate_phase5_rules_decision_ui_v4_exact_shape_correction_design_gate import (  # noqa: E402
    ALLOWED_CATEGORIES,
    CONTROLS,
    DEFAULT_GATE,
    FAILED_AXES,
    MANUAL_SHA256,
    REPAIR_CASES,
    load_json,
    validate,
    validate_boundaries_and_validation,
    validate_exact_shapes,
    validate_failure_scope,
    validate_read_only_bindings,
)


class Phase5RulesDecisionUiV4ExactShapeCorrectionDesignGateTests(
        unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.gate = load_json(DEFAULT_GATE)

    def test_committed_gate_passes_as_design_only(self) -> None:
        result = validate(DEFAULT_GATE)
        self.assertEqual(result["status"],
                         "PASS_V4_EXACT_SHAPE_CORRECTION_DESIGN_GATE")
        self.assertEqual(result["bound_manual_sha256"], MANUAL_SHA256)
        self.assertEqual(result["repair_cases"], 4)
        self.assertEqual(result["repair_axis_occurrences"], 8)
        self.assertEqual(result["machine_cases_required"], 540)
        self.assertEqual(result["future_targeted_cases_required"], 16)
        self.assertEqual(result["future_ta_axes_required"], 160)
        self.assertFalse(result["gpu_authorized"])
        self.assertFalse(result["production_modified"])
        self.assertFalse(result["product_exposed"])

    def test_json_round_trip_keeps_gate_semantics(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "gate.json"
            path.write_text(json.dumps(self.gate, indent=2) + "\n")
            self.assertEqual(json.loads(path.read_text()), self.gate)

    def test_any_read_only_binding_drift_rejects(self) -> None:
        for index in range(4):
            gate = copy.deepcopy(self.gate)
            gate["read_only_bindings"][index]["sha256"] = "0" * 64
            with self.assertRaises(RuntimeError):
                validate_read_only_bindings(gate)
        gate = copy.deepcopy(self.gate)
        gate["read_only_bindings"][2][
            "case_evidence_aggregate_sha256"] = "0" * 64
        with self.assertRaises(RuntimeError):
            validate_read_only_bindings(gate)

    def test_repair_scope_is_exact_four_cases_and_eight_axes(self) -> None:
        failed = self.gate["bound_failure_set"]
        self.assertEqual(failed["failed_case_ids"], REPAIR_CASES)
        self.assertEqual(failed["failed_axes_per_case"], FAILED_AXES)
        self.assertEqual(failed["failed_axis_occurrence_count"], 8)
        for field, value in (
            ("failed_case_count", 5),
            ("failed_axis_occurrence_count", 7),
            ("failed_case_ids", REPAIR_CASES[:-1]),
            ("failed_axes_per_case", FAILED_AXES[:1]),
        ):
            gate = copy.deepcopy(self.gate)
            gate["bound_failure_set"][field] = value
            with self.assertRaises(RuntimeError):
                validate_failure_scope(gate)

    def test_actor_action_copy_freeze_cannot_be_removed(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["repair_contract"]["copy_freeze"]["requirements"] = []
        with self.assertRaises(RuntimeError):
            validate_failure_scope(gate)
        text = json.dumps(self.gate["repair_contract"]["copy_freeze"])
        self.assertIn("actor", text)
        self.assertIn("action", text)
        self.assertIn("submission", text)

    def test_three_exact_shape_tokens_are_distinct_and_non_aliasable(self) -> None:
        shapes = self.gate["repair_contract"]["exact_shape_tokens"]
        self.assertEqual(list(shapes), [
            "focus_chevron", "selected_check_diamond",
            "confirm_double_frame_plus_enter_arrow"])
        gate = copy.deepcopy(self.gate)
        gate["repair_contract"]["exact_shape_tokens"][
            "selected_check_diamond"]["token_id"] = (
                gate["repair_contract"]["exact_shape_tokens"][
                    "focus_chevron"]["token_id"])
        with self.assertRaises(RuntimeError):
            validate_exact_shapes(gate)

    def test_focus_chevron_pixel_and_hud_diamond_distinction_fail_closed(self) -> None:
        for field, value in (
            ("minimum_physical_bounding_box_px", [17, 18]),
            ("minimum_physical_stroke_px", 1),
            ("hud_diamond_confusion_forbidden", False),
        ):
            gate = copy.deepcopy(self.gate)
            gate["repair_contract"]["exact_shape_tokens"][
                "focus_chevron"][field] = value
            with self.assertRaises(RuntimeError):
                validate_exact_shapes(gate)

    def test_selected_check_requires_closed_diamond_and_clearance(self) -> None:
        for field, value in (
            ("minimum_outline_stroke_px", 1),
            ("minimum_check_stroke_px", 1),
            ("minimum_check_to_outline_clearance_px", 1),
            ("closed_diamond_outline_required", False),
        ):
            gate = copy.deepcopy(self.gate)
            gate["repair_contract"]["exact_shape_tokens"][
                "selected_check_diamond"][field] = value
            with self.assertRaises(RuntimeError):
                validate_exact_shapes(gate)

    def test_confirm_requires_two_closed_frames_and_bent_enter_arrow(self) -> None:
        for field, value in (
            ("minimum_outer_frame_stroke_px", 1),
            ("minimum_inner_frame_stroke_px", 1),
            ("minimum_frame_separation_px", 1),
            ("minimum_enter_arrow_stroke_px", 1),
            ("two_closed_rectangular_frames_required", False),
            ("bent_enter_arrow_required", False),
        ):
            gate = copy.deepcopy(self.gate)
            gate["repair_contract"]["exact_shape_tokens"][
                "confirm_double_frame_plus_enter_arrow"][field] = value
            with self.assertRaises(RuntimeError):
                validate_exact_shapes(gate)

    def test_all_three_shapes_must_be_simultaneously_visible_in_standard_cvd(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["repair_contract"]["simultaneous_capture_contract"] = []
        with self.assertRaises(RuntimeError):
            validate_exact_shapes(gate)

    def test_threshold_relaxation_or_scope_expansion_rejects(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["inherited_thresholds_immutable"][
            "critical_shape_minimum_physical_px"] = 17
        with self.assertRaises(RuntimeError):
            validate_boundaries_and_validation(gate)
        gate = copy.deepcopy(self.gate)
        gate["allowed_modification_categories"].append({
            "category": "authority_rules", "boundary": "expanded"})
        with self.assertRaises(RuntimeError):
            validate_boundaries_and_validation(gate)
        self.assertEqual(
            [item["category"]
             for item in self.gate["allowed_modification_categories"]],
            ALLOWED_CATEGORIES)

    def test_cpu_540_and_future_targeted_exact_four_plus_controls(self) -> None:
        targeted = self.gate["future_targeted_actual_gpu_gate"]
        self.assertEqual(targeted["status"], "NOT_AUTHORIZED")
        self.assertEqual(targeted["minimum_case_count"], 16)
        self.assertEqual(targeted["repair_case_ids"], REPAIR_CASES)
        self.assertEqual(targeted["regression_control_case_ids"], CONTROLS)
        for section, field, value in (
            ("cpu_and_machine_gate", "required_case_count", 539),
            ("future_targeted_actual_gpu_gate", "minimum_case_count", 4),
            ("future_targeted_actual_gpu_gate", "status", "AUTHORIZED"),
            ("technical_art_re_review_gate", "axis_review_count", 8),
        ):
            gate = copy.deepcopy(self.gate)
            gate[section][field] = value
            with self.assertRaises(RuntimeError):
                validate_boundaries_and_validation(gate)

    def test_no_gpu_production_exposure_or_session_claim_allowed(self) -> None:
        for field in (
            "implementation_performed_by_this_gate",
            "production_source_modified_by_this_gate",
            "shipping_asset_modified_by_this_gate",
            "machine_pass_claimed_by_this_gate",
            "gpu_execution_authorized",
            "gpu_capture_performed_by_this_gate",
            "technical_art_re_review_claimed_by_this_gate",
            "scoped_visual_repair_gate_pass",
            "product_exposure_authorized", "product_exposed",
            "golden_promotion", "m2_promotion", "beta_pass",
            "true_keyboard_or_pointer_session_claimed",
            "screen_reader_session_claimed",
            "external_human_review_claimed",
        ):
            gate = copy.deepcopy(self.gate)
            gate["evidence_boundary"][field] = True
            with self.assertRaises(RuntimeError):
                validate_boundaries_and_validation(gate)


if __name__ == "__main__":
    unittest.main()
