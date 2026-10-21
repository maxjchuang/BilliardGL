from __future__ import annotations

import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from scripts.generate_phase2_v10i_formal_full_six_ta_review_template import (  # noqa: E402
    CANONICAL_AXES,
    DEFAULT_OUTPUT,
    build_template,
    generate,
)
from scripts.validate_phase2_v10i_formal_full_six_ta_review import (  # noqa: E402
    load_json,
    validate,
    validate_common_structure,
    validate_completed_mode,
    validate_gate_binding,
    validate_template_mode,
)


class Phase2V10iFormalFullSixTaReviewTemplateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.template = load_json(DEFAULT_OUTPUT)

    def assert_template_rejected(self, review) -> None:
        validate_gate_binding(review)
        validate_common_structure(review)
        with self.assertRaises(RuntimeError):
            validate_template_mode(review)

    def test_committed_template_is_exactly_six_not_run_and_42_pending(self) -> None:
        result = validate(DEFAULT_OUTPUT)
        self.assertEqual(result["status"],
                         "PASS_FULL_SIX_TA_TEMPLATE_NOT_RUN")
        self.assertEqual(result["scene_count"], 6)
        self.assertEqual(result["axis_count"], 42)
        self.assertEqual(result["cases_not_run"], 6)
        self.assertEqual(result["axes_pending"], 42)
        self.assertFalse(result["machine_receipt_bound"])
        self.assertEqual(result["ppm_bound_count"], 0)
        self.assertFalse(result["human_ta_review_performed"])

    def test_generator_is_deterministic_and_matches_committed_template(self) -> None:
        self.assertEqual(self.template, build_template())
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "template.json"
            generate(output)
            self.assertEqual(json.loads(output.read_text()), self.template)

    def test_template_binds_only_reentry_gate_not_future_machine_or_ppms(self) -> None:
        binding = self.template["future_machine_pass_binding"]
        self.assertEqual(binding["status"], "NOT_AVAILABLE")
        self.assertIsNone(binding["receipt_path"])
        self.assertIsNone(binding["receipt_sha256"])
        for case in self.template["cases"]:
            self.assertEqual(case["future_ppm_evidence"], {
                "path": None, "sha256": None, "dimensions": None})

    def test_reentry_gate_path_or_sha_drift_rejects(self) -> None:
        for field, value in (("sha256", "0" * 64),
                             ("path", "docs/art/not-the-gate.json")):
            review = copy.deepcopy(self.template)
            review["frozen_reentry_gate_binding"][field] = value
            with self.assertRaises(RuntimeError):
                validate_gate_binding(review)

    def test_scene_order_camera_semantics_or_roi_drift_rejects(self) -> None:
        review = copy.deepcopy(self.template)
        review["cases"][0], review["cases"][1] = (
            review["cases"][1], review["cases"][0])
        with self.assertRaises(RuntimeError):
            validate_common_structure(review)
        review = copy.deepcopy(self.template)
        review["cases"][4]["expected_semantic_camera"]["preset"] = "elevated"
        with self.assertRaises(RuntimeError):
            validate_common_structure(review)
        review = copy.deepcopy(self.template)
        review["cases"][2]["interest_region"]["rect_normalized"] = [0, 0, 1, 1]
        with self.assertRaises(RuntimeError):
            validate_common_structure(review)

    def test_axis_inventory_must_be_exact_canonical_seven_by_six(self) -> None:
        self.assertEqual(self.template["review_scope"][
            "canonical_axes_in_order"], CANONICAL_AXES)
        review = copy.deepcopy(self.template)
        del review["cases"][3]["axis_findings"]["contact_shadow"]
        with self.assertRaises(RuntimeError):
            validate_common_structure(review)

    def test_no_pass_or_fail_may_be_filled_before_machine_and_images(self) -> None:
        for scene_index, axis in ((0, "composition"),
                                  (5, "camera_composition_and_occlusion")):
            review = copy.deepcopy(self.template)
            review["cases"][scene_index]["axis_findings"][axis]["status"] = (
                "PASS")
            self.assert_template_rejected(review)
        review = copy.deepcopy(self.template)
        review["cases"][0]["manual_case_result"] = "FAIL"
        self.assert_template_rejected(review)

    def test_partial_machine_or_capture_binding_rejects(self) -> None:
        review = copy.deepcopy(self.template)
        review["future_machine_pass_binding"]["receipt_path"] = (
            "docs/evidence/not-created.json")
        self.assert_template_rejected(review)
        review = copy.deepcopy(self.template)
        review["cases"][0]["future_ppm_evidence"] = {
            "path": "docs/evidence/not-created.ppm",
            "sha256": "0" * 64,
            "dimensions": [1920, 1080],
        }
        self.assert_template_rejected(review)

    def test_completed_mode_requires_real_machine_6_of_6_before_statuses(self) -> None:
        review = copy.deepcopy(self.template)
        review["status"] = "FULL_SIX_TA_REVIEW_PASS"
        review["overall_result"] = "PASS"
        binding = review["future_machine_pass_binding"]
        binding["status"] = "BOUND_FULL_SIX_MACHINE_PASS"
        binding["receipt_path"] = "docs/evidence/not-created.json"
        binding["receipt_sha256"] = "0" * 64
        with self.assertRaises(RuntimeError):
            validate_completed_mode(review)

    def test_counts_or_not_run_pending_inventory_drift_rejects(self) -> None:
        review = copy.deepcopy(self.template)
        review["review_counts"]["axes_pending"] = 41
        self.assert_template_rejected(review)
        review = copy.deepcopy(self.template)
        review["failed_case_ids"] = ["low_aim"]
        self.assert_template_rejected(review)

    def test_product_golden_m2_or_execution_claim_rejects(self) -> None:
        for field in (
            "human_ta_review_performed", "full_six_ta_pass",
            "full_six_formal_pass", "authorization_prepared_by_template_work",
            "authorization_issued_by_template_work",
            "gpu_executed_by_template_work", "production_modified",
            "m2_promotion", "golden_promotion", "product_review_performed",
            "product_pass", "beta_pass",
        ):
            review = copy.deepcopy(self.template)
            review["evidence_boundary"][field] = True
            self.assert_template_rejected(review)


if __name__ == "__main__":
    unittest.main()
