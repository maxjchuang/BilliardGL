from __future__ import annotations

import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from scripts.generate_phase5_rules_decision_ui_v3_targeted28_ta_review_template import (  # noqa: E402
    CANONICAL_AXES,
    CASE_IDS,
    CONTROL_CASE_IDS,
    CONTROL_REGRESSION_AXES,
    DEFAULT_OUTPUT,
    FAILURE_RECEIPT_PATH,
    FAILURE_RECEIPT_SHA256,
    REPAIR_BLOCKING_AXES,
    REPAIR_CASE_IDS,
    VISUAL_REPAIR_GATE_PATH,
    VISUAL_REPAIR_GATE_SHA256,
    build_template,
    generate,
)
from scripts.validate_phase5_rules_decision_ui_v3_targeted28_ta_review import (  # noqa: E402
    load_json,
    validate,
    validate_common_structure,
    validate_completed_mode,
    validate_frozen_bindings,
    validate_template_mode,
)


class Phase5RulesDecisionUiV3Targeted28TaReviewTemplateTests(
        unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.template = load_json(DEFAULT_OUTPUT)

    def assert_template_rejected(self, review) -> None:
        validate_frozen_bindings(review)
        validate_common_structure(review)
        with self.assertRaises(RuntimeError):
            validate_template_mode(review)

    def test_committed_template_is_28_not_run_and_280_pending(self) -> None:
        result = validate(DEFAULT_OUTPUT)
        self.assertEqual(result["status"],
                         "PASS_TARGETED28_TA_TEMPLATE_NOT_RUN")
        self.assertEqual(result["case_count"], 28)
        self.assertEqual(result["repair_case_count"], 20)
        self.assertEqual(result["regression_control_case_count"], 8)
        self.assertEqual(result["axis_count"], 280)
        self.assertEqual(result["cases_not_run"], 28)
        self.assertEqual(result["axes_pending"], 280)
        self.assertFalse(result["machine_receipt_bound"])
        self.assertEqual(result["ppm_bound_count"], 0)
        self.assertEqual(result["png_bound_count"], 0)
        self.assertFalse(result["stable_aggregate_bound"])
        self.assertFalse(result["human_ta_review_performed"])

    def test_generator_is_deterministic_and_matches_committed_template(self) -> None:
        self.assertEqual(self.template, build_template())
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "template.json"
            generate(output)
            self.assertEqual(json.loads(output.read_text()), self.template)

    def test_template_binds_exactly_gate_and_frozen_failure_only(self) -> None:
        self.assertEqual(self.template["frozen_bindings"], {
            "visual_repair_design_gate": {
                "path": VISUAL_REPAIR_GATE_PATH,
                "sha256": VISUAL_REPAIR_GATE_SHA256,
            },
            "targeted_preordinal_failure_receipt": {
                "path": FAILURE_RECEIPT_PATH,
                "sha256": FAILURE_RECEIPT_SHA256,
            },
        })
        binding = self.template["future_machine_pass_binding"]
        self.assertEqual(binding["status"], "NOT_AVAILABLE")
        self.assertIsNone(binding["receipt_path"])
        self.assertIsNone(binding["receipt_sha256"])
        self.assertIsNone(binding["ppm_aggregate_sha256"])
        self.assertIsNone(binding["png_aggregate_sha256"])
        self.assertIsNone(binding["stable_aggregate_sha256"])
        serialized = json.dumps(self.template)
        self.assertNotIn("authorization_id", serialized)
        self.assertNotIn("readiness_manifest", serialized)

    def test_frozen_path_or_sha_drift_rejects(self) -> None:
        for binding_name in self.template["frozen_bindings"]:
            for field, value in (("sha256", "0" * 64),
                                 ("path", "docs/art/not-bound.json")):
                review = copy.deepcopy(self.template)
                review["frozen_bindings"][binding_name][field] = value
                with self.assertRaises(RuntimeError):
                    validate_frozen_bindings(review)

    def test_exact_20_repair_plus_8_control_order_rejects_drift(self) -> None:
        self.assertEqual(CASE_IDS, REPAIR_CASE_IDS + CONTROL_CASE_IDS)
        self.assertEqual(len(REPAIR_CASE_IDS), 20)
        self.assertEqual(len(CONTROL_CASE_IDS), 8)
        review = copy.deepcopy(self.template)
        review["cases"][19], review["cases"][20] = (
            review["cases"][20], review["cases"][19])
        with self.assertRaises(RuntimeError):
            validate_common_structure(review)

    def test_exact_ten_canonical_axes_per_case_and_280_total(self) -> None:
        self.assertEqual(len(CANONICAL_AXES), 10)
        self.assertEqual(sum(len(case["axis_findings"])
                             for case in self.template["cases"]), 280)
        review = copy.deepcopy(self.template)
        del review["cases"][27]["axis_findings"][CANONICAL_AXES[-1]]
        with self.assertRaises(RuntimeError):
            validate_common_structure(review)

    def test_all_20_repair_cases_bind_exact_32_prior_blocking_axes(self) -> None:
        repair_cases = self.template["cases"][:20]
        self.assertEqual(sum(len(case["prior_blocking_axes"])
                             for case in repair_cases), 32)
        for case in repair_cases:
            self.assertEqual(case["prior_blocking_axes"],
                             REPAIR_BLOCKING_AXES[case["case_id"]])
            self.assertEqual(case["required_regression_axes"], [])
        review = copy.deepcopy(self.template)
        review["cases"][0]["prior_blocking_axes"].pop()
        with self.assertRaises(RuntimeError):
            validate_common_structure(review)

    def test_all_8_controls_bind_gate_derived_regression_axes(self) -> None:
        controls = self.template["cases"][20:]
        self.assertEqual([case["case_id"] for case in controls],
                         CONTROL_CASE_IDS)
        covered = set()
        for case in controls:
            self.assertEqual(case["required_regression_axes"],
                             CONTROL_REGRESSION_AXES[case["case_id"]])
            self.assertEqual(case["prior_blocking_axes"], [])
            covered.update(case["required_regression_axes"])
        self.assertTrue({
            "action_semantics_and_branch_correctness",
            "actor_revision_and_authority_status_readability",
            "no_default_selection_or_accidental_submission_affordance",
            "focus_pointer_keyboard_redundancy",
            "standard_cvd_non_color_redundancy",
            "stale_error_ai_pending_fail_closed_clarity",
            "foundation_and_unoffered_action_absence",
        }.issubset(covered))
        review = copy.deepcopy(self.template)
        review["cases"][27]["required_regression_axes"] = []
        with self.assertRaises(RuntimeError):
            validate_common_structure(review)

    def test_no_pass_or_fail_before_machine_images_and_aggregates(self) -> None:
        for case_index, axis in ((0, CANONICAL_AXES[0]),
                                 (27, CANONICAL_AXES[-1])):
            review = copy.deepcopy(self.template)
            review["cases"][case_index]["axis_findings"][axis][
                "status"] = "PASS"
            self.assert_template_rejected(review)
        review = copy.deepcopy(self.template)
        review["cases"][4]["manual_case_result"] = "FAIL"
        self.assert_template_rejected(review)

    def test_partial_future_evidence_or_aggregate_binding_rejects(self) -> None:
        review = copy.deepcopy(self.template)
        review["future_machine_pass_binding"]["receipt_path"] = (
            "docs/evidence/not-created.json")
        self.assert_template_rejected(review)
        review = copy.deepcopy(self.template)
        review["cases"][0]["future_evidence"]["ppm"] = {
            "path": "docs/evidence/not-created.ppm",
            "sha256": "0" * 64,
            "physical_dimensions": [1280, 720],
        }
        self.assert_template_rejected(review)
        review = copy.deepcopy(self.template)
        review["future_machine_pass_binding"][
            "stable_aggregate_sha256"] = "0" * 64
        self.assert_template_rejected(review)

    def test_completed_mode_requires_real_machine_28_of_28_first(self) -> None:
        review = copy.deepcopy(self.template)
        review["status"] = "TARGETED28_TA_REVIEW_PASS"
        review["overall_result"] = "PASS"
        binding = review["future_machine_pass_binding"]
        binding["status"] = "BOUND_TARGETED28_MACHINE_PASS"
        binding["receipt_path"] = "docs/evidence/not-created.json"
        binding["receipt_sha256"] = "0" * 64
        binding["ppm_aggregate_sha256"] = "0" * 64
        binding["png_aggregate_sha256"] = "0" * 64
        binding["stable_aggregate_sha256"] = "0" * 64
        with self.assertRaises(RuntimeError):
            validate_completed_mode(review)

    def test_counts_and_all_exposure_promotion_claims_fail_closed(self) -> None:
        review = copy.deepcopy(self.template)
        review["review_counts"]["axes_pending"] = 279
        self.assert_template_rejected(review)
        for field in (
            "machine_receipt_bound", "stable_aggregate_bound",
            "human_ta_review_performed", "targeted28_ta_pass",
            "scoped_visual_repair_gate_pass",
            "authorization_prepared_by_template_work",
            "authorization_issued_by_template_work",
            "gpu_executed_by_template_work", "production_modified",
            "m2_promotion", "golden_promotion", "product_exposed",
            "product_pass", "beta_pass",
            "true_keyboard_or_pointer_session_pass",
            "screen_reader_session_pass",
        ):
            review = copy.deepcopy(self.template)
            review["evidence_boundary"][field] = True
            self.assert_template_rejected(review)


if __name__ == "__main__":
    unittest.main()
