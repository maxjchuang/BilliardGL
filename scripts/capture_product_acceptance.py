#!/usr/bin/env python3
"""Capture the original six modern scenes; report evidence, never auto-approve it."""
import argparse
import hashlib
import json
import math
import os
from dataclasses import replace
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tests.e2e.automation_client import AutomationClient
from tests.visual.formal_modern_scene_registry_v2 import FORMAL_MODERN_VISUAL_SCENES
from tests.visual.visual_scene_registry import apply_scene, VisualCommand
from scripts.capture_visual_baselines import read_ppm


def current_scenes():
    """Keep the six purposes, with explicit close-up targets in table space."""
    def command(name, **params):
        return VisualCommand(name, params)
    center = (
        command("set_camera_preset", preset="overhead", instant=True),
        command("set_camera_preset", preset="elevated", instant=True),
    )
    for scene in FORMAL_MODERN_VISUAL_SCENES:
        if scene.scene_id == "pocket_closeup":
            # TA's +X/+Z corner view, using public camera controls.  The
            # 48-degree product FOV keeps the full pocket cavity and the rack
            # in frame as geometry/scale evidence. No hidden renderer seam.
            scene = replace(scene, commands=center + (
                command("pan_camera", x_delta=54.0, z_delta=110.0),
                command("orbit_camera", yaw_delta=math.atan2(80.0, 65.0) + math.pi / 2,
                        pitch_delta=math.atan2(126.0, math.hypot(65.0, 80.0)) - math.radians(42)),
                command("zoom_camera", delta=-100.0),
            ))
        elif scene.scene_id == "ball_closeup":
            scene = replace(scene, commands=center + (
                command("pan_camera", x_delta=0.0, z_delta=-63.5),
                command("orbit_camera", yaw_delta=0.34, pitch_delta=0.0),
                command("zoom_camera", delta=-140.0),
            ))
        yield scene


def capture(executable, output):
    # A new directory prevents silently overwriting a previous evidence bundle.
    output.mkdir(parents=True, exist_ok=False)
    # Match the asset root consumed by the executable. In particular, an
    # isolated lookdev candidate must never inherit the source asset hashes.
    asset_root = Path(os.environ.get("BILLIARDGL_MODERN_PHASE2_ROOT") or
                      ROOT / "assets/modern/phase2").resolve()
    def asset_key(path):
        try:
            return str(path.relative_to(ROOT))
        except ValueError:
            return str(path)
    report = {
        "schema": "billiardgl_product_six_scene_observation_v1",
        "executable": str(executable),
        "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
        "width": 1920, "height": 1080,
        "acceptance": "requires_visual_review_and_comparison",
        "asset_root": str(asset_root),
        "asset_sha256": {asset_key(path):
                         hashlib.sha256(path.read_bytes()).hexdigest()
                         for path in sorted(asset_root.glob("*"))
                         if path.is_file()},
        "scenes": [],
    }
    for scene in current_scenes():
        item = {"id": scene.scene_id, "ok": False}
        item["commands"] = [{"name": command.name, "params": dict(command.params)}
                            for command in scene.commands]
        try:
            with AutomationClient(str(executable), mode="rendered",
                                  extra_args=["--renderer", "modern"]) as client:
                item["ready"] = client.ready
                if client.ready["render_quality"] != "medium":
                    raise RuntimeError("expected shipping Medium quality")
                state = apply_scene(client, scene, 1920, 1080)
                item["state"] = state
                item["presentation"] = client.command("get_presentation_state")
                item["performance"] = client.collect_render_performance(120, 300)
                if (item["performance"]["render_width"],
                        item["performance"]["render_height"]) != (1920, 1080):
                    raise RuntimeError("performance sampled the wrong render resolution")
                if client.state() != state:
                    raise RuntimeError("performance collection changed runtime state")
                path = output / (scene.scene_id + ".ppm")
                client.screenshot(str(path))
                width, height, pixels = read_ppm(path)
                if (width, height) != (1920, 1080) or not any(pixels):
                    raise RuntimeError("invalid or blank screenshot")
                item.update(ok=True, image=str(path), tick=state["tick"],
                            image_sha256=hashlib.sha256(path.read_bytes()).hexdigest())
            if client.return_code != 0:
                raise RuntimeError("renderer exit code " + str(client.return_code))
        except Exception as error:
            item.update(ok=False, error=str(error))
        report["scenes"].append(item)
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps({key: item[key] for key in
                          ("id", "ok", "performance", "error") if key in item}), flush=True)
    return all(item["ok"] for item in report["scenes"])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    sys.exit(0 if capture(args.executable.resolve(), args.output.resolve()) else 1)
