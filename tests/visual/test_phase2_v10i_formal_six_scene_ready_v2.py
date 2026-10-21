import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest import mock

from scripts import capture_visual_baselines as capture
from scripts import prepare_phase2_v10i_formal_six_scene_ready_v2 as ready
from scripts import validate_phase2_production_visuals as production_validator
from scripts import validate_phase2_v10i_formal_six_scene_ready_v2 as ready_validator


class Phase2V10iFormalSixSceneReadinessV2Tests(unittest.TestCase):
    def authorization(self, readiness_sha="a" * 64):
        return {
            "schema": "phase2_v10i_formal_six_scene_one_shot_authorization_v2",
            "status": "AUTHORIZED_FOR_EXACTLY_SIX_FRESH_PROCESS_CAPTURES",
            "gpu_capture_authorized": True,
            "formal_six_scene_evidence_authorized": True,
            "authorized_capture_count": 6,
            "consumed_capture_count": 0,
            "scene_ids_in_order": list(ready.SCENE_IDS),
            "required_readiness_manifest_sha256": readiness_sha,
        }

    def test_blocked_lineage_and_exact_scene_order_are_frozen(self):
        self.assertEqual(ready.sha256(ready.BLOCKED_MANIFEST),
                         ready.BLOCKED_MANIFEST_SHA256)
        self.assertEqual(ready.BLOCKED_MANIFEST_SHA256,
                         "7cdfc1975b42c3e17a17d4f4aa940d08888faeb5b94e6e8702302c4b01632dcf")
        self.assertEqual(ready.SCENE_IDS, (
            "break", "low_aim", "ball_closeup", "pocket_closeup",
            "tactical_overhead", "post_shot_observe"))

    def test_package_is_self_contained_sha_bound_and_preserves_v10i_bytes(self):
        manifest = ready.materialize_formal_asset_package()
        self.assertEqual(len(manifest["files"]), 24)
        self.assertEqual(manifest["required_asset_count"], 6)
        self.assertEqual(manifest["runtime_dependency_count"], 18)
        self.assertFalse(manifest["symlinks_allowed"])
        for item in manifest["files"]:
            path = ready.PACKAGE_ROOT / item["filename"]
            self.assertTrue(path.is_file())
            self.assertFalse(path.is_symlink())
            self.assertEqual(ready.sha256(path), item["packaged_sha256"])
            self.assertEqual(item["source_sha256"], item["packaged_sha256"])
        for filename, digest in ready.V10I_FROZEN.items():
            self.assertEqual(ready.sha256(ready.PACKAGE_ROOT / filename), digest)

    def test_formal_child_environment_replaces_inherited_asset_selection(self):
        inherited = {
            "KEEP": "yes", "BILLIARDGL_MODERN_PHASE2_ROOT": "/wrong",
            "BILLIARDGL_FORMAL_CAPTURE": "0",
        }
        actual = capture.explicit_formal_child_environment(
            ready.PACKAGE_ROOT, inherited)
        self.assertEqual(actual["KEEP"], "yes")
        self.assertEqual(actual["BILLIARDGL_MODERN_PHASE2_ROOT"],
                         str(ready.PACKAGE_ROOT.resolve(strict=True)))
        self.assertEqual(actual["BILLIARDGL_FORMAL_CAPTURE"], "1")

    def test_formal_capture_explicitly_selects_modern_renderer(self):
        source = Path(capture.__file__).read_text(encoding="utf-8")
        self.assertIn('["--renderer", "modern"]', source)
        self.assertRegex(
            source,
            r'AutomationClient\(str\(executable\), mode="rendered",\s*'
            r'extra_args=automation_extra_args')

    def test_missing_authorization_rejects_before_output_or_process(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "capture"
            with mock.patch.object(capture, "AutomationClient") as client:
                with self.assertRaisesRegex(RuntimeError, "authorization is required"):
                    capture.capture_all(Path("unused"), output,
                                        formal_asset_package=ready.PACKAGE_ROOT)
            client.assert_not_called()
            self.assertFalse(output.exists())

    def test_bad_authorization_variants_reject_before_output_or_process(self):
        mutations = (
            ("schema", "wrong", "schema mismatch"),
            ("scene_ids_in_order", list(reversed(ready.SCENE_IDS)), "scene order"),
            ("required_readiness_manifest_sha256", "b" * 64, "readiness SHA"),
            ("authorized_capture_count", 5, "capture count"),
        )
        for field, value, message in mutations:
            with self.subTest(field=field), tempfile.TemporaryDirectory() as directory:
                output = Path(directory) / "capture"
                authorization = self.authorization()
                authorization[field] = value
                with mock.patch.object(capture, "AutomationClient") as client:
                    with self.assertRaisesRegex(RuntimeError, message):
                        capture.capture_all(
                            Path("unused"), output,
                            formal_asset_package=ready.PACKAGE_ROOT,
                            authorization=authorization,
                            readiness_manifest_sha256="a" * 64)
                client.assert_not_called()
                self.assertFalse(output.exists())

    def _attestation(self, asset_root: Path):
        required = []
        for asset_id, filename in capture.FORMAL_ASSET_FILES.items():
            path = (asset_root / filename).resolve(strict=True)
            required.append({
                "id": asset_id, "canonical_path": str(path),
                "sha256": ready.sha256(path),
            })
        dependencies = []
        package = json.loads(
            (asset_root / "formal-asset-package-v2.json").read_text())
        for item in package["files"]:
            path = (asset_root / item["filename"]).resolve(strict=True)
            dependencies.append({
                "id": item["filename"], "canonical_path": str(path),
                "sha256": ready.sha256(path),
            })
        return {
            "schema": "billiardgl_formal_asset_attestation_v1",
            "asset_root": str(asset_root.resolve(strict=True)),
            "load_state": "loaded",
            "silent_material_fallback_allowed": False,
            "required_assets": required,
            "runtime_dependencies": dependencies,
            "load_errors": [],
        }

    def test_ready_attestation_rejects_path_sha_error_and_silent_fallback(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "asset_root"
            shutil.copytree(ready.PACKAGE_ROOT, root)
            base = {
                "formal_capture": True, "pbr_loaded": True,
                "pbr_texture_count": 22, "environment_enabled": True,
                "gltf_scene_loaded": True, "venue_scene_loaded": True,
                "render_quality": "medium", "msaa_samples": 4,
                "formal_asset_attestation": self._attestation(root),
            }
            self.assertEqual(
                len(capture.validate_ready_asset_attestation(base, root)["required_assets"]),
                6)
            for mutation, message in (
                (lambda value: value["required_assets"][0].update(sha256="0" * 64),
                 "path/SHA mismatch"),
                (lambda value: value.update(load_errors=[{"code": "missing"}]),
                 "load contract"),
                (lambda value: value.update(silent_material_fallback_allowed=True),
                 "load contract"),
            ):
                with self.subTest(message=message):
                    attestation = json.loads(json.dumps(base["formal_asset_attestation"]))
                    mutation(attestation)
                    with self.assertRaisesRegex(RuntimeError, message):
                        capture.validate_ready_asset_attestation(
                            {**base, "formal_asset_attestation": attestation}, root)

    def _formal_capture_manifest(self, capture_root: Path):
        asset_root = capture_root / "asset_root"
        shutil.copytree(ready.PACKAGE_ROOT, asset_root)
        attestation = self._attestation(asset_root)
        attestation["renderer_load_checks"] = {
            "pbr_loaded": True, "pbr_texture_count_22": True,
            "environment_enabled": True, "gltf_scene_loaded": True,
            "venue_scene_loaded": True, "medium_quality": True,
            "msaa_4x": True,
        }
        performance = {
            "warmup_frames": 120, "sample_frames": 300,
            "p95_total_frame_ms": 10.0,
            "draw_calls_complete_scene": 80,
            "texture_resident_mib": 100,
            "measurement_method": "gpu_timer_query_plus_cpu_frame",
            "gpu_timer_queries_observed": True,
        }
        value = {
            "schema": "phase2_v10i_formal_six_scene_capture_v2",
            "renderer": "modern", "production_gltf": True,
            "resolution": {"width": 1920, "height": 1080},
            "quality": "medium", "asset_root": "asset_root",
            "formal_asset_attestation_required": True,
            "performance": performance,
            "scenes": [{
                "scene_id": scene_id,
                "loaded_asset_attestation": attestation,
                "performance": performance,
            } for scene_id in ready.SCENE_IDS],
        }
        for asset_id, filename in capture.FORMAL_ASSET_FILES.items():
            value[capture.FORMAL_MANIFEST_HASH_FIELDS[asset_id]] = \
                ready.sha256(asset_root / filename)
        return value

    def test_formal_manifest_contract_accepts_complete_cpu_fixture(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            value = self._formal_capture_manifest(root)
            path = root / "manifest.json"
            path.write_text(json.dumps(value))
            loaded, scenes, _ = production_validator.load_capture_manifest(path)
            self.assertEqual(loaded["schema"],
                             "phase2_v10i_formal_six_scene_capture_v2")
            self.assertEqual(set(scenes), set(ready.SCENE_IDS))

    def test_formal_manifest_rejects_missing_gpu_timer_observation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            value = self._formal_capture_manifest(root)
            value["scenes"][0]["performance"]["gpu_timer_queries_observed"] = False
            path = root / "manifest.json"
            path.write_text(json.dumps(value))
            with self.assertRaisesRegex(production_validator.ReviewError,
                                        "formal performance collection"):
                production_validator.load_capture_manifest(path)

    def test_generated_readiness_validates_with_fake_release_binary(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            executable = root / "Billiards"
            executable.write_bytes(b"fresh-release-test-binding")
            package_root = root / "formal-package"
            ready_root = root / "ready"
            files = ready.prepare(ready_root, package_root, executable)
            result = ready_validator.validate(ready_root)
            self.assertEqual(result["status"], "PASS_CPU_ONLY_READINESS_V2")
            plan = json.loads(files["six_scene_plan"].read_text())
            self.assertEqual([row["status"] for row in plan["rows"]],
                             ["NOT_RUN"] * 6)
            for name in ("implementation_gate", "six_scene_plan",
                         "machine_template", "readiness_manifest",
                         "readiness_receipt"):
                value = json.loads(files[name].read_text())
                for field in ready_validator.FALSE_FIELDS:
                    self.assertIs(value[field], False, name + ":" + field)

    def test_preparer_has_no_process_gpu_or_screenshot_api(self):
        source = Path(ready.__file__).read_text(encoding="utf-8")
        for forbidden in ("import subprocess", "Popen(", "Cocoa", "OpenGL",
                          "AutomationClient", "screenshot("):
            self.assertNotIn(forbidden, source)


if __name__ == "__main__":
    unittest.main()
