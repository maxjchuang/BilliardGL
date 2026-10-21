from __future__ import annotations

import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from scripts.generate_phase5_rules_decision_ui_v5_exact16_ta_review_template import (  # noqa: E402
    CANONICAL_AXES,
    CASE_IDS,
    CONTROL_CASE_IDS,
    DEFAULT_OUTPUT,
    PRIOR_BLOCKING_AXES,
    REPAIR_CASE_IDS,
    REPAIR_REQUIRED_AXES,
    REPAIR_VISUAL_CONTRACT,
    build_template,
    control_axes,
    control_visual_contract,
)
from scripts.validate_phase5_rules_decision_ui_v5_exact16_ta_review import (  # noqa: E402
    load_json,
    validate,
    validate_common_structure,
    validate_completed_mode,
    validate_frozen_bindings,
    validate_template_mode,
)


class Phase5RulesDecisionUiV5Exact16TaReviewTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.template = load_json(DEFAULT_OUTPUT)

    def test_committed_template_passes_not_run(self) -> None:
        result = validate(DEFAULT_OUTPUT)
        self.assertEqual(result["status"],
                         "PASS_V5_EXACT16_TA_TEMPLATE_NOT_RUN")
        self.assertEqual(result["case_count"], 16)
        self.assertEqual(result["repair_case_count"], 4)
        self.assertEqual(result["control_case_count"], 12)
        self.assertEqual(result["axis_count"], 160)
        self.assertEqual(result["cases_not_run"], 16)
        self.assertEqual(result["axes_pending"], 160)
        self.assertFalse(result["readiness_bound"])
        self.assertFalse(result["machine_receipt_bound"])
        self.assertFalse(result["human_ta_review_performed"])
        self.assertFalse(result["targeted16_ta_pass"])

    def test_generator_is_deterministic_and_matches_committed_template(self) -> None:
        self.assertEqual(self.template, build_template())
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "round-trip.json"
            path.write_text(json.dumps(build_template(), ensure_ascii=False,
                                       indent=2) + "\n")
            self.assertEqual(json.loads(path.read_text()), self.template)

    def test_exact_four_repairs_twelve_controls_and_160_pending_axes(self) -> None:
        self.assertEqual(self.template["review_scope"]["case_ids_in_order"],
                         CASE_IDS)
        self.assertEqual(self.template["review_scope"][
            "repair_case_ids_in_order"], REPAIR_CASE_IDS)
        self.assertEqual(self.template["review_scope"][
            "regression_control_case_ids_in_order"], CONTROL_CASE_IDS)
        self.assertEqual(len(self.template["cases"]), 16)
        self.assertEqual(sum(
            len(case["axis_findings"]) for case in self.template["cases"]),
            160)
        self.assertTrue(all(
            finding["status"] == "PENDING"
            for case in self.template["cases"]
            for finding in case["axis_findings"].values()))

    def test_all_four_frozen_binding_drifts_fail_closed(self) -> None:
        for key in self.template["frozen_bindings"]:
            review = copy.deepcopy(self.template)
            review["frozen_bindings"][key]["sha256"] = "0" * 64
            with self.assertRaises(RuntimeError):
                validate_frozen_bindings(review)

    def test_repair_cases_require_visible_closure_copy_focus_and_cvd(self) -> None:
        for case in self.template["cases"][:4]:
            self.assertEqual(case["prior_blocking_axes"], PRIOR_BLOCKING_AXES)
            self.assertEqual(case["required_review_axes"],
                             REPAIR_REQUIRED_AXES)
            self.assertEqual(case["required_visual_contract"],
                             list(REPAIR_VISUAL_CONTRACT.values()))
            text = " ".join(case["required_visual_contract"])
            for term in ("leading focus chevron", "closed four-sided diamond",
                         "Two visibly closed nested rectangular frames",
                         "exact localized actor", "Focus remains",
                         "standard and color_vision_safe"):
                self.assertIn(term, text)
        review = copy.deepcopy(self.template)
        review["cases"][0]["required_visual_contract"] = []
        with self.assertRaises(RuntimeError):
            validate_common_structure(review)

    def test_controls_bind_regression_axes_and_all_placement_states(self) -> None:
        controls = self.template["cases"][4:]
        self.assertEqual([case["case_id"] for case in controls],
                         CONTROL_CASE_IDS)
        for case in controls:
            self.assertEqual(case["required_review_axes"],
                             control_axes(case["case_id"]))
            self.assertEqual(case["required_visual_contract"],
                             control_visual_contract(case["case_id"]))
        placement = [case for case in controls
                     if "preview_" in case["case_id"]]
        self.assertEqual(len(placement), 4)
        text = " ".join(
            row for case in placement for row in case["required_visual_contract"])
        for term in ("ghost cue-ball", "crosshair", "error-cross-octagon",
                     "dashed collision ring", "diagonal hatch",
                     "head-string boundary", "bilingual"):
            self.assertIn(term, text)

    def test_future_readiness_machine_manifest_and_images_are_null(self) -> None:
        future = self.template["future_successor_evidence"]
        for key in ("readiness", "machine_receipt", "stable_png_manifest"):
            self.assertEqual(future[key]["status"], "NOT_AVAILABLE")
            self.assertIsNone(future[key]["path"])
            self.assertIsNone(future[key]["sha256"])
        self.assertIsNone(future["ppm_aggregate_sha256"])
        self.assertIsNone(future["png_aggregate_sha256"])
        self.assertIsNone(future["stable_aggregate_sha256"])
        for case in self.template["cases"]:
            self.assertTrue(all(value is None
                                for value in case["future_evidence"].values()))

    def test_partial_axis_or_case_review_cannot_enter_template(self) -> None:
        review = copy.deepcopy(self.template)
        review["cases"][0]["axis_findings"][CANONICAL_AXES[0]] = {
            "status": "PASS", "observation": "premature"}
        with self.assertRaises(RuntimeError):
            validate_template_mode(review)
        review = copy.deepcopy(self.template)
        review["cases"][0]["manual_case_result"] = "PASS"
        with self.assertRaises(RuntimeError):
            validate_template_mode(review)

    def test_completed_claim_without_successor_evidence_fails_closed(self) -> None:
        review = copy.deepcopy(self.template)
        review["status"] = "COMPLETED_INDEPENDENT_TA_REVIEW"
        review["overall_result"] = "PASS"
        with self.assertRaises(RuntimeError):
            validate_completed_mode(review)

    def test_case_order_axis_order_or_counts_drift_rejects(self) -> None:
        review = copy.deepcopy(self.template)
        review["cases"][0], review["cases"][1] = (
            review["cases"][1], review["cases"][0])
        with self.assertRaises(RuntimeError):
            validate_common_structure(review)
        review = copy.deepcopy(self.template)
        finding = review["cases"][0]["axis_findings"].pop(CANONICAL_AXES[0])
        review["cases"][0]["axis_findings"][CANONICAL_AXES[0]] = finding
        with self.assertRaises(RuntimeError):
            validate_common_structure(review)
        review = copy.deepcopy(self.template)
        review["review_scope"]["total_axis_count"] = 159
        with self.assertRaises(RuntimeError):
            validate_common_structure(review)

    def test_template_never_authorizes_gpu_or_modifies_production(self) -> None:
        boundary = self.template["evidence_boundary"]
        self.assertTrue(boundary["template_only"])
        for field in (
                "successor_readiness_bound", "machine_receipt_bound",
                "stable_png_manifest_bound", "stable_aggregate_bound",
                "human_ta_review_performed", "targeted16_ta_pass",
                "authorization_created_by_template", "gpu_executed_by_template",
                "production_modified", "product_exposed", "golden_promotion",
                "m2_promotion", "beta_pass", "real_input_pass",
                "screen_reader_pass"):
            self.assertFalse(boundary[field])
            review = copy.deepcopy(self.template)
            review["evidence_boundary"][field] = True
            with self.assertRaises(RuntimeError):
                validate_template_mode(review)


if __name__ == "__main__":
    unittest.main()
