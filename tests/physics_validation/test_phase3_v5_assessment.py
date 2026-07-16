import json
import subprocess
import sys
import unittest
from pathlib import Path

from tools.physics_validation.phase3_v5_assessment import (
    build_final_assessment,
    validate_transaction_order,
)


ROOT = Path(__file__).resolve().parents[2]
CANDIDATE = ROOT / "physics_models/candidates/phase3_integrated_v5"
FINAL = CANDIDATE / "final_assessment.json"


def attempt(package, identity):
    return {
        "attempt_id": identity,
        "dataset_id": package,
        "state": "STARTED",
    }


def record(package, identity, result):
    return {
        "attempt_id": identity,
        "dataset_id": package,
        "result": result,
    }


def ledger(attempts, records):
    return {"attempts": attempts, "records": records, "schema_version": 1}


class Phase3V5AssessmentTests(unittest.TestCase):
    def test_live_cross_failure_is_the_canonical_final_rejection(self):
        generated = build_final_assessment(ROOT)
        committed = json.loads(FINAL.read_text(encoding="utf-8"))
        self.assertEqual(generated, committed)
        self.assertEqual(generated["disposition"], "REJECTED")
        self.assertEqual(set(generated["confirmations"]),
                         {"cross_2016_newtons_cradle"})
        self.assertEqual(generated["han_2005"], "NOT_EXECUTED")
        self.assertEqual(
            generated["root_cause"]["category"],
            "CONFIRMATION_EVALUATION_CONTRACT_INTEGRATION_FAILURE")

    def test_cross_success_cannot_accept_without_han(self):
        cross = "1" * 64
        with self.assertRaisesRegex(ValueError, "Han confirmation is absent"):
            validate_transaction_order(ledger(
                [attempt("cross_2016_newtons_cradle", cross)],
                [record("cross_2016_newtons_cradle", cross, "PASSED")],
            ))

    def test_cross_failure_forbids_han(self):
        cross = "1" * 64
        han = "2" * 64
        with self.assertRaisesRegex(ValueError,
                                    "Han must be absent after failed Cross"):
            validate_transaction_order(ledger(
                [attempt("cross_2016_newtons_cradle", cross),
                 attempt("han_2005", han)],
                [record("cross_2016_newtons_cradle", cross, "FAILED"),
                 record("han_2005", han, "PASSED")],
            ))

    def test_duplicate_or_unfinalized_attempt_is_rejected(self):
        cross = "1" * 64
        with self.assertRaisesRegex(ValueError, "distinct and final"):
            validate_transaction_order(ledger(
                [attempt("cross_2016_newtons_cradle", cross),
                 attempt("cross_2016_newtons_cradle", cross)],
                [record("cross_2016_newtons_cradle", cross, "FAILED")],
            ))
        with self.assertRaisesRegex(ValueError, "distinct and final"):
            validate_transaction_order(ledger(
                [attempt("cross_2016_newtons_cradle", cross)], []))

    def test_final_assessment_binds_every_engineering_evidence_class(self):
        assessment = build_final_assessment(ROOT)
        evidence = assessment["engineering_evidence"]
        self.assertEqual(set(evidence), {
            "alciatore_regression", "clean_build", "convergence_contract",
            "freeze", "full_game", "inventory", "ordinary_equivalence",
            "performance_budget", "profile", "readiness",
        })
        self.assertEqual(len(evidence["clean_build"]["executable_sha256"]), 2)
        self.assertEqual(len(set(
            evidence["clean_build"]["executable_sha256"])), 1)
        self.assertTrue(evidence["full_game"]["passed"])

    def test_release_gate_accepts_only_the_canonical_safe_rejection(self):
        script = ROOT / "scripts/check_phase3_physics_release.py"
        rejected = subprocess.run(
            [sys.executable, str(script), "--root", str(ROOT)],
            capture_output=True, text=True)
        self.assertEqual(rejected.returncode, 0, rejected.stderr)
        self.assertIn("REJECTED (not promoted)", rejected.stdout)
        legacy = subprocess.run(
            [sys.executable, str(script), "--root", str(ROOT),
             "--release", str(ROOT /
                "physics_models/promotion/phase3_release_v1.json")],
            capture_output=True, text=True)
        self.assertNotEqual(legacy.returncode, 0)
        self.assertIn("receipt did not pass", legacy.stderr)


if __name__ == "__main__":
    unittest.main()
