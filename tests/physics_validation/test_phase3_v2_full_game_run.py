import csv
import hashlib
import json
import unittest
from pathlib import Path

from tools.physics_validation.generate_full_game_baseline import generate_documents


ROOT = Path(__file__).resolve().parents[2]
FREEZE = ROOT / "physics_models/candidates/phase3_integrated_v2/freeze.json"
OUTPUT = ROOT / "physics_models/candidates/phase3_integrated_v2/full_game"
MATRIX = ROOT / "physics_models/promotion/full_game_matrix_v2.json"
BUDGET = ROOT / "physics_models/promotion/full_game_performance_budget_v2.json"
BASELINE = ROOT / "physics_models/promotion/full_game_performance_baseline_v2.json"
CSV = ROOT / "physics_models/promotion/full_game_stress_v2.csv"


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


class Phase3V2FullGameRunTests(unittest.TestCase):
    def setUp(self):
        self.freeze = json.loads(FREEZE.read_text(encoding="utf-8"))
        self.matrix = json.loads(MATRIX.read_text(encoding="utf-8"))
        self.budget = json.loads(BUDGET.read_text(encoding="utf-8"))
        self.baseline = json.loads(BASELINE.read_text(encoding="utf-8"))
        self.case_ids = {case["id"] for case in self.matrix["cases"]}

    def test_full_game_outputs_bind_the_frozen_executable_and_source(self):
        self.assertEqual(self.baseline["candidate_id"],
                         self.freeze["candidate_id"])
        self.assertEqual(self.baseline["executable_sha256"],
                         self.freeze["executable_sha256"])
        self.assertEqual(self.baseline["runner_source_revision"],
                         self.freeze["source_revision"])
        self.assertRegex(self.baseline["runner_sha256"], r"^[0-9a-f]{64}$")
        self.assertEqual(set(self.baseline["cases"]), self.case_ids)

    def test_every_case_preserves_complete_hash_bound_v2_output(self):
        total_wall = 0.0
        for case_id in self.case_ids:
            case_dir = OUTPUT / case_id
            summary_path = case_dir / "summary.json"
            trace_path = case_dir / "trace.json"
            index_path = case_dir / "index.csv"
            with self.subTest(case_id=case_id):
                summary = json.loads(summary_path.read_text(encoding="utf-8"))
                trace_bytes = trace_path.read_bytes()
                trace = json.loads(trace_bytes)
                evidence = self.baseline["cases"][case_id]
                self.assertTrue(summary["passed"])
                self.assertEqual(summary["dropped_trace_frames"], 0)
                self.assertEqual(summary["step_failures"], 0)
                self.assertEqual(summary["deterministic_hash"],
                                 hashlib.sha256(trace_bytes.rstrip(b"\n")).hexdigest())
                self.assertEqual(summary["frame_count"], len(trace["frames"]))
                self.assertTrue(all(
                    frame["physics_profile_id"] == "chinese_pool_full_game_v2"
                    for frame in trace["frames"]
                ))
                self.assertEqual(evidence["summary_sha256"], sha256(summary_path))
                self.assertEqual(evidence["trace_sha256"], sha256(trace_path))
                self.assertEqual(evidence["index_sha256"], sha256(index_path))
                limits = self.budget["cases"][case_id]
                self.assertLessEqual(summary["wall_seconds"],
                                     limits["maximum_wall_seconds"])
                self.assertLessEqual(summary["peak_rss_bytes"],
                                     limits["maximum_peak_rss_bytes"])
                total_wall += summary["wall_seconds"]
        self.assertLessEqual(total_wall, self.budget["maximum_matrix_wall_seconds"])

    def test_matrix_outputs_and_derived_csv_are_complete(self):
        self.assertEqual(self.baseline["matrix_summary_sha256"],
                         sha256(OUTPUT / "matrix_summary.json"))
        self.assertEqual(self.baseline["matrix_index_sha256"],
                         sha256(OUTPUT / "index.csv"))
        with CSV.open(encoding="utf-8", newline="") as stream:
            rows = list(csv.DictReader(stream))
        self.assertEqual({row["case_id"] for row in rows}, self.case_ids)
        self.assertEqual(len(rows), len(self.case_ids))
        for row in rows:
            evidence = self.baseline["cases"][row["case_id"]]
            self.assertEqual(row["trace_sha256"], evidence["trace_sha256"])
            self.assertEqual(row["summary_sha256"], evidence["summary_sha256"])
            self.assertEqual(row["physics_profile_id"],
                             "chinese_pool_full_game_v2")

    def test_baseline_and_csv_reproduce_from_complete_outputs(self):
        baseline, csv_text = generate_documents(
            ROOT, FREEZE, MATRIX, BUDGET, OUTPUT,
            runner_sha256=self.baseline["runner_sha256"],
        )
        self.assertEqual(baseline.encode(), BASELINE.read_bytes())
        self.assertEqual(csv_text.encode(), CSV.read_bytes())


if __name__ == "__main__":
    unittest.main()
