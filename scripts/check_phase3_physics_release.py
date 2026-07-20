#!/usr/bin/env python3
import argparse
import json
import sys
import hashlib
import subprocess
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


def validate_production_default(root, executable):
    root = Path(root).resolve()
    if executable is None:
        return ["production executable is required for the release gate"]
    policy_path = root / (
        "physics_models/promotion/phase3_production_default.json")
    failures = []
    try:
        policy_text = policy_path.read_text(encoding="utf-8")
        policy = json.loads(policy_text)
        if policy_text != _canonical(policy):
            failures.append("production default policy is not canonical")
        if policy.get("schema_version") != 1 or policy.get("status") != \
                "NO_PROMOTED_PHASE3_CANDIDATE":
            failures.append("production default policy has an invalid status")
        expected_id = policy.get("authorized_profile_id")
        if expected_id != "chinese_pool_legacy_v1":
            failures.append("production default policy does not preserve the legacy baseline")
        for rejection in policy.get("phase3_rejections", []):
            path = root / rejection["path"]
            actual = hashlib.sha256(path.read_bytes()).hexdigest()
            if actual != rejection.get("sha256"):
                failures.append(
                    f"production default policy rejection hash changed: {rejection['path']}")
        if len(policy.get("phase3_rejections", [])) != 5:
            failures.append("production default policy must bind all five rejected candidates")
        completed = subprocess.run(
            [str(Path(executable).resolve()), "--print-physics-profile"],
            check=True, capture_output=True, text=True)
        actual_id = json.loads(completed.stdout).get("id")
        if actual_id != expected_id:
            failures.append(
                "production default profile is not authorized: "
                f"expected {expected_id}, got {actual_id}")
    except (OSError, UnicodeError, json.JSONDecodeError, KeyError,
            TypeError, ValueError, subprocess.SubprocessError) as error:
        failures.append(f"production default validation failed: {error}")
    return failures


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
            production_failures = validate_production_default(
                arguments.root, arguments.executable)
            failures = disposition_failures + production_failures
            if failures:
                for failure in failures:
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
