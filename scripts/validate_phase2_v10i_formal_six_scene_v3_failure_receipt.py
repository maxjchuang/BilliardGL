#!/usr/bin/env python3
"""Read-only CPU validator for the frozen Phase2 v3 failed GPU attempt."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_RECEIPT = ROOT / "docs/evidence/phase2-v10i-formal-six-scene-v3-failure-receipt-v1.json"
EXPECTED_RECEIPT_SHA256 = "818ceb4be65f4ac192676a383ca2892585b4d1fee94bbb3e7471b3fd2da9291d"
EXPECTED_CONSUMED_MARKER_SHA256 = "2a6ef8c3374263c93c2e5c15027c4917daaaa39135b95fbbd0710b36d2966c5f"
EXPECTED_V3_READINESS_SHA256 = "ed8a176769933b8d7c5b8cff1430c02b4bc9deb8b4dd1838e37822e40ee0de00"
EXPECTED_SCENE_RESULTS = (
    ("break", "PASS_CAPTURE_AND_REGION_CHECK"),
    ("low_aim", "FAIL_CAPTURE_REGION_CHECK"),
    ("ball_closeup", "NOT_RUN"),
    ("pocket_closeup", "NOT_RUN"),
    ("tactical_overhead", "NOT_RUN"),
    ("post_shot_observe", "NOT_RUN"),
)


class FailureReceiptError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise FailureReceiptError(message)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(value, dict), f"{path}: JSON object required")
    return value


def bound_path(relative: Any, digest: Any, label: str) -> Path:
    require(isinstance(relative, str), label + ": path missing")
    require(isinstance(digest, str) and len(digest) == 64,
            label + ": SHA missing")
    path = (ROOT / relative).resolve(strict=True)
    require(path.is_file(), label + ": file missing")
    require(sha256(path) == digest, label + ": SHA mismatch")
    return path


def read_ppm(path: Path) -> tuple[int, int, bytes]:
    with path.open("rb") as image:
        require(image.readline().strip() == b"P6", f"{path}: P6 required")
        width, height = map(int, image.readline().split())
        require(image.readline().strip() == b"255", f"{path}: RGB8 required")
        pixels = image.read()
    require(len(pixels) == width * height * 3, f"{path}: pixel length")
    return width, height, pixels


def pixel_diagnostics(pixels: bytes, width: int, height: int,
                      bounds: tuple[int, int, int, int] | None = None
                      ) -> tuple[int, int, list[int] | None]:
    x0, y0, x1, y1 = bounds or (0, 0, width, height)
    visible = 0
    maximum = 0
    min_x, min_y, max_x, max_y = width, height, -1, -1
    for y in range(y0, y1):
        for x in range(x0, x1):
            offset = (y * width + x) * 3
            channel = max(pixels[offset:offset + 3])
            maximum = max(maximum, channel)
            if channel > 8:
                visible += 1
                min_x, min_y = min(min_x, x), min(min_y, y)
                max_x, max_y = max(max_x, x), max(max_y, y)
    bbox = None if visible == 0 else [min_x, min_y, max_x, max_y]
    return visible, maximum, bbox


def validate_receipt(path: Path = DEFAULT_RECEIPT,
                     enforce_frozen_sha: bool = True) -> dict[str, Any]:
    path = path.resolve(strict=True)
    if enforce_frozen_sha:
        require(sha256(path) == EXPECTED_RECEIPT_SHA256,
                "frozen failure receipt SHA mismatch")
    value = load(path)
    require(value.get("schema") ==
            "phase2_v10i_formal_six_scene_v3_failure_receipt_v1",
            "failure receipt schema")
    require(value.get("status") == "FORMAL_SIX_SCENE_FAIL_FROZEN_NO_RERUN",
            "failure receipt status")

    authorization = value.get("authorization", {})
    authorization_path = bound_path(
        authorization.get("authorization_path"),
        authorization.get("authorization_sha256"), "authorization")
    marker_path = bound_path(
        authorization.get("consumed_marker_path"),
        authorization.get("consumed_marker_sha256"), "consumed marker")
    require(authorization.get("consumed_marker_sha256") ==
            EXPECTED_CONSUMED_MARKER_SHA256,
            "consumed marker frozen SHA")
    authorized = load(authorization_path)
    marker = load(marker_path)
    require(marker.get("schema") ==
            "phase2_v10i_formal_six_scene_authorization_consumed_v1" and
            marker.get("status") == "CONSUMED_ON_FAIL_CLOSED_NO_RERUN" and
            marker.get("consumed_for_any_additional_capture") is True and
            marker.get("consumed_capture_count") == 2 and
            marker.get("failure_scene_id") == "low_aim" and
            marker.get("authorization_sha256") == sha256(authorization_path) and
            marker.get("authorization_id") == authorized.get("authorization_id"),
            "consumed marker semantics")
    require(authorized.get("required_readiness_manifest_sha256") ==
            EXPECTED_V3_READINESS_SHA256 and
            authorized.get("authorized_capture_count") == 6 and
            authorized.get("consumed_capture_count") == 0,
            "issued authorization lineage/count")

    counts = value.get("counts", {})
    require(counts == {
        "actual_gpu_ppm_files": 2, "failed_scenes": 1,
        "passed_scenes": 1, "planned_scenes": 6, "unrun_scenes": 4},
        "failure counts")
    boundary = value.get("evidence_boundary", {})
    require(boundary == {
        "formal_pass": False, "golden_promotion": False,
        "m2_promotion": False, "product_review_performed": False,
        "technical_art_review_performed": False},
        "evidence boundary must remain false")

    bindings = value.get("frozen_bindings", {})
    require(bindings.get("readiness_manifest_sha256") ==
            EXPECTED_V3_READINESS_SHA256,
            "v3 readiness receipt binding")
    readiness = ROOT / "docs/art/phase2-v10i-formal-six-scene-ready-v3/readiness-manifest-v3.json"
    require(readiness.is_file() and sha256(readiness) == EXPECTED_V3_READINESS_SHA256,
            "v3 readiness bytes")
    readiness_value = load(readiness)
    source_bindings = readiness_value.get("source_bindings", {})
    require(source_bindings.get("scripts/capture_visual_baselines.py", {}).get("sha256") ==
            bindings.get("capture_runner_sha256") and
            source_bindings.get("tests/visual/visual_scene_registry.py", {}).get("sha256") ==
            bindings.get("scene_registry_sha256"),
            "v3 readiness source lineage")
    fixed_paths = {
        "release_executable_sha256":
            ROOT / "build-phase2-v10i-formal-ready-v2/Billiards",
        "asset_package_manifest_sha256":
            ROOT / "assets/modern/phase2-v10i-formal/formal-asset-package-v2.json",
    }
    for field, fixed_path in fixed_paths.items():
        require(fixed_path.is_file() and sha256(fixed_path) == bindings.get(field),
                "frozen binding mismatch: " + field)
    break_path = bound_path(bindings.get("break_ppm_path"),
                            bindings.get("break_ppm_sha256"), "break PPM")
    low_path = bound_path(bindings.get("low_aim_ppm_path"),
                          bindings.get("low_aim_ppm_sha256"), "low_aim PPM")
    for ppm in (break_path, low_path):
        width, height, _ = read_ppm(ppm)
        require((width, height) == (1920, 1080), f"{ppm}: dimensions")
    capture_root = low_path.parent
    require(break_path.parent == capture_root and
            not (capture_root / "manifest.json").exists(),
            "failed run must not have a formal manifest")
    require(len(tuple(capture_root.glob("*.ppm"))) == 2,
            "failed run PPM count")

    failure = value.get("failure", {})
    require(failure.get("exit_code") == 2 and
            failure.get("failed_scene_id") == "low_aim" and
            failure.get("error") ==
                "low_aim has no visible pixels in region cue_and_ball" and
            failure.get("manifest_created") is False,
            "failure identity")
    width, height, pixels = read_ppm(low_path)
    visible, _, bbox = pixel_diagnostics(pixels, width, height)
    bounds = tuple(failure.get("interest_region_pixel_bounds", ()))
    require(bounds == (345, 388, 1574, 842), "interest region bounds")
    region_visible, region_max, _ = pixel_diagnostics(
        pixels, width, height, bounds)
    require(visible == failure.get("overall_visible_pixels") == 3450 and
            bbox == failure.get("overall_visible_pixel_bbox") ==
                [240, 1029, 469, 1043] and
            region_visible == failure.get("interest_region_visible_pixels") == 0 and
            region_max == failure.get("interest_region_max_channel") == 2,
            "low_aim pixel diagnostics")
    require(failure.get("interest_region_normalized") ==
            [0.18, 0.36, 0.64, 0.42],
            "interest region normalized contract")
    scene_results = value.get("scene_results_in_order")
    require(isinstance(scene_results, list) and
            tuple((item.get("scene_id"), item.get("status"))
                  for item in scene_results) == EXPECTED_SCENE_RESULTS,
            "scene result order/status")
    return {
        "status": "PASS_FROZEN_V3_FAILURE_RECEIPT",
        "failure_receipt_sha256": sha256(path),
        "consumed_marker_sha256": sha256(marker_path),
        "actual_gpu_ppm_files": 2,
        "failed_scene_id": "low_aim",
        "unrun_scene_count": 4,
        "formal_pass": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--receipt", type=Path, default=DEFAULT_RECEIPT)
    arguments = parser.parse_args()
    try:
        print(json.dumps(validate_receipt(arguments.receipt), indent=2,
                         sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError,
            FailureReceiptError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
