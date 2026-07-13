import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(__file__))
from automation_client import AutomationClient


with tempfile.TemporaryDirectory() as directory:
    path = os.path.join(directory, "frame.ppm")
    with AutomationClient(sys.argv[1], mode="rendered") as client:
        assert client.ready["mode"] == "rendered"
        assert client.request("toggle_aim")["ok"]
        assert client.request("set_aim_yaw", {"yaw": 0.5})["ok"]
        assert client.request("set_shot_power", {"power": 80})["ok"]
        tick = client.request("get_state")["result"]["tick"]
        shot = client.request("screenshot", {"path": path})
        assert shot["ok"] and shot["result"]["tick"] == tick
    with open(path, "rb") as image:
        assert image.readline().strip() == b"P6"
        width, height = map(int, image.readline().split())
        assert width > 0 and height > 0
        assert image.readline().strip() == b"255"
        assert any(image.read())
