#!/usr/bin/env python3
import argparse
import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from tools.physics_validation.phase3_release_gate import validate_phase3_release


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Verify frozen Phase 3 physics release evidence")
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--release", type=Path)
    parser.add_argument("--executable", type=Path)
    arguments = parser.parse_args(argv)
    failures = validate_phase3_release(
        arguments.root, release_path=arguments.release,
        executable=arguments.executable)
    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 1
    release = arguments.release or (
        arguments.root / "physics_models/promotion/phase3_release_v1.json")
    status = json.loads(release.read_text(encoding="utf-8"))["status"]
    print(f"Phase 3 physics release gate: {status}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
