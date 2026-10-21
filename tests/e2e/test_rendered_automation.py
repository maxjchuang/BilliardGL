import os
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(__file__))
from automation_client import AutomationClient


with tempfile.TemporaryDirectory() as directory:
    path = os.path.join(directory, "frame.ppm")
    default_legacy_path = os.path.join(directory, "default-legacy.ppm")
    explicit_legacy_path = os.path.join(directory, "explicit-legacy.ppm")
    before_drag_path = os.path.join(directory, "before-drag.ppm")
    after_drag_path = os.path.join(directory, "after-drag.ppm")
    with AutomationClient(sys.argv[1], mode="rendered") as client:
        assert client.ready["mode"] == "rendered"
        assert client.ready["renderer_backend"] == "legacy"
        before_drag_state = client.state()
        client.screenshot(default_legacy_path)
        client.screenshot(before_drag_path)
        client.mouse_button("left", "down", 300, 300)
        client.mouse_move(420, 350)
        client.mouse_button("left", "up", 420, 350)
        after_drag_state = client.state()
        client.screenshot(after_drag_path)
        assert before_drag_state["camera"]["eye"] != after_drag_state["camera"]["eye"]
        assert client.request("toggle_aim")["ok"]
        assert client.request("set_aim_yaw", {"yaw": 0.5})["ok"]
        assert client.request("set_shot_power", {"power": 80})["ok"]
        tick = client.request("get_state")["result"]["tick"]
        shot = client.request("screenshot", {"path": path}, timeout=15)
        assert shot["ok"] and shot["result"]["tick"] == tick
    with AutomationClient(
            sys.argv[1], mode="rendered", extra_args=["--renderer", "legacy"]) as client:
        assert client.ready["renderer_backend"] == "legacy"
        client.screenshot(explicit_legacy_path)
    with open(path, "rb") as image:
        assert image.readline().strip() == b"P6"
        width, height = map(int, image.readline().split())
        assert width > 0 and height > 0
        assert image.readline().strip() == b"255"
        assert any(image.read())
    with open(before_drag_path, "rb") as before_drag, open(after_drag_path, "rb") as after_drag:
        assert before_drag.read() != after_drag.read()
    with open(default_legacy_path, "rb") as default_legacy, open(explicit_legacy_path, "rb") as explicit_legacy:
        assert default_legacy.read() == explicit_legacy.read()

invalid = subprocess.run(
    [sys.argv[1], "--renderer", "future"],
    input="", text=True, capture_output=True, timeout=10)
assert invalid.returncode == 2
assert invalid.stderr.strip() == "renderer must be legacy or modern"
