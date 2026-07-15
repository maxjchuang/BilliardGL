import hashlib
import importlib
import importlib.util
import json
import unittest
from pathlib import Path

from tools.physics_validation.data_lifecycle import load_data_lifecycle


ROOT = Path(__file__).resolve().parents[2]
STATUS = ROOT / "tests/physics_validation/validation_data_status.json"
SUDO = ROOT / "tests/physics_validation/reference_data/sudo_2002"
V2 = ROOT / "physics_models/candidates/phase3_integrated_v2"
REJECTION = ROOT / "physics_models/promotion/phase3_integrated_v2_rejection.json"
TRANSITION = ROOT / "physics_models/promotion/sudo_2002_spent_transition.json"
REJECTING_REVISION = "948c93678d4c459a841131b2d6a29acf35a902ec"
IMMUTABLE_DIGESTS = {
    "freeze.json": "6e297a8c56eadc66d1f00f73fbede4c809e293f71d205c3d0c68769fbd79667b",
    "confirmation/sudo_2002/validation_receipt.json":
        "0ce34e3343c9707c2c9f17beeddcac983c1133cf8f13187fc2fcbe77300e9766",
    "confirmation_consumption.json":
        "f0aa4080079567fd7f0b5b963fd9beb25bb89e23b7a8613a1af2ebfd8309e770",
}
SUDO_MANIFEST_SHA256 = (
    "8cf1dcff355b26827afc50e3974576cdcf14b859bceb3990a6f74d37e2b67441")


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


class Phase3V3LifecycleTests(unittest.TestCase):
    def test_sudo_is_spent_without_changing_original_evidence(self):
        entry = load_data_lifecycle(STATUS).entry("sudo_2002", "1.0.0")
        self.assertEqual(
            (entry.calibration_status, entry.holdout_status),
            ("spent", "spent"),
        )
        self.assertEqual(digest(SUDO / "manifest.json"), SUDO_MANIFEST_SHA256)
        self.assertEqual(
            {relative: digest(V2 / relative) for relative in IMMUTABLE_DIGESTS},
            IMMUTABLE_DIGESTS,
        )

    def test_transition_builder_binds_the_rejected_v2_attempt(self):
        module_name = "tools.physics_validation.successor_lifecycle"
        self.assertIsNotNone(importlib.util.find_spec(module_name))
        module = importlib.import_module(module_name)
        rejection = module.build_rejection(ROOT)
        transition = module.build_spent_transition(
            ROOT, "sudo_2002", "1.0.0", "phase3_integrated_v2")
        self.assertEqual(rejection["status"], "REJECTED")
        self.assertEqual(rejection["rejecting_source_revision"],
                         REJECTING_REVISION)
        self.assertEqual(rejection["receipt_sha256"],
                         IMMUTABLE_DIGESTS[
                             "confirmation/sudo_2002/validation_receipt.json"])
        self.assertEqual(transition["from"], "confirmation")
        self.assertEqual(transition["to"], "spent")
        self.assertEqual(transition["receipt_sha256"],
                         rejection["receipt_sha256"])
        self.assertEqual(transition["ledger_sha256"],
                         rejection["ledger_sha256"])
        self.assertEqual(transition["package_manifest_sha256"],
                         SUDO_MANIFEST_SHA256)

    def test_committed_records_are_canonical_builder_outputs(self):
        self.assertTrue(REJECTION.is_file())
        self.assertTrue(TRANSITION.is_file())
        module = importlib.import_module(
            "tools.physics_validation.successor_lifecycle")
        expected = {
            REJECTION: module.build_rejection(ROOT),
            TRANSITION: module.build_spent_transition(
                ROOT, "sudo_2002", "1.0.0", "phase3_integrated_v2"),
        }
        for path, document in expected.items():
            canonical = json.dumps(
                document, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
            self.assertEqual(path.read_text(encoding="utf-8"), canonical)


if __name__ == "__main__":
    unittest.main()
