#!/usr/bin/env python3
import argparse
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from tools.physics_validation.holdout_access import validate_candidate_holdout_access


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Reject a candidate whose one-shot validation partition is spent")
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--freeze", type=Path, required=True)
    parser.add_argument("--package", type=Path, required=True)
    arguments = parser.parse_args(argv)
    failures = validate_candidate_holdout_access(
        arguments.root, arguments.freeze, arguments.package)
    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 1
    print("Candidate validation partition is registered and unspent")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
