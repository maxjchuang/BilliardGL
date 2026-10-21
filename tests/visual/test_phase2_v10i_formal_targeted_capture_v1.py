from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from scripts import capture_visual_baselines as capture
from scripts import validate_phase2_v10i_formal_targeted_camera_capture as targeted_validator


class Phase2FormalTargetedCaptureV1Tests(unittest.TestCase):
    def authorization(self, output: Path, readiness_sha: str = "a" * 64) -> dict:
        return {
            "schema": "phase2_v10i_formal_targeted_camera_authorization_v1",
            "status": "AUTHORIZED_FOR_EXACTLY_TWO_FRESH_PROCESS_CAPTURES",
            "authorization_id": "test-targeted-only",
            "authorization_nonce": "test-targeted-only-nonce-0001",
            "authorized_by": "test",
            "authorized_at_utc": "2026-08-11T00:00:00Z",
            "authorized_output_canonical_path": str(output.resolve()),
            "gpu_capture_authorized": True,
            "targeted_camera_evidence_authorized": True,
            "formal_six_scene_evidence_authorized": False,
            "full_six_scene_capture_authorized": False,
            "authorized_capture_count": 2,
            "consumed_capture_count": 0,
            "scene_ids_in_order": ["break", "low_aim"],
            "required_readiness_manifest_sha256": readiness_sha,
            "targeted_camera_pass": False,
            "formal_pass": False,
            "golden_promotion": False,
            "m2_promotion": False,
        }

    def test_parser_accepts_only_exact_ordered_two_scene_scope(self) -> None:
        self.assertEqual(
            capture.parse_formal_targeted_scenes("break,low_aim"),
            ("break", "low_aim"))
        for value in ("low_aim,break", "break", "low_aim",
                      "break,low_aim,ball_closeup", "break, low_aim", ""):
            with self.subTest(value=value):
                with self.assertRaisesRegex(RuntimeError, "exactly"):
                    capture.parse_formal_targeted_scenes(value)

    def test_targeted_authorization_binds_schema_count_ids_output_and_readiness(self) -> None:
        output = Path("/tmp/phase2-targeted-auth-test")
        value = self.authorization(output)
        capture.validate_formal_targeted_authorization(value, "a" * 64, output)
        mutations = []
        for field, bad in (
                ("schema", "phase2_v10i_formal_six_scene_one_shot_authorization_v3"),
                ("authorization_nonce", ""),
                ("authorized_capture_count", 6),
                ("scene_ids_in_order", ["low_aim", "break"]),
                ("authorized_output_canonical_path", "/tmp/different"),
                ("required_readiness_manifest_sha256", "b" * 64),
                ("targeted_camera_evidence_authorized", False),
                ("formal_six_scene_evidence_authorized", True),
                ("targeted_camera_pass", True)):
            changed = copy.deepcopy(value); changed[field] = bad
            mutations.append(changed)
        for changed in mutations:
            with self.assertRaisesRegex(RuntimeError, "pre-process rejection"):
                capture.validate_formal_targeted_authorization(
                    changed, "a" * 64, output)

    def test_wrong_subset_rejects_before_output_or_capture_process(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "capture"
            with mock.patch.object(capture, "capture_scene") as capture_scene:
                with self.assertRaisesRegex(RuntimeError, "scope must be exact"):
                    capture.capture_all(
                        root / "Billiards", output,
                        formal_asset_package=root,
                        authorization=self.authorization(output),
                        readiness_manifest_sha256="a" * 64,
                        formal_targeted_scene_ids=("low_aim",))
            capture_scene.assert_not_called()
            self.assertFalse(output.exists())

    def test_exact_targeted_scope_dispatches_only_two_in_order(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            executable = root / "Billiards"; executable.write_bytes(b"test")
            output = root / "capture"
            authorization = self.authorization(output)
            authorization_path = root / "targeted-authorization.json"
            authorization_path.write_text(
                json.dumps(authorization), encoding="utf-8")
            counter = {"value": 0}

            def fake_materialize(_package, destination):
                destination.mkdir(parents=True)

            def fake_capture(_executable, _output, scene, *_args, **_kwargs):
                counter["value"] += 1
                return {
                    "scene_id": scene.scene_id,
                    "performance": {
                        "warmup_frames": 120, "sample_frames": 300,
                        "p95_total_frame_ms": 1.0,
                        "draw_calls_complete_scene": 1,
                        "texture_resident_mib": 1,
                        "measurement_method": "gpu_timer_query_plus_cpu_frame",
                        "gpu_timer_queries_observed": True,
                    },
                }

            with mock.patch.object(capture, "materialize_capture_asset_root",
                                   side_effect=fake_materialize), \
                 mock.patch.object(capture, "capture_scene",
                                   side_effect=fake_capture), \
                 mock.patch.object(capture, "FORMAL_ASSET_FILES", {}):
                manifest = capture.capture_all(
                    executable, output, 1920, 1080,
                    formal_asset_package=root,
                    authorization=authorization,
                    readiness_manifest_sha256="a" * 64,
                    formal_targeted_scene_ids=("break", "low_aim"),
                    formal_authorization_path=authorization_path)
            self.assertEqual(counter["value"], 2)
            self.assertEqual([item["scene_id"] for item in manifest["scenes"]],
                             ["break", "low_aim"])
            self.assertEqual(
                manifest["schema"],
                "phase2_v10i_formal_targeted_camera_capture_v1")
            self.assertFalse(manifest["capture_scope"]["full_six_scene_evidence"])
            consumed = Path(manifest["targeted_authorization_consumption"][
                "canonical_path"])
            self.assertTrue(consumed.is_file())
            self.assertEqual(json.loads(consumed.read_text())["status"],
                             "CONSUMED_BEFORE_PROCESS_START")

    def test_bad_authorization_creates_neither_marker_output_nor_process(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "capture"
            value = self.authorization(output)
            value["authorized_capture_count"] = 6
            path = root / "authorization.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            marker = path.with_name(path.name + ".consumed.json")
            with mock.patch.object(capture, "capture_scene") as capture_scene:
                with self.assertRaisesRegex(RuntimeError, "count/scenes"):
                    capture.capture_all(
                        root / "Billiards", output,
                        formal_asset_package=root,
                        authorization=value,
                        readiness_manifest_sha256="a" * 64,
                        formal_targeted_scene_ids=("break", "low_aim"),
                        formal_authorization_path=path)
            capture_scene.assert_not_called()
            self.assertFalse(marker.exists())
            self.assertFalse(output.exists())

    def test_consumed_targeted_authorization_rejects_reuse(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "capture"
            value = self.authorization(output)
            path = root / "authorization.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            capture.consume_formal_targeted_authorization(
                path, value, "a" * 64, output)
            with self.assertRaisesRegex(RuntimeError, "already consumed"):
                capture.consume_formal_targeted_authorization(
                    path, value, "a" * 64, output)

    def test_fsync_failure_permanently_consumes_without_output_or_process(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "capture"
            value = self.authorization(output)
            path = root / "authorization.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            marker = path.with_name(path.name + ".consumed.json")
            with mock.patch.object(capture.os, "fsync",
                                   side_effect=OSError("forced fsync failure")), \
                 mock.patch.object(capture, "capture_scene") as capture_scene:
                with self.assertRaisesRegex(OSError, "forced fsync failure"):
                    capture.capture_all(
                        root / "Billiards", output,
                        formal_asset_package=root,
                        authorization=value,
                        readiness_manifest_sha256="a" * 64,
                        formal_targeted_scene_ids=("break", "low_aim"),
                        formal_authorization_path=path)
            capture_scene.assert_not_called()
            self.assertTrue(marker.exists())
            self.assertFalse(output.exists())
            with self.assertRaisesRegex(RuntimeError, "already consumed"):
                capture.consume_formal_targeted_authorization(
                    path, value, "a" * 64, output)

    def test_existing_full_six_authorization_contract_is_preserved(self) -> None:
        output = Path("/tmp/phase2-full-six-auth-test")
        value = {
            "schema": "phase2_v10i_formal_six_scene_one_shot_authorization_v3",
            "status": "AUTHORIZED_FOR_EXACTLY_SIX_FRESH_PROCESS_CAPTURES",
            "authorization_id": "full-six-test",
            "authorized_by": "test",
            "authorized_at_utc": "2026-08-11T00:00:00Z",
            "authorized_output_canonical_path": str(output.resolve()),
            "gpu_capture_authorized": True,
            "formal_six_scene_evidence_authorized": True,
            "authorized_capture_count": 6,
            "consumed_capture_count": 0,
            "scene_ids_in_order": list(capture.FORMAL_SCENE_IDS),
            "required_readiness_manifest_sha256": "a" * 64,
            "formal_pass": False,
            "golden_promotion": False,
            "m2_promotion": False,
        }
        capture.validate_formal_authorization(value, "a" * 64, output)

    def test_targeted_output_validator_rejects_wrong_schema_and_scope(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            executable = root / "Billiards"; executable.write_bytes(b"test")
            manifest_path = root / "manifest.json"
            base = {
                "schema": "wrong", "registry_version": 2,
                "renderer": "modern", "production_gltf": True,
                "quality": "medium",
                "resolution": {"width": 1920, "height": 1080},
            }
            manifest_path.write_text(json.dumps(base), encoding="utf-8")
            with self.assertRaisesRegex(
                    targeted_validator.TargetedCaptureError, "identity"):
                targeted_validator.validate(
                    manifest_path, executable, "a" * 64)
            base["schema"] = "phase2_v10i_formal_targeted_camera_capture_v1"
            base["capture_scope"] = {
                "kind": "targeted_camera_break_low_aim",
                "scene_ids_in_order": ["low_aim", "break"],
                "capture_count": 2,
                "full_six_scene_evidence": False,
            }
            manifest_path.write_text(json.dumps(base), encoding="utf-8")
            with self.assertRaisesRegex(
                    targeted_validator.TargetedCaptureError, "scope"):
                targeted_validator.validate(
                    manifest_path, executable, "a" * 64)

    def test_targeted_output_validator_is_read_only_and_gpu_free(self) -> None:
        source = Path(targeted_validator.__file__).read_text(encoding="utf-8")
        for forbidden in ("import subprocess", "AutomationClient", "Popen(",
                          "Cocoa", "OpenGL", "screenshot(", "write_text(",
                          "write_bytes("):
            self.assertNotIn(forbidden, source)


if __name__ == "__main__":
    unittest.main()
