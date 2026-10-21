import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import validate_phase2_v10i_formal_six_scene_v3_failure_receipt as receipt


class Phase2V10iFormalV3FailureReceiptTests(unittest.TestCase):
    def setUp(self):
        self.value = json.loads(receipt.DEFAULT_RECEIPT.read_text(encoding="utf-8"))

    def validate_mutation(self, mutate):
        value = copy.deepcopy(self.value)
        mutate(value)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "receipt.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            return receipt.validate_receipt(path, enforce_frozen_sha=False)

    def test_frozen_receipt_and_consumed_marker_are_exact(self):
        result = receipt.validate_receipt()
        self.assertEqual(result["status"], "PASS_FROZEN_V3_FAILURE_RECEIPT")
        self.assertEqual(result["failure_receipt_sha256"],
                         receipt.EXPECTED_RECEIPT_SHA256)
        self.assertEqual(result["consumed_marker_sha256"],
                         receipt.EXPECTED_CONSUMED_MARKER_SHA256)
        self.assertFalse(result["formal_pass"])

    def test_mutated_receipt_sha_rejects_before_semantics(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "receipt.json"
            path.write_text(json.dumps(self.value), encoding="utf-8")
            with self.assertRaisesRegex(receipt.FailureReceiptError,
                                        "receipt SHA"):
                receipt.validate_receipt(path)

    def test_bad_consumed_marker_binding_is_rejected(self):
        with self.assertRaisesRegex(receipt.FailureReceiptError,
                                    "consumed marker: SHA mismatch"):
            self.validate_mutation(lambda value: value["authorization"].update(
                consumed_marker_sha256="0" * 64))

    def test_bad_ppm_binding_is_rejected(self):
        with self.assertRaisesRegex(receipt.FailureReceiptError,
                                    "low_aim PPM: SHA mismatch"):
            self.validate_mutation(lambda value: value["frozen_bindings"].update(
                low_aim_ppm_sha256="0" * 64))

    def test_bad_counts_are_rejected(self):
        with self.assertRaisesRegex(receipt.FailureReceiptError,
                                    "failure counts"):
            self.validate_mutation(lambda value: value["counts"].update(
                unrun_scenes=3))

    def test_bad_scene_status_is_rejected(self):
        def mutate(value):
            value["scene_results_in_order"][2]["status"] = "PASS"
        with self.assertRaisesRegex(receipt.FailureReceiptError,
                                    "scene result order/status"):
            self.validate_mutation(mutate)

    def test_validator_is_cpu_only_and_read_only(self):
        source = Path(receipt.__file__).read_text(encoding="utf-8")
        for forbidden in ("import subprocess", "Popen(", "AutomationClient",
                          "Cocoa", "OpenGL", "screenshot("):
            self.assertNotIn(forbidden, source)


if __name__ == "__main__":
    unittest.main()
