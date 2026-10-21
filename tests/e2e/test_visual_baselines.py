import json
import pathlib
import subprocess
import sys
import tempfile


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parents[2]
CAPTURE_SCRIPT = REPOSITORY_ROOT / "scripts" / "capture_visual_baselines.py"
EXPECTED_SCENES = {
    "break", "low_aim", "ball_closeup", "pocket_closeup",
    "tactical_overhead", "post_shot_observe",
}
CAPTURE_PROFILES = ((1024, 768), (1920, 1080))


def read_dimensions(path):
    with path.open("rb") as image:
        assert image.readline().strip() == b"P6"
        width, height = map(int, image.readline().split())
        assert image.readline().strip() == b"255"
        pixels = image.read()
    assert len(pixels) == width * height * 3
    assert any(pixels)
    return width, height


with tempfile.TemporaryDirectory(prefix="billiardgl-visual-e2e-") as directory:
    root = pathlib.Path(directory)
    executable = pathlib.Path(sys.argv[1]).resolve()
    for capture_width, capture_height in CAPTURE_PROFILES:
        output = root / ("%dx%d" % (capture_width, capture_height))
        completed = subprocess.run(
            [sys.executable, str(CAPTURE_SCRIPT), "--executable", str(executable),
             "--output", str(output), "--width", str(capture_width),
             "--height", str(capture_height), "--verify-repeatability"],
            cwd=str(REPOSITORY_ROOT), text=True, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, timeout=180)
        assert completed.returncode == 0, completed.stdout + completed.stderr

        manifest = json.loads(
            (output / "manifest.json").read_text(encoding="utf-8"))
        assert manifest["registry_version"] == 1
        assert manifest["resolution"] == {
            "width": capture_width, "height": capture_height}
        assert manifest["repeatability_verified"] is True
        assert manifest["physics_profile_id"] == "chinese_pool_interactive_120hz_v5"
        assert {scene["scene_id"] for scene in manifest["scenes"]} == EXPECTED_SCENES
        assert len({scene["state_sha256"] for scene in manifest["scenes"]}) == 6
        assert len({scene["image_sha256"] for scene in manifest["scenes"]}) >= 5

        for scene in manifest["scenes"]:
            image_path = output / scene["file"]
            assert image_path.is_file()
            assert read_dimensions(image_path) == (capture_width, capture_height)
            assert scene["visible_pixels"] > 0
            assert all(region["visible_pixels"] > 0
                       for region in scene["interest_regions"].values())

        # Standalone screenshot mode shares the offscreen path. Run it twice
        # to verify that native-window limits do not affect dimensions or bytes.
        standalone_images = []
        for repetition in range(2):
            standalone_path = output / ("standalone-%d.ppm" % repetition)
            standalone = subprocess.run(
                [str(executable), "--width", str(capture_width),
                 "--height", str(capture_height), "--screenshot",
                 str(standalone_path), "--screenshot-scene", "aim"],
                cwd=str(output), text=True, stdout=subprocess.PIPE,
                stderr=subprocess.PIPE, timeout=30)
            assert standalone.returncode == 0, standalone.stdout + standalone.stderr
            assert read_dimensions(standalone_path) == (capture_width, capture_height)
            standalone_images.append(standalone_path.read_bytes())
        assert standalone_images[0] == standalone_images[1]

    rejected = subprocess.run(
        [sys.executable, str(CAPTURE_SCRIPT), "--executable", str(executable),
         "--output", str(root / "invalid"), "--width", "1280", "--height", "720"],
        cwd=str(REPOSITORY_ROOT), text=True, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE, timeout=10)
    assert rejected.returncode == 2
    assert "supported capture profiles" in rejected.stderr
