#!/usr/bin/env python3

import argparse
import os
import subprocess
import sys
from pathlib import Path
from typing import Iterable


SUSPICIOUS_TOKENS = (
    ("\ufffd", "U+FFFD"),
    ("\u951f\u65a4\u62f7", "U+951F U+65A4 U+62F7"),
)


def tracked_paths(root: Path) -> list[str]:
    result = subprocess.run(
        ["git", "ls-files", "-z"],
        cwd=root,
        check=True,
        capture_output=True,
    )
    return [os.fsdecode(path) for path in result.stdout.split(b"\0") if path]


def find_violations(root: Path, paths: Iterable[str]) -> list[str]:
    violations = []
    for relative_path in paths:
        try:
            data = (root / relative_path).read_bytes()
        except OSError as error:
            violations.append(f"{relative_path}: unable to read: {error}")
            continue

        if b"\0" in data:
            continue

        try:
            text = data.decode("utf-8")
        except UnicodeDecodeError as error:
            violations.append(
                f"{relative_path}: invalid UTF-8 at byte {error.start}"
            )
            continue

        for token, label in SUSPICIOUS_TOKENS:
            if token in text:
                violations.append(
                    f"{relative_path}: contains suspicious token {label}"
                )

    return violations


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Check every Git-tracked text file for UTF-8 encoding errors."
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="repository root (defaults to the parent of this script directory)",
    )
    return parser.parse_args()


def main() -> int:
    root = parse_args().root.resolve()
    try:
        paths = tracked_paths(root)
    except (OSError, subprocess.CalledProcessError) as error:
        print(f"UTF-8 encoding check could not enumerate tracked files: {error}", file=sys.stderr)
        return 1

    violations = find_violations(root, paths)
    if violations:
        print("UTF-8 encoding check failed:", file=sys.stderr)
        for violation in violations:
            print(f"- {violation}", file=sys.stderr)
        return 1

    print(f"UTF-8 encoding check passed ({len(paths)} tracked files).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
