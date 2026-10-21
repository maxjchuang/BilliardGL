#!/usr/bin/env python3
"""Fail-closed mutations for the Phase4 V17 provenance correction gate."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import validate_phase4_v17_semantic_batch_provenance_cpu_feasibility_correction_gate as validator

ROOT = Path(__file__).resolve().parents[1]
GATE = ROOT / "docs/art/phase4-v17-semantic-batch-provenance-cpu-feasibility-correction-gate-v1.json"


class V17SemanticBatchProvenanceCorrectionGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(GATE.read_text(encoding="utf-8"))

    def errors_for(self, value: dict) -> list[str]:
        with tempfile.TemporaryDirectory(prefix="v17-semantic-provenance-correction-") as directory:
            path = Path(directory) / "gate.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            return validator.validate(path)

    def reject(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(self.errors_for(value))

    def test_committed_gate_passes(self) -> None:
        self.assertEqual(validator.validate(GATE), [])

    def test_rejects_root_statement_drift(self) -> None:
        self.reject(lambda v: v["root_correction_binding"].update(statement="drift"))

    def test_rejects_superseded_gate_binding_drift(self) -> None:
        self.reject(lambda v: v["supersession"].update(superseded_gate_sha256="0" * 64))

    def test_rejects_sparse_probe_count_drift(self) -> None:
        self.reject(lambda v: v["sparse_probe_failure_finding"].update(missing_exact_contrast_probe_actual_descriptor_count=71))

    def test_rejects_false_invalid_identity_drift(self) -> None:
        self.reject(lambda v: v["sparse_probe_failure_finding"].update(false_invalid_pair_count=4895))

    def test_rejects_formal_reproducibility_claim(self) -> None:
        self.reject(lambda v: v["sparse_probe_failure_finding"].update(formal_reproducibility_claimed=True))

    def test_rejects_contrast_probe_as_eligibility(self) -> None:
        self.reject(lambda v: v["post_generation_regression_only_contract"]["forbidden_uses"].remove("descriptor eligibility"))

    def test_rejects_missing_semantic_token(self) -> None:
        self.reject(lambda v: v["corrected_eligibility_provenance_contract"]["required_identity_fields"].remove("existing nonzero finalOwnershipToken identical across the batch vertices"))

    def test_rejects_normalized_stem_source_association(self) -> None:
        self.reject(lambda v: v["corrected_eligibility_provenance_contract"].update(normalized_stem_use="Associate source by normalized stem"))

    def test_rejects_coordinate_algorithm_input(self) -> None:
        self.reject(lambda v: v["preserved_feasibility_contract"].update(coordinates_are_output_only=False))

    def test_rejects_missing_sample_profile(self) -> None:
        self.reject(lambda v: v["preserved_feasibility_contract"]["sample_profiles"].pop())

    def test_rejects_source_digest_drift(self) -> None:
        self.reject(lambda v: v["post_generation_regression_only_contract"].update(ordered_contrast_probe_digest="0" * 64))

    def test_rejects_production_file_in_allowlist(self) -> None:
        self.reject(lambda v: v["allowed_corrected_cpu_diagnostic_files"].append("src/Billiards/training/trajectory_overlay_renderer.cpp"))

    def test_rejects_source_or_token_growth(self) -> None:
        self.reject(lambda v: v["corrected_eligibility_provenance_contract"].update(new_final_ownership_token_allowed=True))

    def test_rejects_gpu_or_product_authorization(self) -> None:
        def mutate(value: dict) -> None:
            value["authorization_boundary"]["product_implementation_authorized"] = True
            value["authorization_boundary"]["gpu_authorized"] = True
        self.reject(mutate)

    def test_rejects_false_feasibility_result(self) -> None:
        self.reject(lambda v: v["proof_boundary"].update(feasibility_result_exists=True))


if __name__ == "__main__":
    unittest.main()
