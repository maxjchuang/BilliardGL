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
    assert client.request("ping")["result"]["pong"]
    assert client.request("set_ball", ball(0, -5, 0, 20, 0))["ok"]
    assert client.request("set_ball", ball(1, 5, 0, 0, 0))["ok"]
    result = client.request("run_until", {"condition": "ball_collision", "max_steps": 20})
    assert result["ok"]
    state = client.request("get_state")["result"]
    assert state["balls"][1]["velocity"]["x"] > 0
    assert client.raw("{broken")["error"]["code"] == "parse_error"
    assert client.request("ping")["ok"]
