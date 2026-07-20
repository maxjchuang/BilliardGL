import json
import tempfile
import unittest
from pathlib import Path

from tools.physics_validation.promotion import validate_golden_registry


ROOT = Path(__file__).resolve().parents[2]
REGISTRY = ROOT / "physics_models/promotion/full_game_goldens_v1.json"
MATRIX = ROOT / "physics_models/promotion/full_game_matrix_v1.json"


class FullGameGoldenTests(unittest.TestCase):
    def test_committed_goldens_preserve_evidence_labels_and_bytes(self):
        self.assertEqual(validate_golden_registry(REGISTRY, MATRIX, ROOT), [])

    def test_behavior_snapshot_cannot_be_promoted_by_registry_edit(self):
        document = json.loads(REGISTRY.read_text(encoding="utf-8"))
        target = next(entry for entry in document["entries"]
                      if entry["label"] == "behavior_snapshot")
        target["label"] = "reality_golden"
        target["validated_point_id"] = "invented"
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "goldens.json"
            path.write_text(json.dumps(document), encoding="utf-8")
            failures = validate_golden_registry(path, MATRIX, ROOT)
        self.assertTrue(any("evidence label changed" in failure for failure in failures))


if __name__ == "__main__":
    unittest.main()
