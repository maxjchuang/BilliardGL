import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SCRIPTS = ROOT / "scripts"
sys.path.insert(0, str(SCRIPTS))

from calibrate_phase2_pocket_v10g import (  # noqa: E402
    barycentric_quality, deterministic_grid,
)
from generate_phase2_modular_gltf_v10g import (  # noqa: E402
    ASSET_NAME, DEFAULT_OUTPUT_ROOT, DESIGN_GATE,
)


REPORT = ROOT / "docs/art/phase2-pocket-v10g-offline-preflight-v2/phase2-pocket-v10g-offline-report-v2.json"


class Phase2PocketV10gCalibrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.report = json.loads(REPORT.read_text())

    def test_committed_report_is_offline_pass_without_authority(self):
        report = self.report
        self.assertEqual(report["status"],
                         "OFFLINE_PREFLIGHT_PASS_GPU_NOT_AUTHORIZED")
        self.assertEqual(report["evidence_scope"],
                         "phase2_pocket_v10g_offline_only")
        self.assertEqual(report["visual_review_status"], "NOT_RUN")
        self.assertEqual(report["machine_status_before_execution"], "NOT_RUN")
        self.assertFalse(report["actual_gpu_used"])
        self.assertFalse(report["gpu_capture_authorized"])
        self.assertFalse(report["ta_review_authorized"])
        self.assertEqual(report["image_output_count"], 0)
        self.assertEqual(report["machine_failures"], [])
        self.assertTrue(all(report["machine_checks"].values()))

    def test_complete_grid_and_lowest_energy_selection_are_exact(self):
        search = self.report["candidate_search"]
        self.assertEqual(search["candidate_count"], 2772)
        self.assertEqual(len(search["all_candidates"]), 2772)
        selected = search["selected"]
        self.assertEqual((selected["base_neutral_scalar"],
                          selected["lower_endpoint_scalar"],
                          selected["upper_endpoint_scalar"],
                          selected["roughness"]), (0.055, 0.3, 0.95, 0.84))
        self.assertAlmostEqual(selected["cpu_post_tonemap"]["D_cpu"],
                               58.9254069952612, places=10)
        self.assertAlmostEqual(selected["calibration"]["D_gpu_lower"],
                               20.28612240219457, places=10)
        candidates, regenerated = deterministic_grid()
        self.assertEqual(len(candidates), 2772)
        self.assertEqual(regenerated, selected)

    def test_roi_statistics_keep_geometric_labels_and_original_thresholds(self):
        image = self.report["predicted_image_measurements"]
        bands = image["bands_geometric_labels"]
        self.assertEqual(bands["orientation_basis"]["frozen_projection"],
                         "local_upper_has_greater_screen_y")
        self.assertGreater(bands["upper"]["median_scalar"],
                           bands["lower"]["median_scalar"])
        self.assertEqual(image["band_brightness_order"],
                         ["geometric_upper", "geometric_lower"])
        self.assertGreaterEqual(bands["upper"]["sample_count"], 128)
        self.assertGreaterEqual(bands["lower"]["sample_count"], 128)
        self.assertGreaterEqual(bands["upper"]["coverage_fraction"], 0.15)
        self.assertGreaterEqual(bands["lower"]["coverage_fraction"], 0.15)
        self.assertGreaterEqual(image["band_statistical_checks"]
                                ["central_column_coverage"], 9)
        self.assertEqual(image["ordered_stage_columns"]["passing"], 11)
        self.assertEqual(image["fragment_continuity"]["passing"], 11)

    def test_barycentric_lattice_and_seams_are_exact(self):
        quality = self.report["barycentric_interpolation_quality"]
        self.assertEqual(quality["lattice_denominator"], 32)
        self.assertEqual(quality["triangles_tested"], 600)
        self.assertEqual(quality["lattice_samples"], 336600)
        self.assertEqual(quality["convex_hull_violations"], 0)
        self.assertEqual(quality["shared_edge_mismatches"], 0)
        self.assertLessEqual(quality["maximum_shared_edge_scalar_mismatch"], 1e-6)
        self.assertEqual(quality["geometric_seam_mismatches"], 0)
        self.assertEqual(quality["positive_derivative_steps"], 0)
        self.assertTrue(all(quality["checks"].values()))
        document = json.loads((DEFAULT_OUTPUT_ROOT / ASSET_NAME).read_text())
        selected = self.report["candidate_search"]["selected"]
        self.assertEqual(barycentric_quality(document, selected), quality)

    def test_report_generation_is_byte_deterministic(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-v10g-report-") as directory:
            outputs = [Path(directory) / "first.json", Path(directory) / "second.json"]
            for output in outputs:
                completed = subprocess.run([
                    sys.executable, str(SCRIPTS / "calibrate_phase2_pocket_v10g.py"),
                    "--output", str(output)], cwd=ROOT,
                    capture_output=True, text=True)
                self.assertEqual(completed.returncode, 0,
                                 completed.stdout + completed.stderr)
            self.assertEqual(outputs[0].read_bytes(), outputs[1].read_bytes())
            self.assertEqual(outputs[0].read_bytes(), REPORT.read_bytes())

    def test_mutated_gate_is_rejected_without_report(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-v10g-bad-gate-") as directory:
            temporary = Path(directory)
            gate = json.loads(DESIGN_GATE.read_text())
            gate["GPU_CAPTURE_AUTHORIZED"] = True
            bad_gate = temporary / "bad_gate.json"
            bad_gate.write_text(json.dumps(gate))
            output = temporary / "report.json"
            completed = subprocess.run([
                sys.executable, str(SCRIPTS / "calibrate_phase2_pocket_v10g.py"),
                "--design-gate", str(bad_gate), "--output", str(output)],
                cwd=ROOT, capture_output=True, text=True)
            self.assertEqual(completed.returncode, 2,
                             completed.stdout + completed.stderr)
            self.assertIn("SHA-256 mismatch", completed.stdout)
            self.assertFalse(output.exists())

    def test_report_contains_no_stale_metadata_or_image_claim(self):
        serialized = json.dumps(self.report)
        self.assertNotIn("phase2_pocket_v10c_single_scene_non_formal", serialized)
        self.assertNotIn("PENDING_TA", serialized)
        self.assertNotIn("PENDING_ROOT_AND_TA", serialized)
        self.assertEqual(self.report["validator_report"]["v10g_image_file_count"], 0)


if __name__ == "__main__":
    unittest.main()
