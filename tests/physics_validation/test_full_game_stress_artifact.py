import csv
import hashlib
import unittest
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
ARTIFACT = ROOT / "physics_models/promotion/full_game_stress_v1.csv"


class FullGameStressArtifactTests(unittest.TestCase):
    def test_committed_matrix_is_complete_deterministic_and_bounded(self):
        self.assertEqual(hashlib.sha256(ARTIFACT.read_bytes()).hexdigest(),
                         "f885543bf7a7738aea6fe5a5998916a528831a8ad6ac1ca8f34d1c11cb2e4ad1")
        with ARTIFACT.open(encoding="utf-8", newline="") as stream:
            rows = list(csv.DictReader(stream))
        self.assertEqual(len(rows), 12)
        hashes = defaultdict(set)
        for row in rows:
            self.assertEqual(row["finite_state"], "true")
            self.assertLessEqual(float(row["maximum_penetration_cm"]), 0.5)
            self.assertLessEqual(float(row["maximum_residual_cm_s"]), 0.001)
            self.assertEqual(int(row["duplicate_contacts"]), 0)
            self.assertEqual(int(row["repeated_breaks"]), 3)
            hashes[int(row["seed"])].add(row["replay_hash"])
        self.assertEqual(set(hashes), {101, 211, 307})
        self.assertTrue(all(len(values) == 1 for values in hashes.values()))


if __name__ == "__main__":
    unittest.main()
