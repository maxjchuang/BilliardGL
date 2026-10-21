from __future__ import annotations

import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from scripts.validate_phase5_rules_decision_ui_v5_pixel_visible_shape_repair_gate import (  # noqa: E402
    CONTROLS,
    DEFAULT_GATE,
    EXPECTED_PROFILES,
    FAILED_AXES,
    FAILURE_RECEIPT_PATH,
    FAILURE_RECEIPT_SHA256,
    NEGATIVE_FIXTURES,
    REPAIR_CASES,
    ROOT_CAUSE_PATH,
    ROOT_CAUSE_SHA256,
    load_json,
    validate,
    validate_boundaries_and_future,
    validate_cpu_raster_gate,
    validate_failure_receipt,
    validate_read_only_bindings,
    validate_root_cause,
    validate_scope_and_repair,
)


class Phase5RulesDecisionUiV5PixelVisibleShapeRepairGateTests(
        unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.gate = load_json(DEFAULT_GATE)
        cls.receipt = load_json(FAILURE_RECEIPT_PATH)
        cls.report = load_json(ROOT_CAUSE_PATH)

    def test_committed_gate_and_frozen_diagnosis_pass(self) -> None:
        result = validate(DEFAULT_GATE)
        self.assertEqual(
            result["status"],
            "PASS_V5_PIXEL_VISIBLE_SHAPE_REPAIR_DESIGN_GATE")
        self.assertEqual(result["failure_receipt_sha256"],
                         FAILURE_RECEIPT_SHA256)
        self.assertEqual(result["root_cause_report_sha256"],
                         ROOT_CAUSE_SHA256)
        self.assertEqual(result["repair_cases"], 4)
        self.assertEqual(result["repair_axis_occurrences"], 8)
        self.assertEqual(result["cpu_snapshot_cases_required"], 16)
        self.assertEqual(result["machine_cases_required"], 540)
        self.assertEqual(result["future_targeted_gpu_cases_required"], 16)
        self.assertEqual(result["future_ta_axes_required"], 160)
        self.assertFalse(result["production_modified"])
        self.assertFalse(result["gpu_authorized"])
        self.assertFalse(result["product_exposed"])

    def test_json_round_trip_preserves_gate(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "gate.json"
            path.write_text(json.dumps(self.gate, indent=2) + "\n")
            self.assertEqual(json.loads(path.read_text()), self.gate)

    def test_any_frozen_binding_drift_fails_closed(self) -> None:
        for index in range(6):
            gate = copy.deepcopy(self.gate)
            gate["read_only_bindings"][index]["sha256"] = "0" * 64
            with self.assertRaises(RuntimeError):
                validate_read_only_bindings(gate)

    def test_failure_receipt_requires_four_pixel_failures(self) -> None:
        validate_failure_receipt(self.receipt)
        for role, field, invalid in (
            ("selected", "visible_closed_outline_count", 1),
            ("confirm", "visible_closed_frame_count", 2),
            ("focus", "independent_chevron_component_count", 1),
            ("focus", "visible_foreground_pixels_in_lower_40_percent", 1),
        ):
            receipt = copy.deepcopy(self.receipt)
            receipt["case_pixel_findings"][0]["slots"][role][field] = invalid
            with self.assertRaises(RuntimeError):
                validate_failure_receipt(receipt)

    def test_root_cause_must_distinguish_emission_from_visibility(self) -> None:
        validate_root_cause(self.report)
        report = copy.deepcopy(self.report)
        report["causal_findings"]["primary_root_cause"]["id"] = (
            "MISSING_GEOMETRY")
        with self.assertRaises(RuntimeError):
            validate_root_cause(report)
        self.assertIn("EMITTED_ZERO_VISIBLE_CONTRAST",
                      json.dumps(self.report))
        self.assertIn("NOT_CLIPPED_NOT_OVERWRITTEN",
                      json.dumps(self.report))

    def test_failure_scope_is_exact_four_cases_eight_axes(self) -> None:
        failure = self.gate["bound_failure_set"]
        self.assertEqual(failure["failed_case_ids"], REPAIR_CASES)
        self.assertEqual(failure["failed_axes_per_case"], FAILED_AXES)
        for field, value in (
            ("failed_case_count", 5),
            ("failed_axis_occurrence_count", 7),
            ("failed_case_ids", REPAIR_CASES[:-1]),
            ("failed_axes_per_case", FAILED_AXES[:1]),
        ):
            gate = copy.deepcopy(self.gate)
            gate["bound_failure_set"][field] = value
            with self.assertRaises(RuntimeError):
                validate_scope_and_repair(gate)

    def test_accent_on_accent_or_less_than_four_contrast_fails(self) -> None:
        for field, value in (
            ("minimum_ratio", 3.0),
            ("accent_on_accent_forbidden", False),
            ("inherited_thresholds_changed", True),
        ):
            gate = copy.deepcopy(self.gate)
            gate["repair_contract"]["row_foreground_contrast"][field] = value
            with self.assertRaises(RuntimeError):
                validate_scope_and_repair(gate)

    def test_focus_requires_reserved_non_text_connected_component(self) -> None:
        for field, value in (
            ("required_joined_component_count", 0),
            ("must_span_upper_and_lower_slot_regions", False),
            ("minimum_text_clearance_px", 0),
            ("copy_leading_inset_must_reserve_slot", False),
            ("must_not_share_pixels_with_text_or_hud_diamond", False),
        ):
            gate = copy.deepcopy(self.gate)
            gate["repair_contract"]["focus_chevron"][field] = value
            with self.assertRaises(RuntimeError):
                validate_scope_and_repair(gate)

    def test_bare_check_or_bare_arrow_cannot_pass(self) -> None:
        for role, field, value in (
            ("selected_check_diamond", "minimum_visible_closed_outline_count", 0),
            ("selected_check_diamond", "bare_check_forbidden", False),
            ("confirm_double_frame_plus_enter_arrow",
             "minimum_visible_nested_closed_frame_count", 1),
            ("confirm_double_frame_plus_enter_arrow",
             "bare_arrow_forbidden", False),
        ):
            gate = copy.deepcopy(self.gate)
            gate["repair_contract"][role][field] = value
            with self.assertRaises(RuntimeError):
                validate_scope_and_repair(gate)

    def test_cpu_gate_is_final_raster_not_token_or_bbox_claim(self) -> None:
        cpu = self.gate["cpu_raster_snapshot_gate"]
        self.assertEqual(cpu["required_profiles"], EXPECTED_PROFILES)
        self.assertEqual(cpu["required_snapshot_cases"],
                         REPAIR_CASES + CONTROLS)
        self.assertEqual(cpu["negative_fixtures_required"], NEGATIVE_FIXTURES)
        for mutation in (
            lambda gate: gate["cpu_raster_snapshot_gate"].update(
                {"required_profiles": EXPECTED_PROFILES[:1]}),
            lambda gate: gate["cpu_raster_snapshot_gate"].update(
                {"required_snapshot_cases": REPAIR_CASES}),
            lambda gate: gate["cpu_raster_snapshot_gate"].update(
                {"negative_fixtures_required": NEGATIVE_FIXTURES[:-1]}),
            lambda gate: gate["cpu_raster_snapshot_gate"][
                "required_final_pixel_checks_per_profile"].update(
                    {"selected_visible_closed_outline_count_minimum": 0}),
        ):
            gate = copy.deepcopy(self.gate)
            mutation(gate)
            with self.assertRaises(RuntimeError):
                validate_cpu_raster_gate(gate)

    def test_missing_cvd_or_flood_fill_language_rejects(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["cpu_raster_snapshot_gate"]["raster_contract"] = [
            "token and bbox only"]
        with self.assertRaises(RuntimeError):
            validate_cpu_raster_gate(gate)
        gate = copy.deepcopy(self.gate)
        gate["cpu_raster_snapshot_gate"][
            "topology_algorithm_requirements"] = []
        with self.assertRaises(RuntimeError):
            validate_cpu_raster_gate(gate)

    def test_minimal_production_allowlist_and_future_gates(self) -> None:
        future = self.gate["future_targeted_actual_gpu_gate"]
        self.assertEqual(future["status"], "NOT_AUTHORIZED")
        self.assertEqual(future["repair_case_ids"], REPAIR_CASES)
        self.assertEqual(future["regression_control_case_ids"], CONTROLS)
        for section, field, value in (
            ("machine_and_static_gate", "required_case_count", 539),
            ("future_targeted_actual_gpu_gate", "minimum_case_count", 4),
            ("future_targeted_actual_gpu_gate", "status", "AUTHORIZED"),
            ("future_technical_art_gate", "axis_review_count", 8),
        ):
            gate = copy.deepcopy(self.gate)
            gate[section][field] = value
            with self.assertRaises(RuntimeError):
                validate_boundaries_and_future(gate)
        gate = copy.deepcopy(self.gate)
        gate["allowed_modification_boundary"]["production_files"].append({
            "path": "src/Billiards/rules/authority.cpp", "allowed": "expanded"})
        with self.assertRaises(RuntimeError):
            validate_boundaries_and_future(gate)

    def test_no_gpu_production_or_exposure_claim_allowed(self) -> None:
        for field in (
            "implementation_performed_by_this_gate",
            "production_source_modified_by_this_gate",
            "shipping_asset_modified_by_this_gate", "cpu_raster_pass_claimed",
            "machine_pass_claimed", "gpu_execution_authorized",
            "gpu_capture_performed", "technical_art_pass_claimed",
            "scoped_visual_repair_pass", "product_exposure_authorized",
            "product_exposed", "golden_promotion", "m2_promotion",
            "beta_pass", "real_input_pass", "screen_reader_pass",
            "external_human_pass"):
            gate = copy.deepcopy(self.gate)
            gate["evidence_boundary"][field] = True
            with self.assertRaises(RuntimeError):
                validate_boundaries_and_future(gate)


if __name__ == "__main__":
    unittest.main()
