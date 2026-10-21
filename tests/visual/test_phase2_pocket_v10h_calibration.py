import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

from calibrate_phase2_pocket_v10h import (  # noqa: E402
    candidate_metrics, deterministic_grid,
)
from freeze_phase2_pocket_v10h_offline import freeze  # noqa: E402


class Phase2V10hCalibrationTests(unittest.TestCase):
    def test_complete_grid_and_selected_candidate(self):
        candidates, selected = deterministic_grid()
        self.assertEqual(225, len(candidates))
        self.assertEqual(6, sum(item["passes_all_hard_and_recommended"]
                                for item in candidates))
        self.assertEqual({"base_neutral_scalar": 0.05625,
                          "lower_endpoint_scalar": 0.055,
                          "upper_endpoint_scalar": 0.99,
                          "roughness": 0.84}, selected["parameters"])
        self.assertTrue(all(selected["checks"].values()))

    def test_grid_is_deterministic_and_every_rejection_is_explained(self):
        first, selected_first = deterministic_grid()
        second, selected_second = deterministic_grid()
        self.assertEqual(first, second)
        self.assertEqual(selected_first, selected_second)
        self.assertTrue(all(item["rejection_reasons"] or
                            item["passes_all_hard_and_recommended"] for item in first))

    def test_lower_overbright_candidate_is_rejected_independently(self):
        item = candidate_metrics(0.0575, 0.08, 1.0)
        self.assertFalse(item["passes_all_hard_and_recommended"])
        self.assertTrue(any("lower" in reason for reason in item["rejection_reasons"]))

    def test_model_divergence_or_interval_failure_rejects_candidate(self):
        item = candidate_metrics(0.0525, 0.04, 0.98)
        self.assertFalse(item["passes_all_hard_and_recommended"])
        self.assertGreater(len(item["rejection_reasons"]), 0)

    def test_offline_report_is_byte_deterministic(self):
        script = ROOT / "scripts/calibrate_phase2_pocket_v10h.py"
        with tempfile.TemporaryDirectory() as directory:
            first = Path(directory) / "first.json"
            second = Path(directory) / "second.json"
            for output in (first, second):
                subprocess.run([sys.executable, str(script), "--output", str(output)],
                               cwd=ROOT, check=True, capture_output=True, text=True)
            self.assertEqual(first.read_bytes(), second.read_bytes())
            report = json.loads(first.read_text())
            self.assertEqual("OFFLINE_PREFLIGHT_PASS_GPU_NOT_AUTHORIZED", report["status"])
            self.assertFalse(report["actual_gpu_used"])
            self.assertGreaterEqual(report["machine_check_count"], 30)

    def test_freeze_rejects_failed_or_gpu_claiming_report(self):
        source = json.loads((ROOT / "docs/art/phase2-pocket-v10h-offline-preflight-v1" /
                             "phase2-pocket-v10h-offline-report-v1.json").read_text())
        with tempfile.TemporaryDirectory(dir=ROOT) as directory:
            bad = Path(directory) / "bad.json"
            source["actual_gpu_used"] = True
            bad.write_text(json.dumps(source))
            with self.assertRaises(ValueError):
                freeze(Path(directory) / "freeze", bad)


if __name__ == "__main__":
    unittest.main()
