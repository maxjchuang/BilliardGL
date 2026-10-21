import copy
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/validate_phase4_v14_cap_mask_premise_falsification_v2.py"
SPEC = importlib.util.spec_from_file_location("v14_premise_v2_validator", SCRIPT)
VALIDATOR = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(VALIDATOR)
RECEIPT = ROOT / "docs/art/phase4-v14-cap-mask-premise-falsification-v2/premise-falsification-receipt-v2.json"


class V14PremiseFalsificationV2Tests(unittest.TestCase):
    def setUp(self):
        self.value = json.loads(RECEIPT.read_text())

    def reject(self, mutation):
        value = copy.deepcopy(self.value)
        mutation(value)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "receipt.json"
            path.write_text(json.dumps(value))
            self.assertTrue(VALIDATOR.validate(path, recompile=False))

    def test_01_canonical_recompile_and_replay(self):
        self.assertEqual([], VALIDATOR.validate(RECEIPT, recompile=True))

    def test_02_v1_lineage_hash_mutation_rejected(self):
        self.reject(lambda v: v["successor_lineage"]["v1_receipt"].update(
            {"sha256": "0" * 64}))

    def test_03_exact_diagnostic_source_path_mutation_rejected(self):
        self.reject(lambda v: v["reproducible_evidence"]["diagnostic_source"].update(
            {"path": "tests/phase4_v12_broadcast_visual_repair_cpu_tests.cpp"}))

    def test_04_exact_diagnostic_source_hash_mutation_rejected(self):
        self.reject(lambda v: v["reproducible_evidence"]["diagnostic_source"].update(
            {"sha256": "f" * 64}))

    def test_05_candidate_source_hash_mutation_rejected(self):
        self.reject(lambda v: v["reproducible_evidence"]["candidate_sources"]
            ["trajectory_renderer_h"].update({"sha256": "1" * 64}))

    def test_06_restored_baseline_mutation_rejected(self):
        self.reject(lambda v: v["required_restored_production_baselines"]
            ["modern_renderer_cpp"].update({"sha256": "2" * 64}))

    def test_07_object_binding_mutation_rejected(self):
        self.reject(lambda v: v["reproducible_evidence"]["diagnostic_object"].update(
            {"sha256": "3" * 64}))

    def test_08_binary_binding_mutation_rejected(self):
        self.reject(lambda v: v["reproducible_evidence"]["diagnostic_binary"].update(
            {"sha256": "4" * 64}))

    def test_09_stderr_stream_mutation_rejected(self):
        self.reject(lambda v: v["reproducible_evidence"]["frozen_process_output"].update(
            {"stream": "stdout"}))

    def test_10_metrics_binding_mutation_rejected(self):
        self.reject(lambda v: v["reproducible_evidence"]["structured_metrics"].update(
            {"sha256": "5" * 64}))

    def test_11_compile_contract_binding_mutation_rejected(self):
        self.reject(lambda v: v["reproducible_evidence"]["compile_contract"].update(
            {"sha256": "6" * 64}))

    def test_12_fixture_coordinate_mutation_rejected(self):
        self.reject(lambda v: v["exact_fixture"].update(
            {"blocking_coordinate": [941, 525]}))

    def test_13_premise_partition_mutation_rejected(self):
        self.reject(lambda v: v["premise_falsification"].update(
            {"final_source_mask_pixels_owned_by_untagged_predicted_inner_body": 6}))

    def test_14_false_authorization_rejected(self):
        self.reject(lambda v: v["authorization"].update({"gpu_authorized": True}))


if __name__ == "__main__":
    unittest.main()
