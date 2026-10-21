from __future__ import annotations

import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from scripts.generate_wpa_official_fixed_seed_rack_combined27_ta_review_template import (  # noqa: E402
    build_template,
)
from scripts.validate_wpa_official_fixed_seed_rack_combined27_ta_review import (  # noqa: E402
    DEFAULT_REVIEW,
    REQUIRED_MACHINE_PASSES,
    load_json,
    validate,
)


class OfficialRackCombined27TaReviewTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.review = load_json(DEFAULT_REVIEW)

    def assert_rejected(self, value: dict) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "mutated.json"
            path.write_text(json.dumps(value, ensure_ascii=False) + "\n",
                            encoding="utf-8")
            with self.assertRaises((RuntimeError, OSError, ValueError,
                                    KeyError, TypeError)):
                validate(path)

    def test_committed_template_is_exact_not_run_pass(self) -> None:
        result = validate(DEFAULT_REVIEW)
        self.assertEqual(result["status"],
                         "PASS_COMBINED27_TA_TEMPLATE_NOT_RUN")
        self.assertEqual(result["case_count"], 27)
        self.assertEqual(result["batch_case_counts"],
                         REQUIRED_MACHINE_PASSES)
        self.assertEqual(result["applicable_axes_pending"], 406)
        self.assertEqual(result["phase4_rack_axes_not_applicable"], 35)
        self.assertEqual(result["total_axis_records"], 441)
        self.assertFalse(result["future_evidence_bound"])
        self.assertFalse(result["review_performed"])
        self.assertFalse(result["ta_acceptance"])
        self.assertFalse(result["product_acceptance"])

    def test_generator_is_deterministic_and_matches_committed_template(self) -> None:
        self.assertEqual(build_template(), self.review)

    def test_exact_batch_and_case_order_is_frozen(self) -> None:
        self.assertEqual(self.review["scope"]["batch_order"],
                         ["phase2_full_six", "phase4_targeted5",
                          "phase5_exact16"])
        self.assertEqual([row["batch_id"] for row in self.review["cases"]],
                         ["phase2_full_six"] * 6
                         + ["phase4_targeted5"] * 5
                         + ["phase5_exact16"] * 16)
        mutated = copy.deepcopy(self.review)
        mutated["cases"][0], mutated["cases"][1] = \
            mutated["cases"][1], mutated["cases"][0]
        self.assert_rejected(mutated)

    def test_official_gate_and_source_template_bindings_are_frozen(self) -> None:
        for name in self.review["frozen_bindings"]:
            mutated = copy.deepcopy(self.review)
            mutated["frozen_bindings"][name]["sha256"] = "0" * 64
            self.assert_rejected(mutated)

    def test_all_future_batch_fields_are_null_except_required_count(self) -> None:
        for batch, evidence in self.review["batch_future_evidence"].items():
            for field, value in evidence.items():
                if field == "required_machine_pass_count":
                    self.assertEqual(value, REQUIRED_MACHINE_PASSES[batch])
                    continue
                self.assertIsNone(value)
                mutated = copy.deepcopy(self.review)
                mutated["batch_future_evidence"][batch][field] = \
                    0 if field.endswith("count") else "0" * 64
                self.assert_rejected(mutated)

    def test_all_combined_future_fields_are_null(self) -> None:
        for field, value in self.review["combined_future_evidence"].items():
            self.assertIsNone(value)
            mutated = copy.deepcopy(self.review)
            mutated["combined_future_evidence"][field] = "0" * 64
            self.assert_rejected(mutated)

    def test_all_27_case_image_bindings_are_null(self) -> None:
        for index, row in enumerate(self.review["cases"]):
            for field, value in row["future_evidence"].items():
                self.assertIsNone(value)
                mutated = copy.deepcopy(self.review)
                mutated["cases"][index]["future_evidence"][field] = \
                    [1920, 1080] if field.endswith("dimensions") else "future"
                self.assert_rejected(mutated)

    def test_exact_406_applicable_axes_are_pending(self) -> None:
        findings = [finding for row in self.review["cases"]
                    for finding in row["applicable_axis_findings"].values()]
        self.assertEqual(len(findings), 406)
        self.assertTrue(all(finding == {
            "status": "PENDING", "observation": None,
            "physical_coordinates": []} for finding in findings))
        mutated = copy.deepcopy(self.review)
        first_axis = next(iter(mutated["cases"][0]
                              ["applicable_axis_findings"]))
        mutated["cases"][0]["applicable_axis_findings"][first_axis] = {
            "status": "PASS", "observation": "premature",
            "physical_coordinates": [[0, 0]]}
        self.assert_rejected(mutated)

    def test_phase4_has_35_immutable_rack_na_and_50_pending_isolation_axes(self) -> None:
        rows = [row for row in self.review["cases"]
                if row["batch_id"] == "phase4_targeted5"]
        self.assertEqual(sum(len(row["not_applicable_axis_findings"])
                             for row in rows), 35)
        self.assertEqual(sum(len(row["applicable_axis_findings"])
                             for row in rows), 50)
        self.assertTrue(all(finding["status"] == "NOT_APPLICABLE"
                            for row in rows for finding in
                            row["not_applicable_axis_findings"].values()))
        mutated = copy.deepcopy(self.review)
        phase4 = mutated["cases"][6]
        axis = next(iter(phase4["not_applicable_axis_findings"]))
        phase4["not_applicable_axis_findings"][axis]["status"] = "PASS"
        self.assert_rejected(mutated)

    def test_phase4_isolation_axis_cannot_be_removed(self) -> None:
        mutated = copy.deepcopy(self.review)
        phase4 = mutated["cases"][6]
        axis = phase4["isolation_axes_in_order"][-1]
        del phase4["applicable_axis_findings"][axis]
        self.assert_rejected(mutated)

    def test_all_case_and_overall_results_are_not_run(self) -> None:
        self.assertEqual(self.review["overall_result"], "NOT_RUN")
        self.assertTrue(all(row["manual_case_result"] == "NOT_RUN"
                            and row["blocking_axes"] == []
                            for row in self.review["cases"]))
        mutated = copy.deepcopy(self.review)
        mutated["cases"][11]["manual_case_result"] = "PASS"
        self.assert_rejected(mutated)

    def test_physical_dimensions_are_frozen_by_batch(self) -> None:
        self.assertTrue(all(row["expected_physical_dimensions"] == [1920, 1080]
                            for row in self.review["cases"][:11]))
        mutated = copy.deepcopy(self.review)
        mutated["cases"][20]["expected_physical_dimensions"] = [1, 1]
        self.assert_rejected(mutated)

    def test_review_and_product_boundaries_are_false(self) -> None:
        fields = ("production_modified", "gpu_executed", "readiness_created",
                  "authorization_created", "machine_acceptance",
                  "review_performed", "ta_acceptance", "m2", "m3",
                  "golden", "beta", "product_acceptance")
        self.assertTrue(all(self.review["evidence_boundary"][field] is False
                            for field in fields))
        for field in fields:
            mutated = copy.deepcopy(self.review)
            mutated["evidence_boundary"][field] = True
            self.assert_rejected(mutated)

    def test_fake_completed_review_without_three_batch_evidence_fails_closed(self) -> None:
        mutated = copy.deepcopy(self.review)
        mutated["status"] = "COMPLETED_INDEPENDENT_TA_REVIEW"
        mutated["overall_result"] = "PASS"
        mutated["review_performed"] = True
        self.assert_rejected(mutated)

    def test_aggregate_contract_is_frozen(self) -> None:
        mutated = copy.deepcopy(self.review)
        mutated["aggregate_contract"]["algorithm"] = "accept-anything"
        self.assert_rejected(mutated)


if __name__ == "__main__":
    unittest.main()
