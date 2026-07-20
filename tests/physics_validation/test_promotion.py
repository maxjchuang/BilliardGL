import json
import tempfile
import unittest
from pathlib import Path

from tools.physics_validation.promotion import _validate_receipt, validate_promotion_manifest


ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "physics_models/promotion/phase3_candidates_v1.json"
STRICT_RECEIPT = ROOT / "tests/physics_validation/fixtures/promotion_v2.json"


class PromotionInventoryTests(unittest.TestCase):
    def test_committed_v1_inventory_is_complete_but_not_promotable(self):
        failures = validate_promotion_manifest(MANIFEST, ROOT)
        failed_receipts = [value for value in failures if "receipt did not pass" in value]
        self.assertEqual(len(failed_receipts), 4)
        self.assertFalse(any("missing artifact" in value for value in failures))
        self.assertFalse(any("artifact hash mismatch" in value for value in failures))

    def test_failed_receipt_is_never_promotable(self):
        document = json.loads(MANIFEST.read_text(encoding="utf-8"))
        document["candidates"][0]["validation_disposition"] = "limitations_preserved"
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "promotion.json"
            path.write_text(json.dumps(document), encoding="utf-8")
            failures = validate_promotion_manifest(path, ROOT)
        self.assertTrue(any("receipt did not pass" in value for value in failures))

    def test_passed_receipt_requires_empty_failure_accounting(self):
        failures = validate_promotion_manifest(MANIFEST, ROOT)
        accounting_failures = [
            value for value in failures
            if "receipt missing or non-empty accounting field" in value
        ]
        self.assertEqual(len(accounting_failures), 8 * 4)

    def test_strict_passed_receipt_contract_is_accepted(self):
        self.assertEqual(_validate_receipt(STRICT_RECEIPT), [])

    def test_hash_and_evidence_overstatement_fail_closed(self):
        document = json.loads(MANIFEST.read_text(encoding="utf-8"))
        document["candidates"][0]["profile"]["sha256"] = "0" * 64
        document["candidates"][1]["real_world_claim"] = "fully_real"
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "promotion.json"
            path.write_text(json.dumps(document), encoding="utf-8")
            failures = validate_promotion_manifest(path, ROOT)
        self.assertTrue(any("hash mismatch" in failure for failure in failures))
        self.assertTrue(any("overstated" in failure for failure in failures))


if __name__ == "__main__":
    unittest.main()
