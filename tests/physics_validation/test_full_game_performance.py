import json
import hashlib
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BUDGET = ROOT / "physics_models/promotion/full_game_performance_budget_v1.json"
BASELINE = ROOT / "physics_models/promotion/full_game_performance_baseline_v1.json"
BUDGET_V2 = ROOT / "physics_models/promotion/full_game_performance_budget_v2.json"
MATRIX_V2 = ROOT / "physics_models/promotion/full_game_matrix_v2.json"


class FullGamePerformanceTests(unittest.TestCase):
    def test_baseline_satisfies_preregistered_budget(self):
        budget = json.loads(BUDGET.read_text(encoding="utf-8"))
        baseline = json.loads(BASELINE.read_text(encoding="utf-8"))
        self.assertEqual(budget["schema_version"], baseline["schema_version"])
        self.assertEqual(budget["ticks"], baseline["ticks"])
        for metric in ("mean_step_ms", "p95_step_ms", "p99_step_ms",
                       "peak_rss_bytes", "artifact_bytes_per_tick"):
            self.assertLessEqual(baseline[metric], budget[f"{metric}_max"])

    def test_committed_v2_cases_satisfy_per_case_wall_and_rss_budgets(self):
        budget = json.loads(BUDGET_V2.read_text(encoding="utf-8"))
        matrix = json.loads(MATRIX_V2.read_text(encoding="utf-8"))
        artifact_root = ROOT / matrix["artifact_root"]
        cases = {case["id"]: case for case in matrix["cases"]}
        ids = set(cases)
        self.assertEqual(set(budget["cases"]), ids)
        total_wall = 0.0
        for case_id in ids:
            summary = json.loads(
                (artifact_root / case_id / "summary.json").read_text())
            trace_bytes = (artifact_root / case_id / "trace.json").read_bytes()
            canonical = trace_bytes.removesuffix(b"\n")
            self.assertTrue(summary["passed"])
            self.assertEqual(summary["dropped_trace_frames"], 0)
            self.assertEqual(summary["step_failures"], 0)
            self.assertEqual(
                summary["deterministic_hash"],
                hashlib.sha256(canonical).hexdigest())
            for metric, expected in cases[case_id]["expected"].items():
                if metric.startswith("minimum_"):
                    self.assertGreaterEqual(
                        summary[metric.removeprefix("minimum_")], expected)
                else:
                    self.assertEqual(summary[metric], expected)
            limits = budget["cases"][case_id]
            self.assertLessEqual(
                summary["wall_seconds"], limits["maximum_wall_seconds"])
            self.assertLessEqual(
                summary["peak_rss_bytes"], limits["maximum_peak_rss_bytes"])
            total_wall += summary["wall_seconds"]
        self.assertLessEqual(total_wall, budget["maximum_matrix_wall_seconds"])


if __name__ == "__main__":
    unittest.main()
