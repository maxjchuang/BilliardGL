import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import (
    validate_phase5_rules_decision_ui_v3_targeted_preordinal_failure_receipt
    as receipt,
)


class Phase5RulesUiV3PreordinalFailureReceiptTests(unittest.TestCase):
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

    def test_frozen_failure_is_exact_zero_of_28(self) -> None:
        result = receipt.validate_receipt()
        self.assertEqual(
            result["status"],
            "PASS_FROZEN_PHASE5_V3_TARGETED_PREORDINAL_FAILURE")
        self.assertEqual(result["failure_receipt_sha256"],
                         receipt.EXPECTED_RECEIPT_SHA256)
        self.assertEqual(result["completed_cases"], 0)
        self.assertEqual(result["failed_capture_cases"], 0)
        self.assertEqual(result["unrun_cases"], 28)
        self.assertFalse(result["runner_physical_consumption"])
        self.assertTrue(result["authorization_reuse_forbidden"])
        self.assertFalse(result["gpu_rerun_authorized"])

    def test_mutated_receipt_sha_rejects_before_semantics(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "receipt.json"
            path.write_text(json.dumps(self.value), encoding="utf-8")
            with self.assertRaisesRegex(
                    receipt.FailureReceiptError, "receipt SHA"):
                receipt.validate_receipt(path)

    def test_false_consumed_marker_claim_rejected(self) -> None:
        with self.assertRaisesRegex(
                receipt.FailureReceiptError, "consumed marker"):
            self.validate_mutation(
                lambda value: value["consumed_marker_audit"].update(
                    exists=True, sha256="0" * 64, mode_octal="0600"))

    def test_bad_counts_rejected(self) -> None:
        with self.assertRaisesRegex(receipt.FailureReceiptError,
                                    "failure counts"):
            self.validate_mutation(
                lambda value: value["counts"].update(completed_cases=1,
                                                      unrun_cases=27))

    def test_bad_authorization_mode_rejected(self) -> None:
        with self.assertRaisesRegex(receipt.FailureReceiptError,
                                    "permissions/metadata"):
            self.validate_mutation(
                lambda value: value["authorization"].update(
                    mode_octal="0644"))

    def test_bad_missing_field_order_rejected(self) -> None:
        with self.assertRaisesRegex(receipt.FailureReceiptError,
                                    "runner-required missing fields"):
            self.validate_mutation(
                lambda value: value["root_cause"].update(
                    signed_authorization_missing_fields_in_runner_check_order=[
                        "shell_token_sha256", "hud_token_sha256",
                        "source_binding_manifest_sha256"]))

    def test_token_sha_drift_claim_rejected(self) -> None:
        with self.assertRaisesRegex(receipt.FailureReceiptError,
                                    "hud_token: SHA mismatch"):
            self.validate_mutation(
                lambda value: value["root_cause"]["hud_token"].update(
                    current_sha256="0" * 64))

    def test_promotion_or_rerun_claim_rejected(self) -> None:
        with self.assertRaisesRegex(receipt.FailureReceiptError,
                                    "evidence boundary"):
            self.validate_mutation(
                lambda value: value["evidence_boundary"].update(
                    rerun_authorized=True))

    def test_validator_is_cpu_only_and_read_only(self) -> None:
        source = Path(receipt.__file__).read_text(encoding="utf-8")
        for forbidden in ("import subprocess", "Popen(", "subprocess.run",
                          "Cocoa", "OpenGL", "screenshot("):
            self.assertNotIn(forbidden, source)


if __name__ == "__main__":
    unittest.main()

