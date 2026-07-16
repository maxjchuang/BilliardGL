#!/usr/bin/env python3
import argparse
import json
import sys
import hashlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from tools.physics_validation.phase3_release_gate import validate_phase3_release
from tools.physics_validation.phase3_v5_assessment import (
    build_final_assessment,
    build_rejection_document,
)


def _canonical(document):
    return json.dumps(
        document, ensure_ascii=False, indent=2, sort_keys=True,
        allow_nan=False) + "\n"


def validate_phase3_v5_disposition(root):
    root = Path(root).resolve()
    assessment_path = root / (
        "physics_models/candidates/phase3_integrated_v5/"
        "final_assessment.json")
    rejection_path = root / (
        "physics_models/promotion/phase3_integrated_v5_rejection.json")
    if not assessment_path.exists() and not rejection_path.exists():
        return None
    failures = []
    if not assessment_path.is_file() or not rejection_path.is_file():
        return ["v5 final assessment and rejection must both exist"]
    try:
        assessment = build_final_assessment(root)
        committed = json.loads(assessment_path.read_text(encoding="utf-8"))
        if committed != assessment or \
                assessment_path.read_text(encoding="utf-8") != _canonical(assessment):
            failures.append("v5 final assessment is not canonical")
        if assessment.get("disposition") != "REJECTED" or \
                assessment.get("han_2005") != "NOT_EXECUTED":
            failures.append("v5 disposition is not a safe rejection")
        expected_rejection = build_rejection_document(
            root, assessment_path, assessment)
        committed_rejection = json.loads(
            rejection_path.read_text(encoding="utf-8"))
        if committed_rejection != expected_rejection or \
                rejection_path.read_text(encoding="utf-8") != \
                _canonical(expected_rejection):
            failures.append("v5 rejection evidence is not canonical")
        expected_hash = hashlib.sha256(
            assessment_path.read_bytes()).hexdigest()
        if committed_rejection.get("assessment_sha256") != expected_hash:
            failures.append("v5 rejection does not bind the assessment")
    except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as error:
        failures.append(f"v5 rejection validation failed: {error}")
    return failures


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Verify frozen Phase 3 physics release evidence")
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--release", type=Path)
    parser.add_argument("--executable", type=Path)
    arguments = parser.parse_args(argv)
    if arguments.release is None:
        disposition_failures = validate_phase3_v5_disposition(arguments.root)
        if disposition_failures is not None:
            if disposition_failures:
                for failure in disposition_failures:
                    print(f"FAIL: {failure}", file=sys.stderr)
                return 1
            print("Phase 3 physics candidate disposition: REJECTED (not promoted)")
            return 0
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
