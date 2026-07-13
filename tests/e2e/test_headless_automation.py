import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from automation_client import AutomationClient


def ball(index, x, z, vx, vz):
    return {"index": index, "position": {"x": x, "y": 92.715, "z": z},
            "velocity": {"x": vx, "y": 0, "z": vz}}


with AutomationClient(sys.argv[1]) as client:
    assert client.ready["event"] == "ready"
    assert client.ready["mode"] == "headless"
    assert client.ping()["pong"]
    assert client.request("set_ball", ball(0, -5, 0, 20, 0))["ok"]
    assert client.request("set_ball", ball(1, 5, 0, 0, 0))["ok"]
    result = client.run_until("ball_collision", 20)
    assert result["steps"] <= 20
    state = client.state()
    assert state["balls"][1]["velocity"]["x"] > 0
    assert client.raw("{broken")["error"]["code"] == "parse_error"
    assert client.request("ping")["ok"]
    client.reset()
    client.key_down("h")
    assert client.state()["hud"]["show_help"]
    client.key_up("h")
    client.toggle_help()
    client.toggle_aim()
    client.set_aim_yaw(0.5)
    client.set_shot_power(40)
    client.mouse_wheel(1)
    client.special_key("left")
    client.orbit_camera(0.1, 0.1)
    client.pan_camera(1, 1)
    client.zoom_camera(5)
    client.resize(800, 600)
