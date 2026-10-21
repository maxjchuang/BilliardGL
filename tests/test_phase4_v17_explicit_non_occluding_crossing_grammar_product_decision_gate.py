#!/usr/bin/env python3
"""Fail-closed tests for the Phase4 V17 crossing-grammar product decision."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import validate_phase4_v17_explicit_non_occluding_crossing_grammar_product_decision_gate as validator

ROOT = Path(__file__).resolve().parents[1]
GATE = ROOT / "docs/art/phase4-v17-explicit-non-occluding-crossing-grammar-product-decision-gate-v1.json"


class V17CrossingGrammarProductDecisionGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(GATE.read_text(encoding="utf-8"))

    def errors_for(self, value: dict) -> list[str]:
        with tempfile.TemporaryDirectory(prefix="v17-crossing-decision-") as directory:
            path = Path(directory) / "gate.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            return validator.validate(path)

    def reject(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(self.errors_for(value))

    def test_committed_gate_passes(self) -> None:
        self.assertEqual(validator.validate(GATE), [])

    def test_rejects_failure_addendum_drift(self) -> None:
        self.reject(lambda v: v["frozen_bindings"][0].update(sha256="0" * 64))

    def test_rejects_priority_one_resume(self) -> None:
        self.reject(lambda v: v["closed_priority_one"].update(disposition="RESUME"))

    def test_rejects_false_priority_two_selection(self) -> None:
        self.reject(lambda v: v["successor_candidate"].update(selected=True))

    def test_rejects_coordinate_independence_loss(self) -> None:
        self.reject(lambda v: v["bounded_design_contract_if_later_selected"].update(semantic_event_input="Use repair coordinates"))

    def test_rejects_source_digest_change(self) -> None:
        self.reject(lambda v: v["bounded_design_contract_if_later_selected"]["frozen_regression_controls"].update(ordered_source_probe_digest="0" * 64))

    def test_rejects_missing_comprehension_evidence(self) -> None:
        self.reject(lambda v: v["required_next_gate_after_explicit_root_selection"]["minimum_cpu_static_evidence"].pop())

    def test_rejects_production_allowlist_claim(self) -> None:
        self.reject(lambda v: v["required_next_gate_after_explicit_root_selection"].update(production_allowlist_defined_by_this_gate=True))

    def test_rejects_gpu_or_product_claim(self) -> None:
        def mutate(value: dict) -> None:
            value["authorization_boundary"]["gpu_authorized"] = True
            value["authorization_boundary"]["product_pass"] = True
        self.reject(mutate)


if __name__ == "__main__":
    unittest.main()
