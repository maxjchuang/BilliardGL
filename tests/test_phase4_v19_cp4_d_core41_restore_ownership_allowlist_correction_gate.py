#!/usr/bin/env python3
"""Fail-closed mutations for the CP4-D Core41 restore correction gate."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import validate_phase4_v19_cp4_d_core41_restore_ownership_allowlist_correction_gate as validator

ROOT = Path(__file__).resolve().parents[1]
GATE = ROOT / "docs/art/phase4-v19-cp4-d-core41-restore-ownership-allowlist-correction-gate-v1.json"


class Core41RestoreCorrectionGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(GATE.read_text(encoding="utf-8"))

    def errors_for(self, value: dict) -> list[str]:
        with tempfile.TemporaryDirectory(prefix="cp4-d-core41-gate-") as directory:
            path = Path(directory) / "gate.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            return validator.validate(path)

    def reject(self, mutation) -> None:
        value = copy.deepcopy(self.value)
        mutation(value)
        self.assertTrue(self.errors_for(value))

    def test_committed_gate_passes(self) -> None:
        self.assertEqual(validator.validate(GATE), [])

    def test_rejects_prior_gate_drift(self) -> None:
        self.reject(lambda v: v["frozen_bindings"][0].update(sha256="0" * 64))

    def test_rejects_failure_source_drift(self) -> None:
        self.reject(lambda v: v["nonfinal_failure_binding"]["source_sha256"].update({"src/Billiards/render/modern/product_ui_gl_core41.cpp": "0" * 64}))

    def test_rejects_candidate_acceptance(self) -> None:
        self.reject(lambda v: v["nonfinal_failure_binding"].update(candidate_accepted=True))

    def test_rejects_core41_as_owner(self) -> None:
        self.reject(lambda v: v["root_decision_binding"].update(selected_owner="CORE41_INTERNAL"))

    def test_rejects_internal_silent_restore(self) -> None:
        self.reject(lambda v: v["root_decision_binding"].update(core41_internal_silent_restore_allowed=True))

    def test_rejects_two_outer_restores(self) -> None:
        self.reject(lambda v: v["root_decision_binding"].update(outer_exact_restore_attempts_after_captured_begin_failure=2))

    def test_rejects_allowlist_growth(self) -> None:
        self.reject(lambda v: v["corrected_exact_allowlist_not_implementation_authority"].append("src/Billiards/render/modern/product_ui_gl_extra.cpp"))

    def test_rejects_missing_core_header_delta(self) -> None:
        self.reject(lambda v: v["allowlist_delta_exact"].pop(0))

    def test_rejects_cmake_edit(self) -> None:
        self.reject(lambda v: v["still_forbidden"].update(cmake="CMakeLists.txt may change"))

    def test_rejects_untyped_begin_receipt(self) -> None:
        self.reject(lambda v: v["typed_begin_receipt_contract"]["required_fields"].remove("restoreRequired"))

    def test_rejects_precapture_restore(self) -> None:
        self.reject(lambda v: v["typed_begin_receipt_contract"].update(invalid_precondition_before_capture="restoreRequired=true"))

    def test_rejects_captured_failure_without_restore(self) -> None:
        self.reject(lambda v: v["typed_begin_receipt_contract"].update(setup_failure_after_capture="restoreRequired=false"))

    def test_rejects_boolean_restore_claim(self) -> None:
        self.reject(lambda v: v["typed_begin_receipt_contract"].update(no_boolean_or_configured_claim_may_substitute_for_actual_receipt=False))

    def test_rejects_core_internal_end(self) -> None:
        self.reject(lambda v: v["typed_begin_receipt_contract"].update(no_core41_end_frame_call_on_begin_failure=False))

    def test_rejects_fabricated_restored_result(self) -> None:
        self.reject(lambda v: v["single_restore_owner_contract"].update(result_rule="begin failed, therefore stateRestored=true"))

    def test_rejects_nonpersistent_poison(self) -> None:
        self.reject(lambda v: v["single_restore_owner_contract"].update(poison_retry="retry is allowed"))

    def test_rejects_mock_only_fixture(self) -> None:
        self.reject(lambda v: v["future_cpu_static_gl_mock_acceptance_not_run"].update(actual_core41_required=False))

    def test_rejects_fixture_inventory_loss(self) -> None:
        self.reject(lambda v: v["future_cpu_static_gl_mock_acceptance_not_run"]["required_positive_and_negative_cases"].pop())

    def test_rejects_false_cpu_pass(self) -> None:
        self.reject(lambda v: v["future_cpu_static_gl_mock_acceptance_not_run"].update(cpu_static_pass=True))

    def test_rejects_production_authority(self) -> None:
        self.reject(lambda v: v["authorization_boundary"].update(production_edit_authorized_by_this_gate=True))

    def test_rejects_gpu_authority(self) -> None:
        self.reject(lambda v: v["authorization_boundary"].update(gpu_authorized=True))


if __name__ == "__main__":
    unittest.main()
