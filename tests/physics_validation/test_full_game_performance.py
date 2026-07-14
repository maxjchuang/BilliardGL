import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BUDGET = ROOT / "physics_models/promotion/full_game_performance_budget_v1.json"
BASELINE = ROOT / "physics_models/promotion/full_game_performance_baseline_v1.json"


class FullGamePerformanceTests(unittest.TestCase):
    def test_baseline_satisfies_preregistered_budget(self):
        budget = json.loads(BUDGET.read_text(encoding="utf-8"))
        baseline = json.loads(BASELINE.read_text(encoding="utf-8"))
        self.assertEqual(budget["schema_version"], baseline["schema_version"])
        self.assertEqual(budget["ticks"], baseline["ticks"])
        for metric in ("mean_step_ms", "p95_step_ms", "p99_step_ms",
                       "peak_rss_bytes", "artifact_bytes_per_tick"):
            self.assertLessEqual(baseline[metric], budget[f"{metric}_max"])


if __name__ == "__main__":
    unittest.main()
