#!/usr/bin/env python3

import copy
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
VALIDATOR_PATH = ROOT / "scripts/validate_phase4_v12_visual_local_alternatives_exhaustion.py"
SPEC = importlib.util.spec_from_file_location("validator", VALIDATOR_PATH)
VALIDATOR = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(VALIDATOR)
RECEIPT = ROOT / "docs/art/phase4-v12-visual-local-alternatives-exhaustion-v1/exhaustion-evidence-receipt-v1.json"


class ExhaustionReceiptTests(unittest.TestCase):
    def setUp(self):
        self.receipt = json.loads(RECEIPT.read_text())

    def validate_mutation(self, mutation):
        value = copy.deepcopy(self.receipt)
        mutation(value)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "receipt.json"
            path.write_text(json.dumps(value))
            self.assertTrue(VALIDATOR.validate(path))

    def test_canonical_passes(self):
        self.assertEqual([], VALIDATOR.validate(RECEIPT))

    def test_gap_coordinate_mutation_fails(self):
        self.validate_mutation(lambda v: v["immutable_contract"].update({"frozen_gap_coordinates": [[783, 413], [892, 491], [941, 525]]}))

    def test_core_threshold_mutation_fails(self):
        self.validate_mutation(lambda v: v["immutable_contract"].update({"required_actual_core_largest_8_connected": 1}))

    def test_halo_substitution_mutation_fails(self):
        self.validate_mutation(lambda v: v["immutable_contract"].update({"forbid_halo_or_all_sublayer_substitution": False}))

    def test_width_boundary_mutation_fails(self):
        self.validate_mutation(lambda v: v["actual_route_body_width_sweep"][2].update({"width_px": 6.5}))

    def test_sweep_success_claim_mutation_fails(self):
        self.validate_mutation(lambda v: v["actual_route_body_width_sweep"][1].update({"standard_actual_core_largest_8_connected": 2, "exit_code": 0}))

    def test_variant_source_mutation_fails(self):
        self.validate_mutation(lambda v: v["actual_route_body_width_sweep"][0].update({"variant_source_sha256": "0" * 64}))

    def test_join_budget_mutation_fails(self):
        self.validate_mutation(lambda v: v["join_trials"][0].update({"error_code": ""}))

    def test_root_cause_mutation_fails(self):
        self.validate_mutation(lambda v: v["root_cause_boundary"].update({"classification": "normal_width"}))

    def test_authorization_mutation_fails(self):
        self.validate_mutation(lambda v: v["authorization"].update({"gpu_capture_authorized": True}))


if __name__ == "__main__":
    unittest.main()
