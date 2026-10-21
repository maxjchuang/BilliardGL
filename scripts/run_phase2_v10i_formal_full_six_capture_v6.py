#!/usr/bin/env python3
"""Fail-closed, single-use dispatcher for a future Phase2 full-six capture.

This module is safe to import and validate offline.  Capture is possible only
with the explicit execution flag and a separately issued exact v3 authorization.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
SCENE_IDS = (
    "break", "low_aim", "ball_closeup", "pocket_closeup",
    "tactical_overhead", "post_shot_observe",
)
AUTH_SCHEMA = "phase2_v10i_formal_six_scene_one_shot_authorization_v3"
AUTH_STATUS = "AUTHORIZED_FOR_EXACTLY_SIX_FRESH_PROCESS_CAPTURES"
READY_SCHEMA = "phase2_v10i_formal_full_six_reentry_readiness_v6"
READY_STATUS = "READY_FOR_EXACT_FULL_SIX_ONE_SHOT_AUTHORIZATION_NO_GPU"
MARKER_SCHEMA = "phase2_v10i_formal_full_six_authorization_consumed_v1"


class DispatchError(RuntimeError):
    pass


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_object(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise DispatchError("JSON object required; pre-process rejection")
    return value


def marker_path(authorization_path: Path) -> Path:
    path = authorization_path.resolve(strict=True)
    return path.with_name(path.name + ".consumed.json")


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise DispatchError(message + "; pre-process rejection")


def _validate_file_binding(value: Any, label: str) -> Path:
    _require(isinstance(value, dict), label + " binding missing")
    path_text = value.get("canonical_path")
    digest = value.get("sha256")
    _require(isinstance(path_text, str) and Path(path_text).is_absolute(),
             label + " canonical path invalid")
    path = Path(path_text)
    _require(path.is_file() and path.resolve(strict=True) == path,
             label + " bound file missing/noncanonical")
    _require(isinstance(digest, str) and len(digest) == 64 and
             sha256(path) == digest, label + " SHA drift")
    return path


def validate_pre_process(readiness_path: Path, authorization_path: Path,
                         output: Path, executable: Path,
                         asset_package: Path) -> tuple[dict[str, Any], dict[str, Any]]:
    """Validate all inputs without creating output, marker, or subprocess."""
    ready_path = readiness_path.resolve(strict=True)
    auth_path = authorization_path.resolve(strict=True)
    exe_path = executable.resolve(strict=True)
    package_path = asset_package.resolve(strict=True)
    output_path = output.resolve()
    readiness = load_object(ready_path)
    authorization = load_object(auth_path)

    _require(readiness.get("schema") == READY_SCHEMA and
             readiness.get("status") == READY_STATUS,
             "readiness identity/status mismatch")
    _require(readiness.get("implementation_ready") is True,
             "readiness is not implementation-ready")
    for field in ("actual_gpu_used", "gpu_capture_authorized",
                  "capture_output_created", "formal_six_scene_evidence",
                  "formal_pass", "ta_review_performed", "golden_promotion",
                  "m2_promotion", "product_review_performed"):
        _require(readiness.get(field) is False,
                 "readiness evidence boundary mismatch: " + field)
    _require(readiness.get("approved_scenes") == list(SCENE_IDS),
             "readiness scene order mismatch")
    _require(readiness.get("fresh_release_executable", {}).get("canonical_path") ==
             str(exe_path) and
             readiness["fresh_release_executable"].get("sha256") == sha256(exe_path),
             "Release executable binding mismatch")
    package_manifest = package_path / "formal-asset-package-v2.json"
    _require(package_manifest.is_file(), "formal asset package manifest missing")
    _require(readiness.get("self_contained_asset_package", {}).get("asset_root") ==
             str(package_path) and
             readiness["self_contained_asset_package"].get("sha256") ==
             sha256(package_manifest), "formal asset package binding mismatch")
    package_value = load_object(package_manifest)
    _require(package_value.get("schema") ==
             "phase2_v10i_self_contained_formal_asset_package_v2" and
             package_value.get("self_contained") is True and
             package_value.get("immutable") is True and
             package_value.get("symlinks_allowed") is False and
             package_value.get("required_asset_count") == 6 and
             package_value.get("runtime_dependency_count") == 18 and
             len(package_value.get("files", [])) == 24,
             "formal asset package contract mismatch")
    for item in package_value["files"]:
        _require(isinstance(item, dict) and isinstance(item.get("filename"), str),
                 "formal asset package file record invalid")
        item_path = package_path / item["filename"]
        _require(item_path.is_file() and not item_path.is_symlink() and
                 item_path.resolve(strict=True).parent == package_path and
                 sha256(item_path) == item.get("source_sha256") ==
                 item.get("packaged_sha256"),
                 "formal asset package file SHA drift: " + item["filename"])
    sources = readiness.get("source_bindings")
    generated = readiness.get("generated_bindings")
    _require(isinstance(sources, dict) and bool(sources),
             "readiness source bindings missing")
    _require(isinstance(generated, dict) and bool(generated),
             "readiness generated bindings missing")
    for name, value in sources.items():
        _validate_file_binding(value, "readiness source " + name)
    for name, value in generated.items():
        _validate_file_binding(value, "readiness generated " + name)
    for field in ("reentry_gate", "targeted_machine_prerequisite",
                  "targeted_ta_prerequisite", "full_six_ta_template"):
        _validate_file_binding(readiness.get(field), "readiness " + field)

    _require(authorization.get("schema") == AUTH_SCHEMA and
             authorization.get("status") == AUTH_STATUS,
             "authorization identity/status mismatch")
    for field in ("authorization_id", "authorization_nonce", "authorized_by",
                  "authorized_at_utc"):
        _require(isinstance(authorization.get(field), str) and
                 bool(authorization[field].strip()),
                 "authorization identity/time incomplete: " + field)
    _require(authorization.get("gpu_capture_authorized") is True and
             authorization.get("formal_six_scene_evidence_authorized") is True,
             "authorization scope flags mismatch")
    _require(authorization.get("authorized_capture_count") == 6 and
             authorization.get("consumed_capture_count") == 0 and
             authorization.get("scene_ids_in_order") == list(SCENE_IDS),
             "authorization count/order mismatch")
    _require(authorization.get("required_readiness_manifest_sha256") ==
             sha256(ready_path), "authorization readiness SHA mismatch")
    _require(authorization.get("authorized_output_canonical_path") ==
             str(output_path) and output_path.is_absolute(),
             "authorization output path mismatch")
    for field in ("formal_pass", "golden_promotion", "m2_promotion"):
        _require(authorization.get(field) is False,
                 "authorization pre-promotes evidence: " + field)
    _require(not output_path.exists(), "capture output must be absent")
    _require(not marker_path(auth_path).exists(), "authorization already consumed")
    return readiness, authorization


def consume_authorization(authorization_path: Path,
                          authorization: dict[str, Any],
                          readiness_path: Path, output: Path) -> dict[str, Any]:
    """Atomically and durably consume authorization before output/process."""
    auth_path = authorization_path.resolve(strict=True)
    ready_path = readiness_path.resolve(strict=True)
    marker = marker_path(auth_path)
    payload = {
        "schema": MARKER_SCHEMA,
        "status": "CONSUMED_BEFORE_OUTPUT_OR_PROCESS_START",
        "authorization_path": str(auth_path),
        "authorization_sha256": sha256(auth_path),
        "authorization_id": authorization["authorization_id"],
        "authorization_nonce": authorization["authorization_nonce"],
        "authorized_output_canonical_path": str(output.resolve()),
        "readiness_manifest_path": str(ready_path),
        "required_readiness_manifest_sha256": sha256(ready_path),
        "scene_ids_in_order": list(SCENE_IDS),
        "authorized_capture_count": 6,
        "consumed_capture_count": 6,
        "formal_pass": False,
        "golden_promotion": False,
        "m2_promotion": False,
    }
    try:
        descriptor = os.open(marker, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(descriptor, "w", encoding="utf-8") as destination:
            destination.write(json.dumps(payload, indent=2, sort_keys=True) + "\n")
            destination.flush()
            os.fsync(destination.fileno())
    except FileExistsError as error:
        raise DispatchError(
            "authorization already consumed; pre-process rejection") from error
    # Any write/fsync failure intentionally leaves an incomplete marker.  It is
    # permanent evidence of consumption and is never removed or reused.
    return payload


def capture_command(executable: Path, output: Path, asset_package: Path,
                    authorization_path: Path, readiness_sha: str) -> list[str]:
    return [
        sys.executable, str(ROOT / "scripts/capture_visual_baselines.py"),
        "--formal-v10i", "--executable", str(executable.resolve(strict=True)),
        "--output", str(output.resolve()), "--formal-asset-package",
        str(asset_package.resolve(strict=True)), "--authorization",
        str(authorization_path.resolve(strict=True)),
        "--readiness-manifest-sha256", readiness_sha,
        "--width", "1920", "--height", "1080", "--warmup-frames", "120",
        "--sample-frames", "300",
    ]


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
            arguments.authorization, sha256(arguments.readiness.resolve(strict=True)))
        return subprocess.run(command, cwd=ROOT, check=False).returncode
    except (OSError, ValueError, json.JSONDecodeError, DispatchError) as error:
        print("FAIL: " + str(error))
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
