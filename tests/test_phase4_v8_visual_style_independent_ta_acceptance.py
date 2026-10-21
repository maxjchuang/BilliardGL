#!/usr/bin/env python3
"""Fail-closed tests for the Phase4 v8 visual-style TA acceptance artifacts."""

from __future__ import annotations

import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
VALIDATOR = ROOT / "scripts/validate_phase4_v8_visual_style_independent_ta_acceptance.py"
SPEC = importlib.util.spec_from_file_location("phase4_v8_ta_acceptance", VALIDATOR)
assert SPEC and SPEC.loader
validator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(validator)


class Phase4V8VisualStyleTaAcceptanceTests(unittest.TestCase):
    def setUp(self) -> None:
        self.addendum = json.loads(validator.ADDENDUM.read_text(encoding="utf-8"))
        self.template = json.loads(validator.TEMPLATE.read_text(encoding="utf-8"))

    def rejects(self, addendum_mutate=None, template_mutate=None) -> None:
        addendum, template = copy.deepcopy(self.addendum), copy.deepcopy(self.template)
        if addendum_mutate:
            addendum_mutate(addendum)
        if template_mutate:
            template_mutate(template)
        with tempfile.TemporaryDirectory(prefix="phase4-v8-ta-") as root:
            addendum_path = Path(root) / "addendum.json"
            template_path = Path(root) / "review.json"
            addendum_path.write_text(json.dumps(addendum), encoding="utf-8")
            template_path.write_text(json.dumps(template), encoding="utf-8")
            with self.assertRaises((RuntimeError, OSError)):
                validator.validate(addendum_path, template_path,
                                   enforce_frozen_sha=False,
                                   enforce_live_bindings=False)

    def test_committed_addendum_and_template_validate_not_run(self) -> None:
        result = validator.validate()
        self.assertEqual(result["status"], "PASS_PHASE4_V8_TA_TEMPLATE_NOT_RUN")
        self.assertEqual(result["image_axes_not_run"], 77)
        self.assertEqual(result["stress_cells_not_run"], 30)
        self.assertFalse(result["future_machine_bound"])

    def test_addendum_and_template_shas_are_frozen(self) -> None:
        self.assertEqual(hashlib.sha256(validator.ADDENDUM.read_bytes()).hexdigest(),
                         validator.ADDENDUM_SHA256)
        self.assertEqual(hashlib.sha256(validator.TEMPLATE.read_bytes()).hexdigest(),
                         validator.TEMPLATE_SHA256)

    def test_exact_visual_gate_ppm_telemetry_and_failure_bindings(self) -> None:
        actual = {row["role"]: (row["path"], row["sha256"])
                  for row in self.addendum["frozen_bindings"]}
        self.assertEqual(actual, validator.FROZEN)
        self.rejects(addendum_mutate=lambda value: value["frozen_bindings"][0].update(
            sha256="0" * 64))
        self.rejects(addendum_mutate=lambda value: value["frozen_bindings"].pop())

    def test_frozen_ordinal1_control_and_ordinal2_failure_are_not_inherited(self) -> None:
        review = self.addendum["independent_frozen_evidence_review"]
        self.assertEqual(review["ordinal1"]["visible_source_hidden"], [92, 95, 3])
        self.assertEqual(review["ordinal2"]["visible_source_hidden"], [72, 83, 11])
        self.assertIn("NOT_INHERITABLE", review["ordinal1"]["ta_interpretation"])
        self.assertIn("FAIL_DIAGNOSTIC_ONLY", review["ordinal2"]["ta_interpretation"])
        self.rejects(addendum_mutate=lambda value: value[
            "independent_frozen_evidence_review"]["ordinal2"].update(
                ta_interpretation="PASS"))

    def test_exact_seven_axes_and_five_context_by_six_role_matrix(self) -> None:
        self.assertEqual([row["axis_id"] for row in self.addendum["canonical_ta_axes"]],
                         validator.AXES)
        stress = self.addendum["stress_context_contract"]
        self.assertEqual([row["context_id"] for row in stress], validator.CONTEXTS)
        self.assertTrue(all(row["required_roles"] == validator.ROLES for row in stress))
        self.rejects(addendum_mutate=lambda value: value["canonical_ta_axes"].pop())
        self.rejects(addendum_mutate=lambda value: value[
            "stress_context_contract"][3]["required_roles"].remove("actual"))

    def test_ordinal2_blocking_axes_are_exact_and_cannot_be_omitted(self) -> None:
        self.assertEqual([row["axis_id"] for row in
                          self.addendum["ordinal2_blocking_axes"]], validator.O2_AXES)
        self.rejects(addendum_mutate=lambda value: value["ordinal2_blocking_axes"].pop())
        self.rejects(addendum_mutate=lambda value: value[
            "ordinal2_blocking_axes"][2].update(axis_id="generic_contrast"))

    def test_ordinal1_regression_gate_is_mandatory_and_independent(self) -> None:
        gate = self.addendum["ordinal1_regression_gate"]
        self.assertEqual(gate["source_probe_count_immutable"], 95)
        self.assertTrue(gate["future_review_must_not_inherit_frozen_pass"])
        self.rejects(addendum_mutate=lambda value: value[
            "ordinal1_regression_gate"].update(future_manual_failures_allowed=1))
        self.rejects(addendum_mutate=lambda value: value[
            "ordinal1_regression_gate"].update(future_review_must_not_inherit_frozen_pass=False))

    def test_future_machine_binding_must_remain_absent_in_template(self) -> None:
        future = self.addendum["future_targeted11_machine_prerequisite"]
        self.assertIsNone(future["future_receipt_sha256"])
        self.assertEqual(future["required_passed_count"], 11)
        self.rejects(addendum_mutate=lambda value: value[
            "future_targeted11_machine_prerequisite"].update(future_receipt_sha256="a" * 64))
        self.rejects(addendum_mutate=lambda value: value[
            "future_targeted11_machine_prerequisite"].update(required_passed_count=10))

    def test_targeted11_ids_order_and_physical_dimensions_are_exact(self) -> None:
        rows = self.addendum["targeted11_case_contract"]
        actual = [(row["ordinal"], row["capture_id"], row["physical_dimensions"],
                   row["role"]) for row in rows]
        self.assertEqual(actual, validator.CASES)
        self.rejects(addendum_mutate=lambda value: value[
            "targeted11_case_contract"].reverse())
        self.rejects(addendum_mutate=lambda value: value[
            "targeted11_case_contract"][4].update(physical_dimensions=[1920, 1080]))

    def test_all_11_by_7_image_axes_are_individually_not_run(self) -> None:
        rows = self.template["case_reviews"]
        self.assertEqual(len(rows), 11)
        self.assertEqual(sum(status == "NOT_RUN" for row in rows
                             for status in row["axis_statuses"].values()), 77)
        self.assertTrue(all(not row["opened_individually_at_original_resolution"]
                            for row in rows))
        self.rejects(template_mutate=lambda value: value["case_reviews"][1][
            "axis_statuses"].update(predicted_track_visual_identity="PASS"))
        self.rejects(template_mutate=lambda value: value["case_reviews"][1].update(
            opened_individually_at_original_resolution=True))

    def test_all_30_stress_cells_and_five_ordinal2_axes_are_not_run(self) -> None:
        stress = self.template["stress_context_results"]
        self.assertEqual(sum(status == "NOT_RUN" for row in stress
                             for status in row["role_statuses"].values()), 30)
        self.assertTrue(all(row["status"] == "NOT_RUN" for row in
                            self.template["ordinal2_blocking_axis_results"]))
        self.rejects(template_mutate=lambda value: value[
            "stress_context_results"][0]["role_statuses"].update(core="FAIL"))
        self.rejects(template_mutate=lambda value: value[
            "ordinal2_blocking_axis_results"][0].update(status="PASS"))

    def test_partial_review_or_fake_pass_before_machine_evidence_rejects(self) -> None:
        self.rejects(template_mutate=lambda value: value.update(overall_result="PASS"))
        def completed(value):
            value.update(status="COMPLETED", overall_result="PASS")
        self.rejects(template_mutate=completed)
        def fake_machine(value):
            value["future_targeted11_machine_binding"].update(
                path="/missing/receipt.json", sha256="a" * 64,
                status="BOUND", machine_result="PASS", case_count=11,
                passed_count=11, failed_count=0,
                ppm_telemetry_manifest_path="/missing/manifest.json",
                ppm_telemetry_manifest_sha256="b" * 64,
                stable_evidence_aggregate_sha256="c" * 64)
            value.update(status="COMPLETED", overall_result="PASS")
        self.rejects(template_mutate=fake_machine)

    def test_missing_ppm_telemetry_or_review_manifest_claim_rejects(self) -> None:
        self.rejects(template_mutate=lambda value: value["case_reviews"][0].update(
            ppm_path="future.ppm", ppm_sha256="a" * 64))
        self.rejects(template_mutate=lambda value: value[
            "review_evidence_manifest"].update(status="PASS"))

    def test_threshold_offset_z_order_camera_and_layout_workarounds_remain_forbidden(self) -> None:
        forbidden = " ".join(self.addendum["forbidden_workarounds"])
        for term in ("threshold", "offset", "z-order", "camera", "layout"):
            self.assertIn(term, forbidden)
        self.rejects(addendum_mutate=lambda value: value.update(forbidden_workarounds=[
            "No product exposure"] * 5))

    def test_no_execution_authorization_product_or_promotion_claim(self) -> None:
        self.assertTrue(all(item is False for item in
                            self.addendum["proof_boundary"].values()))
        self.assertTrue(all(item is False for item in self.template["claims"].values()))
        self.rejects(addendum_mutate=lambda value: value["proof_boundary"].update(
            gpu_executed=True))
        self.rejects(template_mutate=lambda value: value["claims"].update(
            product_exposed=True))


if __name__ == "__main__":
    unittest.main()
