#!/usr/bin/env python3
"""Read-only metric and review validator for six production-modern screenshots.

The validator never changes source screenshots. Thresholds are consumed from
the approved lookdev manifest and cannot be overridden on the command line.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
from typing import Iterable


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_LOOKDEV = ROOT / "assets" / "modern" / "lookdev" / "focused_practice_hall_v1.lookdev.json"
DEFAULT_PHASE2_ROOT = Path(os.environ.get(
    "BILLIARDGL_MODERN_PHASE2_ROOT", ROOT / "assets" / "modern" / "phase2"))
CAPTURE_ASSET_FILES = {
    "modular_gltf_sha256": "focused_practice_hall_modular_v1.gltf",
    "ball_atlas_sha256": "focused_practice_hall_ball_identity_atlas_v1.png",
    "asset_manifest_sha256": "focused_practice_hall_modular_v1.asset.json",
    "pbr_params_sha256": "focused_practice_hall_pbr_v1.params.json",
    "lighting_sha256": "focused_practice_hall_lighting_rig_v1.json",
    "venue_gltf_sha256": "focused_practice_hall_venue_v1.gltf",
}
FORMAL_ATTESTATION_IDS = {
    "modular_gltf": "modular_gltf_sha256",
    "asset_manifest": "asset_manifest_sha256",
    "pbr_params": "pbr_params_sha256",
    "lighting_rig": "lighting_sha256",
    "venue_gltf": "venue_gltf_sha256",
    "ball_atlas": "ball_atlas_sha256",
}
# Compatibility export for callers that build manifests against the default
# production package.  Self-contained captures are resolved per manifest below.
CAPTURE_ASSET_HASHES = {
    field: DEFAULT_PHASE2_ROOT / filename
    for field, filename in CAPTURE_ASSET_FILES.items()
}
SCENE_IDS = (
    "break", "low_aim", "ball_closeup", "pocket_closeup",
    "tactical_overhead", "post_shot_observe",
)
FORMAL_EXPECTED_CAMERA = {
    "break": ("observe_free", "elevated"),
    "low_aim": ("aim_player", "player_low"),
    "ball_closeup": ("observe_free", "elevated"),
    "pocket_closeup": ("observe_free", "elevated"),
    "tactical_overhead": ("aim_overhead", "overhead"),
    "post_shot_observe": ("observe_free", "elevated"),
}
MANUAL_AXES = (
    "composition", "material_color", "contact_shadow", "geometry_alignment",
    "ball_identity_and_rotation_seam", "venue_and_background_focus",
)
LUMA_LUT = tuple(
    value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4
    for value in (index / 255.0 for index in range(256))
)


class ReviewError(Exception):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ReviewError(message)


def validate_semantic_camera(scene_id: str, camera: object) -> None:
    require(isinstance(camera, dict), f"{scene_id}: semantic camera missing")
    expected_state, expected_preset = FORMAL_EXPECTED_CAMERA[scene_id]
    require(camera.get("state") == expected_state and
            camera.get("preset") == expected_preset,
            f"{scene_id}: semantic camera state/preset")
    require(camera.get("transitioning") is False,
            f"{scene_id}: semantic camera must be settled")
    points = {}
    for name in ("eye_cm", "target_cm"):
        point = camera.get(name)
        require(isinstance(point, dict) and all(
            isinstance(point.get(axis), (int, float)) and
            math.isfinite(point[axis]) for axis in ("x", "y", "z")),
            f"{scene_id}: semantic camera {name}")
        points[name] = point
    distance = camera.get("distance_cm")
    fov = camera.get("fov_degrees")
    require(isinstance(distance, (int, float)) and math.isfinite(distance) and
            distance > 0.0, f"{scene_id}: semantic camera distance")
    require(isinstance(fov, (int, float)) and math.isfinite(fov) and
            20.0 <= fov <= 90.0, f"{scene_id}: semantic camera projection")
    projected = math.sqrt(sum(
        (points["eye_cm"][axis] - points["target_cm"][axis]) ** 2
        for axis in ("x", "y", "z")))
    require(math.isclose(projected, distance, rel_tol=1.0e-4, abs_tol=1.0e-3),
            f"{scene_id}: semantic camera projected distance")
    if scene_id != "tactical_overhead":
        require(points["eye_cm"]["y"] < 221.0,
                f"{scene_id}: semantic camera lamp safety")


def read_ppm(path: Path) -> tuple[int, int, bytes]:
    with path.open("rb") as image:
        require(image.readline().strip() == b"P6", f"{path}: expected P6 PPM")
        dimensions = image.readline()
        while dimensions.startswith(b"#"):
            dimensions = image.readline()
        width, height = map(int, dimensions.split())
        require(image.readline().strip() == b"255", f"{path}: expected RGB8 max value")
        pixels = image.read()
    require(len(pixels) == width * height * 3, f"{path}: incomplete pixels")
    return width, height, pixels


def sha256(path: Path) -> str:
    result = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            result.update(chunk)
    return "sha256:" + result.hexdigest()


def relative_luma(pixels: bytes, offset: int) -> float:
    return 0.2126 * LUMA_LUT[pixels[offset]] + 0.7152 * LUMA_LUT[pixels[offset + 1]] + 0.0722 * LUMA_LUT[pixels[offset + 2]]


def rect_pixels(rect: Iterable[float], width: int, height: int) -> tuple[int, int, int, int]:
    x, y, w, h = rect
    return (int(x * width), int(y * height), int((x + w) * width), int((y + h) * height))


def scalar_metrics(pixels: bytes, width: int, height: int, bounds: tuple[int, int, int, int]) -> dict:
    x0, y0, x1, y1 = bounds
    count = max(0, x1 - x0) * max(0, y1 - y0)
    require(count > 0, "empty metric region")
    luma_sum = 0.0
    red_sum = green_sum = blue_sum = 0
    non_black = highlights = shadows = 0
    for y in range(y0, y1):
        row = y * width * 3
        for x in range(x0, x1):
            offset = row + x * 3
            red, green, blue = pixels[offset], pixels[offset + 1], pixels[offset + 2]
            red_sum += red; green_sum += green; blue_sum += blue
            luma_sum += relative_luma(pixels, offset)
            if max(red, green, blue) > 8:
                non_black += 1
            if max(red, green, blue) >= 254:
                highlights += 1
            if max(red, green, blue) <= 1:
                shadows += 1
    return {
        "highlight_clip_fraction": highlights / count,
        "mean_encoded_rgb": [red_sum / count / 255.0, green_sum / count / 255.0, blue_sum / count / 255.0],
        "mean_luma": luma_sum / count,
        "non_black_fraction": non_black / count,
        "shadow_clip_fraction": shadows / count,
    }


def edge_density(pixels: bytes, width: int, bounds: tuple[int, int, int, int]) -> float:
    x0, y0, x1, y1 = bounds
    if x1 - x0 < 3 or y1 - y0 < 3:
        return 0.0
    rw = x1 - x0
    values = [0.0] * (rw * (y1 - y0))
    for local_y, y in enumerate(range(y0, y1)):
        row = y * width * 3
        target = local_y * rw
        for local_x, x in enumerate(range(x0, x1)):
            values[target + local_x] = relative_luma(pixels, row + x * 3)
    edges = 0
    samples = (rw - 2) * (y1 - y0 - 2)
    for y in range(1, y1 - y0 - 1):
        prior = (y - 1) * rw; current = y * rw; following = (y + 1) * rw
        for x in range(1, rw - 1):
            gx = (-values[prior + x - 1] + values[prior + x + 1]
                  - 2 * values[current + x - 1] + 2 * values[current + x + 1]
                  - values[following + x - 1] + values[following + x + 1])
            gy = (-values[prior + x - 1] - 2 * values[prior + x] - values[prior + x + 1]
                  + values[following + x - 1] + 2 * values[following + x] + values[following + x + 1])
            if math.hypot(gx, gy) > 0.08:
                edges += 1
    return edges / samples


def surrounding_luma(pixels: bytes, width: int, height: int, bounds: tuple[int, int, int, int]) -> float:
    x0, y0, x1, y1 = bounds
    pad_x = max(4, int((x1 - x0) * 0.08)); pad_y = max(4, int((y1 - y0) * 0.08))
    outer = (max(0, x0 - pad_x), max(0, y0 - pad_y), min(width, x1 + pad_x), min(height, y1 + pad_y))
    total = 0.0; count = 0
    for y in range(outer[1], outer[3]):
        row = y * width * 3
        for x in range(outer[0], outer[2]):
            if x0 <= x < x1 and y0 <= y < y1:
                continue
            total += relative_luma(pixels, row + x * 3); count += 1
    return total / count if count else 0.0


def evaluate_check(actual: dict, checks: dict) -> list[dict]:
    mapping = {
        "mean_luma_min": ("mean_luma", ">="), "mean_luma_max": ("mean_luma", "<="),
        "non_black_fraction_min": ("non_black_fraction", ">="),
        "highlight_clip_fraction_max": ("highlight_clip_fraction", "<="),
        "shadow_clip_fraction_max": ("shadow_clip_fraction", "<="),
        "minimum_contrast_to_surround": ("contrast_to_surround", ">="),
        "minimum_edge_density": ("edge_density", ">="),
    }
    results = []
    for threshold_name, threshold in checks.items():
        metric, comparison = mapping[threshold_name]
        measured = actual[metric]
        passed = measured >= threshold if comparison == ">=" else measured <= threshold
        results.append({"actual": measured, "comparison": comparison, "metric": metric, "passed": passed, "threshold": threshold})
    return results


def load_capture_manifest(path: Path) -> tuple[dict, dict[str, dict], dict[str, Path]]:
    value = json.loads(path.read_text(encoding="utf-8"))
    require(value.get("renderer") == "modern" or value.get("renderer_backend") == "modern", "capture manifest must identify renderer=modern")
    require(value.get("production_gltf") is True, "capture manifest must attest production_gltf=true")
    resolution = value.get("resolution", {"width": value.get("width"), "height": value.get("height")})
    require(resolution.get("width") == 1920 and resolution.get("height") == 1080, "capture resolution must be 1920x1080")
    require(value.get("quality") == "medium", "capture quality must be medium")
    capture_root = path.parent.resolve()
    declared_asset_root = value.get("asset_root")
    if declared_asset_root is None:
        asset_root = DEFAULT_PHASE2_ROOT.resolve()
    else:
        require(isinstance(declared_asset_root, str) and declared_asset_root == "asset_root",
                "capture asset_root must be the constrained relative path 'asset_root'")
        asset_root = (capture_root / declared_asset_root).resolve()
        require(asset_root.parent == capture_root and asset_root.is_dir(),
                "capture asset_root must resolve inside the capture directory")
    asset_paths = {field: asset_root / filename
                   for field, filename in CAPTURE_ASSET_FILES.items()}
    for field, asset_path in asset_paths.items():
        declared = value.get(field)
        require(isinstance(declared, str) and len(declared) == 64,
                f"capture manifest must bind {field} to a raw SHA-256 digest")
        try:
            int(declared, 16)
        except ValueError as error:
            raise ReviewError(f"capture manifest {field} is not hexadecimal") from error
        require(asset_path.is_file(), f"attested asset is missing: {asset_path}")
        actual = sha256(asset_path).removeprefix("sha256:")
        require(declared.lower() == actual,
                f"capture manifest {field} does not match {asset_path.name}")
    performance = value.get("performance", {})
    require(all(isinstance(performance.get(key), (int, float)) for key in (
        "warmup_frames", "sample_frames", "p95_total_frame_ms",
        "draw_calls_complete_scene", "texture_resident_mib")), "performance measurements are incomplete")
    require(performance.get("measurement_method") in {"gpu_timer_query_plus_cpu_frame", "platform_frame_capture"}, "performance measurement method is not accepted")
    scenes = value.get("scenes")
    require(isinstance(scenes, list), "capture manifest scenes must be an array")
    by_id = {item.get("scene_id"): item for item in scenes if isinstance(item, dict)}
    require(set(by_id) == set(SCENE_IDS) and len(scenes) == len(by_id), "capture manifest must contain exactly the six approved scene ids")
    if value.get("registry_version") == 2:
        for scene_id in SCENE_IDS:
            validate_semantic_camera(
                scene_id, by_id[scene_id].get("semantic_camera"))
    if value.get("formal_asset_attestation_required") is True:
        expected_root = str(asset_root)
        for scene_id in SCENE_IDS:
            attestation = by_id[scene_id].get("loaded_asset_attestation")
            require(isinstance(attestation, dict) and
                    attestation.get("schema") == "billiardgl_formal_asset_attestation_v1",
                    f"{scene_id}: formal loaded-asset attestation schema")
            require(attestation.get("asset_root") == expected_root and
                    attestation.get("load_state") == "loaded",
                    f"{scene_id}: formal loaded-asset root/state")
            require(attestation.get("silent_material_fallback_allowed") is False and
                    attestation.get("load_errors") == [],
                    f"{scene_id}: silent material fallback/load errors")
            renderer_load_checks = attestation.get("renderer_load_checks")
            require(isinstance(renderer_load_checks, dict) and
                    set(renderer_load_checks) == {
                        "pbr_loaded", "pbr_texture_count_22",
                        "environment_enabled", "gltf_scene_loaded",
                        "venue_scene_loaded", "medium_quality", "msaa_4x"} and
                    all(result is True for result in
                        renderer_load_checks.values()),
                    f"{scene_id}: actual renderer load/quality checks")
            assets = attestation.get("required_assets")
            require(isinstance(assets, list) and len(assets) == 6,
                    f"{scene_id}: exact six formal asset bindings")
            attested = {item.get("id"): item for item in assets
                        if isinstance(item, dict)}
            require(set(attested) == set(FORMAL_ATTESTATION_IDS),
                    f"{scene_id}: formal asset ids")
            for asset_id, hash_field in FORMAL_ATTESTATION_IDS.items():
                path = asset_paths[hash_field]
                require(attested[asset_id].get("canonical_path") == str(path) and
                        attested[asset_id].get("sha256") == value[hash_field],
                        f"{scene_id}: formal path/SHA attestation {asset_id}")
            dependencies = attestation.get("runtime_dependencies")
            require(isinstance(dependencies, list) and dependencies,
                    f"{scene_id}: runtime dependency attestation")
            for dependency in dependencies:
                require(isinstance(dependency, dict) and
                        isinstance(dependency.get("canonical_path"), str),
                        f"{scene_id}: runtime dependency shape")
                dependency_path = Path(dependency["canonical_path"]).resolve()
                require(dependency_path.parent == asset_root and
                        dependency_path.is_file() and
                        dependency.get("sha256") == sha256(dependency_path).removeprefix("sha256:"),
                        f"{scene_id}: runtime dependency path/SHA")
            scene_performance = by_id[scene_id].get("performance")
            require(isinstance(scene_performance, dict) and
                    scene_performance.get("warmup_frames", 0) >= 120 and
                    scene_performance.get("sample_frames", 0) >= 300 and
                    scene_performance.get("measurement_method") ==
                        "gpu_timer_query_plus_cpu_frame" and
                    scene_performance.get("gpu_timer_queries_observed") is True,
                    f"{scene_id}: formal performance collection")
    return value, by_id, asset_paths


def evaluate_performance(performance: dict) -> list[dict]:
    definitions = (
        ("warmup_frames", ">=", 120),
        ("sample_frames", ">=", 300),
        ("p95_total_frame_ms", "<=", 16.7),
        ("draw_calls_complete_scene", "<=", 100),
        ("texture_resident_mib", "<=", 256),
    )
    results = []
    for metric, comparison, threshold in definitions:
        actual = performance[metric]
        passed = actual >= threshold if comparison == ">=" else actual <= threshold
        results.append({"actual": actual, "comparison": comparison, "metric": metric, "passed": passed, "threshold": threshold})
    return results


def validate_manual_review(value: dict, capture_hashes: dict[str, str]) -> None:
    require(value.get("schema") == "phase2_ta_manual_review_v1", "manual review schema")
    require(value.get("reviewer_role") == "independent_technical_artist", "manual review must be independent TA")
    scenes = value.get("scenes", {})
    require(set(scenes) == set(SCENE_IDS), "manual review scene coverage")
    for scene_id, scene in scenes.items():
        require(scene.get("screenshot_sha256") == capture_hashes[scene_id], f"manual review hash mismatch: {scene_id}")
        axes = scene.get("axes", {})
        require(set(axes) == set(MANUAL_AXES), f"manual review axis coverage: {scene_id}")
        for axis, result in axes.items():
            require(result.get("status") in {"PASS", "FAIL", "N/A"}, f"manual status: {scene_id}/{axis}")
            require(isinstance(result.get("evidence"), str) and result["evidence"].strip(), f"manual evidence: {scene_id}/{axis}")


OBJECT_PHOTOMETRY_SCHEMA = "billiardgl_visible_surface_photometry_v1"
OBJECT_CLASS_PALETTE = {
    "other": (0, 0, 0), "felt": (255, 0, 0),
    "cue_ball": (0, 255, 0), "venue": (0, 0, 255),
    "numbered_balls": (255, 255, 0),
}


def validate_photometry_binding(scene_id: str, entry: dict, capture: dict) -> dict:
    """Fail closed on mismatched normal/mask provenance, before pixel metrics."""
    binding = entry.get("object_photometry")
    require(isinstance(binding, dict) and binding.get("schema") == OBJECT_PHOTOMETRY_SCHEMA,
            f"{scene_id}: object photometry schema")
    require(binding.get("source_file") == entry.get("file") and
            binding.get("source_sha256") == entry.get("screenshot_sha256"),
            f"{scene_id}: object photometry source binding")
    camera = entry.get("runtime_camera")
    require(isinstance(camera, dict) and
            binding.get("normal_camera") == camera and binding.get("mask_camera") == camera,
            f"{scene_id}: object photometry source-camera mismatch")
    for name in ("binary_sha256", "renderer_source_sha256", "modular_gltf_sha256"):
        expected = entry.get(name)
        require(isinstance(expected, str) and len(expected) == 64 and expected == capture.get(name),
                f"{scene_id}: object photometry global provenance {name}")
        require(binding.get("normal_" + name) == expected and binding.get("mask_" + name) == expected,
                f"{scene_id}: object photometry pair provenance {name}")
    require(binding.get("venue_loaded") is True,
            f"{scene_id}: object photometry requires actually loaded venue, even when absent")
    require(isinstance(binding.get("cue_ball_expected_visible"), bool),
            f"{scene_id}: actual identity0 visibility missing")
    require(binding.get("sample_policy") == "all_exact_class_pixels_including_shadows_no_luminance_filter",
            f"{scene_id}: object photometry must not select bright pixels")
    require(binding.get("class_palette") == {key: list(value) for key, value in OBJECT_CLASS_PALETTE.items()},
            f"{scene_id}: object mask palette/class mismatch")
    require(binding.get("mixed_edge_policy") == "exclude_non_palette_samples_and_report_count",
            f"{scene_id}: object mask edge ownership policy")
    return binding


def masked_surface_statistics(pixels: bytes, mask: bytes, width: int, height: int) -> dict:
    """All depth-winning pure ownership samples, including dark/shadow pixels.

    Mask edge exclusion depends only on encoded ownership, never source RGB.
    Spatial fixed-ROI edge-density thresholds are not reinterpreted here.
    """
    require(len(pixels) == len(mask) == width * height * 3, "object mask/source dimensions")
    owners = {rgb: name for name, rgb in OBJECT_CLASS_PALETTE.items()}
    accumulators = {name: [0, 0.0, 0, 0, 0] for name in owners.values()}
    mixed = 0
    for offset in range(0, len(mask), 3):
        owner = owners.get(tuple(mask[offset:offset + 3]))
        if owner is None:
            mixed += 1
            continue
        rgb = pixels[offset:offset + 3]
        count = accumulators[owner]
        count[0] += 1
        count[1] += relative_luma(pixels, offset)
        count[2] += max(rgb) > 8
        count[3] += max(rgb) >= 254
        count[4] += max(rgb) <= 1
    result = {"mixed_edge_pixels": mixed, "mixed_edge_fraction": mixed / (width * height),
              "selection_policy": "ownership_only_not_source_luminance", "objects": {}}
    for owner, (count, total, nonblack, highlight, shadow) in accumulators.items():
        result["objects"][owner] = {"pixel_count": count, "mean_luma": total / count if count else None,
                                   "non_black_fraction": nonblack / count if count else None,
                                   "highlight_clip_fraction": highlight / count if count else None,
                                   "shadow_clip_fraction": shadow / count if count else None}
    return result


def evaluate_object_photometry(scene_id: str, pixels: bytes, mask: bytes,
                               width: int, height: int, binding: dict, lookdev: dict) -> dict:
    result = masked_surface_statistics(pixels, mask, width, height)
    objects = result["objects"]
    require(objects["felt"]["pixel_count"] > 0, f"{scene_id}: empty felt cannot PASS")
    expected_cue = binding["cue_ball_expected_visible"]
    require((objects["cue_ball"]["pixel_count"] > 0) == expected_cue,
            f"{scene_id}: actual identity0 mask/visibility mismatch")
    if expected_cue:
        point = binding.get("cue_center_probe_pixels")
        require(isinstance(point, list) and len(point) == 2,
                f"{scene_id}: independent projected identity0 ownership probe missing")
        x, y = int(point[0]), int(point[1])
        require(0 <= x < width and 0 <= y < height and
                tuple(mask[(y * width + x) * 3:(y * width + x) * 3 + 3]) == OBJECT_CLASS_PALETTE["cue_ball"],
                f"{scene_id}: projected actual cue0 has wrong mask class")
    original = {
        "felt": ("break/table", next(region["checks"] for region in lookdev["scenes"]["break"]["regions"] if region["id"] == "table")),
        "cue_ball": ("low_aim/cue_ball", next(region["checks"] for region in lookdev["scenes"]["low_aim"]["regions"] if region["id"] == "cue_ball")),
        "venue": ("break/background", next(region["checks"] for region in lookdev["scenes"]["break"]["regions"] if region["id"] == "background")),
    }
    for name, (source_key, thresholds) in original.items():
        measured = objects[name]
        measured["source_key"] = source_key
        measured["source_thresholds_unmodified"] = dict(thresholds)
        if measured["pixel_count"] == 0:
            if name == "venue":
                require(scene_id in {"ball_closeup", "pocket_closeup"} and
                        binding.get("venue_absence_geometry_verified") is True and
                        not any(mask[offset] for offset in range(2, len(mask), 3)) and
                        isinstance(binding.get("venue_absence_evidence"), str) and binding["venue_absence_evidence"].strip(),
                        f"{scene_id}: empty venue must be verified absent, never silently PASS")
            measured["status"] = "N_A_OBJECT_ABSENT"
            measured["checks"] = []
            continue
        if name == "cue_ball":
            surround = objects["felt"]["mean_luma"]
        elif name == "felt":
            surround = objects["venue"]["mean_luma"]
        else:
            surround = objects["felt"]["mean_luma"]
        measured["contrast_to_surround"] = max(measured["mean_luma"], surround) / max(min(measured["mean_luma"], surround), 1e-6) if surround is not None else None
        evaluated = {key: value for key, value in thresholds.items() if key != "minimum_edge_density" and
                     (key != "minimum_contrast_to_surround" or surround is not None)}
        measured["checks"] = evaluate_check(measured, evaluated)
        measured["status"] = "PASS" if all(check["passed"] for check in measured["checks"]) else "FAIL"
        measured["contrast_status"] = "EVALUATED" if surround is not None else "N_A_SURROUND_OBJECT_ABSENT"
    if objects["venue"]["pixel_count"]:
        ev = math.log2(max(objects["felt"]["mean_luma"], 1e-6) / max(objects["venue"]["mean_luma"], 1e-6))
        global_checks = lookdev["global_checks"]
        result["felt_to_venue_ev"] = {"actual": ev, "source_key": "global_checks/table_to_background_ev_min,max",
                                      "threshold": [global_checks["table_to_background_ev_min"], global_checks["table_to_background_ev_max"]],
                                      "passed": global_checks["table_to_background_ev_min"] <= ev <= global_checks["table_to_background_ev_max"]}
    else:
        result["felt_to_venue_ev"] = {"status": "N_A_OBJECT_ABSENT", "passed": None}
    result["scope"] = "independent_object_diagnostic_not_a_replacement_for_fixed_roi_acceptance"
    result["formal_pass_claimed"] = False
    return result


def validate(capture_manifest_path: Path, manual_review_path: Path | None, lookdev_path: Path = DEFAULT_LOOKDEV) -> dict:
    capture, entries, asset_paths = load_capture_manifest(capture_manifest_path)
    lookdev = json.loads(lookdev_path.read_text(encoding="utf-8"))["ldr_interest_region_acceptance"]
    scene_spec = lookdev["scenes"]
    root = capture_manifest_path.parent
    performance_checks = evaluate_performance(capture["performance"])
    report = {
        "capture_manifest": str(capture_manifest_path.resolve()),
        "lookdev_manifest": str(lookdev_path.resolve()),
        "renderer": "modern",
        "resolution": [1920, 1080],
        "performance": capture["performance"],
        "performance_checks": performance_checks,
        "performance_pass": all(item["passed"] for item in performance_checks),
        "source_asset_hashes": {
            field: capture[field] for field in CAPTURE_ASSET_FILES
        },
        "source_asset_paths": {field: str(path.resolve())
                               for field, path in asset_paths.items()},
        "schema": "phase2_production_visual_review_v1",
        "scenes": {},
        "threshold_policy": "approved_lookdev_thresholds_unmodified_no_cli_overrides",
    }
    hashes = {}
    global_checks = lookdev["global_checks"]
    for scene_id in SCENE_IDS:
        image_path = root / entries[scene_id]["file"]
        width, height, pixels = read_ppm(image_path)
        require((width, height) == (1920, 1080), f"{scene_id}: PPM dimensions")
        hashes[scene_id] = sha256(image_path)
        frame = scalar_metrics(pixels, width, height, (0, 0, width, height))
        global_results = [
            {"actual": frame["highlight_clip_fraction"], "comparison": "<=", "metric": "frame_highlight_clip_fraction", "passed": frame["highlight_clip_fraction"] <= global_checks["frame_highlight_clip_fraction_max"], "threshold": global_checks["frame_highlight_clip_fraction_max"]},
            {"actual": frame["shadow_clip_fraction"], "comparison": "<=", "metric": "frame_shadow_clip_fraction", "passed": frame["shadow_clip_fraction"] <= global_checks["frame_shadow_clip_fraction_max"], "threshold": global_checks["frame_shadow_clip_fraction_max"]},
        ]
        regions = {}
        for spec in scene_spec[scene_id]["regions"]:
            bounds = rect_pixels(spec["rect_normalized"], width, height)
            actual = scalar_metrics(pixels, width, height, bounds)
            actual["edge_density"] = edge_density(pixels, width, bounds)
            surround = surrounding_luma(pixels, width, height, bounds)
            actual["surround_mean_luma"] = surround
            actual["contrast_to_surround"] = max(actual["mean_luma"], surround) / max(min(actual["mean_luma"], surround), 1e-6)
            checks = evaluate_check(actual, spec["checks"])
            regions[spec["id"]] = {"actual": actual, "checks": checks, "passed": all(item["passed"] for item in checks), "rect_normalized": spec["rect_normalized"], "role": spec["role"]}
        machine_pass = all(item["passed"] for item in global_results) and all(item["passed"] for item in regions.values())
        report["scenes"][scene_id] = {"file": entries[scene_id]["file"], "global_checks": global_results, "image_sha256": hashes[scene_id], "machine_pass": machine_pass, "regions": regions}

    if capture.get("object_photometry_schema") is not None:
        require(capture["object_photometry_schema"] == OBJECT_PHOTOMETRY_SCHEMA and
                capture.get("object_photometry_replaces_fixed_roi") is False,
                "object diagnostics must not replace fixed-ROI acceptance")
        diagnostics = {}
        for scene_id in SCENE_IDS:
            entry = entries[scene_id]
            binding = validate_photometry_binding(scene_id, entry, capture)
            require(binding["source_sha256"] == hashes[scene_id].removeprefix("sha256:"),
                    f"{scene_id}: photometry source actual hash mismatch")
            mask_file = binding.get("mask_file")
            require(isinstance(mask_file, str) and not Path(mask_file).is_absolute() and
                    ".." not in Path(mask_file).parts, f"{scene_id}: unsafe class-mask path")
            mask_path = root / mask_file
            require(sha256(mask_path).removeprefix("sha256:") == binding.get("mask_sha256"),
                    f"{scene_id}: class-mask actual hash mismatch")
            width, height, pixels = read_ppm(root / entry["file"])
            mask_width, mask_height, mask = read_ppm(mask_path)
            require((mask_width, mask_height) == (width, height), f"{scene_id}: class-mask dimensions")
            diagnostics[scene_id] = evaluate_object_photometry(
                scene_id, pixels, mask, width, height, binding, lookdev)
        report["object_photometry_diagnostics"] = diagnostics
        report["object_photometry_policy"] = "Independent diagnostics only; original fixed-ROI failures and formal summary are retained unchanged"

    break_regions = report["scenes"]["break"]["regions"]
    table_luma = break_regions["table"]["actual"]["mean_luma"]
    background_luma = break_regions["background"]["actual"]["mean_luma"]
    table_ev = math.log2(max(table_luma, 1e-6) / max(background_luma, 1e-6))
    ev_check = {
        "actual": table_ev,
        "comparison": "inclusive_range",
        "metric": "table_to_background_ev",
        "passed": global_checks["table_to_background_ev_min"] <= table_ev <= global_checks["table_to_background_ev_max"],
        "threshold": [global_checks["table_to_background_ev_min"], global_checks["table_to_background_ev_max"]],
    }
    report["scenes"]["break"]["global_checks"].append(ev_check)
    report["scenes"]["break"]["machine_pass"] = report["scenes"]["break"]["machine_pass"] and ev_check["passed"]
    report["unresolved_machine_gates"] = [{
        "metric": "review_ssim",
        "required_floor": global_checks["review_ssim_floor"],
        "status": "NOT_EVALUATED_NO_APPROVED_BASELINE",
    }]

    if manual_review_path:
        manual = json.loads(manual_review_path.read_text(encoding="utf-8"))
        validate_manual_review(manual, hashes)
        for scene_id in SCENE_IDS:
            axes = manual["scenes"][scene_id]["axes"]
            manual_pass = all(item["status"] in {"PASS", "N/A"} for item in axes.values())
            report["scenes"][scene_id]["manual_axes"] = axes
            report["scenes"][scene_id]["manual_pass"] = manual_pass
            report["scenes"][scene_id]["status"] = "PASS" if manual_pass and report["scenes"][scene_id]["machine_pass"] else "FAIL"
    else:
        for scene in report["scenes"].values():
            scene["status"] = "INCOMPLETE_MANUAL_REVIEW"
    report["summary"] = {
        "fail": sum(item["status"] == "FAIL" for item in report["scenes"].values()),
        "incomplete": sum(item["status"].startswith("INCOMPLETE") for item in report["scenes"].values()),
        "pass": sum(item["status"] == "PASS" for item in report["scenes"].values()),
        "status": "PASS" if report["performance_pass"] and not report["unresolved_machine_gates"] and all(item["status"] == "PASS" for item in report["scenes"].values()) else "FAIL",
    }
    return report


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture_manifest", type=Path)
    parser.add_argument("--manual-review", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        report = validate(args.capture_manifest.resolve(), args.manual_review.resolve() if args.manual_review else None)
    except (ReviewError, OSError, KeyError, ValueError, json.JSONDecodeError) as error:
        print(f"FAIL: {error}")
        return 1
    payload = json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(payload, encoding="utf-8")
    else:
        print(payload, end="")
    summary = report["summary"]
    print(f"{summary['status']} phase2 production visual review: pass={summary['pass']} fail={summary['fail']} incomplete={summary['incomplete']}")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
