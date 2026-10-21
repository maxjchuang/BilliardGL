#!/usr/bin/env python3
"""CPU-only tests for the final-ownership capture dispatcher boundary."""

from __future__ import annotations

import argparse
import copy
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
DISPATCH_PATH = ROOT / "scripts/run_phase4_final_ownership_targeted_capture.py"
VALIDATOR_PATH = ROOT / "scripts/validate_phase4_final_ownership_capture_dispatch.py"


def load_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


dispatch_module = load_module("phase4_dispatch", DISPATCH_PATH)
result_validator = load_module("phase4_dispatch_validator", VALIDATOR_PATH)


class CaptureDispatchTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        value = os.environ.get("PHASE4_READINESS_MANIFEST")
        if not value:
            raise unittest.SkipTest("PHASE4_READINESS_MANIFEST is required")
        cls.ready_path = Path(value).resolve()
        cls.ready = json.loads(cls.ready_path.read_text())

    def make_ready(self, root: Path) -> tuple[Path, Path, dict]:
        output = root / "output"
        output.mkdir()
        ready = copy.deepcopy(self.ready)
        ready["targeted_output_root"] = str(output.resolve())
        ready["results_path"] = str(output.resolve() / "results.jsonl")
        path = root / "readiness.json"
        path.write_text(json.dumps(ready, indent=2, sort_keys=True) + "\n")
        return path, output, ready

    def make_auth(self, root: Path, ready_path: Path, ready: dict,
                  output: Path, **updates) -> Path:
        value = dispatch_module.expected_authorization(
            ready_path, ready, output)
        value["authorization_nonce"] = "cpu-only-test-nonce-0123456789"
        value.update(updates)
        path = root / "authorization.json"
        path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")
        return path

    @staticmethod
    def args(ready: Path, output: Path, auth: Path | None = None,
             run: bool = False) -> argparse.Namespace:
        return argparse.Namespace(
            ready=ready, output_root=output, authorization_receipt=auth,
            warmup=120, samples=300, run=run)

    @staticmethod
    def consume(auth: Path) -> None:
        value = json.loads(auth.read_text())
        Path(str(auth) + ".consumed").write_text(
            value["authorization_nonce"] + "\n")

    @staticmethod
    def write_success(output: Path, ready: dict) -> None:
        result_rows = [{
            "capture_id": row["capture_id"],
            "machine_status": "V4_PASS_PREFLIGHT_ONLY_PENDING_TA",
            "authorization_receipt_consumed": True,
            "gpu_capture_authorized_for_this_invocation": True,
        } for row in ready["captures"]]
        result_rows.append({
            "record_type": "summary", "capture_count": 11,
            "machine_status": "V4_PASS_PREFLIGHT_ONLY_PENDING_TA",
            "ta_status": "NOT_RUN", "manual_status": "NOT_RUN",
            "m3_status": "NOT_CLAIMED",
        })
        (output / "results.jsonl").write_text("".join(
            json.dumps(row) + "\n" for row in result_rows))
        telemetry = [{
            "capture_id": row["capture_id"], "machine_status": "PASS",
            "integrated_final_ownership": True,
        } for row in ready["captures"]]
        (output / "results.jsonl.final-ownership.jsonl").write_text("".join(
            json.dumps(row) + "\n" for row in telemetry))

    def test_false_authorization_rejected_before_process_and_output(self) -> None:
        output = Path(self.ready["targeted_output_root"])
        self.assertTrue(output.is_dir())
        self.assertEqual(list(output.iterdir()), [])
        with tempfile.TemporaryDirectory(prefix="phase4-false-auth-") as temp:
            root = Path(temp)
            auth = self.make_auth(root, self.ready_path, self.ready, output,
                                  authorized=False)
            with mock.patch.object(dispatch_module.subprocess, "run") as runner:
                with self.assertRaisesRegex(RuntimeError,
                                            "authorization binding drift:authorized"):
                    dispatch_module.dispatch(self.args(
                        self.ready_path, output, auth, True))
                runner.assert_not_called()
            self.assertEqual(list(output.iterdir()), [])
            self.assertFalse(Path(str(auth) + ".consumed").exists())

    def test_dry_run_validates_without_authorization_or_process(self) -> None:
        output = Path(self.ready["targeted_output_root"])
        with mock.patch.object(dispatch_module.subprocess, "run") as runner:
            self.assertEqual(dispatch_module.dispatch(
                self.args(self.ready_path, output)), 0)
            runner.assert_not_called()
        self.assertEqual(list(output.iterdir()), [])

    def test_exact_auth_extra_field_rejected_before_process(self) -> None:
        with tempfile.TemporaryDirectory(prefix="phase4-extra-auth-") as temp:
            root = Path(temp)
            ready_path, output, ready = self.make_ready(root)
            auth = self.make_auth(root, ready_path, ready, output,
                                  unexpected=True)
            with mock.patch.object(dispatch_module.subprocess, "run") as runner:
                with self.assertRaisesRegex(RuntimeError,
                                            "fields must match exact schema"):
                    dispatch_module.dispatch(self.args(
                        ready_path, output, auth, True))
                runner.assert_not_called()
            self.assertEqual(list(output.iterdir()), [])

    def test_single_use_consumed_marker_rejected_before_process(self) -> None:
        with tempfile.TemporaryDirectory(prefix="phase4-consumed-auth-") as temp:
            root = Path(temp)
            ready_path, output, ready = self.make_ready(root)
            auth = self.make_auth(root, ready_path, ready, output)
            self.consume(auth)
            with mock.patch.object(dispatch_module.subprocess, "run") as runner:
                with self.assertRaisesRegex(RuntimeError, "already consumed"):
                    dispatch_module.dispatch(self.args(
                        ready_path, output, auth, True))
                runner.assert_not_called()
            self.assertEqual(list(output.iterdir()), [])

    def test_mock_pass_writes_exact_plan_and_valid_pass_receipt(self) -> None:
        with tempfile.TemporaryDirectory(prefix="phase4-dispatch-pass-") as temp:
            root = Path(temp)
            ready_path, output, ready = self.make_ready(root)
            auth = self.make_auth(root, ready_path, ready, output)

            def fake_run(*_args, **_kwargs):
                self.consume(auth)
                self.write_success(output, ready)
                return SimpleNamespace(returncode=0)

            with mock.patch.object(dispatch_module.subprocess, "run",
                                   side_effect=fake_run):
                self.assertEqual(dispatch_module.dispatch(self.args(
                    ready_path, output, auth, True)), 0)
            plan = output / result_validator.PLAN_NAME
            self.assertEqual(plan.read_bytes(), Path(ready["plan_path"]).read_bytes())
            receipt_path = output / dispatch_module.PASS_RECEIPT_NAME
            receipt = json.loads(receipt_path.read_text())
            analysis, errors = result_validator.analyze_output(ready, output, 0)
            self.assertEqual(errors, [])
            self.assertEqual(result_validator.validate_receipt(
                receipt, ready, output, 0, analysis), [])

    def test_mock_ordinal_failure_preserves_rows_and_counts(self) -> None:
        with tempfile.TemporaryDirectory(prefix="phase4-dispatch-fail-") as temp:
            root = Path(temp)
            ready_path, output, ready = self.make_ready(root)
            auth = self.make_auth(root, ready_path, ready, output)
            completed_row = {
                "capture_id": ready["captures"][0]["capture_id"],
                "machine_status": "V4_PASS_PREFLIGHT_ONLY_PENDING_TA",
                "authorization_receipt_consumed": True,
                "gpu_capture_authorized_for_this_invocation": True,
            }
            telemetry = [
                {"capture_id": ready["captures"][0]["capture_id"],
                 "machine_status": "PASS", "integrated_final_ownership": True},
                {"capture_id": ready["captures"][1]["capture_id"],
                 "machine_status": "FAILED", "integrated_final_ownership": True},
            ]

            def fake_run(*_args, **_kwargs):
                self.consume(auth)
                (output / "results.jsonl").write_text(json.dumps(completed_row) + "\n")
                (output / "results.jsonl.final-ownership.jsonl").write_text(
                    "".join(json.dumps(row) + "\n" for row in telemetry))
                return SimpleNamespace(returncode=17)

            with mock.patch.object(dispatch_module.subprocess, "run",
                                   side_effect=fake_run):
                self.assertEqual(dispatch_module.dispatch(self.args(
                    ready_path, output, auth, True)), 17)
            self.assertEqual(json.loads((output / "results.jsonl").read_text()),
                             completed_row)
            receipt_path = output / dispatch_module.FAILURE_RECEIPT_NAME
            receipt = json.loads(receipt_path.read_text())
            self.assertEqual((receipt["completed_count"], receipt["failed_count"],
                              receipt["unrun_count"]), (1, 1, 9))
            analysis, errors = result_validator.analyze_output(ready, output, 17)
            self.assertEqual(errors, [])
            self.assertEqual(result_validator.validate_receipt(
                receipt, ready, output, 17, analysis), [])

    def test_result_validator_rejects_missing_pass_summary(self) -> None:
        with tempfile.TemporaryDirectory(prefix="phase4-dispatch-invalid-") as temp:
            root = Path(temp)
            _, output, ready = self.make_ready(root)
            (output / result_validator.PLAN_NAME).write_bytes(
                Path(ready["plan_path"]).read_bytes())
            self.write_success(output, ready)
            rows = (output / "results.jsonl").read_text().splitlines()
            (output / "results.jsonl").write_text("\n".join(rows[:-1]) + "\n")
            _, errors = result_validator.analyze_output(ready, output, 0)
            self.assertIn("pass_requires_11_rows_11_telemetry_and_one_summary",
                          errors)


if __name__ == "__main__":
    unittest.main()
