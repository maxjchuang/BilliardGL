#!/usr/bin/env python3
"""Phase2 full-six v7 single-use dispatcher.

This is a versioned facade over the already-tested v6 fail-closed mechanism.
Only the readiness identity changes; authorization remains the exact frozen v3
six-scene schema.  Importing or running without the explicit execute flag never
starts a process or creates output.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

_ROOT = Path(__file__).resolve().parents[1]
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

from scripts import run_phase2_v10i_formal_full_six_capture_v6 as _impl

ROOT = _impl.ROOT
SCENE_IDS = _impl.SCENE_IDS
AUTH_SCHEMA = _impl.AUTH_SCHEMA
AUTH_STATUS = _impl.AUTH_STATUS
READY_SCHEMA = "phase2_v10i_formal_full_six_reentry_readiness_v7"
READY_STATUS = _impl.READY_STATUS
MARKER_SCHEMA = _impl.MARKER_SCHEMA
DispatchError = _impl.DispatchError
os = _impl.os
subprocess = _impl.subprocess
sha256 = _impl.sha256
load_object = _impl.load_object
marker_path = _impl.marker_path
consume_authorization = _impl.consume_authorization
capture_command = _impl.capture_command


def validate_pre_process(*args, **kwargs):
    """Validate v7 identity without mutating the reusable v6 module."""
    previous = _impl.READY_SCHEMA
    _impl.READY_SCHEMA = READY_SCHEMA
    try:
        return _impl.validate_pre_process(*args, **kwargs)
    finally:
        _impl.READY_SCHEMA = previous


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
