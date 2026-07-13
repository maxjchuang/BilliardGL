import json
import selectors
import subprocess


class AutomationError(RuntimeError):
    def __init__(self, response):
        self.response = response
        self.code = response["error"]["code"]
        self.message = response["error"]["message"]
        super().__init__(f"{self.code}: {self.message}")


class AutomationClient:
    def __init__(self, executable, mode="headless"):
        self.process = subprocess.Popen(
            [executable, "--automation", "--transport", "stdio", "--" + mode],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, bufsize=1)
        self.next_id = 1
        self.ready = self._read(10)

    def _read(self, timeout):
        selector = selectors.DefaultSelector()
        selector.register(self.process.stdout, selectors.EVENT_READ)
        if not selector.select(timeout):
            raise TimeoutError("timed out waiting for automation response")
        line = self.process.stdout.readline()
        selector.close()
        if not line:
            raise RuntimeError("automation process exited: " + self.process.stderr.read())
        return json.loads(line)

    def request(self, command, params=None, timeout=5, raise_errors=False):
        request_id = self.next_id
        self.next_id += 1
        message = {"id": request_id, "version": 1, "command": command, "params": params or {}}
        self.process.stdin.write(json.dumps(message, separators=(",", ":")) + "\n")
        self.process.stdin.flush()
        while True:
            value = self._read(timeout)
            if value.get("id") == request_id:
                if raise_errors and not value["ok"]:
                    raise AutomationError(value)
                return value

    def raw(self, line):
        self.process.stdin.write(line + "\n")
        self.process.stdin.flush()
        return self._read(5)

    def command(self, name, params=None, timeout=5):
        return self.request(name, params, timeout, raise_errors=True)["result"]

    def ping(self): return self.command("ping")
    def state(self): return self.command("get_state")
    def events(self, after_sequence=0): return self.command("get_events", {"after_sequence": after_sequence})
    def reset(self): return self.command("reset_game")
    def step(self, ticks): return self.command("step", {"ticks": ticks})
    def run_until(self, condition, max_steps=10000): return self.command("run_until", {"condition": condition, "max_steps": max_steps})
    def set_ball(self, **params): return self.command("set_ball", params)
    def load_scenario(self, balls): return self.command("load_scenario", {"balls": balls})
    def set_player_state(self, **params): return self.command("set_player_state", params)
    def clear_events(self): return self.command("clear_events")
    def start_physics_trace(self): return self.command("start_physics_trace")
    def stop_physics_trace(self): return self.command("stop_physics_trace")
    def clear_physics_trace(self): return self.command("clear_physics_trace")
    def physics_trace(self, after_tick=0, limit=1000):
        return self.command("get_physics_trace", {"after_tick": after_tick, "limit": limit})
    def toggle_aim(self): return self.command("toggle_aim")
    def set_aim_yaw(self, yaw): return self.command("set_aim_yaw", {"yaw": yaw})
    def set_shot_power(self, power): return self.command("set_shot_power", {"power": power})
    def shoot(self): return self.command("shoot")
    def toggle_help(self): return self.command("toggle_help")
    def key_down(self, key): return self.command("key_down", {"key": key})
    def key_up(self, key): return self.command("key_up", {"key": key})
    def special_key(self, key): return self.command("special_key", {"key": key})
    def mouse_move(self, x, y): return self.command("mouse_move", {"x": x, "y": y})
    def mouse_button(self, button, state, x=0, y=0): return self.command("mouse_button", {"button": button, "state": state, "x": x, "y": y})
    def mouse_wheel(self, direction): return self.command("mouse_wheel", {"direction": direction})
    def resize(self, width, height): return self.command("resize", {"width": width, "height": height})
    def orbit_camera(self, yaw_delta, pitch_delta): return self.command("orbit_camera", {"yaw_delta": yaw_delta, "pitch_delta": pitch_delta})
    def pan_camera(self, x_delta, z_delta): return self.command("pan_camera", {"x_delta": x_delta, "z_delta": z_delta})
    def zoom_camera(self, delta): return self.command("zoom_camera", {"delta": delta})
    def screenshot(self, path): return self.command("screenshot", {"path": path}, timeout=10)

    def close(self):
        if self.process.poll() is None:
            try:
                self.request("quit")
                self.process.wait(timeout=2)
            except (TimeoutError, subprocess.TimeoutExpired):
                self.process.terminate()
                try: self.process.wait(timeout=2)
                except subprocess.TimeoutExpired: self.process.kill()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        self.close()
