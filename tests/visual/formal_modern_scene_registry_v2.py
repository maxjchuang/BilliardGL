"""Formal-modern camera registry using semantic, instant camera presets."""

from __future__ import annotations

import math
from typing import Any

from tests.visual.visual_scene_registry import VisualCommand, VisualScene


REGISTRY_VERSION = 2


def command(name: str, **params: Any) -> VisualCommand:
    return VisualCommand(name, params)


# Scene ids, purposes and interest regions remain byte-for-byte equivalent to
# registry v1. Only the modern camera setup is corrected: every scene starts
# from an explicit semantic preset, and `instant` prevents an unadvanced
# presentation transition from leaking into performance/capture frames.
FORMAL_MODERN_VISUAL_SCENES = (
    VisualScene(
        "break", "Break setup",
        "Whole-table composition with the cue ball and complete rack visible.",
        (command("set_camera_preset", preset="elevated", instant=True),
         command("orbit_camera", yaw_delta=0.0,
                 pitch_delta=math.radians(20.0) - math.radians(42.0)),
         command("zoom_camera", delta=100.0)),
        (("table", 0.10, 0.16, 0.80, 0.68),)),
    VisualScene(
        "low_aim", "Low aiming view",
        "Player-height aiming view with cue and guide presentation enabled.",
        (command("toggle_aim"),
         command("set_camera_preset", preset="player_low", instant=True),
         command("set_aim_yaw", yaw=math.pi / 2.0),
         command("set_shot_power", power=80.0)),
        (("cue_and_ball", 0.18, 0.36, 0.64, 0.42),)),
    VisualScene(
        "ball_closeup", "Ball close-up",
        "Close inspection of ball shading, texture, silhouette, and contact shadow.",
        (command("set_camera_preset", preset="elevated", instant=True),
         command("orbit_camera", yaw_delta=0.34, pitch_delta=0.34),
         command("zoom_camera", delta=-88.0)),
        (("ball", 0.25, 0.24, 0.50, 0.55),)),
    VisualScene(
        "pocket_closeup", "Pocket close-up",
        "Close inspection of a corner pocket, jaws, rails, and cloth seam.",
        (command("set_camera_preset", preset="elevated", instant=True),
         command("pan_camera", x_delta=59.0, z_delta=-59.0),
         command("orbit_camera", yaw_delta=-0.25, pitch_delta=0.34),
         command("zoom_camera", delta=-76.0)),
        (("pocket", 0.18, 0.18, 0.64, 0.64),)),
    VisualScene(
        "tactical_overhead", "Tactical overhead",
        "Near-orthographic-feeling full-table view for tactical readability.",
        (command("set_camera_preset", preset="overhead", instant=True),),
        (("playfield", 0.20, 0.06, 0.60, 0.88),)),
    VisualScene(
        "post_shot_observe", "Post-shot observation",
        "Deterministic frame shortly after the break collision begins.",
        (command("set_camera_preset", preset="player_low", instant=True),
         command("toggle_aim"),
         command("set_camera_preset", preset="player_low", instant=True),
         command("set_aim_yaw", yaw=math.pi / 2.0),
         command("set_shot_power", power=120.0),
         command("shoot"),
         command("run_until", condition="ball_collision", max_steps=5000),
         command("step", ticks=24),
         command("set_camera_preset", preset="elevated", instant=True)),
        (("moving_balls", 0.12, 0.15, 0.76, 0.68),)),
)

EXPECTED_CAMERA = {
    "break": ("observe_free", "elevated"),
    "low_aim": ("aim_player", "player_low"),
    "ball_closeup": ("observe_free", "elevated"),
    "pocket_closeup": ("observe_free", "elevated"),
    "tactical_overhead": ("aim_overhead", "overhead"),
    "post_shot_observe": ("observe_free", "elevated"),
}


def validate_registry() -> None:
    scene_ids = tuple(scene.scene_id for scene in FORMAL_MODERN_VISUAL_SCENES)
    if scene_ids != tuple(EXPECTED_CAMERA):
        raise ValueError("formal modern scene order differs")
    for scene in FORMAL_MODERN_VISUAL_SCENES:
        first_camera = next(item for item in scene.commands if item.name in {
            "set_camera_preset", "orbit_camera", "pan_camera", "zoom_camera",
            "toggle_aim",
        })
        if scene.scene_id != "low_aim" and \
                (first_camera.name != "set_camera_preset" or
                 first_camera.params.get("instant") is not True):
            raise ValueError(scene.scene_id + ": first camera action must be instant semantic preset")
        preset_commands = [item for item in scene.commands
                           if item.name == "set_camera_preset"]
        if not preset_commands or not all(
                item.params.get("instant") is True for item in preset_commands):
            raise ValueError(scene.scene_id + ": instant semantic preset required")
    low_aim = next(scene for scene in FORMAL_MODERN_VISUAL_SCENES
                   if scene.scene_id == "low_aim")
    exact_low_recipe = (
        ("toggle_aim", {}),
        ("set_camera_preset", {"preset": "player_low", "instant": True}),
        ("set_aim_yaw", {"yaw": math.pi / 2.0}),
        ("set_shot_power", {"power": 80.0}),
    )
    if tuple((item.name, dict(item.params)) for item in low_aim.commands) != \
            exact_low_recipe:
        raise ValueError("low_aim: exact TA-approved four-command recipe required")
    forbidden = {"orbit_camera", "zoom_camera", "pan_camera"}
    if any(item.name in forbidden for item in low_aim.commands):
        raise ValueError("low_aim: fixed PlayerLow forbids orbit/zoom/pan")


validate_registry()
