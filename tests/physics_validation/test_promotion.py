import json
import tempfile
import unittest
from pathlib import Path

from tools.physics_validation.promotion import validate_promotion_manifest


ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "physics_models/promotion/phase3_candidates_v1.json"


class PromotionInventoryTests(unittest.TestCase):
    def test_committed_inventory_is_complete_and_immutable(self):
        self.assertEqual(validate_promotion_manifest(MANIFEST, ROOT), [])

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
