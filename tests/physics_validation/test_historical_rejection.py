import importlib
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
REJECTION = ROOT / "physics_models/promotion/phase3_release_v1_rejection.json"
MODULE = "tools.physics_validation.historical_rejection"


def load_rejection_module(test_case):
    test_case.assertIsNotNone(
        importlib.util.find_spec(MODULE),
        "historical rejection validator is not implemented",
    )
    return importlib.import_module(MODULE)


class HistoricalRejectionTests(unittest.TestCase):
    def test_committed_rejection_binds_original_release(self):
        module = load_rejection_module(self)
        self.assertEqual(module.validate_historical_rejection(REJECTION, ROOT), [])

    def test_changed_release_hash_is_rejected(self):
        module = load_rejection_module(self)
        document = json.loads(REJECTION.read_text(encoding="utf-8"))
        document["rejected_release_sha256"] = "0" * 64
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "rejection.json"
            path.write_text(json.dumps(document), encoding="utf-8")
            failures = module.validate_historical_rejection(path, ROOT)
        self.assertIn("rejected release hash mismatch", failures)

    def test_every_failed_receipt_and_report_failure_is_named(self):
        module = load_rejection_module(self)
        document = json.loads(REJECTION.read_text(encoding="utf-8"))
        self.assertEqual(
            set(document["failed_receipts"]),
            module.discover_failed_v1_receipts(ROOT),
        )
        self.assertEqual(
            document["unallowlistable_integration_failures"],
            module.discover_v1_accounting(ROOT)["unallowlistable_integration_failures"],
        )
        self.assertEqual(
            document["new_model_mismatches"],
            module.discover_v1_accounting(ROOT)["new_model_mismatches"],
        )


if __name__ == "__main__":
    unittest.main()
