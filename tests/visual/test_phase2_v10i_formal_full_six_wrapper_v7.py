from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from scripts import run_phase2_v10i_formal_full_six_capture_v7 as wrapper
from tests.visual import test_phase2_v10i_formal_full_six_wrapper_v6 as fixtures


class Phase2FullSixWrapperV7Tests(unittest.TestCase):
    @staticmethod
    def fixture(root: Path):
        helper = object.__new__(fixtures.Phase2FullSixWrapperV6Tests)
        result = helper.fixture(root)
        ready, auth, _, _, _, authorization = result
        readiness = json.loads(ready.read_text())
        readiness["schema"] = wrapper.READY_SCHEMA
        ready.write_text(json.dumps(readiness))
        authorization["required_readiness_manifest_sha256"] = wrapper.sha256(ready)
        auth.write_text(json.dumps(authorization))
        return result

    def test_v7_exact_authorization_validates_without_side_effects(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            args = self.fixture(Path(directory))
            wrapper.validate_pre_process(*args[:5])
            self.assertFalse(args[2].exists())
            self.assertFalse(wrapper.marker_path(args[1]).exists())

    def test_bad_scope_fails_before_marker_output_or_process(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            ready, auth, output, exe, package, value = self.fixture(Path(directory))
            changed = copy.deepcopy(value)
            changed["scene_ids_in_order"] = list(reversed(wrapper.SCENE_IDS))
            auth.write_text(json.dumps(changed))
            with mock.patch.object(wrapper.subprocess, "run") as process:
                with self.assertRaises(wrapper.DispatchError):
                    wrapper.validate_pre_process(ready, auth, output, exe, package)
                process.assert_not_called()
            self.assertFalse(output.exists())
            self.assertFalse(wrapper.marker_path(auth).exists())

    def test_source_drift_fails_before_consumption(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            ready, auth, output, exe, package, _ = self.fixture(Path(directory))
            exe.write_bytes(b"drift")
            with self.assertRaises(wrapper.DispatchError):
                wrapper.validate_pre_process(ready, auth, output, exe, package)
            self.assertFalse(output.exists())
            self.assertFalse(wrapper.marker_path(auth).exists())

    def test_o_excl_marker_precedes_output_and_is_single_use(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            ready, auth, output, exe, package, value = self.fixture(Path(directory))
            marker = wrapper.marker_path(auth)
            payload = wrapper.consume_authorization(auth, value, ready, output)
            self.assertTrue(marker.is_file())
            self.assertEqual(payload["consumed_capture_count"], 6)
            self.assertFalse(output.exists())
            with self.assertRaises(wrapper.DispatchError):
                wrapper.validate_pre_process(ready, auth, output, exe, package)

    def test_fsync_failure_is_permanently_consumed_preprocess(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            ready, auth, output, _, _, value = self.fixture(Path(directory))
            with mock.patch.object(wrapper.os, "fsync", side_effect=OSError("forced")), \
                    mock.patch.object(wrapper.subprocess, "run") as process:
                with self.assertRaises(OSError):
                    wrapper.consume_authorization(auth, value, ready, output)
                process.assert_not_called()
            self.assertTrue(wrapper.marker_path(auth).exists())
            self.assertFalse(output.exists())

    def test_command_is_full_six_not_targeted_and_frozen_quality(self) -> None:
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

    def test_missing_execute_flag_never_validates_or_starts_process(self) -> None:
        argv = ["wrapper", "--readiness", "/missing/r", "--authorization",
                "/missing/a", "--output", "/missing/o", "--executable",
                "/missing/e", "--asset-package", "/missing/p"]
        with mock.patch("sys.argv", argv), \
                mock.patch.object(wrapper._impl, "validate_pre_process") as validate, \
                mock.patch.object(wrapper.subprocess, "run") as process:
            self.assertEqual(wrapper.main(), 2)
            validate.assert_not_called()
            process.assert_not_called()


if __name__ == "__main__":
    unittest.main()
