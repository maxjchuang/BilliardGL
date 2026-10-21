from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from scripts import capture_visual_baselines as capture
from scripts import prepare_phase2_v10i_formal_six_scene_ready_v4 as ready
from scripts import validate_phase2_v10i_formal_six_scene_ready_v4 as validator
from tests.visual.visual_scene_registry import apply_scene


class FakeClient:
    def __init__(self) -> None:
        self.commands = []

    def resize(self, width, height):
        self.commands.append(("resize", {"width": width, "height": height}))

    def command(self, name, params, timeout=0):
        self.commands.append((name, params))

    def state(self):
        return {"tick": 0}


class Phase2V10iFormalSixSceneReadinessV4Tests(unittest.TestCase):
    def test_final_lineage_hashes_are_exact(self) -> None:
        self.assertEqual(ready.v2.sha256(ready.V3_READINESS),
                         ready.V3_READINESS_SHA256)
        self.assertEqual(ready.v2.sha256(ready.FAILURE_RECEIPT),
                         ready.FAILURE_RECEIPT_SHA256)
        self.assertEqual(ready.v2.sha256(ready.CONSUMED_MARKER),
                         ready.CONSUMED_MARKER_SHA256)
        self.assertEqual(ready.v2.sha256(ready.CAMERA_GATE),
                         ready.CAMERA_GATE_SHA256)

    def test_generated_v4_validates_and_all_six_remain_not_run(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            executable = root / "Billiards"
            executable.write_bytes(b"integrated-fresh-release-test")
            with mock.patch.object(ready, "DEFAULT_EXECUTABLE",
                                   executable.resolve()):
                files = ready.prepare(
                    root / "ready", root / "package", executable)
                with mock.patch.object(ready, "DEFAULT_EXECUTABLE",
                                       executable.resolve()):
                    result = validator.validate(root / "ready")
            self.assertEqual(result["status"],
                             "PASS_CPU_ONLY_READINESS_V4")
            plan = json.loads(files["six_scene_plan"].read_text())
            self.assertEqual([row["status"] for row in plan["rows"]],
                             ["NOT_RUN"] * 6)
            self.assertEqual(plan["required_next_targeted_scenes"],
                             ["break", "low_aim"])
            authorization = json.loads(
                files["authorization_template"].read_text())
            self.assertEqual(authorization, ready.authorization_template())

    def test_tampered_generated_artifact_rejects(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            executable = root / "Billiards"
            executable.write_bytes(b"integrated-fresh-release-test")
            with mock.patch.object(ready, "DEFAULT_EXECUTABLE",
                                   executable.resolve()):
                files = ready.prepare(
                    root / "ready", root / "package", executable)
                plan = json.loads(files["six_scene_plan"].read_text())
                plan["rows"][0]["status"] = "PASS"
                files["six_scene_plan"].write_text(json.dumps(plan))
                with self.assertRaises(validator.base.ValidationError):
                    validator.validate(root / "ready")

    def test_missing_authorization_rejects_before_output_and_process(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "capture"
            with mock.patch.object(capture, "AutomationClient") as client:
                with self.assertRaisesRegex(RuntimeError,
                                            "authorization is required"):
                    capture.capture_all(
                        Path("unused"), output,
                        formal_asset_package=ready.v2.PACKAGE_ROOT)
            client.assert_not_called()
            self.assertFalse(output.exists())

    def test_wire_dispatch_adds_camera_schema_without_changing_recipe(self) -> None:
        low = next(scene for scene in
                   capture.FORMAL_MODERN_VISUAL_SCENES
                   if scene.scene_id == "low_aim")
        self.assertNotIn("schema_version", low.commands[1].params)
        client = FakeClient()
        apply_scene(client, low, 1920, 1080)
        preset = next(params for name, params in client.commands
                      if name == "set_camera_preset")
        self.assertEqual(preset["schema_version"], 1)
        self.assertIs(preset["instant"], True)

    def test_preparer_cannot_start_process_gpu_or_issue_authorization(self) -> None:
        source = Path(ready.__file__).read_text(encoding="utf-8")
        for forbidden in ("import subprocess", "Popen(", "AutomationClient",
                          "Cocoa", "OpenGL", "screenshot("):
            self.assertNotIn(forbidden, source)


if __name__ == "__main__":
    unittest.main()
