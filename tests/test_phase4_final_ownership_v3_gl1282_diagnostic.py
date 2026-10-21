#!/usr/bin/env python3
"""Mutation tests for the CPU-only v3 GL 1282 diagnostic package."""

from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
VALIDATOR_PATH = ROOT / \
    "scripts/validate_phase4_final_ownership_v3_gl1282_diagnostic.py"


def load_validator():
    spec = importlib.util.spec_from_file_location("gl1282_validator", VALIDATOR_PATH)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


validator = load_validator()


class Gl1282DiagnosticMutationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.evidence = json.loads(validator.DEFAULT_EVIDENCE.read_text())
        self.gate = json.loads(validator.DEFAULT_GATE.read_text())

    def validate_mutation(self, evidence_mutation=None, gate_mutation=None,
                          renderer_mutation=None) -> list[str]:
        with tempfile.TemporaryDirectory(prefix="phase4-gl1282-mutation-") as temp:
            root = Path(temp)
            evidence = copy.deepcopy(self.evidence)
            gate = copy.deepcopy(self.gate)
            if evidence_mutation:
                evidence_mutation(evidence)
            if gate_mutation:
                gate_mutation(gate)
            evidence_path = root / "evidence.json"
            evidence_path.write_text(json.dumps(evidence, indent=2, sort_keys=True) + "\n")
            gate["failure_evidence_receipt_sha256"] = validator.sha(evidence_path)
            gate_path = root / "gate.json"
            gate_path.write_text(json.dumps(gate, indent=2, sort_keys=True) + "\n")
            renderer_path = validator.DEFAULT_RENDERER
            if renderer_mutation:
                renderer_path = root / "modern_renderer.cpp"
                renderer_path.write_text(renderer_mutation(
                    validator.DEFAULT_RENDERER.read_text()))
            return validator.validate(evidence_path, gate_path, renderer_path,
                                      validator.DEFAULT_RUNNER_SOURCE)

    def test_baseline_passes(self) -> None:
        self.assertEqual(validator.validate(), [])

    def test_existing_failure_receipt_sha_mutation_fails(self) -> None:
        errors = self.validate_mutation(lambda value: value["frozen_files"][
            "dispatch_failure_receipt"].update({"sha256": "0" * 64}))
        self.assertTrue(any("frozen_file_binding_invalid" in error for error in errors))

    def test_authorization_reuse_claim_fails(self) -> None:
        errors = self.validate_mutation(
            lambda value: value.update({"authorization_reusable": True}))
        self.assertIn("authorization_consumption_semantics_invalid", errors)

    def test_root_cause_mutation_fails(self) -> None:
        errors = self.validate_mutation(
            lambda value: value["root_cause"].update({"isolated": False}))
        self.assertIn("root_cause_decision_invalid", errors)

    def test_gate_gpu_authorization_mutation_fails(self) -> None:
        errors = self.validate_mutation(gate_mutation=lambda value:
            value["authorization_boundary"].update({"gpu_capture_authorized": True}))
        self.assertIn("minimal_correction_gate_authorization_invalid", errors)

    def test_renderer_source_mutation_fails_closed(self) -> None:
        errors = self.validate_mutation(renderer_mutation=lambda text:
            text.replace("uniform usampler2DMS uSemanticMsaa;", ""))
        self.assertIn("frozen_source_or_binary_binding_invalid", errors)
        self.assertIn("root_cause_static_chain_missing", errors)


if __name__ == "__main__":
    unittest.main()
