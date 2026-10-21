#!/usr/bin/env python3
"""Read-only validator for the exact Phase2 break/low_aim targeted capture."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts import capture_visual_baselines as capture
from scripts.validate_phase2_production_visuals import validate_semantic_camera


SCENE_IDS = capture.FORMAL_TARGETED_SCENE_IDS
REGIONS = {
    "break": ("table", 0.10, 0.16, 0.80, 0.68),
    "low_aim": ("cue_and_ball", 0.18, 0.36, 0.64, 0.42),
}


class TargetedCaptureError(RuntimeError):
    pass


def require(value: bool, message: str) -> None:
    if not value:
        raise TargetedCaptureError(message)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(value, dict), f"{path}: object required")
    return value


def constrained_file(root: Path, name: Any, expected_name: str) -> Path:
    require(name == expected_name, "targeted capture filename differs")
    path = (root / name).resolve(strict=True)
    require(path.parent == root and path.is_file() and not path.is_symlink(),
            "targeted capture path escapes output root")
    return path


def validate(
        manifest_path: Path, expected_executable: Path,
        expected_readiness_sha256: str) -> dict[str, Any]:
    manifest_path = manifest_path.resolve(strict=True)
    root = manifest_path.parent
    value = load(manifest_path)
    require(value.get("schema") ==
            "phase2_v10i_formal_targeted_camera_capture_v1" and
            value.get("registry_version") == 2 and
            value.get("renderer") == "modern" and
            value.get("production_gltf") is True and
            value.get("quality") == "medium" and
            value.get("resolution") == {"width": 1920, "height": 1080},
            "targeted capture identity")
    scope = value.get("capture_scope", {})
    require(scope == {
        "kind": "targeted_camera_break_low_aim",
        "scene_ids_in_order": list(SCENE_IDS),
        "capture_count": 2,
        "full_six_scene_evidence": False,
    }, "targeted capture scope")
    expected_executable = expected_executable.resolve(strict=True)
    require(value.get("executable") == str(expected_executable) and
            value.get("executable_sha256") ==
                "sha256:" + sha256(expected_executable),
            "targeted executable binding")

    consumption_binding = value.get("targeted_authorization_consumption", {})
    marker_path = Path(consumption_binding.get("canonical_path", ""))
    require(marker_path.is_absolute() and marker_path.is_file() and
            marker_path.resolve(strict=True) == marker_path and
            consumption_binding.get("sha256") == sha256(marker_path),
            "targeted consumed marker binding")
    marker = load(marker_path)
    require(marker.get("schema") ==
            "phase2_v10i_formal_targeted_camera_authorization_consumed_v1" and
            marker.get("status") == "CONSUMED_BEFORE_PROCESS_START" and
            marker.get("authorization_id") ==
                consumption_binding.get("authorization_id") and
            marker.get("authorization_nonce") ==
                consumption_binding.get("authorization_nonce") and
            marker.get("authorization_sha256") ==
                consumption_binding.get("authorization_sha256") and
            marker.get("required_readiness_manifest_sha256") ==
                expected_readiness_sha256 and
            marker.get("authorized_output_canonical_path") == str(root) and
            marker.get("scene_ids_in_order") == list(SCENE_IDS) and
            marker.get("authorized_capture_count") == 2 and
            marker.get("consumed_capture_count") == 2 and
            marker.get("formal_six_scene_evidence") is False and
            marker.get("formal_pass") is False,
            "targeted consumed marker contract")
    authorization_path = Path(marker.get("authorization_path", ""))
    require(authorization_path.is_file() and
            sha256(authorization_path) == marker["authorization_sha256"],
            "targeted authorization source binding")

    asset_root = (root / value.get("asset_root", "")).resolve(strict=True)
    require(value.get("asset_root") == "asset_root" and
            asset_root.parent == root and asset_root.is_dir(),
            "targeted asset root")
    for asset_id, filename in capture.FORMAL_ASSET_FILES.items():
        path = constrained_file(asset_root, filename, filename)
        field = capture.FORMAL_MANIFEST_HASH_FIELDS[asset_id]
        require(value.get(field) == sha256(path),
                "targeted asset SHA " + asset_id)

    scenes = value.get("scenes")
    require(isinstance(scenes, list) and
            [item.get("scene_id") for item in scenes] == list(SCENE_IDS),
            "targeted scene order/count")
    total_visible = 0
    for item in scenes:
        scene_id = item["scene_id"]
        image = constrained_file(root, item.get("file"), scene_id + ".ppm")
        width, height, pixels = capture.read_ppm(image)
        require((width, height) == (1920, 1080) and
                item.get("ppm_sha256") == "sha256:" + sha256(image),
                scene_id + ": targeted PPM binding")
        visible = capture._visible_pixels(pixels)
        region_visible = capture._visible_pixels_in_region(
            pixels, width, height, REGIONS[scene_id])
        require(visible > 0 and item.get("visible_pixels") == visible and
                item.get("interest_regions", {}).get(
                    REGIONS[scene_id][0], {}).get("visible_pixels") ==
                    region_visible and region_visible > 0,
                scene_id + ": visible/ROI evidence")
        total_visible += visible
        try:
            validate_semantic_camera(scene_id, item.get("semantic_camera"))
        except Exception as error:
            raise TargetedCaptureError(
                scene_id + ": semantic camera evidence") from error
        performance = item.get("performance", {})
        require(performance.get("warmup_frames", 0) >= 120 and
                performance.get("sample_frames", 0) >= 300 and
                performance.get("measurement_method") ==
                    "gpu_timer_query_plus_cpu_frame" and
                performance.get("gpu_timer_queries_observed") is True,
                scene_id + ": performance evidence")
        attestation = item.get("loaded_asset_attestation", {})
        require(attestation.get("schema") ==
                    "billiardgl_formal_asset_attestation_v1" and
                attestation.get("asset_root") == str(asset_root) and
                attestation.get("load_state") == "loaded" and
                attestation.get("silent_material_fallback_allowed") is False and
                attestation.get("load_errors") == [],
                scene_id + ": loaded asset attestation")
    performance = value.get("performance", {})
    require(performance.get("warmup_frames", 0) >= 120 and
            performance.get("sample_frames", 0) >= 300 and
            performance.get("measurement_method") ==
                "gpu_timer_query_plus_cpu_frame" and
            performance.get("gpu_timer_queries_observed") is True,
            "targeted aggregate performance")
    return {
        "status": "PASS_TARGETED_BREAK_LOW_AIM_MACHINE_CAPTURE",
        "scene_count": 2,
        "total_visible_pixels": total_visible,
        "manifest_sha256": sha256(manifest_path),
        "executable_sha256": sha256(expected_executable),
        "readiness_manifest_sha256": expected_readiness_sha256,
        "formal_six_scene_evidence": False,
        "formal_pass": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--expected-executable", type=Path, required=True)
    parser.add_argument("--expected-readiness-sha256", required=True)
    arguments = parser.parse_args()
    try:
        print(json.dumps(validate(
            arguments.manifest, arguments.expected_executable,
            arguments.expected_readiness_sha256), indent=2, sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError,
            TargetedCaptureError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
