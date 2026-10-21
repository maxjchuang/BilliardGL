import copy
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from validate_phase2_production_visuals import (
    OBJECT_CLASS_PALETTE, OBJECT_PHOTOMETRY_SCHEMA, ReviewError,
    evaluate_object_photometry, masked_surface_statistics, validate_photometry_binding,
)


class ObjectPhotometryTests(unittest.TestCase):
    def setUp(self):
        self.lookdev = json.loads((ROOT / "assets/modern/lookdev/focused_practice_hall_v1.lookdev.json").read_text())["ldr_interest_region_acceptance"]
        camera = {"eye_cm": [0, 500, 0], "target_cm": [0, 86.48, 0],
                  "vertical_fov_degrees": 42, "viewport_pixels": [1920, 1080]}
        self.entry = {"file": "normal.ppm", "screenshot_sha256": "a" * 64, "runtime_camera": camera,
                      "binary_sha256": "b" * 64, "renderer_source_sha256": "c" * 64,
                      "modular_gltf_sha256": "d" * 64}
        self.capture = {key: self.entry[key] for key in ("binary_sha256", "renderer_source_sha256", "modular_gltf_sha256")}
        self.binding = {"schema": OBJECT_PHOTOMETRY_SCHEMA, "source_file": "normal.ppm",
                        "source_sha256": "a" * 64, "normal_camera": camera, "mask_camera": camera,
                        "venue_loaded": True, "cue_ball_expected_visible": True,
                        "cue_center_probe_pixels": [1, 0],
                        "sample_policy": "all_exact_class_pixels_including_shadows_no_luminance_filter",
                        "mixed_edge_policy": "exclude_non_palette_samples_and_report_count",
                        "class_palette": {key: list(value) for key, value in OBJECT_CLASS_PALETTE.items()}}
        for name in self.capture:
            self.binding["normal_" + name] = self.binding["mask_" + name] = self.capture[name]
        self.entry["object_photometry"] = self.binding
        self.mask = bytes([255, 0, 0, 0, 255, 0, 0, 0, 255, 127, 0, 0])
        self.pixels = bytes([0, 0, 0, 180, 180, 180, 50, 50, 50, 255, 255, 255])

    def test_positive_binding_and_dark_shadow_included(self):
        validate_photometry_binding("break", self.entry, self.capture)
        result = masked_surface_statistics(self.pixels, self.mask, 4, 1)
        self.assertEqual(result["objects"]["felt"]["pixel_count"], 1)
        self.assertEqual(result["objects"]["felt"]["mean_luma"], 0)
        self.assertEqual(result["mixed_edge_pixels"], 1)
        diagnostic = evaluate_object_photometry("break", self.pixels, self.mask, 4, 1, self.binding, self.lookdev)
        self.assertEqual(diagnostic["objects"]["felt"]["status"], "FAIL")
        self.assertEqual(diagnostic["objects"]["felt"]["source_thresholds_unmodified"], self.lookdev["scenes"]["break"]["regions"][0]["checks"])

    def test_wrong_cue_class_rejected_not_circle_guessed(self):
        wrong = bytes([255, 0, 0, 255, 255, 0, 0, 255, 0, 0, 0, 255])
        with self.assertRaisesRegex(ReviewError, "wrong mask class"):
            evaluate_object_photometry("break", self.pixels, wrong, 4, 1, self.binding, self.lookdev)

    def test_empty_required_object_cannot_pass(self):
        with self.assertRaisesRegex(ReviewError, "empty felt"):
            evaluate_object_photometry("break", self.pixels, bytes(12), 4, 1, self.binding, self.lookdev)
        wrong = bytes([255, 0, 0] * 4)
        with self.assertRaisesRegex(ReviewError, "identity0 mask/visibility"):
            evaluate_object_photometry("break", self.pixels, wrong, 4, 1, self.binding, self.lookdev)

    def test_missing_venue_rejected_even_in_closeup(self):
        for scene in ("break", "ball_closeup"):
            changed = copy.deepcopy(self.entry)
            changed["object_photometry"]["venue_loaded"] = False
            with self.assertRaisesRegex(ReviewError, "actually loaded venue"):
                validate_photometry_binding(scene, changed, self.capture)

    def test_source_camera_and_binary_mismatch_rejected(self):
        changed = copy.deepcopy(self.entry)
        changed["object_photometry"]["mask_camera"] = copy.deepcopy(changed["runtime_camera"])
        changed["object_photometry"]["mask_camera"]["eye_cm"][1] = 345
        with self.assertRaisesRegex(ReviewError, "source-camera"):
            validate_photometry_binding("break", changed, self.capture)
        changed = copy.deepcopy(self.entry)
        changed["object_photometry"]["mask_binary_sha256"] = "e" * 64
        with self.assertRaisesRegex(ReviewError, "pair provenance"):
            validate_photometry_binding("break", changed, self.capture)

    def test_verified_absence_is_na_not_pass(self):
        binding = copy.deepcopy(self.binding)
        binding.update(cue_ball_expected_visible=False, venue_absence_geometry_verified=True,
                       venue_absence_evidence="Same authenticated depth-winning GPU class pass contains no venue surface; closeup geometry reviewed")
        result = evaluate_object_photometry("ball_closeup", self.pixels, bytes([255, 0, 0] * 4), 4, 1, binding, self.lookdev)
        self.assertEqual(result["objects"]["venue"]["status"], "N_A_OBJECT_ABSENT")
        self.assertEqual(result["objects"]["cue_ball"]["status"], "N_A_OBJECT_ABSENT")
        self.assertIsNone(result["felt_to_venue_ev"]["passed"])
        with self.assertRaisesRegex(ReviewError, "verified absent"):
            evaluate_object_photometry("break", self.pixels, bytes([255, 0, 0] * 4), 4, 1, binding, self.lookdev)


if __name__ == "__main__":
    unittest.main()
