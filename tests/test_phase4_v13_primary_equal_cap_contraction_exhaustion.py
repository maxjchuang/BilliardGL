import copy
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/validate_phase4_v13_primary_equal_cap_contraction_exhaustion.py"
SPEC = importlib.util.spec_from_file_location("v13_primary_validator", SCRIPT)
VALIDATOR = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(VALIDATOR)
RECEIPT = ROOT / "docs/art/phase4-v13-primary-equal-cap-contraction-exhaustion-v1/primary-exhaustion-receipt-v1.json"


class V13PrimaryExhaustionTests(unittest.TestCase):
    def setUp(self):
        self.value = json.loads(RECEIPT.read_text())

    def reject(self, mutation):
        value = copy.deepcopy(self.value)
        mutation(value)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "receipt.json"
            path.write_text(json.dumps(value))
            self.assertTrue(VALIDATOR.validate(path))

    def test_canonical(self):
        self.assertEqual([], VALIDATOR.validate(RECEIPT))

    def test_skip_candidate_rejected(self):
        self.reject(lambda v: v["ordered_primary_sweep"].pop(1))

    def test_cap_annulus_mutation_rejected(self):
        self.reject(lambda v: v["ordered_primary_sweep"][1].update({"inner_cap_px": 3.0}))

    def test_candidate_false_pass_rejected(self):
        self.reject(lambda v: v["ordered_primary_sweep"][2].update({"exit_code": 0}))

    def test_halo_substitution_rejected(self):
        self.reject(lambda v: v["shared_failure_metrics"].update({"halo_substitution_forbidden": False}))

    def test_core_threshold_relaxation_rejected(self):
        self.reject(lambda v: v["shared_failure_metrics"].update({"required_actual_core_largest_8_connected": 1}))

    def test_source_variant_mutation_rejected(self):
        self.reject(lambda v: v["ordered_primary_sweep"][1].update({"variant_source_sha256": "0" * 64}))

    def test_dash_pattern_mutation_rejected(self):
        self.reject(lambda v: v["unchanged_contract"].update({"dash_off_px": 13.0}))

    def test_fallback_claim_rejected(self):
        self.reject(lambda v: v["fallback_boundary"].update({"fallback_implemented": True}))

    def test_authorization_rejected(self):
        self.reject(lambda v: v["authorization"].update({"gpu_capture_authorized": True}))


if __name__ == "__main__":
    unittest.main()
