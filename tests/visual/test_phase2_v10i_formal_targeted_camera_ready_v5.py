from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from scripts import capture_visual_baselines as capture
from scripts import prepare_phase2_v10i_formal_targeted_camera_ready_v5 as ready
from scripts import validate_phase2_v10i_formal_targeted_camera_ready_v5 as validator


class Phase2FormalTargetedCameraReadinessV5Tests(unittest.TestCase):
    def test_frozen_v4_gate_and_failure_lineage_are_exact(self) -> None:
        self.assertEqual(ready.v2.sha256(ready.V4_READINESS),
                         ready.V4_READINESS_SHA256)
        self.assertEqual(ready.v2.sha256(ready.CAMERA_GATE),
                         ready.CAMERA_GATE_SHA256)
        self.assertEqual(ready.v2.sha256(ready.FAILURE_RECEIPT),
                         ready.FAILURE_RECEIPT_SHA256)

    def test_generated_v5_validates_with_exact_two_not_run(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            executable = root / "Billiards"
            executable.write_bytes(b"integrated-release-python-only-delta")
            with mock.patch.object(ready, "DEFAULT_EXECUTABLE",
                                   executable.resolve()):
                files = ready.prepare(
                    root / "ready", root / "package", executable)
                result = validator.validate(root / "ready")
            self.assertEqual(result["status"],
                             "PASS_CPU_ONLY_TARGETED_READINESS_V5")
            plan = json.loads(files["targeted_plan"].read_text())
            self.assertEqual([item["scene_id"] for item in plan["rows"]],
                             ["break", "low_aim"])
            self.assertEqual([item["status"] for item in plan["rows"]],
                             ["NOT_RUN", "NOT_RUN"])
            authorization = json.loads(
                files["authorization_template"].read_text())
            self.assertEqual(authorization, ready.authorization_template())
            self.assertEqual(authorization["authorized_capture_count"], 0)
            self.assertIsNone(authorization["authorization_nonce"])

    def test_plan_or_authorization_mutation_rejects(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            executable = root / "Billiards"; executable.write_bytes(b"test")
            with mock.patch.object(ready, "DEFAULT_EXECUTABLE",
                                   executable.resolve()):
                files = ready.prepare(
                    root / "ready", root / "package", executable)
                plan = json.loads(files["targeted_plan"].read_text())
                plan["rows"].append({"scene_id": "ball_closeup",
                                     "status": "NOT_RUN"})
                files["targeted_plan"].write_text(json.dumps(plan))
                with self.assertRaises(validator.base.ValidationError):
                    validator.validate(root / "ready")

    def test_capture_protocol_uses_atomic_durable_consumption_before_output(self) -> None:
        source = Path(capture.__file__).read_text(encoding="utf-8")
        for token in ("os.O_CREAT", "os.O_EXCL", "os.O_WRONLY",
                      "destination.flush()", "os.fsync(destination.fileno())"):
            self.assertIn(token, source)
        consume_call = source.index("consume_formal_targeted_authorization(",
                                    source.index("def capture_all("))
        output_create = source.index("output.mkdir(parents=True)", consume_call)
        self.assertLess(consume_call, output_create)

    def test_preparer_and_validator_are_gpu_free(self) -> None:
        for module in (ready, validator):
            source = Path(module.__file__).read_text(encoding="utf-8")
            for forbidden in ("import subprocess", "Popen(",
                              "AutomationClient", "Cocoa", "OpenGL",
                              "screenshot("):
                self.assertNotIn(forbidden, source)


if __name__ == "__main__":
    unittest.main()
