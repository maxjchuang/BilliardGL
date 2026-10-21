#!/usr/bin/env python3
"""Phase2 full-six v8 dispatcher with corrected capture-driver CLI binding."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from scripts import run_phase2_v10i_formal_full_six_capture_v7 as _impl

SCENE_IDS = _impl.SCENE_IDS
AUTH_SCHEMA = _impl.AUTH_SCHEMA
AUTH_STATUS = _impl.AUTH_STATUS
READY_SCHEMA = "phase2_v10i_formal_full_six_reentry_readiness_v8"
READY_STATUS = _impl.READY_STATUS
MARKER_SCHEMA = _impl.MARKER_SCHEMA
DispatchError = _impl.DispatchError
os = _impl.os
subprocess = _impl.subprocess
sha256 = _impl.sha256
marker_path = _impl.marker_path
consume_authorization = _impl.consume_authorization


def validate_pre_process(*args, **kwargs):
    previous = _impl.READY_SCHEMA
    _impl.READY_SCHEMA = READY_SCHEMA
    try:
        return _impl.validate_pre_process(*args, **kwargs)
    finally:
        _impl.READY_SCHEMA = previous


def capture_command(executable: Path, output: Path, asset_package: Path,
                    authorization_path: Path, readiness_sha: str) -> list[str]:
    command = _impl.capture_command(executable, output, asset_package,
                                    authorization_path, readiness_sha)
    if command.count("--formal-asset-package") != 1 or \
            "--asset-package" in command:
        raise DispatchError("delegated capture command asset option contract drift")
    command[command.index("--formal-asset-package")] = "--asset-package"
    return command


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--execute-authorized-capture", action="store_true")
    parser.add_argument("--readiness", type=Path, required=True)
    parser.add_argument("--authorization", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--asset-package", type=Path, required=True)
    arguments = parser.parse_args()
    if not arguments.execute_authorized_capture:
        print("FAIL: explicit --execute-authorized-capture required")
        return 2
    try:
        _, authorization = validate_pre_process(
            arguments.readiness, arguments.authorization, arguments.output,
            arguments.executable, arguments.asset_package)
        consume_authorization(arguments.authorization, authorization,
                              arguments.readiness, arguments.output)
        command = capture_command(
            arguments.executable, arguments.output, arguments.asset_package,
            arguments.authorization,
            sha256(arguments.readiness.resolve(strict=True)))
        return subprocess.run(command, cwd=ROOT, check=False).returncode
    except (OSError, ValueError, json.JSONDecodeError, DispatchError) as error:
        print("FAIL: " + str(error))
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
