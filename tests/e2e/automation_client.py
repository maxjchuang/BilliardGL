import json
import subprocess


class AutomationClient:
    def __init__(self, executable, mode="headless"):
        self.process = subprocess.Popen(
            [executable, "--automation", "--transport", "stdio", "--" + mode],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, bufsize=1)
        self.next_id = 1
        self.ready = json.loads(self.process.stdout.readline())

    def request(self, command, params=None):
        request_id = self.next_id
        self.next_id += 1
        message = {"id": request_id, "version": 1, "command": command, "params": params or {}}
        self.process.stdin.write(json.dumps(message, separators=(",", ":")) + "\n")
        self.process.stdin.flush()
        while True:
            line = self.process.stdout.readline()
            if not line:
                raise RuntimeError("automation process exited: " + self.process.stderr.read())
            value = json.loads(line)
            if value.get("id") == request_id:
                return value

    def raw(self, line):
        self.process.stdin.write(line + "\n")
        self.process.stdin.flush()
        return json.loads(self.process.stdout.readline())

    def close(self):
        if self.process.poll() is None:
            self.request("quit")
            self.process.wait(timeout=2)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        self.close()
