import csv
import hashlib
import json
import os
import subprocess
import tempfile
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

    def test_executable_writes_complete_hash_bound_trace(self):
        build = Path(os.environ.get("BILLIARDGL_BUILD_DIR", ROOT / "build"))
        executable = build / "BilliardsFullGameStress"
        self.assertTrue(executable.is_file(), f"missing {executable}")
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            subprocess.run([
                str(executable), "--case", "legacy_break_stress",
                "--seed", "101", "--write", str(output),
            ], check=True)
            summary = json.loads((output / "summary.json").read_text())
            trace_bytes = (output / "trace.json").read_bytes()
            trace = json.loads(trace_bytes)
            canonical_bytes = trace_bytes.removesuffix(b"\n")
            self.assertEqual(summary["frame_count"], len(trace["frames"]))
            self.assertGreater(summary["frame_count"], 0)
            self.assertEqual(summary["dropped_trace_frames"], 0)
            self.assertEqual(summary["step_failures"], 0)
            self.assertEqual(
                summary["deterministic_hash"],
                hashlib.sha256(canonical_bytes).hexdigest())
            required = {
                "position_cm", "velocity_cm_s", "acceleration_cm_s2",
                "angular_velocity_rad_s", "motion_state", "pocketed",
                "pocket_id", "pocket_phase", "pocket_capture_sequence",
            }
            self.assertTrue(required.issubset(trace["frames"][0]["balls"][0]))
            self.assertIn("contacts", trace["frames"][0])
            self.assertIn("solver_events", trace["frames"][0])
            self.assertIn("events", trace)


if __name__ == "__main__":
    unittest.main()
