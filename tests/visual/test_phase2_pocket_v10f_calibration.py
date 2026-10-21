import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SCRIPTS = ROOT / "scripts"
sys.path.insert(0, str(SCRIPTS))

import calibrate_phase2_pocket_v10f as calibration  # noqa: E402


ASSET_ROOT = ROOT / "assets/modern/phase2-v10f"
DESIGN_GATE = ROOT / "docs/art/phase2-pocket-v10f-design-gate-v1.json"
REPORT = ROOT / "docs/art/phase2-pocket-v10f-offline-preflight-v1" / \
    "phase2-pocket-v10f-cpu-projection-report-v1.json"
CALIBRATOR = SCRIPTS / "calibrate_phase2_pocket_v10f.py"


class Phase2PocketV10fCalibrationTests(unittest.TestCase):
    def test_frozen_v10d_model_residual_is_bounded(self):
        result = calibration.frozen_v10d_calibration()
        self.assertTrue(result["pixel_axes_within_1px"], result)
        self.assertTrue(result["walnut_fraction_residual_at_most_0_005"], result)
        self.assertTrue(result["luma_anchor_exact"], result)

    def test_committed_report_is_offline_pass_without_gpu_authority(self):
        report = json.loads(REPORT.read_text(encoding="utf-8"))
        self.assertEqual(report["status"],
                         "OFFLINE_PREFLIGHT_PASS_GPU_NOT_AUTHORIZED")
        self.assertFalse(report["actual_gpu_used"])
        self.assertFalse(report["actual_gpu_authorized"])
        self.assertEqual(report["machine_failures"], [])
        self.assertEqual(report["predicted_image_measurements"]
                         ["ordered_stage_columns"]["sampled"], 11)

    def test_report_generation_is_byte_deterministic(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-v10f-determinism-") as directory:
            outputs = [Path(directory) / name for name in ("first.json", "second.json")]
            for output in outputs:
                completed = subprocess.run(
                    [sys.executable, str(CALIBRATOR), "--asset-root", str(ASSET_ROOT),
                     "--design-gate", str(DESIGN_GATE), "--output", str(output)],
                    cwd=str(ROOT), text=True, capture_output=True, timeout=30)
                self.assertEqual(completed.returncode, 0,
                                 completed.stdout + completed.stderr)
            self.assertEqual(outputs[0].read_bytes(), outputs[1].read_bytes())

    def test_design_gate_that_authorizes_gpu_is_rejected(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-v10f-bad-gate-") as directory:
            invalid_gate = copy.deepcopy(json.loads(DESIGN_GATE.read_text(encoding="utf-8")))
            invalid_gate["actual_gpu_authorized"] = True
            gate_path = Path(directory) / "invalid-gate.json"
            gate_path.write_text(json.dumps(invalid_gate), encoding="utf-8")
            output = Path(directory) / "must-not-exist.json"
            completed = subprocess.run(
                [sys.executable, str(CALIBRATOR), "--asset-root", str(ASSET_ROOT),
                 "--design-gate", str(gate_path), "--output", str(output)],
                cwd=str(ROOT), text=True, capture_output=True, timeout=30)
            self.assertEqual(completed.returncode, 2,
                             completed.stdout + completed.stderr)
            self.assertIn("authorizes GPU capture",
                          completed.stdout + completed.stderr)
            self.assertFalse(output.exists())

    def test_missing_embedded_buffer_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "one embedded glTF buffer"):
            calibration.decode_buffer({"buffers": []})

    def test_incomplete_object_id_roles_are_rejected(self):
        width = (calibration.ROI[2] - calibration.ROI[0]) * \
            (calibration.ROI[3] - calibration.ROI[1])
        with self.assertRaisesRegex(ValueError, "all three pocket stages"):
            calibration.measurements(bytearray(width),
                                     calibration.array("f", [1.0]) * width)


if __name__ == "__main__":
    unittest.main()
