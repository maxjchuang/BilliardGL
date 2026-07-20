import hashlib
import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CANDIDATE = ROOT / "physics_models/candidates/phase3_integrated_v4"
PROOF = CANDIDATE / "confirmation_contract_proof.json"
READINESS = CANDIDATE / "confirmation_readiness.json"
LEDGER = CANDIDATE / "confirmation_consumption.json"


class Phase3V4ConfirmationTests(unittest.TestCase):
    def test_committed_readiness_preserves_the_pre_attempt_checkpoint(self):
        readiness = json.loads(READINESS.read_text(encoding="utf-8"))
        self.assertEqual(readiness["status"], "READY")
        self.assertEqual(readiness["failures"], [])
        self.assertTrue(readiness["checks"]["confirmation_contract_real_path"])
        self.assertEqual(
            set(readiness["confirmation_packages"]),
            {"alciatore_2005_tp_a15", "han_2005"},
        )
        self.assertTrue(all(
            package["attempt"] == "UNOPENED" and package["ready"]
            for package in readiness["confirmation_packages"].values()))

    def test_alciatore_is_consumed_and_han_remains_unopened(self):
        ledger = json.loads(LEDGER.read_text(encoding="utf-8"))
        entries = ledger["attempts"] + ledger["records"]
        self.assertTrue(any(
            row.get("dataset_id") == "alciatore_2005_tp_a15"
            for row in entries))
        self.assertFalse(any(
            row.get("dataset_id") == "han_2005" for row in entries))
        self.assertFalse((CANDIDATE / "confirmation/han_2005").exists())

    def test_failed_receipt_binds_complete_deterministic_physical_output(self):
        output = CANDIDATE / "confirmation/alciatore_2005_tp_a15"
        receipt = json.loads(
            (output / "validation_receipt.json").read_text(encoding="utf-8"))
        ledger = json.loads(LEDGER.read_text(encoding="utf-8"))
        self.assertEqual(receipt["result"], "FAILED")
        self.assertIn(receipt, ledger["records"])
        for relative, expected in receipt["files"].items():
            self.assertEqual(
                hashlib.sha256((output / relative).read_bytes()).hexdigest(),
                expected, relative)
        executions = output / "execution"
        first = sorted(executions.glob("*-first.json"))
        second = sorted(executions.glob("*-second.json"))
        self.assertEqual(len(first), 9)
        self.assertEqual(len(second), 9)
        for first_path, second_path in zip(first, second):
            self.assertEqual(
                first_path.name.replace("-first", "-second"), second_path.name)
            self.assertEqual(first_path.read_bytes(), second_path.read_bytes())

    def test_failure_is_physical_not_execution_or_integrity_failure(self):
        report = json.loads((
            CANDIDATE / "confirmation/alciatore_2005_tp_a15/reference_report.json"
        ).read_text(encoding="utf-8"))
        metrics = report["summary_metrics"]
        self.assertEqual(report["result"], "FAILED")
        self.assertEqual(report["summary"], {
            "failed": 7, "known_model_mismatches": 0,
            "passed": 2, "points": 9,
        })
        self.assertTrue(metrics["contact_complete_passed"])
        self.assertTrue(metrics["finite_state_passed"])
        self.assertTrue(metrics["nonincreasing_total_energy_passed"])
        self.assertFalse(metrics["interior_rmse_passed"])
        self.assertFalse(metrics["interior_maximum_passed"])
        self.assertAlmostEqual(metrics["interior_rmse_degrees"],
                               38.753578370555665)
        self.assertAlmostEqual(
            metrics["interior_maximum_absolute_error_degrees"],
            69.90211689066608)

    def test_contract_proof_is_real_deterministic_and_uses_no_real_package(self):
        proof_text = PROOF.read_text(encoding="utf-8")
        proof = json.loads(proof_text)
        self.assertEqual(proof["result"], "PASSED")
        self.assertTrue(proof["parse_succeeded"])
        self.assertGreater(proof["frames"], 0)
        self.assertEqual(proof["first_trace_sha256"],
                         proof["second_trace_sha256"])
        self.assertFalse(proof.get("fixture_executor", False))
        self.assertNotIn("alciatore_2005_tp_a15", proof_text)
        self.assertNotIn("han_2005", proof_text)


if __name__ == "__main__":
    unittest.main()
