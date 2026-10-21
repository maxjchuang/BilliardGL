import contextlib
import io
import hashlib
import os
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from scripts import capture_product_acceptance as capture


class FakeClient:
    ready = {"render_quality": "medium"}
    return_code = 0
    resolution = (1920, 1080)

    def __init__(self, *args, **kwargs):
        pass

    def __enter__(self):
        return self

    def __exit__(self, *args):
        return False

    def command(self, name):
        return {"camera": {"preset": "elevated"}}

    def collect_render_performance(self, warmup, sample):
        return dict(render_width=self.resolution[0], render_height=self.resolution[1])

    def state(self):
        return {"tick": 0}

    def screenshot(self, path):
        Path(path).write_bytes(b"fake screenshot; decoding is separately mocked")


class ProductAcceptanceCaptureTests(unittest.TestCase):
    def run_capture(self, root, client=FakeClient):
        executable = root / "executable"
        executable.write_bytes(b"fixture")
        with patch.object(capture, "AutomationClient", client), \
                patch.object(capture, "apply_scene", return_value={"tick": 0}), \
                patch.object(capture, "read_ppm", return_value=(1920, 1080, b"x")), \
                contextlib.redirect_stdout(io.StringIO()):
            return capture.capture(executable, root / "output")

    def test_records_all_six_without_auto_acceptance(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.assertTrue(self.run_capture(root))
            report = json.loads((root / "output/report.json").read_text())
            self.assertEqual(len(report["scenes"]), 6)
            self.assertEqual(report["acceptance"], "requires_visual_review_and_comparison")
            self.assertTrue(all(scene["commands"] and scene["image_sha256"]
                                for scene in report["scenes"]))
            with self.assertRaises(FileExistsError):
                capture.capture(root / "executable", root / "output")

    def test_wrong_sample_resolution_is_failure_not_visual_pass(self):
        class WrongResolution(FakeClient):
            resolution = (1024, 768)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.assertFalse(self.run_capture(root, WrongResolution))
            report = json.loads((root / "output/report.json").read_text())
            self.assertTrue(all(not scene["ok"] and "wrong render resolution" in scene["error"]
                                for scene in report["scenes"]))

    def test_override_binds_actual_candidate_assets_not_source_assets(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            candidate = root / "candidate"
            candidate.mkdir()
            lighting = candidate / "lighting.json"
            lighting.write_bytes(b'{"ev100":7.6}')
            with patch.dict(os.environ, {"BILLIARDGL_MODERN_PHASE2_ROOT": str(candidate)}):
                self.assertTrue(self.run_capture(root))
            report = json.loads((root / "output/report.json").read_text())
            self.assertEqual(report["asset_root"], str(candidate.resolve()))
            self.assertEqual(report["asset_sha256"], {
                str(lighting.resolve()): hashlib.sha256(lighting.read_bytes()).hexdigest()})

    def test_closeups_have_explicit_targets(self):
        scenes = {scene.scene_id: scene for scene in capture.current_scenes()}
        targets = {name: next(command.params for command in scenes[name].commands
                              if command.name == "pan_camera")
                   for name in ("pocket_closeup", "ball_closeup")}
        self.assertEqual(targets["pocket_closeup"], {"x_delta": 54.0, "z_delta": 110.0})
        self.assertEqual(targets["ball_closeup"], {"x_delta": 0.0, "z_delta": -63.5})

    def test_pocket_closeup_locks_full_cavity_backoff(self):
        scene = next(scene for scene in capture.current_scenes()
                     if scene.scene_id == "pocket_closeup")
        self.assertEqual(
            [command.name for command in scene.commands],
            ["set_camera_preset", "set_camera_preset", "pan_camera",
             "orbit_camera", "zoom_camera"])
        self.assertEqual(scene.commands[-1].params, {"delta": -100.0})


if __name__ == "__main__":
    unittest.main()
