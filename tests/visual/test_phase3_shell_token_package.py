import copy
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
ASSET_DIR = ROOT / "assets/modern/hud"
MANIFEST = ASSET_DIR / "focused_practice_shell_shipping_tokens_v1.manifest.json"
TOKENS = ASSET_DIR / "focused_practice_shell_shipping_tokens_v1.json"
PROOF = ASSET_DIR / "focused_practice_shell_shipping_tokens_flat_proof_v1.svg"
GENERATOR = ROOT / "scripts/generate_phase3_shell_token_package.py"
VALIDATOR = ROOT / "scripts/validate_phase3_shell_token_package.py"

sys.path.insert(0, str(ROOT / "scripts"))
from generate_phase3_shell_token_package import SCREEN_USAGE, build_token_set  # noqa: E402
from validate_phase3_shell_token_package import audit_cpp_icon_tokens, validate  # noqa: E402


class Phase3ShellTokenPackageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = json.loads(MANIFEST.read_text())

    def _copy_package(self, directory):
        target = Path(directory)
        for source in (MANIFEST, TOKENS, PROOF):
            shutil.copyfile(source, target / source.name)
        return target

    def test_committed_package_passes_and_covers_exact_inventory(self):
        errors, report = validate(self.manifest, ASSET_DIR)
        self.assertEqual(errors, [])
        self.assertEqual(report, {"icons": 43, "palettes": 2, "screens": 9,
                                  "proof_icons": 86})

    def test_generator_is_byte_deterministic(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-shell-token-") as directory:
            completed = subprocess.run(
                [sys.executable, str(GENERATOR), "--output-dir", directory],
                cwd=ROOT, text=True, capture_output=True, timeout=20)
            self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
            for source in (MANIFEST, TOKENS, PROOF):
                self.assertEqual((Path(directory) / source.name).read_bytes(),
                                 source.read_bytes(), source.name)

    def test_validator_cli_reports_inventory(self):
        completed = subprocess.run([sys.executable, str(VALIDATOR), str(MANIFEST)],
                                   cwd=ROOT, text=True, capture_output=True, timeout=20)
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        self.assertIn("icons=43", completed.stdout)
        self.assertIn("palettes=2", completed.stdout)
        self.assertIn("screens=9", completed.stdout)

    def test_cpp_audit_matches_all_emitted_tokens(self):
        controller = (ROOT / "src/Billiards/product_shell_controller.cpp").read_text()
        draw = (ROOT / "src/Billiards/render/ui/product_shell_draw_list.cpp").read_text()
        self.assertEqual(audit_cpp_icon_tokens(controller, draw),
                         set(build_token_set()["icon_tokens"]))

    def test_cpp_audit_detects_new_literal_without_fallback(self):
        controller = 'items.push_back(item("id", "Label", "V", "D", "new_shape"));'
        draw = ('addPrimitive(draw, UiPrimitiveKind::Icon, rect, clip, color, '
                '"fixed_shape");')
        self.assertEqual(audit_cpp_icon_tokens(controller, draw),
                         {"new_shape", "fixed_shape"})

    def test_missing_icon_is_rejected(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-shell-invalid-") as directory:
            target = self._copy_package(directory)
            data = json.loads((target / TOKENS.name).read_text())
            del data["icon_tokens"]["focus_chevron"]
            (target / TOKENS.name).write_text(json.dumps(data))
            errors, _ = validate(self.manifest, target, verify_hashes=False,
                                 audit_sources=False)
        self.assertTrue(any("43 emitted tokens" in error or "differs" in error
                            for error in errors), errors)

    def test_alias_or_approximation_is_rejected(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-shell-invalid-") as directory:
            target = self._copy_package(directory)
            data = json.loads((target / TOKENS.name).read_text())
            data["aliases"] = {"replay_file": "empty_box"}
            (target / TOKENS.name).write_text(json.dumps(data))
            errors, _ = validate(self.manifest, target, verify_hashes=False,
                                 audit_sources=False)
        self.assertTrue(any("aliases must remain empty" in error for error in errors), errors)

    def test_geometry_outside_normalized_bounds_is_rejected(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-shell-invalid-") as directory:
            target = self._copy_package(directory)
            data = json.loads((target / TOKENS.name).read_text())
            data["icon_tokens"]["item_square"]["commands"][0]["x"] = -1
            (target / TOKENS.name).write_text(json.dumps(data))
            errors, _ = validate(self.manifest, target, verify_hashes=False,
                                 audit_sources=False)
        self.assertTrue(any("exits normalized 24x24 bounds" in error for error in errors), errors)

    def test_low_contrast_cvd_palette_is_rejected(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-shell-invalid-") as directory:
            target = self._copy_package(directory)
            data = json.loads((target / TOKENS.name).read_text())
            data["palette_modes"]["color_vision_safe"]["accent"] = "#10181E"
            (target / TOKENS.name).write_text(json.dumps(data))
            errors, _ = validate(self.manifest, target, verify_hashes=False,
                                 audit_sources=False)
        self.assertTrue(any("graphic contrast is below 3:1" in error for error in errors), errors)

    def test_flat_proof_cannot_claim_runtime_or_research_evidence(self):
        invalid = copy.deepcopy(self.manifest)
        invalid["flat_proof"]["runtime_capture"] = True
        errors, _ = validate(invalid, ASSET_DIR, verify_hashes=False,
                             audit_sources=False)
        self.assertTrue(any("cannot claim GPU capture" in error for error in errors), errors)

    def test_standard_and_cvd_palettes_are_required(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-shell-invalid-") as directory:
            target = self._copy_package(directory)
            data = json.loads((target / TOKENS.name).read_text())
            del data["palette_modes"]["color_vision_safe"]
            (target / TOKENS.name).write_text(json.dumps(data))
            errors, _ = validate(self.manifest, target, verify_hashes=False,
                                 audit_sources=False)
        self.assertTrue(any("palette modes differ" in error or "differs" in error
                            for error in errors), errors)

    def test_all_nine_screen_usages_are_required(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-shell-invalid-") as directory:
            target = self._copy_package(directory)
            data = json.loads((target / TOKENS.name).read_text())
            del data["screen_icon_usage"]["help"]
            (target / TOKENS.name).write_text(json.dumps(data))
            errors, _ = validate(self.manifest, target, verify_hashes=False,
                                 audit_sources=False)
        self.assertTrue(any("nine ProductShell screen" in error or "differs" in error
                            for error in errors), errors)
        self.assertEqual(len(SCREEN_USAGE), 9)

    def test_color_never_carries_state_alone(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-shell-invalid-") as directory:
            target = self._copy_package(directory)
            data = json.loads((target / TOKENS.name).read_text())
            data["accessibility_contract"]["never_color_only"] = False
            (target / TOKENS.name).write_text(json.dumps(data))
            errors, _ = validate(self.manifest, target, verify_hashes=False,
                                 audit_sources=False)
        self.assertTrue(any("accessibility redundancy" in error or "differs" in error
                            for error in errors), errors)

    def test_debug_and_shipping_namespaces_cannot_merge(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-shell-invalid-") as directory:
            target = self._copy_package(directory)
            data = json.loads((target / TOKENS.name).read_text())
            data["debug_isolation"]["debug_namespace"] = "product_shell"
            (target / TOKENS.name).write_text(json.dumps(data))
            errors, _ = validate(self.manifest, target, verify_hashes=False,
                                 audit_sources=False)
        self.assertTrue(any("isolation is invalid" in error for error in errors), errors)

    def test_manifest_hash_binding_is_enforced(self):
        invalid = copy.deepcopy(self.manifest)
        invalid["token_set"]["sha256"] = "sha256:" + "0" * 64
        errors, _ = validate(invalid, ASSET_DIR, audit_sources=False)
        self.assertTrue(any("token SHA-256 differs" in error for error in errors), errors)


if __name__ == "__main__":
    unittest.main()
