import base64
import copy
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
ASSET_DIR = REPOSITORY_ROOT / "assets/modern/phase2-v10f"
ASSET = ASSET_DIR / "focused_practice_hall_modular_v1.gltf"
MANIFEST = ASSET_DIR / "focused_practice_hall_modular_v1.asset.json"
ATLAS = ASSET_DIR / "focused_practice_hall_ball_identity_atlas_v1.png"
SVG_SOURCE = ASSET_DIR / "focused_practice_hall_ball_identity_atlas_v1.svg"
CLOSEUP = ASSET_DIR / "focused_practice_hall_ball_identity_closeup_1_8_11_15_v2.png"
GENERATOR = REPOSITORY_ROOT / "scripts/generate_phase2_modular_gltf_v10f.py"
VALIDATOR = REPOSITORY_ROOT / "scripts/validate_phase2_modular_gltf_v10f.py"

sys.path.insert(0, str(REPOSITORY_ROOT / "scripts"))
from validate_phase2_modular_gltf_v10f import validate  # noqa: E402
from generate_m1_graybox_gltf import POCKETS  # noqa: E402


class Phase2ModularGltfTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.document = json.loads(ASSET.read_text(encoding="utf-8"))
        cls.manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))

    def _validate_mutation(self, document=None, manifest=None):
        return validate(document or copy.deepcopy(self.document),
                        manifest or copy.deepcopy(self.manifest),
                        ASSET_DIR, verify_hashes=False)

    def test_committed_candidate_passes_offline_validation(self):
        errors, report = validate(self.document, self.manifest, ASSET_DIR)
        self.assertEqual(errors, [])
        self.assertEqual(report["unique_triangles"], 7054)
        self.assertEqual(report["materials"], 28)
        self.assertEqual(report["textures"], 19)
        self.assertEqual(report["markers"], 23)

    def test_generator_is_byte_deterministic_for_all_outputs(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-phase2-") as directory:
            completed = subprocess.run(
                [sys.executable, str(GENERATOR), "--output-dir", directory],
                cwd=str(REPOSITORY_ROOT), text=True, capture_output=True, timeout=60)
            self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
            generated = Path(directory)
            for committed in sorted(ASSET_DIR.iterdir()):
                if committed.is_dir() or committed.name == "README.md" or \
                        committed.name.startswith("focused_practice_hall_venue_"):
                    continue
                self.assertEqual((generated / committed.name).read_bytes(), committed.read_bytes(),
                                 committed.name)

    def test_validator_cli_reports_budget_counts(self):
        completed = subprocess.run(
            [sys.executable, str(VALIDATOR), str(ASSET)], cwd=str(REPOSITORY_ROOT),
            text=True, capture_output=True, timeout=20)
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        self.assertIn("unique_triangles=7054", completed.stdout)
        self.assertIn("materials=28", completed.stdout)
        self.assertIn("textures=19", completed.stdout)

    def test_all_lod_sets_strictly_reduce_triangles_and_share_markers(self):
        marker_lists = []
        for lod_set in self.manifest["lod_sets"].values():
            triangles = lod_set["triangles"]
            self.assertGreater(triangles["0"], triangles["1"])
            self.assertGreater(triangles["1"], triangles["2"])
            marker_lists.append(lod_set["alignment_markers"])
        self.assertTrue(all(markers == marker_lists[0] for markers in marker_lists[1:]))

    def test_invalid_tangent_layout_is_rejected(self):
        invalid = copy.deepcopy(self.document)
        tangent_accessor = invalid["meshes"][0]["primitives"][0]["attributes"]["TANGENT"]
        invalid["accessors"][tangent_accessor]["type"] = "VEC3"
        errors, _ = self._validate_mutation(document=invalid)
        self.assertTrue(any("invalid tangent" in error or "attribute counts" in error
                            for error in errors), errors)

    def test_unpromoted_pocket_marker_is_rejected(self):
        invalid = copy.deepcopy(self.document)
        invalid["nodes"].append({
            "name": "SOCKET_pocket_0_throat",
            "translation": [-0.67, 0.84, -1.31],
        })
        errors, _ = self._validate_mutation(document=invalid)
        self.assertTrue(any("unpromoted pocket marker" in error for error in errors), errors)

    def test_external_image_uri_is_rejected(self):
        invalid = copy.deepcopy(self.document)
        invalid["images"][0]["uri"] = "https://example.invalid/competitor-atlas.png"
        errors, _ = self._validate_mutation(document=invalid)
        self.assertTrue(any("external" in error or "generated ball atlas" in error
                            for error in errors), errors)

    def test_missing_lod_marker_reference_is_rejected(self):
        invalid = copy.deepcopy(self.document)
        root = self.manifest["lod_sets"]["ball"]["nodes"]["2"]
        invalid["nodes"][root]["extras"]["alignment_marker_nodes"] = []
        errors, _ = self._validate_mutation(document=invalid)
        self.assertTrue(any("identical marker list" in error for error in errors), errors)

    def test_geometry_entering_pocket_clearance_is_rejected(self):
        invalid = copy.deepcopy(self.document)
        leather = next(node for node in invalid["nodes"]
                       if node.get("extras", {}).get("asset_role") ==
                       "pocket_facing_visual_only")
        mesh = invalid["meshes"][leather["mesh"]]
        accessor_index = mesh["primitives"][0]["attributes"]["POSITION"]
        accessor = invalid["accessors"][accessor_index]
        view = invalid["bufferViews"][accessor["bufferView"]]
        prefix = "data:application/octet-stream;base64,"
        encoded = invalid["buffers"][0]["uri"]
        payload = bytearray(base64.b64decode(encoded[len(prefix):]))
        offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
        struct.pack_into("<3f", payload, offset, -0.635, 0.86483976, 0.0)
        invalid["buffers"][0]["uri"] = prefix + base64.b64encode(payload).decode("ascii")
        errors, _ = self._validate_mutation(document=invalid)
        self.assertTrue(any("NO_VISUAL_SOLID" in error for error in errors), errors)

    def test_manifest_cannot_claim_unpromoted_parameters(self):
        invalid_manifest = copy.deepcopy(self.manifest)
        invalid_manifest["constraints"]["unpromoted_pocket_parameters_present"] = True
        errors, _ = self._validate_mutation(manifest=invalid_manifest)
        self.assertTrue(any("unpromoted_pocket_parameters_present" in error
                            for error in errors), errors)

    def test_felt_topology_contract_rejects_cracked_boundary_claim(self):
        invalid_manifest = copy.deepcopy(self.manifest)
        invalid_manifest["felt_topology"]["boundary_loops"] = 7
        errors, _ = self._validate_mutation(manifest=invalid_manifest)
        self.assertTrue(any("felt topology evidence" in error for error in errors), errors)

    def test_ball_atlas_uses_mip_safe_inset_and_review_identities(self):
        contract = self.manifest["ball_atlas_contract"]
        self.assertEqual(contract["tile_size_px"], [512, 256])
        self.assertEqual(contract["safe_inset_px"], 24)
        self.assertEqual(contract["review_identities"], [1, 8, 11, 15])
        self.assertEqual(contract["sphere_uv_u_orientation"],
                         "decreases_with_positive_longitude")
        self.assertEqual(contract["glyph_storage_orientation"], "upright_source")
        self.assertTrue(CLOSEUP.is_file())
        invalid_manifest = copy.deepcopy(self.manifest)
        invalid_manifest["ball_atlas_contract"]["safe_inset_px"] = 4
        errors, _ = self._validate_mutation(manifest=invalid_manifest)
        self.assertTrue(any("tile/mip-safe" in error for error in errors), errors)

    def test_textured_base_color_factor_must_remain_neutral(self):
        invalid = copy.deepcopy(self.document)
        material = next(item for item in invalid["materials"]
                        if item["name"] == "MAT_table_felt_teal")
        material["pbrMetallicRoughness"]["baseColorFactor"] = [0.0144, 0.107, 0.1248, 1.0]
        errors, _ = self._validate_mutation(document=invalid)
        self.assertTrue(any("double tint" in error for error in errors), errors)

    def test_generated_v10f_pocket_profile_enforces_connected_topology_and_halfspaces(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-phase2-v10f-") as directory:
            completed = subprocess.run(
                [sys.executable, str(GENERATOR), "--output-dir", directory],
                cwd=str(REPOSITORY_ROOT), text=True, capture_output=True, timeout=60)
            self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
            generated = Path(directory)
            document = json.loads((generated / ASSET.name).read_text(encoding="utf-8"))
            manifest = json.loads((generated / MANIFEST.name).read_text(encoding="utf-8"))
            errors, _ = validate(document, manifest, generated)
            self.assertEqual(errors, [])

            def move_first_vertex_across_plane(source, role, inward_sign):
                invalid = copy.deepcopy(source)
                node = next(item for item in invalid["nodes"]
                            if item.get("extras", {}).get("asset_role") == role and
                            item.get("extras", {}).get("lod") == 0)
                pocket_id = node["extras"]["local_ranges"][0]["pocket_id"]
                pocket = next(item for item in POCKETS if item[0] == pocket_id)
                center, inward = pocket[3], pocket[4]
                mesh = invalid["meshes"][node["mesh"]]
                accessor = invalid["accessors"][
                    mesh["primitives"][0]["attributes"]["POSITION"]]
                view = invalid["bufferViews"][accessor["bufferView"]]
                prefix = "data:application/octet-stream;base64,"
                payload = bytearray(base64.b64decode(invalid["buffers"][0]["uri"][len(prefix):]))
                offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
                distance = 0.01 * inward_sign
                struct.pack_into("<3f", payload, offset,
                                 center[0] + inward[0] * distance,
                                 0.82,
                                 center[1] + inward[1] * distance)
                invalid["buffers"][0]["uri"] = prefix + base64.b64encode(payload).decode("ascii")
                return invalid

            invalid_liner = move_first_vertex_across_plane(
                document, "pocket_liner_rim_visual_only", -1.0)
            errors, _ = validate(invalid_liner, manifest, generated, verify_hashes=False)
            self.assertTrue(any("throat/bottom geometry outside mouth plane" in error
                                for error in errors), errors)

            invalid_apron = move_first_vertex_across_plane(
                document, "pocket_apron_integrated_wedge", 1.0)
            errors, _ = validate(invalid_apron, manifest, generated, verify_hashes=False)
            self.assertTrue(any("apron geometry inside mouth plane" in error
                                for error in errors), errors)

            invalid_contract = copy.deepcopy(document)
            liner = next(item for item in invalid_contract["nodes"]
                         if item.get("extras", {}).get("asset_role") ==
                         "pocket_liner_rim_visual_only")
            liner["extras"]["shape"] = "rectangular_slot"
            errors, _ = validate(invalid_contract, manifest, generated, verify_hashes=False)
            self.assertTrue(any("v10f rim contract" in error for error in errors), errors)

            invalid_seam = copy.deepcopy(document)
            sidewall = next(item for item in invalid_seam["nodes"]
                            if item.get("extras", {}).get("asset_role") ==
                            "pocket_liner_sidewall_visual_only")
            sidewall["extras"]["local_ranges"][0]["upper_boundary"][0][0] += 0.001
            errors, _ = validate(invalid_seam, manifest, generated, verify_hashes=False)
            self.assertTrue(any("boundaries are not position matched" in error
                                for error in errors), errors)

            invalid_frame = copy.deepcopy(document)
            invalid_frame["asset"]["extras"]["pocket_visual_frame"][
                "marker_z_mapping"] = "z_visual=z_marker"
            errors, _ = validate(invalid_frame, manifest, generated, verify_hashes=False)
            self.assertTrue(any("frame mapping or handedness" in error for error in errors),
                            errors)

            invalid_marker_sha = copy.deepcopy(document)
            invalid_marker_sha["asset"]["extras"]["physics_marker_binding_sha256"] = \
                "sha256:" + "0" * 64
            errors, _ = validate(invalid_marker_sha, manifest, generated,
                                 verify_hashes=False)
            self.assertTrue(any("marker binding SHA-256" in error for error in errors), errors)


if __name__ == "__main__":
    unittest.main()
