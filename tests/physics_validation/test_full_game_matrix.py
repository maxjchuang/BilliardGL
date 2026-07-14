import json
import tempfile
import unittest
from pathlib import Path

from tools.physics_validation.promotion import validate_full_game_matrix


ROOT = Path(__file__).resolve().parents[2]
MATRIX = ROOT / "physics_models/promotion/full_game_matrix_v1.json"


class FullGameMatrixTests(unittest.TestCase):
    def test_matrix_covers_every_preregistered_game_dimension(self):
        self.assertEqual(validate_full_game_matrix(MATRIX, ROOT), [])

    def test_missing_coverage_and_unlabeled_goldens_fail_closed(self):
        document = json.loads(MATRIX.read_text(encoding="utf-8"))
        document["cases"] = document["cases"][:-1]
        document["cases"][0]["evidence_label"] = "looks_real"
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "matrix.json"
            path.write_text(json.dumps(document), encoding="utf-8")
            failures = validate_full_game_matrix(path, ROOT)
        self.assertTrue(any("coverage mismatch" in failure for failure in failures))
        self.assertTrue(any("evidence label" in failure for failure in failures))


if __name__ == "__main__":
    unittest.main()
