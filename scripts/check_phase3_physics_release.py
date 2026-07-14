#!/usr/bin/env python3
import argparse
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from tools.physics_validation.phase3_release_gate import validate_phase3_release


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Verify frozen Phase 3 physics release evidence")
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--executable", type=Path)
    arguments = parser.parse_args(argv)
    failures = validate_phase3_release(
        arguments.root, executable=arguments.executable)
    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 1
    print("Phase 3 physics release gate: PASSED_WITH_DECLARED_LIMITATIONS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
