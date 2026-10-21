import copy
import importlib.util
import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
VALIDATOR_PATH = ROOT / "scripts/validate_phase4_v15_broadcast_overwrite_provenance_diagnostic.py"
SPEC = importlib.util.spec_from_file_location("phase4_v15_validator", VALIDATOR_PATH)
VALIDATOR = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(VALIDATOR)


class Phase4V15BroadcastOverwriteProvenanceDiagnosticTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.receipt = json.loads(VALIDATOR.RECEIPT.read_text())
        cls.ledger = json.loads((VALIDATOR.EVIDENCE /
            "provenance-ledger-v1.json").read_text())

    def errors(self, receipt=None, ledger=None):
        return VALIDATOR.semantic_errors(
            copy.deepcopy(receipt if receipt is not None else self.receipt),
            copy.deepcopy(ledger if ledger is not None else self.ledger))

    def test_01_canonical_full_validator(self):
        self.assertEqual([], VALIDATOR.validate(run_replay=True))

    def test_02_ledger_schema_mutation(self):
        ledger = copy.deepcopy(self.ledger)
        ledger["schema"] = "mutated"
        self.assertIn("ledger_schema_invalid", self.errors(ledger=ledger))

    def test_03_capture_mutation(self):
        ledger = copy.deepcopy(self.ledger)
        ledger["capture_id"] = "camera_overhead_1080"
        self.assertIn("capture_id_invalid", self.errors(ledger=ledger))

    def test_04_source_inventory_mutation(self):
        ledger = copy.deepcopy(self.ledger)
        ledger["ordered_source_count"] = 80
        self.assertIn("source_inventory_invalid", self.errors(ledger=ledger))

    def test_05_palette_mutation(self):
        ledger = copy.deepcopy(self.ledger)
        ledger["palettes"][1]["palette"] = "standard"
        self.assertIn("palette_inventory_invalid", self.errors(ledger=ledger))

    def test_06_blocking_partition_mutation(self):
        ledger = copy.deepcopy(self.ledger)
        ledger["palettes"][0]["blocking_predicted_core"] = 80
        self.assertTrue(any(error.startswith("blocking_partition_invalid")
            for error in self.errors(ledger=ledger)))

    def test_07_nonzero_foreign_mutation(self):
        ledger = copy.deepcopy(self.ledger)
        ledger["palettes"][0]["foreign_evidence_count"] = 1
        self.assertTrue(any(error.startswith("ownership_error_nonzero")
            for error in self.errors(ledger=ledger)))

    def test_08_required_field_mutation(self):
        ledger = copy.deepcopy(self.ledger)
        del ledger["palettes"][0]["samples"][0]["prior_actual_primitive_id"]
        self.assertTrue(any(error.startswith("sample_fields_missing")
            for error in self.errors(ledger=ledger)))

    def test_09_sample_position_mutation(self):
        ledger = copy.deepcopy(self.ledger)
        ledger["palettes"][0]["samples"][0][
            "sample_index_and_position"]["position"] = [0.25, 0.25]
        self.assertTrue(any(error.startswith("sample_position_invalid")
            for error in self.errors(ledger=ledger)))

    def test_10_route_body_mutation(self):
        ledger = copy.deepcopy(self.ledger)
        ledger["palettes"][0]["samples"][0][
            "prior_actual_route_body_boolean"] = False
        self.assertTrue(any(error.startswith("route_body_boolean_invalid")
            for error in self.errors(ledger=ledger)))

    def test_11_fragment_class_mutation(self):
        ledger = copy.deepcopy(self.ledger)
        ledger["palettes"][0]["samples"][0][
            "predicted_fragment_class"] = "color_selected_cap"
        self.assertTrue(any(error.startswith("classification_invalid")
            for error in self.errors(ledger=ledger)))

    def test_12_repair_id_mutation(self):
        ledger = copy.deepcopy(self.ledger)
        ledger["palettes"][0]["repair_probe_owner_chains"][0][
            "repair_probe_id"] = "0" * 64
        self.assertTrue(any(error.startswith("repair_chain_inventory_invalid")
            for error in self.errors(ledger=ledger)))

    def test_13_repair_owner_mutation(self):
        ledger = copy.deepcopy(self.ledger)
        ledger["palettes"][0]["repair_probe_owner_chains"][0][
            "final_owner_kind"] = ""
        self.assertTrue(any(error.startswith("repair_chain_owner_missing")
            for error in self.errors(ledger=ledger)))

    def test_14_gap_candidate_mutation(self):
        ledger = copy.deepcopy(self.ledger)
        row = next(row for row in ledger["palettes"][0]["samples"]
            if abs(row["physical_pixel"][0] - 940) <= 8 and
            abs(row["physical_pixel"][1] - 525) <= 8)
        row["would_be_candidate_for_future_gap_only_non_overwrite"] = True
        self.assertIn("gap_partition_invalid:standard:940,525",
            self.errors(ledger=ledger))

    def test_15_standard_cvd_topology_mutation(self):
        ledger = copy.deepcopy(self.ledger)
        ledger["palettes"][1]["samples"][0]["final_predicted_primitive_id"] += "x"
        self.assertIn("standard_cvd_topology_not_identical",
            self.errors(ledger=ledger))

    def test_16_conclusion_mutation(self):
        receipt = copy.deepcopy(self.receipt)
        receipt["diagnostic_conclusion"]["visual_fix_authorized"] = True
        self.assertIn("conclusion_not_fail_closed",
            self.errors(receipt=receipt))


if __name__ == "__main__":
    unittest.main()
