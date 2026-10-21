import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
ASSET_DIR = REPOSITORY_ROOT / "assets/modern/phase2"
ASSET = ASSET_DIR / "focused_practice_hall_venue_v1.gltf"
MANIFEST = ASSET_DIR / "focused_practice_hall_venue_v1.asset.json"
PREVIEW = ASSET_DIR / "focused_practice_hall_venue_four_camera_v1.png"
GENERATOR = REPOSITORY_ROOT / "scripts/generate_phase2_venue_gltf.py"
VALIDATOR = REPOSITORY_ROOT / "scripts/validate_phase2_venue_gltf.py"

sys.path.insert(0, str(REPOSITORY_ROOT / "scripts"))
from validate_phase2_venue_gltf import validate  # noqa: E402


class Phase2VenueGltfTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.document = json.loads(ASSET.read_text(encoding="utf-8"))
        cls.manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))

    def _validate(self, document=None, manifest=None):
        return validate(document if document is not None else copy.deepcopy(self.document),
                        manifest if manifest is not None else copy.deepcopy(self.manifest),
                        ASSET_DIR, verify_hashes=False)

    def test_committed_venue_passes_offline_validation(self):
        errors, report = validate(self.document, self.manifest, ASSET_DIR)
        self.assertEqual(errors, [])
        self.assertEqual(report["unique_triangles"], 636)
        self.assertEqual(report["lod_triangles"], {"0": 2376, "1": 408, "2": 168})
        self.assertEqual(report["lod_draws"], {"0": 34, "1": 34, "2": 14})
        self.assertEqual(report["materials"], 6)
        self.assertEqual(report["textures"], 9)
        self.assertEqual(report["cameras"], 4)

    def _matte_candidate(self):
        document = copy.deepcopy(self.document)
        floor = document["materials"][0]
        floor["name"] = "MAT_venue_matte_ground"
        floor.pop("normalTexture", None)
        floor.pop("occlusionTexture", None)
        floor["pbrMetallicRoughness"] = {"baseColorFactor": [0.028, 0.032, 0.036, 1.0],
                                         "metallicFactor": 0.0, "roughnessFactor": 0.92}
        manifest = copy.deepcopy(self.manifest)
        manifest["candidate_provenance"] = {"default_promoted": False,
                                             "approved_floor_material": "MAT_venue_matte_ground"}
        manifest["independent_matte_ground_contract"] = {
            "schema": "root_approved_matte_ground_v1", "material_index": 0,
            "material_name": "MAT_venue_matte_ground", "linear_factor": [0.028, 0.032, 0.036],
            "metallic": 0.0, "roughness": 0.92, "maps": "none",
            "provenance": "root-approved-existing-focused-practice-hall-calibration"}
        return document, manifest

    def test_explicit_authored_matte_candidate_is_accepted(self):
        document, manifest = self._matte_candidate()
        errors, _ = self._validate(document, manifest)
        self.assertEqual(errors, [])

    def test_legacy_renaming_floor_without_contract_is_rejected(self):
        document, _ = self._matte_candidate()
        errors, _ = self._validate(document, copy.deepcopy(self.manifest))
        self.assertTrue(any("six approved material" in error for error in errors), errors)

    def test_candidate_wrong_linear_factor_is_rejected(self):
        document, manifest = self._matte_candidate()
        document["materials"][0]["pbrMetallicRoughness"]["baseColorFactor"][0] = 0.28
        errors, _ = self._validate(document, manifest)
        self.assertTrue(any("independent matte-ground" in error for error in errors), errors)

    def test_candidate_secret_texture_map_is_rejected(self):
        for location in ("base", "normal", "clearcoat"):
            document, manifest = self._matte_candidate()
            floor = document["materials"][0]
            if location == "base":
                floor["pbrMetallicRoughness"]["baseColorTexture"] = {"index": 0}
            elif location == "normal":
                floor["normalTexture"] = {"index": 1}
            else:
                floor["extensions"] = {"KHR_materials_clearcoat": {"clearcoatTexture": {"index": 0}}}
            errors, _ = self._validate(document, manifest)
            self.assertTrue(any("independent matte-ground" in error for error in errors), errors)

    def test_generator_is_byte_deterministic(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-venue-") as directory:
            completed = subprocess.run(
                [sys.executable, str(GENERATOR), "--output-dir", directory,
                 "--shared-texture-dir", str(ASSET_DIR)],
                cwd=str(REPOSITORY_ROOT), text=True, capture_output=True, timeout=30)
            self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
            for committed in (ASSET, MANIFEST, PREVIEW):
                self.assertEqual((Path(directory) / committed.name).read_bytes(),
                                 committed.read_bytes(), committed.name)

    def test_validator_cli_reports_geometry_and_camera_budget(self):
        completed = subprocess.run(
            [sys.executable, str(VALIDATOR), str(ASSET)], cwd=str(REPOSITORY_ROOT),
            text=True, capture_output=True, timeout=20)
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        self.assertIn("unique_triangles=636", completed.stdout)
        self.assertIn("LOD0_draws=34", completed.stdout)
        self.assertIn("cameras=4", completed.stdout)

    def test_lod_render_cost_strictly_decreases(self):
        triangles = self.manifest["lod_set"]["rendered_triangles"]
        self.assertGreater(triangles["0"], triangles["1"])
        self.assertGreater(triangles["1"], triangles["2"])
        self.assertLessEqual(max(self.manifest["lod_set"]["draw_calls_unbatched"].values()), 64)

    def test_furniture_or_brand_node_is_rejected(self):
        invalid = copy.deepcopy(self.document)
        invalid["nodes"].append({"name": "NODE_brand_bench", "translation": [0, 0, 0]})
        errors, _ = self._validate(document=invalid)
        self.assertTrue(any("furniture, brand" in error for error in errors), errors)

    def test_background_brighter_than_two_ev_is_rejected(self):
        invalid = copy.deepcopy(self.document)
        invalid["materials"][0]["extras"]["background_relative_to_table_ev"] = -1.5
        errors, _ = self._validate(document=invalid)
        self.assertTrue(any("2-3 EV" in error for error in errors), errors)

    def test_external_texture_uri_is_rejected(self):
        invalid = copy.deepcopy(self.document)
        invalid["images"][0]["uri"] = "https://example.invalid/floor.png"
        errors, _ = self._validate(document=invalid)
        self.assertTrue(any("approved nine" in error for error in errors), errors)

    def test_camera_socket_drift_is_rejected(self):
        invalid = copy.deepcopy(self.document)
        node = next(node for node in invalid["nodes"]
                    if node.get("name") == "SOCKET_camera_broadcast_oblique")
        node["translation"][0] += 0.25
        errors, _ = self._validate(document=invalid)
        self.assertTrue(any("broadcast_oblique camera sockets" in error for error in errors), errors)

    def test_venue_geometry_crossing_protected_camera_ray_is_rejected(self):
        invalid = copy.deepcopy(self.document)
        wall = next(node for node in invalid["nodes"]
                    if node.get("name") == "NODE_venue_wall_z_pos_LOD0")
        wall["translation"] = [0.0, 2.0, 0.45]
        errors, _ = self._validate(document=invalid)
        self.assertTrue(any("is occluded by" in error for error in errors), errors)

    def test_tactical_overhead_visibility_override_is_required(self):
        invalid = copy.deepcopy(self.manifest)
        invalid["visibility_overrides"]["tactical_overhead"] = []
        errors, _ = self._validate(manifest=invalid)
        self.assertTrue(any("tactical overhead" in error for error in errors), errors)


if __name__ == "__main__":
    unittest.main()
