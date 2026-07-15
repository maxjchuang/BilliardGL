import hashlib
import importlib
import importlib.util
import json
import unittest
from pathlib import Path

from tools.physics_validation.data_lifecycle import load_data_lifecycle


ROOT = Path(__file__).resolve().parents[2]
STATUS = ROOT / "tests/physics_validation/validation_data_status.json"
V3 = ROOT / "physics_models/candidates/phase3_integrated_v3"
TRANSITION = ROOT / (
    "physics_models/promotion/phase3_integrated_v3_derby_spent_transition.json")
MODULE = "tools.physics_validation.phase3_v4_lifecycle"
V3_DIGESTS = {
    "freeze.json":
        "cc7cd5f2b583fc13550de1677fc5ea13c5460786794ef0bc7ff1364b834c9c08",
    "confirmation/derby_fuller_1999/failure.json":
        "07a3d014cdcfa2fb35ea4398769ca7848f6e0e30844735fe29e50538fee25c7b",
    "confirmation/derby_fuller_1999/validation_receipt.json":
        "c001f4693ab24cb50ef84b1434ab693355499b374cd388480064764a5b1d9ce7",
    "confirmation_consumption.json":
        "27239d6285f0cbf26593d5b9f49733882c4e627e0c675db00428a0b37cea3aa3",
}
REJECTION_SHA256 = (
    "0c8a179288fe712f292f1f61047be2ba4ac8f2f8b3f3bc828c55b5eb247062b5")
DERBY_MANIFEST_SHA256 = (
    "3e8c979e48e207317384dc63e47d4d5f28bae9b21cc8486183cd35ea9fe2b3d1")


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def lifecycle_module(test_case):
    test_case.assertIsNotNone(
        importlib.util.find_spec(MODULE),
        "phase3_v4_lifecycle must provide the reviewed Derby transition")
    return importlib.import_module(MODULE)


class Phase3V4LifecycleTests(unittest.TestCase):
    def test_derby_is_spent_and_v3_evidence_is_immutable(self):
        entry = load_data_lifecycle(STATUS).entry("derby_fuller_1999", "1.0.0")
        self.assertEqual(
            (entry.calibration_status, entry.holdout_status),
            ("spent", "spent"),
        )
        self.assertEqual(
            {relative: digest(V3 / relative) for relative in V3_DIGESTS},
            V3_DIGESTS,
        )
        self.assertEqual(
            digest(ROOT / "physics_models/promotion/"
                   "phase3_integrated_v3_rejection.json"),
            REJECTION_SHA256,
        )
        self.assertEqual(
            digest(ROOT / "tests/physics_validation/reference_data/"
                   "derby_fuller_1999/manifest.json"),
            DERBY_MANIFEST_SHA256,
        )

    def test_transition_binds_every_failed_attempt_artifact(self):
        transition = lifecycle_module(self).build_derby_spent_transition(ROOT)
        self.assertEqual(transition["from"], "confirmation")
        self.assertEqual(transition["to"], "spent")
        self.assertEqual(
            transition["failure_message"],
            "invalid_scenario: expectations must be a nonempty array",
        )
        self.assertEqual(set(transition["evidence_sha256"]), {
            "failure", "freeze", "ledger", "package_manifest", "receipt",
            "rejection",
        })
        self.assertEqual(
            transition["evidence_sha256"]["package_manifest"],
            DERBY_MANIFEST_SHA256,
        )

    def test_committed_transition_is_canonical_builder_output(self):
        module = lifecycle_module(self)
        self.assertTrue(TRANSITION.is_file())
        expected = json.dumps(
            module.build_derby_spent_transition(ROOT),
            ensure_ascii=False,
            indent=2,
            sort_keys=True,
        ) + "\n"
        self.assertEqual(TRANSITION.read_text(encoding="utf-8"), expected)


if __name__ == "__main__":
    unittest.main()
