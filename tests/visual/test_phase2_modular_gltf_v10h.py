import base64
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

from generate_phase2_modular_gltf_v10h import (  # noqa: E402
    ASSET_NAME, MANIFEST_NAME, DEFAULT_OUTPUT_ROOT, build_document, generate,
)
from validate_phase2_modular_gltf_v10h import validate  # noqa: E402


class Phase2V10hAssetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.document = json.loads((DEFAULT_OUTPUT_ROOT / ASSET_NAME).read_text())
        cls.manifest = json.loads((DEFAULT_OUTPUT_ROOT / MANIFEST_NAME).read_text())

    def validation_errors(self, document=None, manifest=None):
        errors, _ = validate(document or copy.deepcopy(self.document),
                             manifest or copy.deepcopy(self.manifest),
                             DEFAULT_OUTPUT_ROOT, verify_hashes=False)
        return errors

    def test_committed_asset_passes(self):
        errors, report = validate(self.document, self.manifest, DEFAULT_OUTPUT_ROOT)
        self.assertEqual([], errors)
        self.assertTrue(report["decoded_non_COLOR_accessors_exact"])
        self.assertTrue(report["scene_json_exact_after_permitted_restore"])

    def test_generation_is_byte_deterministic(self):
        with tempfile.TemporaryDirectory() as first, tempfile.TemporaryDirectory() as second:
            generate(Path(first)); generate(Path(second))
            for name in (ASSET_NAME, MANIFEST_NAME):
                self.assertEqual((Path(first) / name).read_bytes(),
                                 (Path(second) / name).read_bytes())

    def test_rejects_non_color_buffer_change(self):
        document = copy.deepcopy(self.document)
        prefix = "data:application/octet-stream;base64,"
        payload = bytearray(base64.b64decode(document["buffers"][0]["uri"][len(prefix):]))
        payload[0] ^= 1
        document["buffers"][0]["uri"] = prefix + base64.b64encode(payload).decode()
        self.assertTrue(any("outside sidewall COLOR_0" in e or "non-COLOR" in e
                            for e in self.validation_errors(document=document)))

    def test_rejects_scene_camera_renderer_metadata_change(self):
        document = copy.deepcopy(self.document)
        document["asset"]["generator"] += "_mutated"
        self.assertTrue(any("glTF JSON changed outside" in e
                            for e in self.validation_errors(document=document)))

    def test_rejects_authorization_escalation(self):
        manifest = copy.deepcopy(self.manifest)
        manifest["v10h_offline_contract"]["gpu_capture_authorized"] = True
        self.assertTrue(any("authorization boundary" in e
                            for e in self.validation_errors(manifest=manifest)))

    def test_rejects_material_texture_workaround(self):
        document = copy.deepcopy(self.document)
        mesh = next(item for item in document["meshes"]
                    if item.get("extras", {}).get("asset_role") ==
                    "pocket_liner_sidewall_visual_only")
        index = mesh["primitives"][0]["material"]
        document["materials"][index]["pbrMetallicRoughness"]["baseColorTexture"] = {"index": 0}
        self.assertTrue(any("material violates" in e
                            for e in self.validation_errors(document=document)))

    def test_parameter_grid_and_endpoint_difference_fail_closed(self):
        baseline = json.loads((ROOT / "assets/modern/phase2-v10g" / ASSET_NAME).read_text())
        with self.assertRaises(ValueError):
            build_document(baseline, 0.056, 0.055, 0.99, 0.84)
        with self.assertRaises(ValueError):
            build_document(baseline, 0.05625, 0.08, 0.98, 0.83)


if __name__ == "__main__":
    unittest.main()
