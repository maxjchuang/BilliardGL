#!/usr/bin/env python3
"""Mutation tests for the Phase4 v4 ordinal-1 offline diagnosis."""

from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
VALIDATOR = ROOT / "scripts/validate_phase4_final_ownership_v4_ordinal1_offline_diagnostic.py"
CANONICAL = ROOT / "docs/art/phase4-final-ownership-v4-ordinal1-offline-diagnostic-v1"


def load_module():
    spec = importlib.util.spec_from_file_location("phase4_v4_diag_validator", VALIDATOR)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


validator = load_module()


class OfflineDiagnosticMutationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.evidence = json.loads((CANONICAL / "failure-evidence-receipt-v1.json").read_text())
        self.gate = json.loads((CANONICAL / "minimal-pairing-correction-design-gate-v1.json").read_text())

    def mutate(self, evidence_mutation=None, gate_mutation=None) -> list[str]:
        with tempfile.TemporaryDirectory(prefix="phase4-v4-diag-mutation-") as temp:
            root = Path(temp)
            evidence = copy.deepcopy(self.evidence)
            gate = copy.deepcopy(self.gate)
            if evidence_mutation:
                evidence_mutation(evidence)
            evidence_path = root / "failure-evidence-receipt-v1.json"
            evidence_path.write_text(json.dumps(evidence, indent=2, sort_keys=True) + "\n")
            gate["bound_evidence_receipt_path"] = str(evidence_path)
            gate["bound_evidence_receipt_sha256"] = validator.sha(evidence_path)
            if gate_mutation:
                gate_mutation(gate)
            (root / "minimal-pairing-correction-design-gate-v1.json").write_text(
                json.dumps(gate, indent=2, sort_keys=True) + "\n")
            return validator.validate(root, require_canonical_hash=False)

    def test_canonical_passes(self) -> None:
        self.assertEqual(validator.validate(CANONICAL), [])

    def test_semantic_not_consumed_mutation_fails(self) -> None:
        errors = self.mutate(lambda value: value[
            "integrated_telemetry_decision"].update({"integrated_final_ownership": False}))
        self.assertIn("integrated_decision_invalid", errors)

    def test_candidate_matrix_mutation_fails(self) -> None:
        errors = self.mutate(lambda value: value["candidate_audit"].update(
            {"eligible_count": 95}))
        self.assertIn("candidate_matrix_invalid", errors)

    def test_arbitration_mutation_fails(self) -> None:
        errors = self.mutate(lambda value: value["arbitration_audit"].update(
            {"winner_source_probe_count": 94}))
        self.assertIn("arbitration_matrix_invalid", errors)

    def test_legacy_gate_hypothesis_mutation_fails(self) -> None:
        def change(value):
            value["hypotheses"][1]["decision"] = "FALSIFIED"
        self.assertIn("hypothesis_decisions_invalid", self.mutate(change))

    def test_threshold_mutation_fails(self) -> None:
        errors = self.mutate(gate_mutation=lambda value: value[
            "immutable_invariants"].update({"minimum_edge_contrast_ratio": 3.9}))
        self.assertIn("design_gate_invariants_invalid", errors)

    def test_sequence_mutation_fails(self) -> None:
        errors = self.mutate(gate_mutation=lambda value: value[
            "required_implementation_sequence"].reverse())
        self.assertIn("design_gate_sequence_invalid", errors)

    def test_authorization_mutation_fails(self) -> None:
        errors = self.mutate(gate_mutation=lambda value: value.update(
            {"capture_authorized": True}))
        self.assertIn("design_gate_boundary_invalid", errors)


if __name__ == "__main__":
    unittest.main()
