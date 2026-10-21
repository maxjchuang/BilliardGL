#!/usr/bin/env python3
"""Fail-closed tests for the Phase5 v4 exact16 independent TA manual."""

from __future__ import annotations

import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
VALIDATOR = ROOT / "scripts/validate_phase5_rules_decision_ui_v4_exact16_independent_ta_manual.py"
SPEC = importlib.util.spec_from_file_location("phase5_v4_exact16_ta", VALIDATOR)
assert SPEC and SPEC.loader
validator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(validator)


class Phase5RulesDecisionUiV4Exact16TaManualTests(unittest.TestCase):
    def setUp(self) -> None:
        self.manual = json.loads(validator.MANUAL.read_text(encoding="utf-8"))

    def rejects(self, mutate) -> None:
        value = copy.deepcopy(self.manual)
        mutate(value)
        with tempfile.TemporaryDirectory(prefix="phase5-v4-exact16-ta-") as root:
            path = Path(root) / "manual.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            with self.assertRaises(RuntimeError):
                validator.validate(path, enforce_frozen_sha=False,
                                   enforce_live_evidence=False)

    def test_committed_manual_validates_as_ta_failure(self) -> None:
        result = validator.validate()
        self.assertEqual(result["status"],
                         "PASS_VALIDATED_PHASE5_V4_EXACT16_TA_FAILURE")
        self.assertEqual([result["case_pass"], result["case_fail"]], [12, 4])
        self.assertEqual([result["axes_pass"], result["axes_fail"]], [152, 8])
        self.assertEqual(result["overall"], "FAIL")

    def test_manual_sha_is_frozen(self) -> None:
        self.assertEqual(hashlib.sha256(validator.MANUAL.read_bytes()).hexdigest(),
                         validator.MANUAL_SHA256)

    def test_exact_v3_gate_receipt_and_manifest_bindings(self) -> None:
        actual = {row["role"]: (row["path"], row["sha256"])
                  for row in self.manual["frozen_bindings"]}
        self.assertEqual(actual, validator.FROZEN)
        self.rejects(lambda value: value["frozen_bindings"][2].update(
            sha256="0" * 64))
        self.rejects(lambda value: value["frozen_bindings"].pop())

    def test_all_stable_aggregates_are_exact(self) -> None:
        self.assertEqual(self.manual["stable_aggregates"], validator.AGGREGATES)
        self.rejects(lambda value: value["stable_aggregates"].update(
            png_path_sha_aggregate_sha256="0" * 64))
        self.rejects(lambda value: value["stable_aggregates"].pop(
            "original_gpu_output_inventory_aggregate_sha256"))

    def test_every_ppm_png_sha_and_dimension_is_bound(self) -> None:
        rows = self.manual["evidence_bindings"]
        self.assertEqual(len(rows), 16)
        self.assertEqual([row["capture_id"] for row in rows], validator.CASES)
        self.assertTrue(all(row["png_decoded_rgb_equals_ppm_rgb"] for row in rows))
        self.assertTrue(all(len(row["physical_dimensions"]) == 2 for row in rows))
        self.rejects(lambda value: value["evidence_bindings"][0].update(
            png_sha256="0" * 64))
        self.rejects(lambda value: value["evidence_bindings"][3].update(
            physical_dimensions=[1280, 720]))

    def test_exact_16_by_10_axis_inventory_and_order(self) -> None:
        reviews = self.manual["case_reviews"]
        self.assertEqual(len(reviews), 16)
        self.assertEqual(sum(len(row["axis_statuses"]) for row in reviews), 160)
        self.assertTrue(all(list(row["axis_statuses"]) == validator.AXES
                            for row in reviews))
        self.rejects(lambda value: value["case_reviews"][0][
            "axis_statuses"].pop("debug_and_product_surface_separation"))

    def test_four_repair_cases_keep_exact_original_eight_blocking_axes(self) -> None:
        for row in self.manual["case_reviews"][:4]:
            self.assertEqual(row["blocking_axes"], validator.FAIL_AXES)
            self.assertEqual(row["case_result"], "FAIL")
        self.assertEqual(self.manual["failed_case_ids"], validator.CASES[:4])
        self.rejects(lambda value: value["case_reviews"][0]["axis_statuses"].update(
            focus_pointer_keyboard_redundancy="PASS"))
        self.rejects(lambda value: value["case_reviews"][1]["blocking_axes"].pop())

    def test_all_twelve_regression_controls_are_full_axis_pass(self) -> None:
        for row in self.manual["case_reviews"][4:]:
            self.assertEqual(set(row["axis_statuses"].values()), {"PASS"})
            self.assertEqual(row["blocking_axes"], [])
            self.assertEqual(row["case_result"], "PASS")
        self.rejects(lambda value: value["case_reviews"][12][
            "axis_statuses"].update(placement_scope_and_legality_readability="FAIL"))

    def test_token_or_machine_claim_cannot_promote_pixel_failure(self) -> None:
        finding = self.manual["blocking_finding"]
        self.assertIn("Token IDs", finding["machine_claim_boundary"])
        self.assertIn("closed diamond", finding["pixel_level_finding"])
        self.assertIn("double rectangular frame", finding["pixel_level_finding"])
        self.rejects(lambda value: value.update(overall_result="PASS"))
        self.rejects(lambda value: value["blocking_finding"].update(
            failed_axis_occurrence_count=0))

    def test_v3_conclusion_and_machine_pass_are_not_inherited_as_ta_pass(self) -> None:
        method = self.manual["review_method"]
        self.assertFalse(method["v3_manual_conclusion_inherited"])
        self.assertFalse(method["machine_or_token_claim_inherited_as_visual_pass"])
        self.rejects(lambda value: value["review_method"].update(
            v3_manual_conclusion_inherited=True))
        self.rejects(lambda value: value["review_method"].update(
            machine_or_token_claim_inherited_as_visual_pass=True))

    def test_original_resolution_review_cannot_be_downgraded(self) -> None:
        self.assertTrue(all(row["reviewed_at_original_resolution"] for row in
                            self.manual["case_reviews"]))
        self.rejects(lambda value: value["case_reviews"][15].update(
            reviewed_at_original_resolution=False))
        self.rejects(lambda value: value["review_method"].update(
            thumbnail_or_contact_sheet_only_review=True))

    def test_case_or_axis_reordering_rejects(self) -> None:
        self.rejects(lambda value: value["case_reviews"].reverse())
        def reorder(value):
            statuses = value["case_reviews"][0]["axis_statuses"]
            first = statuses.pop(validator.AXES[0])
            statuses[validator.AXES[0]] = first
        self.rejects(reorder)

    def test_declared_counts_are_exact_and_derived(self) -> None:
        counts = self.manual["review_counts"]
        self.assertEqual([counts["cases_pass"], counts["cases_fail"],
                          counts["axes_pass"], counts["axes_fail"]], [12, 4, 152, 8])
        self.rejects(lambda value: value["review_counts"].update(axes_fail=0))
        self.rejects(lambda value: value["review_counts"].update(
            regression_controls_pass=11))

    def test_product_golden_m2_beta_real_input_and_screen_reader_remain_closed(self) -> None:
        boundary = self.manual["proof_boundary"]
        for field in ("product_exposed", "product_pass", "golden_promotion",
                      "m2_promotion", "beta_pass", "real_input_pass",
                      "screen_reader_pass"):
            self.assertFalse(boundary[field])
        self.assertEqual(boundary["real_input_status"], "NOT_RUN")
        self.assertEqual(boundary["screen_reader_status"], "NOT_RUN")
        self.rejects(lambda value: value["proof_boundary"].update(product_exposed=True))
        self.rejects(lambda value: value["proof_boundary"].update(beta_pass=True))
        self.rejects(lambda value: value["proof_boundary"].update(real_input_status="PASS"))

    def test_machine_pass_is_preserved_but_exact_shape_gate_stays_failed(self) -> None:
        boundary = self.manual["proof_boundary"]
        self.assertTrue(boundary["targeted_machine_16_pass"])
        self.assertTrue(boundary["independent_ta_review_complete"])
        self.assertFalse(boundary["independent_ta_pass"])
        self.assertFalse(boundary["v4_exact_shape_raster_gate_pass"])
        self.rejects(lambda value: value["proof_boundary"].update(
            v4_exact_shape_raster_gate_pass=True))


if __name__ == "__main__":
    unittest.main()
