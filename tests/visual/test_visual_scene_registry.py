import pathlib
import sys
import unittest


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT))

from tests.visual.visual_scene_registry import (  # noqa: E402
    CAPTURE_HEIGHT,
    CAPTURE_PROFILES,
    CAPTURE_WIDTH,
    VISUAL_SCENES,
    apply_scene,
    get_scene,
    scene_ids,
    validate_registry,
)


class RecordingClient:
    def __init__(self):
        self.calls = []

    def reset(self):
        self.calls.append(("reset_game", {}))

    def resize(self, width, height):
        self.calls.append(("resize", {"width": width, "height": height}))

    def command(self, name, params, timeout=5):
        self.calls.append((name, params))
        return {}

    def state(self):
        return {"tick": 0}


class VisualSceneRegistryTests(unittest.TestCase):
    def test_registry_has_the_six_phase_zero_scenes(self):
        validate_registry()
        self.assertEqual(
            scene_ids(),
            ("break", "low_aim", "ball_closeup", "pocket_closeup",
             "tactical_overhead", "post_shot_observe"))

    def test_unknown_scene_is_rejected(self):
        with self.assertRaises(KeyError):
            get_scene("not-a-scene")

    def test_apply_scene_pins_reliable_drawable_size(self):
        client = RecordingClient()
        state = apply_scene(client, get_scene("low_aim"))
        self.assertEqual(state, {"tick": 0})
        self.assertEqual(
            client.calls[0],
            ("resize", {"width": CAPTURE_WIDTH, "height": CAPTURE_HEIGHT}))
        self.assertIn(("toggle_aim", {}), client.calls)

    def test_recipe_does_not_clear_initialized_render_texture_handles(self):
        client = RecordingClient()
        apply_scene(client, get_scene("break"))
        self.assertNotIn(("reset_game", {}), client.calls)

    def test_registry_accepts_only_validated_capture_profiles(self):
        self.assertEqual(CAPTURE_PROFILES, ((1024, 768), (1920, 1080)))
        client = RecordingClient()
        apply_scene(client, get_scene("break"), 1920, 1080)
        self.assertEqual(
            client.calls[0],
            ("resize", {"width": 1920, "height": 1080}))
        with self.assertRaises(ValueError):
            apply_scene(RecordingClient(), get_scene("break"), 1280, 720)

    def test_every_scene_has_a_distinct_command_recipe(self):
        recipes = {
            tuple((command.name, tuple(sorted(command.params.items())))
                  for command in scene.commands)
            for scene in VISUAL_SCENES
        }
        self.assertEqual(len(recipes), len(VISUAL_SCENES))


if __name__ == "__main__":
    unittest.main()
