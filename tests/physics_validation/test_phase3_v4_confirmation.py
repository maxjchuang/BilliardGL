import json
import unittest
from pathlib import Path

from tools.physics_validation.confirmation_readiness import build_readiness


ROOT = Path(__file__).resolve().parents[2]
CANDIDATE = ROOT / "physics_models/candidates/phase3_integrated_v4"
FREEZE = CANDIDATE / "freeze.json"
INVENTORY = ROOT / "physics_models/promotion/phase3_candidates_v4.json"
FULL_GAME = CANDIDATE / "full_game"
PROOF = CANDIDATE / "confirmation_contract_proof.json"
READINESS = CANDIDATE / "confirmation_readiness.json"
LEDGER = CANDIDATE / "confirmation_consumption.json"


class Phase3V4ConfirmationTests(unittest.TestCase):
    def test_readiness_uses_v4_inventory_packages_without_consuming_them(self):
        readiness = build_readiness(
            ROOT, FREEZE, INVENTORY, FULL_GAME, PROOF)
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
        self.assertFalse(LEDGER.exists())
        self.assertFalse((CANDIDATE / "confirmation").exists())

    def test_committed_readiness_matches_recomputation(self):
        self.assertEqual(
            json.loads(READINESS.read_text(encoding="utf-8")),
            build_readiness(ROOT, FREEZE, INVENTORY, FULL_GAME, PROOF),
        )

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
