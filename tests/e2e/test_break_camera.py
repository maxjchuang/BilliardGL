import math
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(__file__))
from automation_client import AutomationClient


with tempfile.TemporaryDirectory() as directory:
    before_path = os.path.join(directory, "before-drag.ppm")
    after_path = os.path.join(directory, "after-drag.ppm")
    with AutomationClient(sys.argv[1], mode="rendered") as game:
        game.start_physics_trace()
        game.toggle_aim()
        game.set_aim_yaw(math.pi / 2.0)
        # Exercise the real interactive break-speed path. This also guards
        # against high-energy rack and subsequent rail contacts exceeding the
        # recoverable penetration budget.
        game.set_shot_power(200.0)
        game.shoot()
        game.run_until("ball_collision", 5000)

        before = game.state()
        assert before["balls_moving"]
        game.screenshot(before_path)
        game.mouse_button("left", "down", 300, 300)
        game.mouse_move(420, 350)
        game.mouse_button("left", "up", 420, 350)
        after = game.state()
        game.screenshot(after_path)

        assert before["camera"]["angle_x"] != after["camera"]["angle_x"]
        assert before["camera"]["angle_y"] != after["camera"]["angle_y"]
        assert before["camera"]["eye"] != after["camera"]["eye"]
        stopped = game.run_until("balls_stopped", 100000)
        assert not stopped["balls_moving"]
        final = game.state()
        assert not final["balls_moving"]
        trace = game.physics_trace(limit=1000)
        assert all(frame["step_status"] == "succeeded" for frame in trace["frames"])
        assert all(frame["physics_profile_id"] ==
                   "chinese_pool_interactive_120hz_v5" for frame in trace["frames"])
        assert max(frame["maximum_penetration_cm"] for frame in trace["frames"]) < 2.75

    with open(before_path, "rb") as before_image, open(after_path, "rb") as after_image:
        assert before_image.read() != after_image.read()
