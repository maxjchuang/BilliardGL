import hashlib
import json
import unittest
from pathlib import Path

from tools.physics_validation.confirmation_readiness import build_rejection
from tools.physics_validation.confirmation_transaction import (
    validate_confirmation_access_from_freeze,
)


ROOT = Path(__file__).resolve().parents[2]
FREEZE = ROOT / "physics_models/candidates/phase3_integrated_v3/freeze.json"
INVENTORY = ROOT / "physics_models/promotion/phase3_candidates_v3.json"
FULL_GAME = ROOT / "physics_models/candidates/phase3_integrated_v3/full_game"
READINESS = ROOT / ("physics_models/candidates/phase3_integrated_v3/"
                    "confirmation_readiness.json")
LEDGER = ROOT / ("physics_models/candidates/phase3_integrated_v3/"
                 "confirmation_consumption.json")
REJECTION = ROOT / ("physics_models/promotion/"
                    "phase3_integrated_v3_rejection.json")
DERBY = LEDGER.parent / "confirmation/derby_fuller_1999"


class Phase3V3ConfirmationTests(unittest.TestCase):
    def test_committed_readiness_preserves_the_pre_attempt_checkpoint(self):
        committed = json.loads(READINESS.read_text(encoding="utf-8"))
        self.assertEqual(committed["status"], "READY")
        self.assertEqual(committed["candidate_id"], "phase3_integrated_v3")
        self.assertEqual(committed["failures"], [])

    def test_both_packages_are_declared_but_unconsumed(self):
        readiness = json.loads(READINESS.read_text(encoding="utf-8"))
        self.assertEqual(set(readiness["confirmation_packages"]),
                         {"derby_fuller_1999", "han_2005"})
        self.assertTrue(all(
            package["attempt"] == "UNOPENED"
            for package in readiness["confirmation_packages"].values()))
        self.assertTrue(LEDGER.exists())

    def test_derby_is_consumed_and_han_was_not_executed(self):
        self.assertIn("already consumed", " ".join(
            validate_confirmation_access_from_freeze(
                ROOT, FREEZE, "derby_fuller_1999", LEDGER)))
        ledger = json.loads(LEDGER.read_text(encoding="utf-8"))
        records = ledger["attempts"] + ledger["records"]
        self.assertFalse(any(
            record.get("dataset_id") == "han_2005" for record in records))
        self.assertFalse((LEDGER.parent / "confirmation/han_2005").exists())

    def test_readiness_binds_frozen_and_full_game_bytes(self):
        readiness = json.loads(READINESS.read_text(encoding="utf-8"))
        self.assertEqual(
            readiness["freeze_sha256"],
            hashlib.sha256(FREEZE.read_bytes()).hexdigest())
        self.assertEqual(
            readiness["full_game_matrix_summary_sha256"],
            hashlib.sha256((FULL_GAME / "matrix_summary.json").read_bytes())
            .hexdigest())
        self.assertEqual(readiness["full_game_case_count"], 12)

    def test_failed_receipt_and_rejection_are_hash_bound(self):
        receipt = json.loads(
            (DERBY / "validation_receipt.json").read_text(encoding="utf-8"))
        self.assertEqual(receipt["result"], "FAILED")
        self.assertEqual(receipt["failure_code"], "FAILED_EVALUATOR_EXCEPTION")
        self.assertEqual(
            json.loads((DERBY / "failure.json").read_text(encoding="utf-8"))
            ["message"],
            "invalid_scenario: expectations must be a nonempty array")
        rejection = json.loads(REJECTION.read_text(encoding="utf-8"))
        self.assertEqual(rejection, build_rejection(ROOT, FREEZE, READINESS))
        self.assertEqual(rejection["disposition"], "REJECTED")
        self.assertEqual(rejection["han_2005"], "NOT_EXECUTED")


if __name__ == "__main__":
    unittest.main()
