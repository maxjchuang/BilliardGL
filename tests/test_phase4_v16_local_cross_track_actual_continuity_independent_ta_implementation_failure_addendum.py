#!/usr/bin/env python3
"""Fail-closed tests for the independent V16 TA failure addendum."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import validate_phase4_v16_local_cross_track_actual_continuity_independent_ta_implementation_failure_addendum as validator

ROOT = Path(__file__).resolve().parents[1]
ADDENDUM = ROOT / "docs/art/phase4-v16-local-cross-track-actual-continuity-independent-ta-implementation-failure-addendum-v1.json"


class V16IndependentTaImplementationFailureAddendumTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(ADDENDUM.read_text(encoding="utf-8"))

    def errors_for(self, value: dict) -> list[str]:
        with tempfile.TemporaryDirectory(prefix="v16-ta-failure-") as directory:
            path = Path(directory) / "addendum.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            return validator.validate(path)

    def reject(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(self.errors_for(value))

    def test_committed_addendum_passes(self) -> None:
        self.assertEqual(validator.validate(ADDENDUM), [])

    def test_rejects_receipt_drift(self) -> None:
        self.reject(lambda v: v["frozen_bindings"][1].update(sha256="0" * 64))

    def test_rejects_source_baseline_drift(self) -> None:
        self.reject(lambda v: v["restored_baselines"]["modern_cpp"].update(sha256="0" * 64))

    def test_rejects_false_full_frame_pass(self) -> None:
        self.reject(lambda v: v["independent_contract_audit"].update(generic_full_frame_zero_one_two_contract_passed=True))

    def test_rejects_failed_island_relaxation(self) -> None:
        self.reject(lambda v: v["independent_failure_finding"].update(single_sample_unsatisfiable_islands_standard_cvd=[0, 0]))

    def test_rejects_largest8_relaxation(self) -> None:
        self.reject(lambda v: v["independent_failure_finding"]["constructive_counterexample"].update(required_largest8=1))

    def test_rejects_upload_blocker_hidden(self) -> None:
        self.reject(lambda v: v["unclosed_pipeline_blocker"].update(resource_upload_failure_is_fail_closed=True))

    def test_rejects_priority_one_resume(self) -> None:
        self.reject(lambda v: v["selected_priority_one_disposition"].update(may_be_resumed_under_v16_gate=True))

    def test_rejects_priority_two_false_selection(self) -> None:
        self.reject(lambda v: v["remaining_bounded_product_path"].update(selected_or_authorized_now=True))

    def test_rejects_production_or_gpu_claim(self) -> None:
        def mutate(value: dict) -> None:
            value["authorization_boundary"]["production_edit_authorized"] = True
            value["authorization_boundary"]["gpu_authorized"] = True
        self.reject(mutate)


if __name__ == "__main__":
    unittest.main()
