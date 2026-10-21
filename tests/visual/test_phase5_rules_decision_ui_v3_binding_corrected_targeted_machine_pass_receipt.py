import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import (
    validate_phase5_rules_decision_ui_v3_binding_corrected_targeted_machine_pass_receipt
    as receipt,
)


class Phase5RulesUiV3CorrectedMachinePassReceiptTests(unittest.TestCase):
    def setUp(self) -> None:
        self.value = json.loads(
            receipt.DEFAULT_RECEIPT.read_text(encoding="utf-8"))

    def validate_mutation(self, mutate):
        value = copy.deepcopy(self.value)
        mutate(value)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "receipt.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            return receipt.validate_receipt(path, enforce_frozen_sha=False)

    def test_frozen_exact_28_machine_pass(self) -> None:
        result = receipt.validate_receipt()
        self.assertEqual(
            result["status"],
            "PASS_FROZEN_PHASE5_V3_BINDING_CORRECTED_TARGETED_MACHINE_28")
        self.assertEqual(result["machine_pass_receipt_sha256"],
                         receipt.EXPECTED_RECEIPT_SHA256)
        self.assertEqual(result["case_count"], 28)
        self.assertEqual(result["ppm_count"], 28)
        self.assertEqual(result["png_count"], 28)
        self.assertFalse(result["legacy_stdout_used_as_count"])
        self.assertEqual(result["technical_art_review_completed"], 0)
        self.assertFalse(result["product_exposed"])
        self.assertFalse(result["golden_promotion"])
        self.assertFalse(result["m2_promotion"])

    def test_mutated_receipt_sha_rejects_first(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "receipt.json"
            path.write_text(json.dumps(self.value), encoding="utf-8")
            with self.assertRaisesRegex(
                    receipt.MachinePassReceiptError, "receipt SHA"):
                receipt.validate_receipt(path)

    def test_bad_authorization_binding_rejected(self) -> None:
        with self.assertRaisesRegex(
                receipt.MachinePassReceiptError, "authorization"):
            self.validate_mutation(
                lambda value: value["lineage"].update(
                    authorization_sha256="0" * 64))

    def test_bad_marker_binding_rejected(self) -> None:
        with self.assertRaisesRegex(
                receipt.MachinePassReceiptError, "consumed marker"):
            self.validate_mutation(
                lambda value: value["lineage"].update(
                    consumed_marker_sha256="0" * 64))

    def test_bad_counts_rejected(self) -> None:
        with self.assertRaisesRegex(receipt.MachinePassReceiptError,
                                    "verified counts"):
            self.validate_mutation(
                lambda value: value["verified_counts"].update(
                    result_rows=60))

    def test_bad_dimensions_rejected(self) -> None:
        with self.assertRaisesRegex(receipt.MachinePassReceiptError,
                                    "decoded dimension counts"):
            self.validate_mutation(
                lambda value: value[
                    "decoded_physical_dimension_counts"].update(
                        {"1280x720": 13}))

    def test_bad_stable_aggregate_rejected(self) -> None:
        with self.assertRaisesRegex(receipt.MachinePassReceiptError,
                                    "stable evidence aggregates"):
            self.validate_mutation(
                lambda value: value["stable_aggregates"].update(
                    output_aggregate_sha256="0" * 64))

    def test_legacy_stdout_cannot_be_used_as_count(self) -> None:
        with self.assertRaisesRegex(receipt.MachinePassReceiptError,
                                    "non-authoritative"):
            self.validate_mutation(
                lambda value: value["runner_stdout_observation"].update(
                    used_as_case_count=True))

    def test_ta_or_promotion_claim_rejected(self) -> None:
        with self.assertRaisesRegex(receipt.MachinePassReceiptError,
                                    "machine-only claims"):
            self.validate_mutation(
                lambda value: value["claims"].update(
                    technical_art_review_completed=28,
                    golden_promotion=True))

    def test_bad_ppm_and_png_inputs_fail_closed(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            ppm = root / "bad.ppm"
            png = root / "bad.png"
            ppm.write_bytes(b"P3\n1 1\n255\n0 0 0\n")
            png.write_bytes(b"not-a-png")
            with self.assertRaises(receipt.MachinePassReceiptError):
                receipt.read_ppm(ppm)
            with self.assertRaises(receipt.MachinePassReceiptError):
                receipt.read_png_dimensions(png)

    def test_validator_is_cpu_only_and_read_only(self) -> None:
        source = Path(receipt.__file__).read_text(encoding="utf-8")
        for forbidden in ("import subprocess", "subprocess.run", "Popen(",
                          "Cocoa", "OpenGL", "screenshot(", "chmod(",
                          "write_bytes(", "write_text("):
            self.assertNotIn(forbidden, source)


if __name__ == "__main__":
    unittest.main()

