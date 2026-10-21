import os
import math
import pathlib
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(__file__))
from automation_client import AutomationClient


def assert_ppm(path, expected_width, expected_height):
    with open(path, "rb") as image:
        assert image.readline().strip() == b"P6"
        width, height = map(int, image.readline().split())
        assert (width, height) == (expected_width, expected_height)
        assert image.readline().strip() == b"255"
        pixels = image.read()
    assert len(pixels) == width * height * 3
    assert any(pixels)


with tempfile.TemporaryDirectory(prefix="billiardgl-modern-production-") as directory:
    root = pathlib.Path(directory)
    aim = root / "automation-aim.ppm"
    with AutomationClient(
            sys.argv[1], mode="rendered",
            extra_args=["--renderer", "modern"]) as client:
        assert client.ready["event"] == "ready"
        assert client.ready["mode"] == "rendered"
        assert client.ready["renderer_backend"] == "modern"
        assert client.ready["pbr_loaded"] is True
        assert client.ready["pbr_texture_count"] == 22
        assert client.ready["environment_enabled"] is True
        assert client.ready["gltf_scene_loaded"] is True
        assert client.ready["venue_scene_loaded"] is True
        assert client.ready["render_quality"] == "medium"
        assert client.ready["msaa_samples"] == 4
        assert client.ready["shadow_map_size"] == 2048
        client.toggle_aim()
        client.set_aim_yaw(1.5707963)
        client.set_shot_power(120)
        client.screenshot(str(aim))
        client.resize(1920, 1080)
        before_measurement = client.state()
        performance = client.collect_render_performance(120, 300)
        assert client.state() == before_measurement
        assert performance["sample_frames"] == 300
        assert (performance["render_width"], performance["render_height"]) == (1920, 1080)
        assert performance["gpu_timer_queries_observed"] is True
        total = [performance["p" + str(q) + "_total_frame_ms"]
                 for q in (50, 95, 99)]
        assert all(math.isfinite(value) and value >= 0 for value in total)
        assert total == sorted(total)
        for domain in ("cpu_pass_percentiles", "gpu_pass_percentiles"):
            assert performance[domain]
            for measured in performance[domain].values():
                assert 0 < measured["sample_count"] <= 300
                values = [measured["p" + str(q) + "_ms"] for q in (50, 95, 99)]
                assert all(math.isfinite(value) and value >= 0 for value in values)
                assert values == sorted(values)
        client.resize(1024, 768)
        client.shoot()
        stopped = client.run_until("balls_stopped", 100000)
        assert stopped["balls_moving"] is False
        assert client.state()["balls_moving"] is False
    assert_ppm(aim, 1024, 768)

    standalone = root / "standalone-modern-1920x1080.ppm"
    completed = subprocess.run(
        [sys.argv[1], "--renderer", "modern", "--width", "1920",
         "--height", "1080", "--screenshot", str(standalone),
         "--screenshot-scene", "aim"],
        text=True, capture_output=True, timeout=30)
    assert completed.returncode == 0, completed.stdout + completed.stderr
    assert_ppm(standalone, 1920, 1080)

    missing_capture = root / "must-not-fallback.ppm"
    missing_environment = os.environ.copy()
    missing_environment["BILLIARDGL_MODERN_PHASE2_ROOT"] = str(root / "missing-phase2")
    failed = subprocess.run(
        [sys.argv[1], "--renderer", "modern", "--width", "1024",
         "--height", "768", "--screenshot", str(missing_capture)],
        text=True, capture_output=True, timeout=30, env=missing_environment)
    assert failed.returncode == 2, failed.stdout + failed.stderr
    assert "PBR parameter manifest unavailable" in failed.stderr
    assert not missing_capture.exists()
