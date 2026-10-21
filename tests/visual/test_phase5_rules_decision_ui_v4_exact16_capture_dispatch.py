#!/usr/bin/env python3
"""NO-GPU mock tests for the Phase5 v4 exact16 single-use dispatcher."""

from __future__ import annotations

import argparse
import copy
import importlib.util
import json
import os
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]


def load_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


dispatch = load_module("phase5_v4_exact16_dispatch",
                       ROOT / "scripts/run_phase5_rules_decision_ui_v4_exact16_capture_dispatch.py")
validator = load_module("phase5_v4_exact16_dispatch_validator",
                        ROOT / "scripts/validate_phase5_rules_decision_ui_v4_exact16_capture_dispatch.py")


class Exact16DispatchTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        path = os.environ.get("PHASE5_V4_EXACT16_READY")
        if not path:
            raise unittest.SkipTest("PHASE5_V4_EXACT16_READY required")
        cls.source_ready_path = Path(path).resolve()
        cls.source_ready = json.loads(cls.source_ready_path.read_text())

    def fixture(self, root: Path) -> tuple[Path, Path, Path, dict]:
        ready = copy.deepcopy(self.source_ready)
        ready["dispatch_gate"]["compatible_capture_producer_bound"] = True
        ready["dispatch_gate"]["authorization_may_not_be_issued"] = False
        ready_path = root / "ready.json"
        ready_path.write_text(json.dumps(ready, indent=2, sort_keys=True) + "\n")
        output = root / "capture-output"
        runner = Path("/usr/bin/true")
        return ready_path, output, runner, ready

    def auth(self, root: Path, ready_path: Path, ready: dict,
             output: Path, runner: Path, **updates) -> Path:
        value = dispatch.expected_auth(ready_path, ready, output, runner)
        value.update({"authorization_id": "test-auth-12345678",
                      "authorization_nonce": "test-nonce-12345678",
                      "approved_by": "offline-mock-authority",
                      "approved_at": "2099-01-01T00:00:00Z",
                      "expires_unix_seconds": 4070908800})
        value.update(updates)
        path = root / "auth.json"
        path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")
        return path

    @staticmethod
    def args(ready: Path, output: Path | None = None,
             runner: Path | None = None, auth: Path | None = None,
             execute: bool = False) -> argparse.Namespace:
        return argparse.Namespace(ready=ready, output_root=output,
                                  runner_binary=runner, authorization=auth,
                                  execute_authorized_capture=execute)

    @staticmethod
    def write_success(output: Path, ready: dict) -> None:
        (output / "ppm").mkdir(parents=True)
        results = []
        telemetry = []
        for item in ready["case_ids_in_order"]:
            ppm = output / "ppm" / (item + ".ppm")
            ppm.write_bytes(b"P6\n1 1\n255\n\x00\x00\x00")
            results.append({
                "capture_id": item, "actual_gpu_capture": True,
                "rules_ui_after_tone_map": True,
                "render_passes": ["product_ui", "product_present"],
                "authorization_consumed_before_process": True,
                "ppm_path": str(ppm), "ppm_sha256": dispatch.sha(ppm),
                "physical_width": 1, "physical_height": 1,
            })
            telemetry.append({"capture_id": item, "machine_status": "PASS",
                              "actual_gpu_capture": True,
                              "rules_ui_after_tone_map": True})
        (output / validator.RESULTS_NAME).write_text("".join(
            json.dumps(row) + "\n" for row in results))
        (output / validator.TELEMETRY_NAME).write_text("".join(
            json.dumps(row) + "\n" for row in telemetry))

    def test_dry_run_is_no_process_no_output_no_marker(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            ready_path, output, _, _ = self.fixture(Path(temp))
            with mock.patch.object(dispatch.subprocess, "run") as process:
                self.assertEqual(dispatch.dispatch(self.args(ready_path)), 0)
                process.assert_not_called()
            self.assertFalse(output.exists())

    def test_no_execute_rejects_authorization_arguments(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            ready_path, output, runner, ready = self.fixture(root)
            auth = self.auth(root, ready_path, ready, output, runner)
            with mock.patch.object(dispatch.subprocess, "run") as process:
                with self.assertRaisesRegex(RuntimeError, "accepted only with"):
                    dispatch.dispatch(self.args(ready_path, output, runner, auth))
                process.assert_not_called()
            self.assertFalse(Path(str(auth) + ".consumed.json").exists())
            self.assertFalse(output.exists())

    def test_real_readiness_blocks_unbound_producer_before_auth(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            output = root / "output"
            ready_path = self.source_ready_path
            if self.source_ready.get("dispatch_gate", {}).get(
                    "compatible_capture_producer_bound") is True:
                blocked = copy.deepcopy(self.source_ready)
                blocked["dispatch_gate"]["compatible_capture_producer_bound"] = False
                blocked["dispatch_gate"]["authorization_may_not_be_issued"] = True
                ready_path = root / "producer-blocked-ready.json"
                ready_path.write_text(json.dumps(blocked, indent=2, sort_keys=True) + "\n")
            with mock.patch.object(dispatch.subprocess, "run") as process:
                with self.assertRaisesRegex(RuntimeError, "does not bind"):
                    dispatch.dispatch(self.args(ready_path, output,
                                                Path("/usr/bin/true"),
                                                Path("/usr/bin/false"), True))
                process.assert_not_called()
            self.assertFalse(output.exists())

    def test_bad_scope_rejected_before_marker_output_process(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            ready_path, output, runner, ready = self.fixture(root)
            auth = self.auth(root, ready_path, ready, output, runner,
                             case_count=15)
            with mock.patch.object(dispatch.subprocess, "run") as process:
                with self.assertRaisesRegex(RuntimeError, "case_count"):
                    dispatch.dispatch(self.args(ready_path, output, runner, auth, True))
                process.assert_not_called()
            self.assertFalse(Path(str(auth) + ".consumed.json").exists())
            self.assertFalse(output.exists())

    def test_all_runtime_bindings_fail_closed_before_consumption(self) -> None:
        fields = ("ready_manifest_sha256", "dispatcher_sha256",
                  "runner_binary_sha256", "runner_source_sha256",
                  "plan_sha256", "source_binding_manifest_sha256",
                  "source_aggregate_sha256", "billiards_binary_sha256",
                  "font_sha256", "hud_token_sha256", "shell_token_sha256",
                  "authorized_output_canonical_path")
        for field in fields:
            with self.subTest(field=field), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                ready_path, output, runner, ready = self.fixture(root)
                auth = self.auth(root, ready_path, ready, output, runner,
                                 **{field: "0" * 64})
                with mock.patch.object(dispatch.subprocess, "run") as process:
                    with self.assertRaisesRegex(RuntimeError, field):
                        dispatch.dispatch(self.args(
                            ready_path, output, runner, auth, True))
                    process.assert_not_called()
                self.assertFalse(Path(str(auth) + ".consumed.json").exists())
                self.assertFalse(output.exists())

    def test_extra_auth_field_rejected_before_consumption(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            ready_path, output, runner, ready = self.fixture(root)
            auth = self.auth(root, ready_path, ready, output, runner,
                             unexpected_field=True)
            with mock.patch.object(dispatch.subprocess, "run") as process:
                with self.assertRaisesRegex(RuntimeError, "exact fields"):
                    dispatch.dispatch(self.args(ready_path, output, runner, auth, True))
                process.assert_not_called()
            self.assertFalse(Path(str(auth) + ".consumed.json").exists())
            self.assertFalse(output.exists())

    def test_existing_marker_rejected_before_process_output(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            ready_path, output, runner, ready = self.fixture(root)
            auth = self.auth(root, ready_path, ready, output, runner)
            marker = Path(str(auth) + ".consumed.json")
            marker.write_text("incomplete-but-permanently-consumed")
            with mock.patch.object(dispatch.subprocess, "run") as process:
                with self.assertRaisesRegex(RuntimeError, "already consumed"):
                    dispatch.dispatch(self.args(ready_path, output, runner, auth, True))
                process.assert_not_called()
            self.assertFalse(output.exists())

    def test_fsync_failure_is_consumed_and_preprocess(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            ready_path, output, runner, ready = self.fixture(root)
            auth = self.auth(root, ready_path, ready, output, runner)
            marker = Path(str(auth) + ".consumed.json")
            with mock.patch.object(dispatch.os, "fsync", side_effect=OSError("forced")), \
                    mock.patch.object(dispatch.subprocess, "run") as process:
                with self.assertRaises(OSError):
                    dispatch.dispatch(self.args(ready_path, output, runner, auth, True))
                process.assert_not_called()
            self.assertTrue(marker.exists())
            self.assertFalse(output.exists())

    def test_mock_success_validates_exact16_and_receipt(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            ready_path, output, runner, ready = self.fixture(root)
            auth = self.auth(root, ready_path, ready, output, runner)
            marker = Path(str(auth) + ".consumed.json")

            def fake_process(*_args, **_kwargs):
                self.assertTrue(marker.is_file())
                self.assertFalse(output.exists())
                self.write_success(output, ready)
                return SimpleNamespace(returncode=0)

            with mock.patch.object(dispatch.subprocess, "run", side_effect=fake_process):
                self.assertEqual(dispatch.dispatch(
                    self.args(ready_path, output, runner, auth, True)), 0)
            receipt = json.loads((output / validator.PASS_RECEIPT_NAME).read_text())
            analysis, errors = validator.analyze_output(ready, output, 0)
            self.assertEqual(errors, [])
            self.assertEqual(validator.validate_receipt(
                receipt, ready, output, 0, analysis), [])

    def test_mock_failure_preserves_prefix_and_permanent_marker(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            ready_path, output, runner, ready = self.fixture(root)
            auth = self.auth(root, ready_path, ready, output, runner)

            def fake_process(*_args, **_kwargs):
                self.write_success(output, ready)
                result_path = output / validator.RESULTS_NAME
                result_path.write_text("\n".join(result_path.read_text().splitlines()[:2]) + "\n")
                telemetry_path = output / validator.TELEMETRY_NAME
                rows = [json.loads(line) for line in telemetry_path.read_text().splitlines()[:3]]
                rows[-1]["machine_status"] = "FAILED"
                telemetry_path.write_text("".join(json.dumps(row) + "\n" for row in rows))
                return SimpleNamespace(returncode=17)

            with mock.patch.object(dispatch.subprocess, "run", side_effect=fake_process):
                self.assertEqual(dispatch.dispatch(
                    self.args(ready_path, output, runner, auth, True)), 17)
            receipt = json.loads((output / validator.FAILURE_RECEIPT_NAME).read_text())
            self.assertEqual((receipt["completed_count"], receipt["failed_count"],
                              receipt["unrun_count"]), (2, 1, 13))
            self.assertTrue(Path(str(auth) + ".consumed.json").exists())
            self.assertTrue((output / "ppm" /
                             (ready["case_ids_in_order"][0] + ".ppm")).exists())


if __name__ == "__main__":
    unittest.main()
