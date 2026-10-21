import copy
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/validate_phase4_v14_cap_mask_premise_falsification.py"
SPEC = importlib.util.spec_from_file_location("v14_premise_validator", SCRIPT)
VALIDATOR = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(VALIDATOR)
RECEIPT = ROOT / "docs/art/phase4-v14-cap-mask-premise-falsification-v1/premise-falsification-receipt-v1.json"


class V14PremiseFalsificationTests(unittest.TestCase):
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

    def test_gate_binding_mutation_rejected(self):
        self.reject(lambda v: v["frozen_bindings"]["v14_design_gate"].update({"sha256": "0" * 64}))

    def test_coordinate_mutation_rejected(self):
        self.reject(lambda v: v["exact_fixture"].update({"blocking_coordinate": [941, 525]}))

    def test_cap_discard_false_zero_rejected(self):
        self.reject(lambda v: v["observed_metrics_per_palette"].update({"tagged_predicted_inner_cap_discards": 0}))

    def test_predicted_core_count_mutation_rejected(self):
        self.reject(lambda v: v["observed_metrics_per_palette"].update({"final_source_mask_pixels_owned_by_predicted_core": 80}))

    def test_cap_final_owner_false_positive_rejected(self):
        self.reject(lambda v: v["observed_metrics_per_palette"].update({"final_source_mask_pixels_owned_by_tagged_predicted_inner_cap": 1}))

    def test_tick_misclassification_erasure_rejected(self):
        self.reject(lambda v: v["observed_metrics_per_palette"].update({"final_actual_tick_pixels": 0}))

    def test_false_route_body_success_rejected(self):
        self.reject(lambda v: v["observed_metrics_per_palette"].update({"final_actual_route_body_pixels": 2}))

    def test_premise_success_claim_rejected(self):
        self.reject(lambda v: v["premise_falsification"].update({"v14_cap_only_strategy_can_meet_required_result": True}))

    def test_revert_waiver_rejected(self):
        self.reject(lambda v: v["revert_boundary"].update({"partial_v14_production_delta_must_be_absent": False}))

    def test_authorization_rejected(self):
        self.reject(lambda v: v["authorization"].update({"gpu_authorized": True}))


if __name__ == "__main__":
    unittest.main()
