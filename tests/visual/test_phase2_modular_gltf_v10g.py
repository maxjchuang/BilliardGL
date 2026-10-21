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

from generate_phase2_modular_gltf_v10g import (  # noqa: E402
    ASSET_NAME, BASELINE_ROOT, DEFAULT_OUTPUT_ROOT, EXPECTED_SHA256,
    MANIFEST_NAME, generate, sha256_path,
)
from validate_phase2_modular_gltf_v10g import validate  # noqa: E402


class Phase2ModularGltfV10gTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.document = json.loads((DEFAULT_OUTPUT_ROOT / ASSET_NAME).read_text())
        cls.manifest = json.loads((DEFAULT_OUTPUT_ROOT / MANIFEST_NAME).read_text())

    def test_committed_candidate_passes_strict_offline_validation(self):
        errors, report = validate(self.document, self.manifest,
                                  DEFAULT_OUTPUT_ROOT, BASELINE_ROOT, True)
        self.assertEqual(errors, [])
        self.assertEqual(report["material_count"], 29)
        self.assertEqual(report["changed_buffer_byte_count"], 12474)
        self.assertTrue(report["all_changed_buffer_bytes_within_sidewall_color_accessors"])
        self.assertEqual(report["v10g_image_file_count"], 0)
        self.assertEqual(report["topology"], {
            "0": {"vertices": 576, "indices": 864, "edges": 720, "cross_edges": 432},
            "1": {"vertices": 384, "indices": 576, "edges": 480, "cross_edges": 288},
            "2": {"vertices": 240, "indices": 360, "edges": 300, "cross_edges": 180},
        })

    def test_generator_is_byte_deterministic_and_outputs_no_images(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-v10g-generation-") as directory:
            first = Path(directory) / "first"
            second = Path(directory) / "second"
            generate(first)
            generate(second)
            for name in (ASSET_NAME, MANIFEST_NAME):
                self.assertEqual((first / name).read_bytes(), (second / name).read_bytes())
                self.assertEqual((first / name).read_bytes(),
                                 (DEFAULT_OUTPUT_ROOT / name).read_bytes())
            self.assertFalse(any(path.suffix.lower() in (".png", ".ppm")
                                 for path in first.iterdir()))

    def test_v10f_frozen_inputs_remain_exact(self):
        self.assertEqual(sha256_path(BASELINE_ROOT / ASSET_NAME),
                         EXPECTED_SHA256["baseline_gltf"])
        self.assertEqual(sha256_path(BASELINE_ROOT / MANIFEST_NAME),
                         EXPECTED_SHA256["baseline_manifest"])

    def test_sidewall_material_redirection_is_required(self):
        invalid = copy.deepcopy(self.document)
        mesh = next(item for item in invalid["meshes"]
                    if item.get("extras", {}).get("asset_role") ==
                    "pocket_liner_sidewall_visual_only")
        mesh["primitives"][0]["material"] = 5
        errors, _ = validate(invalid, self.manifest, DEFAULT_OUTPUT_ROOT,
                             BASELINE_ROOT, False)
        self.assertTrue(any("appended material" in error or
                            "glTF JSON differs" in error for error in errors), errors)

    def test_geometry_or_ownership_drift_is_rejected(self):
        invalid = copy.deepcopy(self.document)
        node = next(item for item in invalid["nodes"]
                    if item.get("extras", {}).get("asset_role") ==
                    "pocket_liner_sidewall_visual_only")
        node["extras"]["asset_role"] = "wrong_sidewall_owner"
        errors, _ = validate(invalid, self.manifest, DEFAULT_OUTPUT_ROOT,
                             BASELINE_ROOT, False)
        self.assertTrue(any("three sidewall" in error or
                            "glTF JSON differs" in error for error in errors), errors)

    def test_non_neutral_or_out_of_range_material_is_rejected(self):
        invalid = copy.deepcopy(self.document)
        invalid["materials"][-1]["pbrMetallicRoughness"]["baseColorFactor"][0] += 0.01
        errors, _ = validate(invalid, self.manifest, DEFAULT_OUTPUT_ROOT,
                             BASELINE_ROOT, False)
        self.assertTrue(any("untextured neutral contract" in error for error in errors), errors)

    def test_shared_texture_hash_binding_cannot_drift(self):
        invalid_manifest = copy.deepcopy(self.manifest)
        invalid_manifest["files"]["pbr_textures"]["felt"]["basecolor"]["sha256"] = \
            "sha256:" + "0" * 64
        errors, _ = validate(self.document, invalid_manifest, DEFAULT_OUTPUT_ROOT,
                             BASELINE_ROOT, False)
        self.assertTrue(any("shared non-glTF asset bindings" in error
                            for error in errors), errors)

    def test_generator_rejects_out_of_contract_parameters(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-v10g-bad-param-") as directory:
            completed = subprocess.run([
                sys.executable, str(SCRIPTS / "generate_phase2_modular_gltf_v10g.py"),
                "--output-root", directory, "--lower-scalar", "0.29"],
                cwd=ROOT, capture_output=True, text=True)
            self.assertEqual(completed.returncode, 2, completed.stdout + completed.stderr)
            self.assertIn("outside [0.30,0.42]", completed.stdout)
            self.assertEqual(list(Path(directory).iterdir()), [])

    def test_generator_rejects_mutated_gate_before_output(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-v10g-bad-gate-") as directory:
            temporary = Path(directory)
            gate = json.loads((ROOT / "docs/art/phase2-pocket-v10g-offline-correction-design-gate-v2.json").read_text())
            gate["GPU_CAPTURE_AUTHORIZED"] = True
            bad_gate = temporary / "bad_gate.json"
            bad_gate.write_text(json.dumps(gate))
            output = temporary / "out"
            completed = subprocess.run([
                sys.executable, str(SCRIPTS / "generate_phase2_modular_gltf_v10g.py"),
                "--output-root", str(output), "--design-gate", str(bad_gate)],
                cwd=ROOT, capture_output=True, text=True)
            self.assertEqual(completed.returncode, 2, completed.stdout + completed.stderr)
            self.assertIn("SHA-256 mismatch", completed.stdout)
            self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
