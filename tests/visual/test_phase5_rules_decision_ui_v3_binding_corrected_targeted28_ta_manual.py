from __future__ import annotations

import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from scripts.generate_phase5_rules_decision_ui_v3_binding_corrected_targeted28_ta_manual import (  # noqa: E402
    ARMED_CASE_IDS,
    ARMED_FAIL_AXES,
    CANONICAL_AXES,
    CASE_EVIDENCE_AGGREGATE_SHA256,
    CASE_IDS,
    DEFAULT_OUTPUT,
    EVIDENCE_MANIFEST_SHA256,
    MACHINE_RECEIPT_SHA256,
    OUTPUT_AGGREGATE_SHA256,
    build_manual,
    generate,
)
from scripts.validate_phase5_rules_decision_ui_v3_binding_corrected_targeted28_ta_manual import (  # noqa: E402
    load_json,
    validate,
    validate_boundary,
    validate_evidence_files,
    validate_input_bindings,
    validate_machine_receipt,
    validate_manual_conclusion,
)


class Phase5RulesDecisionUiV3CorrectedTargeted28TaManualTests(
        unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.manual = load_json(DEFAULT_OUTPUT)

    def test_committed_manual_validates_as_visual_fail(self) -> None:
        result = validate(DEFAULT_OUTPUT)
        self.assertEqual(result["status"],
                         "PASS_VALIDATED_MANUAL_VISUAL_FAIL_BLOCKING")
        self.assertEqual(result["manual_result"], "FAIL")
        self.assertEqual(result["cases_reviewed"], 28)
        self.assertEqual(result["cases_pass"], 24)
        self.assertEqual(result["cases_fail"], 4)
        self.assertEqual(result["axes_reviewed"], 280)
        self.assertEqual(result["axes_pass"], 272)
        self.assertEqual(result["axes_fail"], 8)
        self.assertFalse(result["product_exposed"])

    def test_generator_is_deterministic(self) -> None:
        self.assertEqual(self.manual, build_manual())
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "manual.json"
            generate(output)
            self.assertEqual(json.loads(output.read_text()), self.manual)

    def test_exact_frozen_machine_manifest_and_aggregates_are_bound(self) -> None:
        bindings = self.manual["input_bindings"]
        self.assertEqual(bindings["binding_corrected_machine_receipt"][
            "sha256"], MACHINE_RECEIPT_SHA256)
        self.assertEqual(bindings["actual_gpu_evidence_manifest"][
            "sha256"], EVIDENCE_MANIFEST_SHA256)
        self.assertEqual(bindings["stable_aggregates"][
            "case_evidence_sha256"], CASE_EVIDENCE_AGGREGATE_SHA256)
        self.assertEqual(bindings["stable_aggregates"][
            "output_sha256"], OUTPUT_AGGREGATE_SHA256)
        validate_input_bindings(self.manual)

    def test_any_frozen_binding_or_aggregate_drift_rejects(self) -> None:
        paths = (
            ("binding_corrected_machine_receipt", "sha256"),
            ("actual_gpu_evidence_manifest", "sha256"),
            ("actual_gpu_results_jsonl", "sha256"),
            ("stable_aggregates", "case_evidence_sha256"),
            ("stable_aggregates", "output_sha256"),
        )
        for section, field in paths:
            manual = copy.deepcopy(self.manual)
            manual["input_bindings"][section][field] = "0" * 64
            with self.assertRaises(RuntimeError):
                validate_input_bindings(manual)

    def test_machine_receipt_is_exact_28_pass_input_only(self) -> None:
        receipt = validate_machine_receipt()
        self.assertEqual(receipt["verified_counts"]["planned_cases"], 28)
        self.assertEqual(receipt["verified_counts"]["ppm_files"], 28)
        self.assertEqual(receipt["verified_counts"]["png_files"], 28)
        self.assertEqual(receipt["claims"]["technical_art_review_completed"],
                         0)
        self.assertFalse(receipt["claims"]["product_exposed"])

    def test_all_28_ppm_png_pairs_dimensions_and_aggregates_validate(self) -> None:
        manifest = validate_evidence_files(self.manual)
        self.assertEqual(manifest["case_count"], 28)
        self.assertEqual([case["capture_id"] for case in manifest["cases"]],
                         CASE_IDS)
        for case in self.manual["cases"]:
            self.assertEqual(case["ppm"]["physical_dimensions"],
                             case["png"]["physical_dimensions"])

    def test_case_order_or_ten_axis_inventory_drift_rejects(self) -> None:
        manual = copy.deepcopy(self.manual)
        manual["cases"][0], manual["cases"][1] = (
            manual["cases"][1], manual["cases"][0])
        with self.assertRaises(RuntimeError):
            validate_manual_conclusion(manual)
        manual = copy.deepcopy(self.manual)
        del manual["cases"][27]["axis_findings"][CANONICAL_AXES[-1]]
        with self.assertRaises(RuntimeError):
            validate_manual_conclusion(manual)

    def test_exact_four_armed_cases_fail_exact_two_axes_each(self) -> None:
        self.assertEqual(self.manual["failed_case_ids"], ARMED_CASE_IDS)
        armed = self.manual["cases"][:4]
        for case in armed:
            self.assertEqual(case["manual_case_result"], "FAIL")
            self.assertEqual(case["blocking_axes"], ARMED_FAIL_AXES)
            self.assertEqual(
                [axis for axis, finding in case["axis_findings"].items()
                 if finding["status"] == "FAIL"], ARMED_FAIL_AXES)

    def test_promoting_any_armed_failed_axis_rejects(self) -> None:
        for case_index in range(4):
            for axis in ARMED_FAIL_AXES:
                manual = copy.deepcopy(self.manual)
                finding = manual["cases"][case_index]["axis_findings"][axis]
                finding["status"] = "PASS"
                finding["blocking"] = False
                with self.assertRaises(RuntimeError):
                    validate_manual_conclusion(manual)

    def test_all_other_24_cases_are_pass_and_any_regression_claim_rejects(self) -> None:
        for case in self.manual["cases"][4:]:
            self.assertEqual(case["manual_case_result"], "PASS")
            self.assertEqual(case["blocking_axes"], [])
            self.assertTrue(all(finding["status"] == "PASS"
                                for finding in case["axis_findings"].values()))
        manual = copy.deepcopy(self.manual)
        manual["cases"][10]["manual_case_result"] = "FAIL"
        with self.assertRaises(RuntimeError):
            validate_manual_conclusion(manual)

    def test_exact_shape_failure_explanation_is_mandatory(self) -> None:
        required = (
            "bare checkmark", "bare right arrow", "focus_chevron",
            "selected_check_diamond",
            "confirm_double_frame_plus_enter_arrow",
        )
        text = " ".join(
            self.manual["cases"][0]["axis_findings"][axis]["observation"]
            for axis in ARMED_FAIL_AXES)
        for token in required:
            self.assertIn(token, text)
        manual = copy.deepcopy(self.manual)
        for axis in ARMED_FAIL_AXES:
            manual["cases"][0]["axis_findings"][axis]["observation"] = (
                "Blocking shape distinction remains absent at original resolution."
            )
        with self.assertRaises(RuntimeError):
            validate_manual_conclusion(manual)

    def test_original_resolution_and_review_counts_fail_closed(self) -> None:
        manual = copy.deepcopy(self.manual)
        manual["cases"][5]["reviewed_original_resolution"] = False
        with self.assertRaises(RuntimeError):
            validate_manual_conclusion(manual)
        manual = copy.deepcopy(self.manual)
        manual["review_counts"]["axes_pass"] = 273
        with self.assertRaises(RuntimeError):
            validate_manual_conclusion(manual)

    def test_product_golden_m2_beta_input_screen_reader_remain_blocked(self) -> None:
        for field, value in (
            ("scoped_visual_repair_gate_pass", True),
            ("product_exposed", True),
            ("product_pass", True),
            ("golden_promotion", True),
            ("m2_promotion", True),
            ("beta_pass", True),
            ("required_real_keyboard_pointer_sessions", "PASS"),
            ("required_screen_reader_sessions", "PASS"),
            ("required_external_human_cvd_localization_qa", "PASS"),
            ("gpu_executed_by_this_review", True),
            ("production_modified_by_this_review", True),
        ):
            manual = copy.deepcopy(self.manual)
            manual["evidence_boundary"][field] = value
            with self.assertRaises(RuntimeError):
                validate_boundary(manual)


if __name__ == "__main__":
    unittest.main()
