from __future__ import annotations

import copy
import math
import unittest

from scripts.capture_visual_baselines import validate_formal_presentation_camera
from scripts.validate_phase2_production_visuals import (
    ReviewError,
    validate_semantic_camera,
)
from tests.visual.formal_modern_scene_registry_v2 import (
    EXPECTED_CAMERA,
    FORMAL_MODERN_VISUAL_SCENES,
    REGISTRY_VERSION,
)
from tests.visual.visual_scene_registry import VISUAL_SCENES


def presentation(scene_id: str) -> dict:
    state, preset = EXPECTED_CAMERA[scene_id]
    target = {"x": 0.0, "y": 86.0, "z": 0.0}
    if scene_id == "tactical_overhead":
        eye = {"x": 0.0, "y": 466.0, "z": 0.0}
        distance = 380.0
    else:
        eye = {"x": 0.0, "y": 186.0, "z": 0.0}
        distance = 100.0
    return {"camera": {
        "state": state,
        "preset": preset,
        "transitioning": False,
        "eye_cm": eye,
        "target_cm": target,
        "distance_cm": distance,
        "fov_degrees": 45.0,
    }}


class Phase2FormalModernRegistryV2Tests(unittest.TestCase):
    def test_six_scene_identity_and_interest_regions_are_unchanged(self) -> None:
        self.assertEqual(REGISTRY_VERSION, 2)
        self.assertEqual(
            [scene.scene_id for scene in FORMAL_MODERN_VISUAL_SCENES],
            [scene.scene_id for scene in VISUAL_SCENES])
        self.assertEqual(
            [scene.interest_regions for scene in FORMAL_MODERN_VISUAL_SCENES],
            [scene.interest_regions for scene in VISUAL_SCENES])

    def test_each_scene_has_gate_approved_explicit_instant_semantic_preset(self) -> None:
        for scene in FORMAL_MODERN_VISUAL_SCENES:
            presets = [item for item in scene.commands
                       if item.name == "set_camera_preset"]
            self.assertTrue(presets, scene.scene_id)
            self.assertTrue(all(item.params.get("instant") is True
                                for item in presets), scene.scene_id)
            if scene.scene_id != "low_aim":
                self.assertEqual(scene.commands[0].name,
                                 "set_camera_preset", scene.scene_id)

    def test_low_aim_is_fixed_player_low_without_legacy_camera_deltas(self) -> None:
        scene = next(item for item in FORMAL_MODERN_VISUAL_SCENES
                     if item.scene_id == "low_aim")
        self.assertEqual(
            [(item.name, dict(item.params)) for item in scene.commands],
            [("toggle_aim", {}),
             ("set_camera_preset",
              {"preset": "player_low", "instant": True}),
             ("set_aim_yaw", {"yaw": math.pi / 2.0}),
             ("set_shot_power", {"power": 80.0})])
        self.assertFalse({"orbit_camera", "zoom_camera", "pan_camera"} &
                         {item.name for item in scene.commands})

    def test_expected_semantic_map_matches_ta_gate(self) -> None:
        self.assertEqual(EXPECTED_CAMERA, {
            "break": ("observe_free", "elevated"),
            "low_aim": ("aim_player", "player_low"),
            "ball_closeup": ("observe_free", "elevated"),
            "pocket_closeup": ("observe_free", "elevated"),
            "tactical_overhead": ("aim_overhead", "overhead"),
            "post_shot_observe": ("observe_free", "elevated"),
        })

    def test_runner_camera_gate_accepts_exact_settled_projection(self) -> None:
        for scene_id in EXPECTED_CAMERA:
            actual = presentation(scene_id)
            self.assertEqual(
                validate_formal_presentation_camera(scene_id, actual),
                actual["camera"])
            validate_semantic_camera(scene_id, actual["camera"])

    def test_runner_camera_gate_rejects_wrong_or_unsafe_presentation(self) -> None:
        mutations = []
        base = presentation("low_aim")
        wrong_state = copy.deepcopy(base); wrong_state["camera"]["state"] = "observe_free"
        mutations.append((wrong_state, "applied/settled"))
        transitioning = copy.deepcopy(base); transitioning["camera"]["transitioning"] = True
        mutations.append((transitioning, "applied/settled"))
        nonfinite = copy.deepcopy(base); nonfinite["camera"]["eye_cm"]["x"] = math.nan
        mutations.append((nonfinite, "point"))
        projection = copy.deepcopy(base); projection["camera"]["distance_cm"] = 101.0
        mutations.append((projection, "projection differs"))
        lamp = copy.deepcopy(base)
        lamp["camera"]["eye_cm"]["y"] = 221.0
        lamp["camera"]["target_cm"]["y"] = 121.0
        mutations.append((lamp, "lamp-safe"))
        for value, message in mutations:
            with self.subTest(message=message):
                with self.assertRaisesRegex(RuntimeError, message):
                    validate_formal_presentation_camera("low_aim", value)

    def test_output_manifest_gate_rejects_transition_and_lamp_intersection(self) -> None:
        camera = presentation("low_aim")["camera"]
        camera["transitioning"] = True
        with self.assertRaisesRegex(ReviewError, "settled"):
            validate_semantic_camera("low_aim", camera)
        camera = presentation("low_aim")["camera"]
        camera["eye_cm"]["y"] = 222.0
        camera["target_cm"]["y"] = 122.0
        with self.assertRaisesRegex(ReviewError, "lamp safety"):
            validate_semantic_camera("low_aim", camera)


if __name__ == "__main__":
    unittest.main()
