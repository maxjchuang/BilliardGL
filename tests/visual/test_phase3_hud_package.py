import copy
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
ASSET_DIR = REPOSITORY_ROOT / "assets/modern/hud"
MANIFEST = ASSET_DIR / "focused_practice_hud_v1.hud.json"
GENERATOR = REPOSITORY_ROOT / "scripts/generate_phase3_hud_package.py"
VALIDATOR = REPOSITORY_ROOT / "scripts/validate_phase3_hud_package.py"
SHIPPING_TOKENS = ASSET_DIR / "focused_practice_hud_shipping_tokens_v1.json"
TOKEN_PROOF = ASSET_DIR / "focused_practice_hud_shipping_tokens_flat_proof_v1.svg"

sys.path.insert(0, str(REPOSITORY_ROOT / "scripts"))
from validate_phase3_hud_package import validate  # noqa: E402


class Phase3HudPackageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))

    def test_committed_hud_package_passes_offline_validation(self):
        errors, report = validate(self.manifest, ASSET_DIR)
        self.assertEqual(errors, [])
        self.assertEqual(report["profiles"], 2)
        self.assertEqual(report["states"], 5)
        self.assertEqual(report["mockups"], 8)
        self.assertEqual(report["product_elements"], 7)
        self.assertEqual(report["first_shot_seconds"], 30)
        self.assertEqual(report["responsive_cases"], 3)
        self.assertGreaterEqual(report["shipping_icons"], 10)
        self.assertEqual(report["shipping_palettes"], 2)
        self.assertGreaterEqual(report["minimum_text_px"], 13)

    def test_generator_is_byte_deterministic(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-hud-") as directory:
            completed = subprocess.run(
                [sys.executable, str(GENERATOR), "--output-dir", directory],
                cwd=str(REPOSITORY_ROOT), text=True, capture_output=True, timeout=20)
            self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
            for committed in sorted(ASSET_DIR.iterdir()):
                if committed.is_file() and committed.name.startswith(
                        "focused_practice_hud_"):
                    self.assertEqual((Path(directory) / committed.name).read_bytes(),
                                     committed.read_bytes(), committed.name)

    def test_validator_cli_reports_profiles_states_and_mockups(self):
        completed = subprocess.run(
            [sys.executable, str(VALIDATOR), str(MANIFEST)], cwd=str(REPOSITORY_ROOT),
            text=True, capture_output=True, timeout=20)
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        self.assertIn("profiles=2", completed.stdout)
        self.assertIn("states=5", completed.stdout)
        self.assertIn("mockups=8", completed.stdout)
        self.assertIn("responsive_cases=3", completed.stdout)
        self.assertIn("shipping_icons=", completed.stdout)
        self.assertIn("shipping_palettes=2", completed.stdout)

    def test_element_leaving_safe_area_is_rejected(self):
        invalid = copy.deepcopy(self.manifest)
        invalid["profiles"]["compact_1024x768"]["element_rects_px"]["player_status"][0] = 0
        errors, _ = validate(invalid, ASSET_DIR, verify_hashes=False)
        self.assertTrue(any("exits the safe area" in error for error in errors), errors)

    def test_low_contrast_palette_mutation_is_rejected(self):
        invalid = copy.deepcopy(self.manifest)
        invalid["colors"]["text_secondary"] = "#20282E"
        errors, _ = validate(invalid, ASSET_DIR, verify_hashes=False)
        self.assertTrue(any("color token" in error or "contrast pair" in error
                            for error in errors), errors)

    def test_color_vision_encoding_cannot_drop_ball_number(self):
        invalid = copy.deepcopy(self.manifest)
        invalid["color_vision_accessibility"]["target_always_shows_ball_number"] = False
        errors, _ = validate(invalid, ASSET_DIR, verify_hashes=False)
        self.assertTrue(any("cannot rely on color alone" in error for error in errors), errors)

    def test_first_shot_path_cannot_have_a_timing_gap(self):
        invalid = copy.deepcopy(self.manifest)
        invalid["first_shot_path_30s"][3]["seconds"][0] = 12
        errors, _ = validate(invalid, ASSET_DIR, verify_hashes=False)
        self.assertTrue(any("gap" in error for error in errors), errors)

    def test_debug_overlay_cannot_share_product_nodes(self):
        invalid = copy.deepcopy(self.manifest)
        invalid["debug_overlay"]["shares_product_layout_nodes"] = True
        invalid["debug_overlay"]["default_visible"] = True
        errors, _ = validate(invalid, ASSET_DIR, verify_hashes=False)
        self.assertTrue(any("independent hidden overlay" in error for error in errors), errors)

    def test_unpromoted_cue_elevation_cannot_be_default_visible(self):
        invalid = copy.deepcopy(self.manifest)
        invalid["capability_gated_placeholders"]["cue_elevation"]["default_visible"] = True
        errors, _ = validate(invalid, ASSET_DIR, verify_hashes=False)
        self.assertTrue(any("cue_elevation" in error for error in errors), errors)

    def test_1280_and_high_dpi_overlap_proofs_are_enforced(self):
        invalid = copy.deepcopy(self.manifest)
        case = invalid["responsive_validation_cases"]["minimum_wide_1280x720_96dpi"]
        case["element_rects_physical_px"]["foul_banner"] = [200, 24, 660, 88]
        errors, _ = validate(invalid, ASSET_DIR, verify_hashes=False)
        self.assertTrue(any("minimum_wide_1280x720_96dpi" in error and "overlaps" in error
                            for error in errors), errors)

    def test_first_shot_automatic_proxy_requires_exactly_one_release(self):
        invalid = copy.deepcopy(self.manifest)
        invalid["first_shot_automation_proxies"]["shot_release_command_count"] = 2
        errors, _ = validate(invalid, ASSET_DIR, verify_hashes=False)
        self.assertTrue(any("shot_release_command_count" in error for error in errors), errors)

    def test_aiming_mockup_must_render_tip_offset_control(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-hud-invalid-") as directory:
            temporary = Path(directory)
            for source in ASSET_DIR.iterdir():
                if source.is_file():
                    shutil.copyfile(source, temporary / source.name)
            path = temporary / "focused_practice_hud_compact_1024x768_aiming_v1.svg"
            path.write_text(path.read_text(encoding="utf-8").replace(
                'data-element="tip_offset"', 'data-element="tip_offset_removed"'), encoding="utf-8")
            errors, _ = validate(self.manifest, temporary, verify_hashes=False)
        self.assertTrue(any("lacks elements: tip_offset" in error for error in errors), errors)

    def test_shipping_tokens_cover_gpu_icons_palettes_and_evidence_boundary(self):
        tokens = json.loads(SHIPPING_TOKENS.read_text(encoding="utf-8"))
        self.assertEqual(set(tokens["palette_modes"]),
                         {"standard", "color_vision_safe"})
        self.assertTrue({"power_bars", "tip_target", "camera_frame",
                         "warning_triangle", "help_circle"}.issubset(
                             tokens["icon_tokens"]))
        self.assertEqual(set(tokens["state_icon_usage"]),
                         {"default", "aiming", "post_shot", "foul", "help"})
        proof = TOKEN_PROOF.read_text(encoding="utf-8")
        self.assertIn('data-runtime-capture="false"', proof)
        self.assertIn('data-user-research="false"', proof)

    def test_shipping_palette_cannot_become_color_only(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-hud-tokens-invalid-") as directory:
            temporary = Path(directory)
            for source in ASSET_DIR.iterdir():
                if source.is_file():
                    shutil.copyfile(source, temporary / source.name)
            tokens_path = temporary / SHIPPING_TOKENS.name
            tokens = json.loads(tokens_path.read_text(encoding="utf-8"))
            tokens["palette_accessibility"]["never_color_only"] = False
            tokens_path.write_text(json.dumps(tokens), encoding="utf-8")
            errors, _ = validate(self.manifest, temporary, verify_hashes=False)
        self.assertTrue(any("geometry/palette contract" in error or
                            "color-only" in error for error in errors), errors)

    def test_flat_proof_cannot_claim_runtime_capture(self):
        invalid = copy.deepcopy(self.manifest)
        invalid["files"]["flat_token_proof"]["runtime_capture"] = True
        errors, _ = validate(invalid, ASSET_DIR, verify_hashes=False)
        self.assertTrue(any("cannot claim GPU capture" in error for error in errors), errors)


if __name__ == "__main__":
    unittest.main()
