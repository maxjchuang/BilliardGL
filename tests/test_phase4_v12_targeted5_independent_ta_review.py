from __future__ import annotations

import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts.validate_phase4_v12_targeted5_independent_ta_review import (  # noqa: E402
    AXES,
    BINDINGS,
    CASE_IDS,
    DEFAULT_REVIEW,
    TEMPLATE_FALSE_FIELDS,
    load_json,
    validate,
    validate_bindings,
    validate_inventory,
    validate_template_state,
)


class Phase4V12Targeted5IndependentTaReviewTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.review = load_json(DEFAULT_REVIEW)

    def test_committed_template_passes_not_run(self) -> None:
        result = validate(DEFAULT_REVIEW)
        self.assertEqual(result["status"],
                         "PASS_PHASE4_V12_TARGETED5_TA_TEMPLATE_NOT_RUN")
        self.assertEqual(result["case_count"], 5)
        self.assertEqual(result["axis_count"], 35)
        self.assertEqual(result["cases_not_run"], 5)
        self.assertEqual(result["axes_pending"], 35)
        self.assertEqual(result["future_ppm_count"], 0)
        self.assertEqual(result["future_telemetry_row_count"], 0)
        self.assertFalse(result["targeted5_ta_pass"])
        self.assertFalse(result["targeted11_pass"])
        self.assertFalse(result["formal68_pass"])
        self.assertFalse(result["m3_pass"])
        self.assertFalse(result["product_exposed"])

    def test_json_round_trip_preserves_template(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "review.json"
            path.write_text(json.dumps(self.review, indent=2) + "\n")
            self.assertEqual(json.loads(path.read_text()), self.review)

    def test_any_frozen_binding_drift_fails_closed(self) -> None:
        for index in range(len(BINDINGS)):
            review = copy.deepcopy(self.review)
            review["frozen_bindings"][index]["sha256"] = "0" * 64
            with self.assertRaises(RuntimeError):
                validate_bindings(review)

    def test_exact_five_cases_and_seven_axes_are_required(self) -> None:
        self.assertEqual(self.review["review_scope"]["case_ids_in_order"],
                         CASE_IDS)
        self.assertEqual(self.review["review_scope"]["canonical_axes_in_order"],
                         AXES)
        review = copy.deepcopy(self.review)
        review["cases"] = review["cases"][:-1]
        with self.assertRaises(RuntimeError):
            validate_inventory(review)
        review = copy.deepcopy(self.review)
        del review["cases"][3]["axis_findings"][AXES[-1]]
        with self.assertRaises(RuntimeError):
            validate_inventory(review)

    def test_future_readiness_machine_and_aggregates_must_remain_null(self) -> None:
        for name in ("readiness", "machine_receipt"):
            review = copy.deepcopy(self.review)
            review["future_successor_evidence"][name]["path"] = "future.json"
            with self.assertRaises(RuntimeError):
                validate_template_state(review)
        for field in ("ppm_aggregate_sha256", "telemetry_aggregate_sha256",
                      "stable_aggregate_sha256"):
            review = copy.deepcopy(self.review)
            review["future_successor_evidence"][field] = "0" * 64
            with self.assertRaises(RuntimeError):
                validate_template_state(review)

    def test_all_per_case_ppm_and_telemetry_fields_must_remain_null(self) -> None:
        for case_index in range(5):
            for field in ("ppm_path", "ppm_sha256", "ppm_physical_dimensions",
                          "telemetry_path", "telemetry_file_sha256",
                          "telemetry_row_sha256"):
                review = copy.deepcopy(self.review)
                review["cases"][case_index]["future_evidence"][field] = \
                    [1920, 1080] if field == "ppm_physical_dimensions" else "not-null"
                with self.assertRaises(RuntimeError):
                    validate_template_state(review)

    def test_partial_axis_review_is_forbidden(self) -> None:
        review = copy.deepcopy(self.review)
        finding = review["cases"][3]["axis_findings"][
            "actual_track_visual_identity"]
        finding.update({"status": "PASS", "observation": "looks fixed",
                        "physical_coordinates": [[892, 491]]})
        with self.assertRaises(RuntimeError):
            validate_template_state(review)

    def test_premature_case_or_overall_pass_is_forbidden(self) -> None:
        review = copy.deepcopy(self.review)
        review["cases"][3]["manual_case_result"] = "PASS"
        with self.assertRaises(RuntimeError):
            validate_template_state(review)
        review = copy.deepcopy(self.review)
        review["overall_result"] = "PASS"
        with self.assertRaises(RuntimeError):
            validate_template_state(review)

    def test_broadcast_three_gaps_and_two_contrast_locations_are_mandatory(self) -> None:
        contract = "\n".join(
            self.review["cases"][3]["required_review_contract"])
        for coordinate in ("[783,413]", "[892,491]", "[940,525]",
                           "[855,469]", "[879,486]"):
            self.assertIn(coordinate, contract)
            review = copy.deepcopy(self.review)
            review["cases"][3]["required_review_contract"] = [
                item.replace(coordinate, "[0,0]") for item in
                review["cases"][3]["required_review_contract"]]
            with self.assertRaises(RuntimeError):
                validate_inventory(review)

    def test_cvd_control_requires_non_color_distinction(self) -> None:
        contract = "\n".join(
            self.review["cases"][4]["required_review_contract"])
        self.assertIn("without hue-only inference", contract)
        review = copy.deepcopy(self.review)
        review["cases"][4]["required_review_contract"] = ["hue is enough"]
        with self.assertRaises(RuntimeError):
            validate_inventory(review)

    def test_summary_must_remain_exactly_5_not_run_and_35_pending(self) -> None:
        review = copy.deepcopy(self.review)
        review["summary"]["axis_result_counts"]["PENDING"] = 34
        with self.assertRaises(RuntimeError):
            validate_template_state(review)

    def test_product_and_milestone_boundaries_remain_false(self) -> None:
        for field in TEMPLATE_FALSE_FIELDS:
            review = copy.deepcopy(self.review)
            review["proof_boundary"][field] = True
            with self.assertRaises(RuntimeError):
                validate_template_state(review)


if __name__ == "__main__":
    unittest.main()
