import json
import tempfile
import unittest
from pathlib import Path

from tools.physics_validation.promotion import validate_full_game_matrix


ROOT = Path(__file__).resolve().parents[2]
MATRIX = ROOT / "physics_models/promotion/full_game_matrix_v1.json"
MATRIX_V2 = ROOT / "physics_models/promotion/full_game_matrix_v2.json"
REQUIRED_V2_CASES = {
    "cue_center_hit", "cue_near_miscue", "sliding_to_rolling",
    "oblique_ball_collision", "rail_rebound", "side_pocket_capture",
    "seeded_break", "continuous_scoring", "cue_ball_scratch",
    "randomized_legal_sequence", "cadence_equivalence",
    "host_load_equivalence",
}


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

    def test_v2_matrix_executes_every_required_case(self):
        document = json.loads(MATRIX_V2.read_text(encoding="utf-8"))
        self.assertEqual(validate_full_game_matrix(MATRIX_V2, ROOT), [])
        self.assertEqual({case["id"] for case in document["cases"]},
                         REQUIRED_V2_CASES)
        self.assertTrue(all(
            case["replay"].startswith("full-game-stress --case ")
            for case in document["cases"]))

    def test_v2_duplicate_and_noncanonical_replay_fail_closed(self):
        document = json.loads(MATRIX_V2.read_text(encoding="utf-8"))
        document["cases"].append(dict(document["cases"][0]))
        document["cases"][0]["replay"] = "hand-written claim"
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "matrix.json"
            path.write_text(json.dumps(document), encoding="utf-8")
            failures = validate_full_game_matrix(path, ROOT)
        self.assertTrue(any("unique" in failure for failure in failures))
        self.assertTrue(any("canonical" in failure for failure in failures))


if __name__ == "__main__":
    unittest.main()
