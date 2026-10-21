from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from scripts import run_phase2_v10i_formal_full_six_capture_v6 as wrapper


class Phase2FullSixWrapperV6Tests(unittest.TestCase):
    def fixture(self, root: Path) -> tuple[Path, Path, Path, Path, Path, dict]:
        executable = root / "Billiards"
        executable.write_bytes(b"fresh-release")
        package = root / "formal-package"
        package.mkdir()
        files = []
        for index in range(24):
            item_path = package / ("asset-%02d.bin" % index)
            item_path.write_bytes(("asset-%02d" % index).encode())
            digest = wrapper.sha256(item_path)
            files.append({"filename": item_path.name,
                          "source_sha256": digest,
                          "packaged_sha256": digest})
        package_manifest = package / "formal-asset-package-v2.json"
        package_manifest.write_text(json.dumps({
            "schema": "phase2_v10i_self_contained_formal_asset_package_v2",
            "self_contained": True, "immutable": True,
            "symlinks_allowed": False, "required_asset_count": 6,
            "runtime_dependency_count": 18, "files": files,
        }), encoding="utf-8")
        output = root / "capture-output"
        readiness_path = root / "readiness.json"
        live_binding = {
            "canonical_path": str(executable.resolve()),
            "sha256": wrapper.sha256(executable),
        }
        readiness = {
            "schema": wrapper.READY_SCHEMA,
            "status": wrapper.READY_STATUS,
            "implementation_ready": True,
            "actual_gpu_used": False,
            "gpu_capture_authorized": False,
            "capture_output_created": False,
            "formal_six_scene_evidence": False,
            "formal_pass": False,
            "ta_review_performed": False,
            "golden_promotion": False,
            "m2_promotion": False,
            "product_review_performed": False,
            "approved_scenes": list(wrapper.SCENE_IDS),
            "fresh_release_executable": {
                "canonical_path": str(executable.resolve()),
                "sha256": wrapper.sha256(executable),
            },
            "self_contained_asset_package": {
                "asset_root": str(package.resolve()),
                "sha256": wrapper.sha256(package_manifest),
            },
            "source_bindings": {"dummy_source": live_binding},
            "generated_bindings": {"dummy_generated": live_binding},
            "reentry_gate": live_binding,
            "targeted_machine_prerequisite": live_binding,
            "targeted_ta_prerequisite": live_binding,
            "full_six_ta_template": live_binding,
        }
        readiness_path.write_text(json.dumps(readiness), encoding="utf-8")
        authorization = {
            "schema": wrapper.AUTH_SCHEMA,
            "status": wrapper.AUTH_STATUS,
            "authorization_id": "full-six-test-id",
            "authorization_nonce": "full-six-test-nonce",
            "authorized_by": "unit-test",
            "authorized_at_utc": "2026-08-11T00:00:00Z",
            "authorized_output_canonical_path": str(output.resolve()),
            "gpu_capture_authorized": True,
            "formal_six_scene_evidence_authorized": True,
            "authorized_capture_count": 6,
            "consumed_capture_count": 0,
            "scene_ids_in_order": list(wrapper.SCENE_IDS),
            "required_readiness_manifest_sha256": wrapper.sha256(readiness_path),
            "formal_pass": False,
            "golden_promotion": False,
            "m2_promotion": False,
        }
        auth_path = root / "authorization.json"
        auth_path.write_text(json.dumps(authorization), encoding="utf-8")
        return readiness_path, auth_path, output, executable, package, authorization

    def test_exact_authorization_validates_without_side_effects(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            args = self.fixture(Path(directory))
            wrapper.validate_pre_process(*args[:5])
            self.assertFalse(args[2].exists())
            self.assertFalse(wrapper.marker_path(args[1]).exists())

    def test_mutated_authorizations_fail_before_marker_output_or_process(self) -> None:
        mutations = (
            ("schema", "wrong"), ("status", "NOT_ISSUED"),
            ("authorization_nonce", ""), ("authorized_capture_count", 2),
            ("scene_ids_in_order", list(reversed(wrapper.SCENE_IDS))),
            ("required_readiness_manifest_sha256", "0" * 64),
            ("formal_six_scene_evidence_authorized", False),
            ("formal_pass", True),
        )
        for field, bad in mutations:
            with self.subTest(field=field), tempfile.TemporaryDirectory() as directory:
                ready, auth, output, exe, package, value = self.fixture(Path(directory))
                changed = copy.deepcopy(value); changed[field] = bad
                auth.write_text(json.dumps(changed), encoding="utf-8")
                with mock.patch.object(wrapper.subprocess, "run") as process:
                    with self.assertRaisesRegex(wrapper.DispatchError,
                                                "pre-process rejection"):
                        wrapper.validate_pre_process(ready, auth, output, exe, package)
                process.assert_not_called()
                self.assertFalse(output.exists())
                self.assertFalse(wrapper.marker_path(auth).exists())

    def test_preexisting_output_fails_before_consumption(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            ready, auth, output, exe, package, _ = self.fixture(Path(directory))
            output.mkdir()
            with self.assertRaisesRegex(wrapper.DispatchError, "output must be absent"):
                wrapper.validate_pre_process(ready, auth, output, exe, package)
            self.assertFalse(wrapper.marker_path(auth).exists())

    def test_live_source_drift_fails_before_consumption_or_process(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            ready, auth, output, exe, package, _ = self.fixture(Path(directory))
            exe.write_bytes(b"drifted-after-readiness")
            with mock.patch.object(wrapper.subprocess, "run") as process:
                with self.assertRaisesRegex(wrapper.DispatchError, "SHA drift|binding mismatch"):
                    wrapper.validate_pre_process(ready, auth, output, exe, package)
            process.assert_not_called()
            self.assertFalse(wrapper.marker_path(auth).exists())
            self.assertFalse(output.exists())

    def test_atomic_marker_binds_identity_nonce_readiness_and_exact_six(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            ready, auth, output, exe, package, value = self.fixture(Path(directory))
            _, validated = wrapper.validate_pre_process(ready, auth, output, exe, package)
            payload = wrapper.consume_authorization(auth, validated, ready, output)
            marker = wrapper.marker_path(auth)
            self.assertTrue(marker.is_file())
            self.assertEqual(payload["authorization_id"], value["authorization_id"])
            self.assertEqual(payload["authorization_nonce"], value["authorization_nonce"])
            self.assertEqual(payload["scene_ids_in_order"], list(wrapper.SCENE_IDS))
            self.assertEqual(payload["consumed_capture_count"], 6)
            self.assertFalse(output.exists())

    def test_existing_marker_rejects_before_output_or_process(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            ready, auth, output, exe, package, value = self.fixture(Path(directory))
            wrapper.consume_authorization(auth, value, ready, output)
            with mock.patch.object(wrapper.subprocess, "run") as process:
                with self.assertRaisesRegex(wrapper.DispatchError, "already consumed"):
                    wrapper.validate_pre_process(ready, auth, output, exe, package)
            process.assert_not_called()
            self.assertFalse(output.exists())

    def test_fsync_failure_leaves_permanent_marker_and_no_output_or_process(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            ready, auth, output, exe, package, value = self.fixture(Path(directory))
            with mock.patch.object(wrapper.os, "fsync",
                                   side_effect=OSError("forced fsync failure")), \
                 mock.patch.object(wrapper.subprocess, "run") as process:
                with self.assertRaisesRegex(OSError, "forced fsync failure"):
                    wrapper.consume_authorization(auth, value, ready, output)
            process.assert_not_called()
            self.assertTrue(wrapper.marker_path(auth).exists())
            self.assertFalse(output.exists())
            with self.assertRaisesRegex(wrapper.DispatchError, "already consumed"):
                wrapper.validate_pre_process(ready, auth, output, exe, package)

    def test_capture_command_is_full_six_and_frozen_capture_contract(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            ready, auth, output, exe, package, _ = self.fixture(Path(directory))
            command = wrapper.capture_command(exe, output, package, auth,
                                              wrapper.sha256(ready))
            self.assertIn("--formal-v10i", command)
            self.assertNotIn("--formal-targeted-scenes", command)
            self.assertEqual(command[command.index("--width") + 1], "1920")
            self.assertEqual(command[command.index("--height") + 1], "1080")
            self.assertEqual(command[command.index("--warmup-frames") + 1], "120")
            self.assertEqual(command[command.index("--sample-frames") + 1], "300")

    def test_missing_explicit_execute_flag_never_validates_or_starts_process(self) -> None:
        argv = ["wrapper", "--readiness", "/missing/r", "--authorization",
                "/missing/a", "--output", "/missing/o", "--executable",
                "/missing/e", "--asset-package", "/missing/p"]
        with mock.patch("sys.argv", argv), \
             mock.patch.object(wrapper, "validate_pre_process") as validate, \
             mock.patch.object(wrapper.subprocess, "run") as process:
            self.assertEqual(wrapper.main(), 2)
        validate.assert_not_called()
        process.assert_not_called()


if __name__ == "__main__":
    unittest.main()
