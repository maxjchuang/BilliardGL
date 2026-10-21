from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from scripts import capture_visual_baselines as capture_driver
from scripts import run_phase2_v10i_formal_full_six_capture_v8 as wrapper
from tests.visual import test_phase2_v10i_formal_full_six_wrapper_v7 as fixtures


class Phase2FullSixWrapperV8Tests(unittest.TestCase):
    @staticmethod
    def fixture(root: Path):
        helper = object.__new__(fixtures.Phase2FullSixWrapperV7Tests)
        result = helper.fixture(root)
        ready, auth, _, _, _, authorization = result
        readiness = json.loads(ready.read_text())
        readiness["schema"] = wrapper.READY_SCHEMA
        ready.write_text(json.dumps(readiness))
        authorization["required_readiness_manifest_sha256"] = wrapper.sha256(ready)
        auth.write_text(json.dumps(authorization))
        return result

    def test_real_capture_command_is_accepted_by_live_parser(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            ready, auth, output, exe, package, _ = self.fixture(Path(directory))
            command = wrapper.capture_command(exe, output, package, auth,
                                              wrapper.sha256(ready))
            self.assertEqual(command.count("--asset-package"), 1)
            self.assertNotIn("--formal-asset-package", command)
            with mock.patch("sys.argv", command[1:]):
                parsed = capture_driver._parse_arguments()
            self.assertTrue(parsed.formal_v10i)
            self.assertEqual(parsed.asset_package.resolve(), package.resolve())
            self.assertEqual(parsed.executable.resolve(), exe.resolve())
            self.assertEqual(parsed.output.resolve(), output.resolve())
            self.assertEqual(parsed.authorization.resolve(), auth.resolve())
            self.assertEqual(parsed.readiness_manifest_sha256,
                             wrapper.sha256(ready))
            self.assertFalse(output.exists())
            self.assertFalse(wrapper.marker_path(auth).exists())

    def test_old_flag_is_rejected_by_live_parser_without_side_effects(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            ready, auth, output, exe, package, _ = self.fixture(Path(directory))
            command = wrapper.capture_command(exe, output, package, auth,
                                              wrapper.sha256(ready))
            command[command.index("--asset-package")] = "--formal-asset-package"
            with mock.patch("sys.argv", command[1:]), \
                    self.assertRaises(SystemExit) as failure, \
                    mock.patch.object(wrapper.subprocess, "run") as process:
                capture_driver._parse_arguments()
            self.assertEqual(failure.exception.code, 2)
            process.assert_not_called()
            self.assertFalse(output.exists())
            self.assertFalse(wrapper.marker_path(auth).exists())

    def test_v8_authorization_validates_without_side_effects(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            args = self.fixture(Path(directory))
            wrapper.validate_pre_process(*args[:5])
            self.assertFalse(args[2].exists())
            self.assertFalse(wrapper.marker_path(args[1]).exists())

    def test_bad_scope_rejected_before_marker_output_process(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            ready, auth, output, exe, package, value = self.fixture(Path(directory))
            changed = copy.deepcopy(value)
            changed["authorized_capture_count"] = 2
            auth.write_text(json.dumps(changed))
            with mock.patch.object(wrapper.subprocess, "run") as process:
                with self.assertRaises(wrapper.DispatchError):
                    wrapper.validate_pre_process(ready, auth, output, exe, package)
                process.assert_not_called()
            self.assertFalse(output.exists())
            self.assertFalse(wrapper.marker_path(auth).exists())

    def test_existing_marker_is_permanent_preprocess_rejection(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            ready, auth, output, exe, package, value = self.fixture(Path(directory))
            wrapper.consume_authorization(auth, value, ready, output)
            with mock.patch.object(wrapper.subprocess, "run") as process:
                with self.assertRaises(wrapper.DispatchError):
                    wrapper.validate_pre_process(ready, auth, output, exe, package)
                process.assert_not_called()
            self.assertFalse(output.exists())

    def test_fsync_failure_leaves_permanent_marker_no_process(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            ready, auth, output, _, _, value = self.fixture(Path(directory))
            with mock.patch.object(wrapper.os, "fsync", side_effect=OSError("forced")), \
                    mock.patch.object(wrapper.subprocess, "run") as process:
                with self.assertRaises(OSError):
                    wrapper.consume_authorization(auth, value, ready, output)
                process.assert_not_called()
            self.assertTrue(wrapper.marker_path(auth).exists())
            self.assertFalse(output.exists())

    def test_no_execute_flag_never_validates_or_starts_process(self) -> None:
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
