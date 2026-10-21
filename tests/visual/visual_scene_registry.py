"""Versioned visual-scene registry for rendered automation.

The registry deliberately uses only public automation commands.  It does not
duplicate physics setup or reach into renderer internals, so the same scenes
can survive the legacy-to-modern renderer migration.
"""

from dataclasses import dataclass
import math
from typing import Any, Dict, Iterable, Mapping, Sequence, Tuple


REGISTRY_VERSION = 1
CAPTURE_WIDTH = 1024
CAPTURE_HEIGHT = 768
CAPTURE_PROFILES = ((1024, 768), (1920, 1080))
RANDOM_SEED = 0  # The current runtime has no visual RNG; reserved in metadata.
PHYSICS_PROFILE_ID = "chinese_pool_interactive_120hz_v5"


@dataclass(frozen=True)
class VisualCommand:
    name: str
    params: Mapping[str, Any]


@dataclass(frozen=True)
class VisualScene:
    scene_id: str
    title: str
    purpose: str
    commands: Tuple[VisualCommand, ...]
    interest_regions: Tuple[Tuple[str, float, float, float, float], ...]


def _command(name: str, **params: Any) -> VisualCommand:
    return VisualCommand(name, params)


# Interest regions use top-left normalized image coordinates.  They are broad
# composition guards, not pixel-perfect goldens, and are therefore suitable
# across driver versions.
VISUAL_SCENES: Tuple[VisualScene, ...] = (
    VisualScene(
        "break",
        "Break setup",
        "Whole-table composition with the cue ball and complete rack visible.",
        (_command("orbit_camera", yaw_delta=0.22, pitch_delta=-0.10),),
        (("table", 0.10, 0.16, 0.80, 0.68),),
    ),
    VisualScene(
        "low_aim",
        "Low aiming view",
        "Player-height aiming view with cue and guide presentation enabled.",
        (
            _command("orbit_camera", yaw_delta=0.0, pitch_delta=0.40),
            _command("zoom_camera", delta=-38.0),
            _command("toggle_aim"),
            _command("set_aim_yaw", yaw=math.pi / 2.0),
            _command("set_shot_power", power=80.0),
        ),
        (("cue_and_ball", 0.18, 0.36, 0.64, 0.42),),
    ),
    VisualScene(
        "ball_closeup",
        "Ball close-up",
        "Close inspection of ball shading, texture, silhouette, and contact shadow.",
        (
            _command("orbit_camera", yaw_delta=0.34, pitch_delta=0.34),
            _command("zoom_camera", delta=-88.0),
        ),
        (("ball", 0.25, 0.24, 0.50, 0.55),),
    ),
    VisualScene(
        "pocket_closeup",
        "Pocket close-up",
        "Close inspection of a corner pocket, jaws, rails, and cloth seam.",
        (
            _command("pan_camera", x_delta=59.0, z_delta=-59.0),
            _command("orbit_camera", yaw_delta=-0.25, pitch_delta=0.34),
            _command("zoom_camera", delta=-76.0),
        ),
        (("pocket", 0.18, 0.18, 0.64, 0.64),),
    ),
    VisualScene(
        "tactical_overhead",
        "Tactical overhead",
        "Near-orthographic-feeling full-table view for tactical readability.",
        (
            _command("pan_camera", x_delta=0.0, z_delta=63.5),
            _command("orbit_camera", yaw_delta=0.0, pitch_delta=-0.94),
            _command("zoom_camera", delta=92.0),
        ),
        (("playfield", 0.20, 0.06, 0.60, 0.88),),
    ),
    VisualScene(
        "post_shot_observe",
        "Post-shot observation",
        "Deterministic frame shortly after the break collision begins.",
        (
            _command("toggle_aim"),
            _command("set_aim_yaw", yaw=math.pi / 2.0),
            _command("set_shot_power", power=120.0),
            _command("shoot"),
            _command("run_until", condition="ball_collision", max_steps=5000),
            _command("step", ticks=24),
            _command("orbit_camera", yaw_delta=0.18, pitch_delta=-0.08),
            _command("zoom_camera", delta=24.0),
        ),
        (("moving_balls", 0.12, 0.15, 0.76, 0.68),),
    ),
)


def scene_ids() -> Tuple[str, ...]:
    return tuple(scene.scene_id for scene in VISUAL_SCENES)


def get_scene(scene_id: str) -> VisualScene:
    for scene in VISUAL_SCENES:
        if scene.scene_id == scene_id:
            return scene
    raise KeyError("unknown visual scene: " + scene_id)


def validate_registry(scenes: Iterable[VisualScene] = VISUAL_SCENES) -> None:
    scenes = tuple(scenes)
    ids = [scene.scene_id for scene in scenes]
    if len(scenes) != 6:
        raise ValueError("the phase-0 registry must contain exactly six scenes")
    if len(ids) != len(set(ids)):
        raise ValueError("visual scene ids must be unique")
    for scene in scenes:
        if not scene.scene_id or not scene.commands or not scene.interest_regions:
            raise ValueError("each visual scene needs an id, commands, and interest regions")
        for name, x, y, width, height in scene.interest_regions:
            if not name or min(x, y, width, height) < 0.0 or x + width > 1.0 or y + height > 1.0:
                raise ValueError("invalid normalized interest region in " + scene.scene_id)


def apply_scene(
    client: Any, scene: VisualScene,
    width: int = CAPTURE_WIDTH, height: int = CAPTURE_HEIGHT,
) -> Dict[str, Any]:
    """Configure *scene* in a fresh process and return authoritative state.

    The capture runner launches one process per scene.  Do not issue
    ``reset_game`` here: the legacy renderer currently stores initialized ball
    texture handles in runtime state, and a reset clears those presentation
    handles.  The process boundary is the deterministic reset boundary until
    renderer resources move out of GameState.
    """
    if (width, height) not in CAPTURE_PROFILES:
        raise ValueError("unsupported visual capture profile: %dx%d" % (width, height))
    client.resize(width, height)
    for command in scene.commands:
        params = dict(command.params)
        # Semantic camera preset v1 is schema-versioned on the wire.  Keep the
        # authored formal recipe identical to the approved design gate while
        # making the transport contract explicit at dispatch.
        if command.name == "set_camera_preset":
            params["schema_version"] = 1
        client.command(command.name, params, timeout=15)
    return client.state()


validate_registry()
