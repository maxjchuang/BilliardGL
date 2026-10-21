import copy
import json
from pathlib import Path
import subprocess
import sys
import unittest


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
MANIFEST = REPOSITORY_ROOT / "assets/modern/lookdev/focused_practice_hall_v1.lookdev.json"
SCHEMA = REPOSITORY_ROOT / "assets/modern/lookdev/lookdev_manifest.schema.json"
VALIDATOR = REPOSITORY_ROOT / "scripts/validate_lookdev_manifest.py"

sys.path.insert(0, str(REPOSITORY_ROOT / "scripts"))
from validate_lookdev_manifest import validate  # noqa: E402


class LookdevManifestTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.document = json.loads(MANIFEST.read_text(encoding="utf-8"))

    def test_committed_manifest_passes_semantic_validation(self):
        self.assertEqual(validate(self.document), [])
        completed = subprocess.run(
            [sys.executable, str(VALIDATOR), str(MANIFEST), "--schema", str(SCHEMA)],
            cwd=str(REPOSITORY_ROOT), text=True, capture_output=True, timeout=10)
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        self.assertIn("focused_practice_hall_v1", completed.stdout)

    def test_clearcoat_and_color_spaces_are_explicit(self):
        self.assertEqual(
            self.document["clearcoat_policy"]["preferred"],
            "KHR_materials_clearcoat")
        for material in self.document["materials"].values():
            self.assertEqual(
                material["base_color_texture_encoding"],
                "srgb_iec61966_2_1")
            self.assertEqual(len(material["base_color_factor_linear_srgb"]), 4)

    def test_all_six_scenes_and_four_cameras_are_governed(self):
        self.assertEqual(
            set(self.document["cameras"]),
            {"player_shot", "low_aim", "tactical_overhead", "broadcast_oblique"})
        self.assertEqual(
            set(self.document["ldr_interest_region_acceptance"]["scenes"]),
            {"break", "low_aim", "ball_closeup", "pocket_closeup",
             "tactical_overhead", "post_shot_observe"})

    def test_invalid_linear_color_is_rejected(self):
        invalid = copy.deepcopy(self.document)
        invalid["materials"]["table_felt"]["base_color_factor_linear_srgb"][1] = 1.2
        self.assertTrue(any("table_felt" in error for error in validate(invalid)))

    def test_out_of_frame_interest_region_is_rejected(self):
        invalid = copy.deepcopy(self.document)
        invalid["ldr_interest_region_acceptance"]["scenes"]["break"]["regions"][0]["rect_normalized"] = [0.8, 0.1, 0.4, 0.4]
        self.assertTrue(any("exceeds the frame" in error for error in validate(invalid)))

    def test_production_high_mesh_gate_cannot_be_removed(self):
        invalid = copy.deepcopy(self.document)
        invalid["m1_scope"]["production_high_mesh_blocked_by"] = "none"
        self.assertTrue(any("alignment contract" in error for error in validate(invalid)))


if __name__ == "__main__":
    unittest.main()
