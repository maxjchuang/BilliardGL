#!/usr/bin/env python3
"""Fail-closed tests for the independent V17 feasibility failure addendum."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import validate_phase4_v17_explicit_crossing_grammar_independent_ta_feasibility_failure_addendum as validator

ROOT = Path(__file__).resolve().parents[1]
ADDENDUM = ROOT / "docs/art/phase4-v17-explicit-crossing-grammar-independent-ta-feasibility-failure-addendum-v1.json"


class V17IndependentTaFeasibilityFailureAddendumTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(ADDENDUM.read_text(encoding="utf-8"))

    def errors_for(self, value: dict) -> list[str]:
        with tempfile.TemporaryDirectory(prefix="v17-ta-feasibility-failure-") as directory:
            path = Path(directory) / "addendum.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            return validator.validate(path)

    def reject(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(self.errors_for(value))

    def test_committed_addendum_passes(self) -> None:
        self.assertEqual(validator.validate(ADDENDUM), [])

    def test_rejects_final_receipt_drift(self) -> None:
        self.reject(lambda v: v["frozen_bindings"][0].update(sha256="0" * 64))

    def test_rejects_false_probe_eligibility(self) -> None:
        self.reject(lambda v: v["independent_reproduction_audit"].update(contrast_probe_or_repair_id_used_for_eligibility=True))

    def test_rejects_missing_semantic_descriptor(self) -> None:
        self.reject(lambda v: v["independent_failure_finding"].update(missing_actual_semantic_descriptor_count=1))

    def test_rejects_pair_partition_drift(self) -> None:
        self.reject(lambda v: v["independent_failure_finding"].update(disjoint_pair_count=5507))

    def test_rejects_false_event(self) -> None:
        self.reject(lambda v: v["independent_failure_finding"].update(strict_interior_nonparallel_event_count=1))

    def test_rejects_synthetic_as_frozen_feasibility(self) -> None:
        self.reject(lambda v: v["independent_failure_finding"].update(synthetic_fixture_is_frozen_scene_feasibility=True))

    def test_rejects_priority_one_resume(self) -> None:
        self.reject(lambda v: v["two_choice_disposition"]["priority_one"].update(may_resume_under_existing_gate=True))

    def test_rejects_priority_two_resume(self) -> None:
        self.reject(lambda v: v["two_choice_disposition"]["priority_two"].update(may_resume_under_existing_gate=True))

    def test_rejects_false_remaining_bounded_path(self) -> None:
        self.reject(lambda v: v["two_choice_disposition"].update(bounded_choice_remaining=True))

    def test_rejects_automatic_scope_expansion(self) -> None:
        self.reject(lambda v: v["two_choice_disposition"].update(automatic_fallback_or_scope_expansion_allowed=True))

    def test_rejects_missing_new_product_decision(self) -> None:
        self.reject(lambda v: v["successor_product_decision_requirement"].update(new_root_product_decision_required=False))

    def test_rejects_successor_false_selection(self) -> None:
        self.reject(lambda v: v["successor_product_decision_requirement"].update(candidate_selected_now=True))

    def test_rejects_production_or_gpu_authorization(self) -> None:
        def mutate(value: dict) -> None:
            value["authorization_boundary"]["production_edit_authorized"] = True
            value["authorization_boundary"]["gpu_authorized"] = True
        self.reject(mutate)


if __name__ == "__main__":
    unittest.main()
