import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from automation_client import AutomationClient


with AutomationClient(sys.argv[1]) as client:
    assert client.ready["event"] == "ready"
    assert client.ready["mode"] == "headless"
    assert client.ready["renderer_backend"] == "none"

with AutomationClient(
        sys.argv[1], extra_args=["--renderer", "modern"]) as client:
    assert client.ready["event"] == "ready"
    assert client.ready["mode"] == "headless"
    assert client.ready["renderer_backend"] == "none"
    assert client.state()["tick"] == 0
