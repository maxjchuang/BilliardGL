#!/usr/bin/env python3
"""Fail-closed tests for the Phase5 v4 exact16 producer correction gate."""

from __future__ import annotations

import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
VALIDATOR_PATH = ROOT / "scripts/validate_phase5_rules_decision_ui_v4_exact16_capture_producer_contract_correction_gate.py"
SPEC = importlib.util.spec_from_file_location("phase5_v4_exact16_producer_gate", VALIDATOR_PATH)
assert SPEC and SPEC.loader
validator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(validator)


class Phase5RulesDecisionUiV4Exact16ProducerGateTests(unittest.TestCase):
    def setUp(self) -> None:
        self.gate = json.loads(validator.GATE.read_text(encoding="utf-8"))

    def rejects(self, mutate) -> None:
        changed = copy.deepcopy(self.gate)
        mutate(changed)
        with tempfile.TemporaryDirectory(prefix="phase5-v4-producer-gate-") as root:
            path = Path(root) / "gate.json"
            path.write_text(json.dumps(changed), encoding="utf-8")
            with self.assertRaises(RuntimeError):
                validator.validate(path, enforce_frozen_gate_sha=False)

    def test_committed_gate_validates_no_gpu_no_authorization(self) -> None:
        result = validator.validate(enforce_live_baselines=False)
        self.assertEqual(result["status"],
                         "PASS_CAPTURE_ONLY_PRODUCER_CORRECTION_GATE_NO_GPU_NO_AUTH")
        self.assertFalse(result["authorization_may_be_signed_now"])
        self.assertFalse(result["gpu_execution_authorized"])

    def test_gate_sha_is_frozen(self) -> None:
        digest = hashlib.sha256(validator.GATE.read_bytes()).hexdigest()
        self.assertEqual(digest, validator.GATE_SHA256)

    def test_diagnosis_freezes_default60_v3_exact28_and_missing_v4(self) -> None:
        problem = self.gate["frozen_problem_statement"]
        self.assertEqual([item["required_case_count"] for item in
                          problem["current_capture_producer_modes"]], [60, 28])
        self.assertEqual(problem["missing_mode"],
                         "BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16")
        self.rejects(lambda value: value["frozen_problem_statement"][
            "current_capture_producer_modes"][1].update(required_case_count=16))

    def test_exact16_inventory_reorder_duplicate_or_count_drift_rejects(self) -> None:
        self.assertEqual(self.gate["exact16_case_ids_in_order"], validator.EXACT16)
        self.rejects(lambda value: value["exact16_case_ids_in_order"].reverse())
        self.rejects(lambda value: value["exact16_case_ids_in_order"].__setitem__(
            15, value["exact16_case_ids_in_order"][0]))
        self.rejects(lambda value: value["capture_producer_contract"][
            "plan_contract"].update(required_case_count=15))

    def test_binding_path_or_sha_drift_rejects(self) -> None:
        self.rejects(lambda value: value["baseline_bindings"][0].update(
            sha256="0" * 64))
        self.rejects(lambda value: value["baseline_bindings"][4].update(
            path="missing-plan.tsv"))
        self.rejects(lambda value: value["baseline_bindings"].pop())

    def test_immutable_evidence_cannot_be_reclassified_mutable(self) -> None:
        def mutate(value):
            policy = value["baseline_binding_policy"]
            role = policy["immutable_evidence_roles"].pop()
            policy["implementation_baseline_roles"].append(role)
        self.rejects(mutate)

    def test_allowed_surface_cannot_expand_into_product_or_raster(self) -> None:
        self.rejects(lambda value: value["allowed_modification_surface"].append({
            "category": "product_raster", "allowed_path":
            "src/Billiards/render/ui/rules_decision_product_draw.cpp",
            "boundary": "change pixels"}))
        self.rejects(lambda value: value["allowed_modification_surface"][1].update(
            allowed_path="src/Billiards/render/modern/modern_renderer.cpp"))

    def test_compile_target_and_mode_are_exact(self) -> None:
        producer = self.gate["capture_producer_contract"]
        self.assertEqual(producer["required_distinct_binary_target"],
                         "BilliardsPhase5RulesDecisionUiV4Exact16GpuCaptureRunner")
        self.rejects(lambda value: value["capture_producer_contract"].update(
            compile_definition="BILLIARDGL_PHASE5_RULES_UI_V3_TARGETED=1"))
        self.rejects(lambda value: value["capture_producer_contract"].update(
            required_distinct_binary_target="BilliardsPhase5RulesDecisionUiGpuCaptureRunner"))

    def test_authorization_schema_count_flag_and_bindings_fail_closed(self) -> None:
        auth = self.gate["capture_producer_contract"]["authorization_contract"]
        self.assertEqual(auth["required_schema"],
                         "phase5_rules_decision_ui_v4_exact16_gpu_authorization_v2")
        self.rejects(lambda value: value["capture_producer_contract"][
            "authorization_contract"].update(required_case_count=28))
        self.rejects(lambda value: value["capture_producer_contract"][
            "authorization_contract"].update(required_flag={}))
        self.rejects(lambda value: value["capture_producer_contract"][
            "authorization_contract"]["required_bindings"].remove(
                "runner_binary_sha256"))

    def test_output_prefix_partial_extra_or_reorder_cannot_pass(self) -> None:
        self.rejects(lambda value: value["capture_producer_contract"][
            "output_contract"].update(results_rows=15))
        self.rejects(lambda value: value["capture_producer_contract"][
            "output_contract"].update(partial_or_extra_output_is_failure=False))
        self.rejects(lambda value: value["capture_producer_contract"][
            "output_contract"].update(capture_order_must_equal_plan=False))

    def test_dispatcher_and_producer_binding_must_precede_signing(self) -> None:
        dispatch = self.gate["single_use_dispatcher_binding_gate"]
        self.assertEqual(len(dispatch["required_before_authorization_may_be_signed"]), 6)
        self.rejects(lambda value: value["single_use_dispatcher_binding_gate"].update(
            authorization_may_be_signed_now=True))
        self.rejects(lambda value: value["single_use_dispatcher_binding_gate"].update(
            gpu_execution_authorized=True))
        self.rejects(lambda value: value["single_use_dispatcher_binding_gate"][
            "required_before_authorization_may_be_signed"].pop())

    def test_fail_closed_offline_inventory_cannot_be_weakened(self) -> None:
        self.assertEqual(len(self.gate["required_offline_tests"]), 8)
        self.rejects(lambda value: value["required_offline_tests"].pop())
        self.rejects(lambda value: value["required_offline_tests"].__setitem__(
            1, "only happy path"))
        self.rejects(lambda value: value["required_offline_tests"].__setitem__(
            6, "do not test old modes"))

    def test_forbidden_product_authority_threshold_and_evidence_boundary(self) -> None:
        self.rejects(lambda value: value["forbidden_changes"].pop("thresholds"))
        self.rejects(lambda value: value["forbidden_changes"].update(
            production_and_visual=[]))
        self.rejects(lambda value: value["forbidden_changes"].update(
            authority_and_rules=[]))
        self.rejects(lambda value: value["forbidden_changes"].update(
            authorization_and_evidence=[]))

    def test_every_execution_exposure_and_promotion_claim_remains_false(self) -> None:
        boundary = self.gate["non_execution_boundary"]
        self.assertEqual(set(boundary), validator.FALSE_BOUNDARY)
        self.assertTrue(all(value is False for value in boundary.values()))
        for field in ("capture_runner_modified_by_this_gate", "authorization_issued",
                      "gpu_executed", "technical_art_review_pass", "product_exposed",
                      "golden_promotion", "m2_promotion", "beta_pass"):
            self.rejects(lambda value, field=field: value[
                "non_execution_boundary"].update({field: True}))

    def test_frozen_template_is_genuinely_not_issued_and_unbound(self) -> None:
        path = ROOT / validator.EXPECTED_BINDINGS[
            "non_executable_authorization_template"][0]
        auth = json.loads(path.read_text(encoding="utf-8"))
        self.assertFalse(auth["authorized"])
        self.assertIsNone(auth["runner_binary_sha256"])
        self.assertFalse(auth["exact16_capture_producer_contract"])


if __name__ == "__main__":
    unittest.main()
