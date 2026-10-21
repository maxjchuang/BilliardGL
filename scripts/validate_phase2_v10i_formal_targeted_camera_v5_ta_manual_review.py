#!/usr/bin/env python3
"""Validate the independent Phase2 v5 targeted-camera TA manual review.

The validator is read-only and CPU-only.  It rebinds the frozen readiness,
capture manifest, v4 TA gate, PPM bytes, camera state and visible-pixel facts;
it does not run the candidate or create a graphics context.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_REVIEW = ROOT / (
    "docs/evidence/phase2-v10i-formal-targeted-camera-v5-"
    "independent-ta-manual-review-v1.json"
)
EXPECTED_BINDINGS = {
    "v5_readiness_manifest": (
        "docs/art/phase2-v10i-formal-targeted-camera-ready-v5/"
        "readiness-manifest-v5.json",
        "687fce04e0521ab26465b77ba35434353d6b210bfa703f3b9564633c9a28e01f",
    ),
    "v5_targeted_capture_manifest": (
        "docs/evidence/phase2-v10i-formal-targeted-camera-v5-run-1/"
        "manifest.json",
        "807e2371c5e3bfd245cf67dc4209f4ddd4e4fab0c21c273218cf131e49aa85d8",
    ),
    "v4_ta_camera_correction_design_gate": (
        "docs/art/phase2-v10i-formal-six-scene-v4-camera-"
        "correction-design-gate-v1.json",
        "5ab090f57ff49b40bfbe12f13f568614fc9a4a193e8b123c93f3f9f9d75d9979",
    ),
    "break_ppm": (
        "docs/evidence/phase2-v10i-formal-targeted-camera-v5-run-1/break.ppm",
        "93fba7595b291b64500091f0e0a3c69d644caf827499df0c88ed8cf2127afc60",
    ),
    "low_aim_ppm": (
        "docs/evidence/phase2-v10i-formal-targeted-camera-v5-run-1/low_aim.ppm",
        "c8beb0ab0a4724c1ef79a7aae52f4fd7621a478138aef702c1248c7e6fc8bade",
    ),
}
SCENE_IDS = ["break", "low_aim"]
REGIONS = {
    "break": ("table", 0.10, 0.16, 0.80, 0.68),
    "low_aim": ("cue_and_ball", 0.18, 0.36, 0.64, 0.42),
}
EXPECTED_VISIBLE = {
    "break": (2065987, 1121348),
    "low_aim": (1996633, 555841),
}
EXPECTED_CAMERAS = {
    "break": {
        "state": "observe_free", "preset": "elevated",
        "transitioning": False, "distance_cm": 320, "fov_degrees": 48,
        "eye_cm": {"x": 0.000022702341084368527,
                   "y": 185.930419921875, "z": -300.7016296386719},
        "target_cm": {"x": 0, "y": 76.48397827148438, "z": 0},
    },
    "low_aim": {
        "state": "aim_player", "preset": "player_low",
        "transitioning": False, "distance_cm": 145, "fov_degrees": 42,
        "eye_cm": {"x": 0.000010286998985975515,
                   "y": 138.9344024658203, "z": -199.75543212890625},
        "target_cm": {"x": 0, "y": 89.34147644042969, "z": -63.5},
    },
}
AXES = [
    "scene_composition_matches_purpose",
    "ball_readability",
    "cue_and_aim_axis_readability",
    "rail_and_pocket_readability",
    "lighting_and_material_readability",
    "lamp_occlusion_clearance",
    "roi_and_black_frame_integrity",
]
CASE_AXES = {
    "break": [axis for axis in AXES
              if axis != "cue_and_aim_axis_readability"],
    "low_aim": list(AXES),
}
EXPECTED_BOUNDARY = {
    "targeted_break_low_aim_ta_review_performed": True,
    "targeted_break_low_aim_ta_pass": True,
    "review_gpu_executed": False,
    "production_modified_by_review": False,
    "capture_authorized_by_review": False,
    "capture_performed_by_review": False,
    "full_six_scene_evidence": False,
    "full_six_scene_formal_pass": False,
    "m2_promotion": False,
    "golden_promotion": False,
    "product_review_performed": False,
    "product_pass": False,
    "beta_pass": False,
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def load_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(value, dict), f"{path}: object required")
    return value


def read_ppm(path: Path) -> tuple[int, int, bytes]:
    with path.open("rb") as image:
        require(image.readline().strip() == b"P6", f"{path}: P6 required")
        width, height = map(int, image.readline().split())
        require(image.readline().strip() == b"255", f"{path}: RGB8 required")
        pixels = image.read()
    require(len(pixels) == width * height * 3, f"{path}: pixel count")
    return width, height, pixels


def diagnostics(
    pixels: bytes, width: int, height: int,
    region: tuple[str, float, float, float, float],
) -> tuple[int, list[int] | None, int]:
    visible = 0
    min_x, min_y, max_x, max_y = width, height, -1, -1
    for y in range(height):
        for x in range(width):
            offset = (y * width + x) * 3
            if max(pixels[offset:offset + 3]) > 8:
                visible += 1
                min_x, min_y = min(min_x, x), min(min_y, y)
                max_x, max_y = max(max_x, x), max(max_y, y)
    bbox = None if visible == 0 else [min_x, min_y, max_x, max_y]
    _, rx, ry, rw, rh = region
    x0, y0 = int(rx * width), int(ry * height)
    x1, y1 = int((rx + rw) * width), int((ry + rh) * height)
    region_visible = 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            offset = (y * width + x) * 3
            if max(pixels[offset:offset + 3]) > 8:
                region_visible += 1
    return visible, bbox, region_visible


def validate_bindings(review: dict[str, Any]) -> dict[str, dict[str, Any]]:
    bindings = review.get("frozen_bindings", {})
    require(set(bindings) == set(EXPECTED_BINDINGS),
            "frozen binding role inventory")
    loaded: dict[str, dict[str, Any]] = {}
    for role, (relative, digest) in EXPECTED_BINDINGS.items():
        item = bindings.get(role, {})
        require(item.get("path") == relative and item.get("sha256") == digest,
                f"{role}: review binding drift")
        path = (ROOT / relative).resolve(strict=True)
        require(path.is_file() and sha256(path) == digest,
                f"{role}: frozen bytes drift")
        if role.endswith("_ppm"):
            width, height, _ = read_ppm(path)
            require(item.get("dimensions") == [width, height] == [1920, 1080],
                    f"{role}: dimensions")
        else:
            loaded[role] = load_json(path)
    return loaded


def validate_manifest_and_pixels(
    review: dict[str, Any], loaded: dict[str, dict[str, Any]],
) -> None:
    readiness = loaded["v5_readiness_manifest"]
    manifest = loaded["v5_targeted_capture_manifest"]
    gate = loaded["v4_ta_camera_correction_design_gate"]
    require(readiness.get("schema") ==
            "phase2_v10i_formal_targeted_camera_readiness_v5" and
            readiness.get("camera_design_gate_lineage", {}).get("sha256") ==
            EXPECTED_BINDINGS["v4_ta_camera_correction_design_gate"][1] and
            readiness.get("targeted_scene_ids_in_order") == SCENE_IDS and
            readiness.get("formal_pass") is False and
            readiness.get("golden_promotion") is False and
            readiness.get("m2_promotion") is False and
            readiness.get("product_review_performed") is False,
            "v5 readiness scope/lineage")
    require(gate.get("schema") ==
            "phase2_v10i_formal_six_scene_v4_camera_correction_design_gate_v1",
            "v4 TA gate identity")
    require(manifest.get("schema") ==
            "phase2_v10i_formal_targeted_camera_capture_v1" and
            manifest.get("registry_version") == 2 and
            manifest.get("renderer") == "modern" and
            manifest.get("quality") == "medium" and
            manifest.get("resolution") == {"width": 1920, "height": 1080}
            and manifest.get("capture_scope") == {
                "kind": "targeted_camera_break_low_aim",
                "scene_ids_in_order": SCENE_IDS,
                "capture_count": 2,
                "full_six_scene_evidence": False,
            }, "targeted manifest identity/scope")
    scenes = manifest.get("scenes", [])
    require([scene.get("scene_id") for scene in scenes] == SCENE_IDS,
            "targeted manifest scene order")
    manual_facts = review.get("machine_facts_reverified_read_only", {})
    require(manual_facts.get("manifest_schema") == manifest["schema"] and
            manual_facts.get("registry_version") == 2 and
            manual_facts.get("renderer") == "modern" and
            manual_facts.get("quality") == "medium" and
            manual_facts.get("resolution") == [1920, 1080] and
            manual_facts.get("capture_count") == 2 and
            manual_facts.get("full_six_scene_evidence") is False and
            manual_facts.get("visible_pixel_channel_rule") ==
            "max_rgb_channel_gt_8",
            "manual machine-fact header")

    for scene in scenes:
        scene_id = scene["scene_id"]
        path = ROOT / EXPECTED_BINDINGS[scene_id + "_ppm"][0]
        width, height, pixels = read_ppm(path)
        visible, bbox, region_visible = diagnostics(
            pixels, width, height, REGIONS[scene_id])
        expected_visible, expected_region_visible = EXPECTED_VISIBLE[scene_id]
        require(visible == expected_visible and region_visible ==
                expected_region_visible and bbox == [0, 0, 1919, 1079],
                f"{scene_id}: independent pixel diagnostics")
        require(scene.get("ppm_sha256") == "sha256:" + sha256(path) and
                scene.get("image_sha256") == "sha256:" + sha256_bytes(pixels)
                and scene.get("visible_pixels") == visible and
                scene.get("interest_regions", {}).get(
                    REGIONS[scene_id][0], {}).get("visible_pixels") ==
                region_visible,
                f"{scene_id}: manifest pixel facts")
        require(scene.get("semantic_camera") == EXPECTED_CAMERAS[scene_id],
                f"{scene_id}: state/preset/transition camera binding")
        facts = manual_facts.get("scenes", {}).get(scene_id, {})
        require(facts == {
            "visible_pixels": visible,
            "visible_pixel_bbox": bbox,
            "interest_region": {
                "name": REGIONS[scene_id][0],
                "rect_normalized": list(REGIONS[scene_id][1:]),
                "visible_pixels": region_visible,
            },
            "semantic_camera": EXPECTED_CAMERAS[scene_id],
        }, f"{scene_id}: manual machine-fact binding")


def validate_review_structure(review: dict[str, Any]) -> None:
    require(review.get("schema") ==
            "phase2_v10i_formal_targeted_camera_v5_independent_ta_manual_review_v1"
            and review.get("status") ==
            "PASS_TARGETED_BREAK_LOW_AIM_TA_REVIEW"
            and review.get("overall_result") == "PASS",
            "manual review identity/result")
    require(review.get("review_scope") == {
        "kind": "targeted_camera_break_low_aim_only",
        "scene_ids_in_order": SCENE_IDS,
        "case_count": 2,
        "full_six_scene_review": False,
    }, "manual review scope")
    method = review.get("review_method", {})
    require(method == {
        "reviewer_role": "independent_technical_art",
        "source_format": "PPM P6 RGB8",
        "source_dimensions": [1920, 1080],
        "each_source_reviewed_at_original_resolution": True,
        "lossless_local_view_conversion_only": True,
        "gpu_or_graphics_context_used_for_review": False,
        "production_file_modified": False,
    }, "manual review method")
    require(review.get("axis_vocabulary") == AXES,
            "manual axis vocabulary")
    cases = review.get("cases", [])
    require([case.get("scene_id") for case in cases] == SCENE_IDS,
            "manual case order")
    axis_pass = axis_fail = case_pass = case_fail = 0
    all_blockers: list[str] = []
    failed_cases: list[str] = []
    for case in cases:
        scene_id = case["scene_id"]
        result = case.get("manual_result")
        require(result in ("PASS", "FAIL"), f"{scene_id}: PASS/FAIL only")
        findings = case.get("axis_findings", {})
        require(list(findings) == CASE_AXES[scene_id],
                f"{scene_id}: exact axis order/inventory")
        failed_axes: list[str] = []
        for axis, finding in findings.items():
            status = finding.get("status")
            require(status in ("PASS", "FAIL"),
                    f"{scene_id}/{axis}: PASS/FAIL only")
            require(isinstance(finding.get("observation"), str) and
                    len(finding["observation"]) >= 40,
                    f"{scene_id}/{axis}: observation missing")
            if status == "PASS":
                axis_pass += 1
            else:
                axis_fail += 1
                failed_axes.append(axis)
        require(case.get("blocking_axes") == failed_axes,
                f"{scene_id}: exact blocking axes")
        require((result == "FAIL") == bool(failed_axes),
                f"{scene_id}: case/axis result mismatch")
        if result == "PASS":
            case_pass += 1
        else:
            case_fail += 1
            failed_cases.append(scene_id)
            all_blockers.extend(f"{scene_id}:{axis}" for axis in failed_axes)
    require(case_fail == 0 and axis_fail == 0,
            "committed independent review must remain the observed PASS")
    require(review.get("review_counts") == {
        "cases_total": 2, "cases_pass": case_pass,
        "cases_fail": case_fail, "axes_total": axis_pass + axis_fail,
        "axes_pass": axis_pass, "axes_fail": axis_fail,
    }, "manual review counts")
    require(review.get("failed_case_ids") == failed_cases == [] and
            review.get("blocking_axes") == all_blockers == [],
            "manual failed-case/blocker inventory")
    decision = review.get("technical_art_decision", "")
    require("exact v5 targeted break/low_aim" in decision and
            "low_aim composition" in decision and
            "lamp-occlusion repair" in decision,
            "technical-art decision precision")
    require(review.get("evidence_boundary") == EXPECTED_BOUNDARY,
            "manual review evidence boundary promoted")


def validate(path: Path = DEFAULT_REVIEW) -> dict[str, Any]:
    path = path.resolve(strict=True)
    review = load_json(path)
    loaded = validate_bindings(review)
    validate_manifest_and_pixels(review, loaded)
    validate_review_structure(review)
    return {
        "status": "PASS_INDEPENDENT_TARGETED_CAMERA_V5_TA_MANUAL_REVIEW",
        "review_sha256": sha256(path),
        "readiness_sha256": EXPECTED_BINDINGS["v5_readiness_manifest"][1],
        "manifest_sha256": EXPECTED_BINDINGS["v5_targeted_capture_manifest"][1],
        "v4_ta_gate_sha256":
            EXPECTED_BINDINGS["v4_ta_camera_correction_design_gate"][1],
        "scene_results": {"break": "PASS", "low_aim": "PASS"},
        "blocking_axes": [],
        "full_six_scene_evidence": False,
        "m2_promotion": False,
        "golden_promotion": False,
        "product_pass": False,
        "gpu_executed_by_review": False,
        "production_modified_by_review": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--review", type=Path, default=DEFAULT_REVIEW)
    arguments = parser.parse_args()
    try:
        print(json.dumps(validate(arguments.review), indent=2, sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
