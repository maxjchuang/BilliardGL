import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
ASSET_DIR = REPOSITORY_ROOT / "assets/modern/phase2"
PARAMETERS = ASSET_DIR / "focused_practice_hall_pbr_v1.params.json"
LIGHTING_RIG = ASSET_DIR / "focused_practice_hall_lighting_rig_v1.json"
MANIFEST = ASSET_DIR / "focused_practice_hall_modular_v1.asset.json"
GENERATOR = REPOSITORY_ROOT / "scripts/generate_phase2_pbr_package.py"
VALIDATOR = REPOSITORY_ROOT / "scripts/validate_phase2_pbr_package.py"

sys.path.insert(0, str(REPOSITORY_ROOT / "scripts"))
from generate_phase2_pbr_package import (  # noqa: E402
    LIGHTING_RIG_NAME, PARAMETERS_NAME, PREVIEW_NAME, TEXTURE_SPECS,
    _noise, build_texture_set, texture_filename,
)
from validate_phase2_pbr_package import (  # noqa: E402
    _mip_wrap_discontinuities, _read_png, validate,
)


class Phase2PbrPackageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.parameters = json.loads(PARAMETERS.read_text(encoding="utf-8"))
        cls.rig = json.loads(LIGHTING_RIG.read_text(encoding="utf-8"))
        cls.manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))

    def _validate(self, parameters=None, rig=None, manifest=None):
        return validate(parameters if parameters is not None else copy.deepcopy(self.parameters),
                        rig if rig is not None else copy.deepcopy(self.rig),
                        manifest if manifest is not None else copy.deepcopy(self.manifest),
                        ASSET_DIR, verify_hashes=False)

    def test_committed_package_passes_offline_validation(self):
        errors, report = validate(self.parameters, self.rig, self.manifest, ASSET_DIR)
        self.assertEqual(errors, [])
        self.assertEqual(report["texture_sets"], 7)
        self.assertEqual(report["runtime_textures"], 21)
        self.assertLessEqual(report["runtime_gpu_bytes_full_mips"], 16 * 1024 * 1024)
        self.assertEqual(report["shadow_tiers"], 3)

    def test_standalone_generator_is_byte_deterministic(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-pbr-a-") as first, \
                tempfile.TemporaryDirectory(prefix="billiardgl-pbr-b-") as second:
            for directory in (first, second):
                completed = subprocess.run(
                    [sys.executable, str(GENERATOR), "--output-dir", directory],
                    cwd=str(REPOSITORY_ROOT), text=True, capture_output=True, timeout=60)
                self.assertEqual(completed.returncode, 0,
                                 completed.stdout + completed.stderr)
            names = [PARAMETERS_NAME, LIGHTING_RIG_NAME, PREVIEW_NAME] + [
                texture_filename(spec[0], channel) for spec in TEXTURE_SPECS
                for channel in ("basecolor", "normal", "orm")]
            for name in names:
                self.assertEqual((Path(first) / name).read_bytes(),
                                 (Path(second) / name).read_bytes(), name)
            felt_names = {texture_filename("felt", channel)
                          for channel in ("basecolor", "normal", "orm")}
            for name in names:
                if name not in felt_names:
                    self.assertEqual((Path(first) / name).read_bytes(),
                                     (ASSET_DIR / name).read_bytes(), name)

    def test_felt_noise_and_full_mip_chain_are_wrap_continuous(self):
        seed = next(spec[2] for spec in TEXTURE_SPECS if spec[0] == "felt")
        for cells in (4, 11, 29, 48):
            for coordinate in (0.0, 0.123, 0.5, 0.9375):
                self.assertAlmostEqual(
                    _noise(coordinate, 0.0, seed, cells, periodic=True),
                    _noise(coordinate, 1.0, seed, cells, periodic=True),
                    places=13)
                self.assertAlmostEqual(
                    _noise(0.0, coordinate, seed, cells, periodic=True),
                    _noise(1.0, coordinate, seed, cells, periodic=True),
                    places=13)
            epsilon = 1.0e-4
            self.assertAlmostEqual(
                _noise(-epsilon, 0.37, seed, cells, periodic=True),
                _noise(1.0 - epsilon, 0.37, seed, cells, periodic=True),
                places=12)
            self.assertAlmostEqual(
                _noise(epsilon, 0.37, seed, cells, periodic=True),
                _noise(1.0 + epsilon, 0.37, seed, cells, periodic=True),
                places=12)

        felt = next(spec for spec in TEXTURE_SPECS if spec[0] == "felt")
        _, periodic_pixels = build_texture_set(felt, periodic_felt=True)
        _, former_pixels = build_texture_set(felt, periodic_felt=False)
        for channel, pixels in zip(("basecolor", "normal", "orm"),
                                   periodic_pixels):
            failures = _mip_wrap_discontinuities(
                pixels, felt[1], felt[1], srgb=channel == "basecolor")
            self.assertEqual(failures, [], channel)
        former_failures = []
        for channel, pixels in zip(("basecolor", "normal", "orm"), former_pixels):
            former_failures.extend(_mip_wrap_discontinuities(
                pixels, felt[1], felt[1], srgb=channel == "basecolor"))
        self.assertGreater(len(former_failures), 100,
                           "the former non-periodic felt must fail the relative mip seam audit")

    def test_declared_periodic_felt_contract_runs_in_validator(self):
        contract = {
            "felt_noise_wrap": "toroidal_lattice_v1",
            "mip_wrap_audit": "relative_edge_step_plus_rgba8_quantization_v1",
        }
        modular_generator = REPOSITORY_ROOT / "scripts/generate_phase2_modular_gltf.py"
        with tempfile.TemporaryDirectory(prefix="billiardgl-periodic-felt-") as output:
            completed = subprocess.run(
                [sys.executable, str(modular_generator), "--output-dir", output],
                cwd=str(REPOSITORY_ROOT), text=True, capture_output=True, timeout=60)
            self.assertEqual(completed.returncode, 0,
                             completed.stdout + completed.stderr)
            output = Path(output)
            parameters = json.loads((output / PARAMETERS_NAME).read_text(encoding="utf-8"))
            rig = json.loads((output / LIGHTING_RIG_NAME).read_text(encoding="utf-8"))
            manifest = json.loads((output / MANIFEST.name).read_text(encoding="utf-8"))
            self.assertEqual(manifest["pbr_texture_sampling_contract"], contract)
            errors, _ = validate(parameters, rig, manifest, output)
            self.assertEqual(errors, [])

            felt = next(spec for spec in TEXTURE_SPECS if spec[0] == "felt")
            former_outputs, _ = build_texture_set(felt, periodic_felt=False)
            for name, payload in former_outputs.items():
                (output / name).write_bytes(payload)
            errors, _ = validate(
                parameters, rig, manifest, output, verify_hashes=False)
            self.assertTrue(any("periodic full-mip wrap contract" in error
                                for error in errors), errors)

    def test_validator_cli_reports_texture_and_shadow_budgets(self):
        completed = subprocess.run(
            [sys.executable, str(VALIDATOR), str(ASSET_DIR)], cwd=str(REPOSITORY_ROOT),
            text=True, capture_output=True, timeout=20)
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        self.assertIn("runtime_textures=21", completed.stdout)
        self.assertIn("shadow_tiers=3", completed.stdout)

    def test_pocket_void_is_neutral_flat_and_depth_occluded(self):
        definition = self.parameters["texture_sets"]["pocket_void"]
        self.assertEqual(definition["resolution_px"], [256, 256])
        self.assertEqual(definition["uv_repeat"], [1.0, 0.5])
        decoded = {}
        errors = []
        for channel in ("basecolor", "normal", "orm"):
            result = _read_png(
                ASSET_DIR / texture_filename("pocket_void", channel),
                errors, (256, 256))
            self.assertIsNotNone(result, errors)
            decoded[channel] = result[2]
        self.assertEqual(errors, [])
        self.assertEqual(set(tuple(decoded["basecolor"][offset:offset + 4])
                             for offset in range(0, len(decoded["basecolor"]), 4)),
                         {(255, 255, 255, 255)})
        self.assertEqual(set(tuple(decoded["normal"][offset:offset + 4])
                             for offset in range(0, len(decoded["normal"]), 4)),
                         {(128, 128, 255, 255)})
        orm = decoded["orm"]
        opening = orm[0]
        deepest = orm[(128 * 256) * 4]
        self.assertGreaterEqual(opening, 0.86 * 255)
        self.assertLessEqual(opening, 0.90 * 255)
        self.assertGreaterEqual(deepest, 0.06 * 255)
        self.assertLessEqual(deepest, 0.10 * 255)
        self.assertLess(deepest, opening)
        self.assertEqual(set(orm[1::4]), {230})
        self.assertEqual(set(orm[2::4]), {0})

    def test_directx_normal_convention_is_rejected(self):
        invalid = copy.deepcopy(self.parameters)
        invalid["color_management"]["normal"] = "linear_directx_negative_y_rgba8"
        errors, _ = self._validate(parameters=invalid)
        self.assertTrue(any("color management" in error for error in errors), errors)

    def test_external_hdr_environment_is_rejected(self):
        invalid = copy.deepcopy(self.rig)
        invalid["environment"]["source"] = "https://example.invalid/studio.hdr"
        errors, _ = self._validate(rig=invalid)
        self.assertTrue(any("external HDR" in error for error in errors), errors)

    def test_missing_normal_mip_renormalization_is_rejected(self):
        invalid = copy.deepcopy(self.parameters)
        invalid["mip_policy"]["normal"] = "linear_filter_without_renormalization"
        errors, _ = self._validate(parameters=invalid)
        self.assertTrue(any("mip generation policy" in error for error in errors), errors)

    def test_shadow_softness_tiers_must_increase(self):
        invalid = copy.deepcopy(self.rig)
        invalid["shadow_quality_tiers"]["high"]["softness_world_radius_m"] = 0.02
        errors, _ = self._validate(rig=invalid)
        self.assertTrue(any("strictly increase" in error for error in errors), errors)

    def test_review_preview_cannot_enter_runtime_budget(self):
        invalid = copy.deepcopy(self.manifest)
        invalid["files"]["material_review_preview"]["runtime"] = True
        errors, _ = self._validate(manifest=invalid)
        self.assertTrue(any("excluded from runtime" in error for error in errors), errors)

    def test_color_frequency_revision_is_explicit_and_guarded(self):
        revision = self.parameters["color_frequency_revision"]
        self.assertEqual(revision["revision_id"], "phase2_ta_color_frequency_correction_v2")
        self.assertLess(revision["felt"]["after_chroma_range_srgb8"],
                        revision["felt"]["before_chroma_range_srgb8"])
        self.assertLess(revision["walnut"]["after_grain_cycles"],
                        revision["walnut"]["before_grain_cycles"])
        invalid = copy.deepcopy(self.parameters)
        invalid["color_frequency_revision"]["walnut"]["after_grain_cycles"] = 18.0
        errors, _ = self._validate(parameters=invalid)
        self.assertTrue(any("before-after" in error for error in errors), errors)

    def test_felt_moire_control_disallows_periodic_fiber_bands(self):
        control = self.parameters["felt_moire_control"]
        self.assertFalse(control["regular_sine_fibers"])
        self.assertEqual(control["runtime_uv_repeat"], [1.0, 1.0])
        invalid = copy.deepcopy(self.parameters)
        invalid["felt_moire_control"]["regular_sine_fibers"] = True
        errors, _ = self._validate(parameters=invalid)
        self.assertTrue(any("moire-control" in error for error in errors), errors)


if __name__ == "__main__":
    unittest.main()
