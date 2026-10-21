import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
ASSET = REPOSITORY_ROOT / "assets/modern/graybox/focused_practice_hall_m1_graybox.gltf"
ALIGNMENT = REPOSITORY_ROOT / "assets/modern/graybox/focused_practice_hall_m1_alignment_v1.json"
GENERATOR = REPOSITORY_ROOT / "scripts/generate_m1_graybox_gltf.py"
VALIDATOR = REPOSITORY_ROOT / "scripts/validate_m1_graybox_gltf.py"

sys.path.insert(0, str(REPOSITORY_ROOT / "scripts"))
from validate_m1_graybox_gltf import validate  # noqa: E402


class M1GrayboxGltfTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.document = json.loads(ASSET.read_text(encoding="utf-8"))
        cls.alignment = json.loads(ALIGNMENT.read_text(encoding="utf-8"))

    def test_committed_asset_passes_offline_validation(self):
        errors, report = validate(self.document, self.alignment)
        self.assertEqual(errors, [])
        self.assertEqual(report["materials"], 9)
        self.assertEqual(report["markers"], 23)
        self.assertLess(report["unique_triangles"], 5000)

    def test_generator_is_byte_deterministic(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-graybox-") as directory:
            generated = Path(directory) / "graybox.gltf"
            generated_alignment = Path(directory) / "alignment.json"
            completed = subprocess.run(
                [sys.executable, str(GENERATOR), "--output", str(generated),
                 "--alignment-output", str(generated_alignment)],
                cwd=str(REPOSITORY_ROOT), text=True, capture_output=True, timeout=10)
            self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
            self.assertEqual(generated.read_bytes(), ASSET.read_bytes())
            self.assertEqual(generated_alignment.read_bytes(), ALIGNMENT.read_bytes())

    def test_validator_cli_reports_asset_counts(self):
        completed = subprocess.run(
            [sys.executable, str(VALIDATOR), str(ASSET)], cwd=str(REPOSITORY_ROOT),
            text=True, capture_output=True, timeout=10)
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        self.assertIn("unique_triangles=", completed.stdout)
        self.assertIn("materials=9", completed.stdout)

    def test_missing_mouth_marker_is_rejected(self):
        invalid = copy.deepcopy(self.document)
        invalid["nodes"] = [node for node in invalid["nodes"]
                            if node.get("name") != "SOCKET_pocket_4_mouth"]
        errors, _ = validate(invalid, self.alignment)
        self.assertTrue(any("SOCKET_pocket_4_mouth" in error for error in errors))

    def test_unpromoted_throat_marker_is_rejected(self):
        invalid = copy.deepcopy(self.document)
        invalid["nodes"].append({
            "name": "SOCKET_pocket_0_throat", "translation": [-0.66, 0.86483976, -1.3]})
        errors, _ = validate(invalid, self.alignment)
        self.assertTrue(any("unpromoted pocket markers" in error for error in errors))

    def test_external_buffer_is_rejected(self):
        invalid = copy.deepcopy(self.document)
        invalid["buffers"][0]["uri"] = "graybox.bin"
        errors, _ = validate(invalid, self.alignment)
        self.assertTrue(any("embedded" in error for error in errors))

    def test_alignment_binding_cannot_enable_capture_detach(self):
        invalid_alignment = copy.deepcopy(self.alignment)
        invalid_alignment["capture_presentation"]["presentation_detach_enabled"] = True
        errors, _ = validate(self.document, invalid_alignment)
        self.assertTrue(any("simulation-driven" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
