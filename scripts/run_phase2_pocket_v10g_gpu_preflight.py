#!/usr/bin/env python3
"""Run exactly one authorized Phase-2 v10g actual-GPU pocket close-up.

All immutable bindings and metadata are checked before the renderer process is
started.  The output directory and freeze directory are single-use guards: an
existing path is a hard failure, and no resume or overwrite mode exists.
"""

import argparse
from collections import Counter, defaultdict
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from generate_m1_graybox_gltf import PLAY_SURFACE_Y_M, POCKETS
from generate_phase2_modular_gltf_v10f import (
    BALL_DIAMETER_M, POCKET_BACK_Z_M, POCKET_FRONT_Z_M,
    POCKET_RIM_WIDTH_M, POCKET_SIDEWALL_DROP_M, marker_binding_sha256,
)
from generate_phase2_modular_gltf_v10g import ASSET_NAME, MANIFEST_NAME
from run_phase2_pocket_v10f_gpu_preflight import (
    geometry_measurements, largest_black_component, read_ppm,
    object_id_measurements as legacy_object_id_measurements,
)
from validate_phase2_modular_gltf_v10g import validate


ROOT = Path(__file__).resolve().parents[1]
ASSET_ROOT = ROOT / "assets/modern/phase2-v10g"
SHARED_ROOT = ROOT / "assets/modern/phase2-v10f"
GATE = ROOT / "docs/art/phase2-pocket-v10g-offline-correction-design-gate-v2.json"
OFFLINE_REPORT = ROOT / "docs/art/phase2-pocket-v10g-offline-preflight-v2/phase2-pocket-v10g-offline-report-v2.json"
OFFLINE_FREEZE = ROOT / "docs/art/phase2-pocket-v10g-offline-source-freeze-v2/receipt.json"
DEFAULT_OUTPUT = ROOT / "docs/art/phase2-pocket-v10g-gpu-preflight-v1"
DEFAULT_FREEZE = ROOT / "docs/art/phase2-pocket-v10g-gpu-source-freeze-v1"
CAPTURE_NAME = "pocket_closeup.png"
METRICS_NAME = "pocket_gpu_metrics.json"
MANIFEST_NAME_OUT = "phase2-pocket-v10g-gpu-preflight-manifest-v1.json"
EVIDENCE_SCOPE = "phase2_pocket_v10g_single_scene_non_formal"

EXPECTED = {
    "gate": "eea60f8a01bf185f052ce278fc9d26f911dce57a736f29c3c27eb9a649245ae4",
    "offline_report": "0830044860a94ac157c3953b995531ac695460370271d2feaaeeabbf1b3a39e1",
    "offline_freeze": "dcd148cba07deaa47cd112d48a9e228126ed264ffd0b7192b00354b2cbad1284",
    "gltf": "a72c5ab4dd477fe3a6d85e2ced1421af93bb768ff613a411dabb0a02def6226f",
    "asset_manifest": "27387e5c06606ab90cb91e8620868b02cc8f8417c5ce6db7618512b290f11e4b",
    "renderer_source": "7626026f5cbbb8df443df3a89e9cb7c4243c97d74113f927ea0705d1bc7e745b",
    "smoke_source": "e820c76e64fccd4bb0d58b531104d708d90182b9e6fcc0332869737831fc7dd5",
}


def sha256_bytes(payload):
    return hashlib.sha256(payload).hexdigest()


def sha256(path):
    return "sha256:" + sha256_bytes(path.read_bytes())


def _median(values):
    ordered = sorted(values)
    if not ordered:
        return 0.0
    middle = len(ordered) // 2
    return ordered[middle] if len(ordered) % 2 else \
        (ordered[middle - 1] + ordered[middle]) * 0.5


def _percentile(values, fraction):
    ordered = sorted(values)
    if not ordered:
        return 0.0
    return ordered[min(len(ordered) - 1, int(len(ordered) * fraction))]


def _classify_masks(width, height, id_pixels):
    roles = {name: bytearray(width * height) for name in
             ("rim", "sidewall", "void", "walnut", "facing", "cushion")}
    for index in range(width * height):
        r, g, b = id_pixels[index * 3:index * 3 + 3]
        maximum = max(r, g, b)
        if maximum < 40:
            continue
        if r > g * 1.45 and r > b * 1.45:
            roles["rim"][index] = 1
        elif g > r * 1.45 and g > b * 1.45:
            roles["sidewall"][index] = 1
        elif b > r * 1.45 and b > g * 1.45:
            roles["void"][index] = 1
        elif r > 80 and g > 80 and b < min(r, g) * 0.65:
            roles["walnut"][index] = 1
        elif r > 80 and b > 80 and g < min(r, b) * 0.65:
            roles["facing"][index] = 1
        elif g > 80 and b > 80 and r < min(g, b) * 0.65:
            roles["cushion"][index] = 1
    return roles


def _eroded_points(width, height, mask, roi):
    x0, y0, x1, y1 = roi
    result = []
    for y in range(y0, y1):
        for x in range(x0, x1):
            index = y * width + x
            if not mask[index]:
                continue
            if any(not mask[(y + dy) * width + x + dx]
                   for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1))
                   if 0 <= x + dx < width and 0 <= y + dy < height):
                continue
            result.append((x, y))
    return result


def _pixel_stats(points, width, pixels):
    rgbs = [tuple(pixels[(y * width + x) * 3:(y * width + x) * 3 + 3])
            for x, y in points]
    lumas = [0.2126 * r + 0.7152 * g + 0.0722 * b for r, g, b in rgbs]
    channel_spreads = [max(rgb) - min(rgb) for rgb in rgbs]
    return {"sample_count": len(points), "median_luma": _median(lumas),
            "p95_luma": _percentile(lumas, 0.95),
            "median_rgb": [_median([rgb[channel] for rgb in rgbs])
                           for channel in range(3)],
            "maximum_channel_spread": max(channel_spreads or [0])}


def object_id_measurements_v10g(width, height, id_pixels, color_pixels):
    base = legacy_object_id_measurements(width, height, id_pixels, color_pixels)
    roi = tuple(base["roi_px"])
    roles = _classify_masks(width, height, id_pixels)
    eroded = {name: _eroded_points(width, height, mask, roi)
              for name, mask in roles.items()}
    if any(not eroded[name] for name in ("rim", "sidewall", "void")):
        raise RuntimeError("eroded actual-GPU object-ID masks are incomplete")
    # Preserve geometry labels.  Under the frozen camera the local upper ring
    # projects to greater screen Y.  Merge the geometric endpoint quintile of
    # every eroded connected column; brightness is reported independently.
    by_x = defaultdict(list)
    for x, y in eroded["sidewall"]:
        by_x[x].append((y, x))
    upper_points, lower_points = [], []
    for column in by_x.values():
        column.sort()
        count = max(1, int(math.ceil(len(column) * 0.20)))
        lower_points.extend((x, y) for y, x in column[:count])
        upper_points.extend((x, y) for y, x in column[-count:])
    upper = _pixel_stats(upper_points, width, color_pixels)
    lower = _pixel_stats(lower_points, width, color_pixels)
    rim = _pixel_stats(eroded["rim"], width, color_pixels)
    void = _pixel_stats(eroded["void"], width, color_pixels)
    walnut = _pixel_stats(eroded["walnut"], width, color_pixels)
    side_count = len(eroded["sidewall"])
    sampled_columns = base["sampled_central_columns_x"]
    column_coverage = sum(
        any(x == target for x, _ in upper_points) and
        any(x == target for x, _ in lower_points)
        for target in sampled_columns)
    base["luma"] = {
        "rim_median": rim["median_luma"],
        "sidewall_upper_median": upper["median_luma"],
        "sidewall_lower_median": lower["median_luma"],
        "void_median": void["median_luma"],
        "void_p95": void["p95_luma"],
        "walnut_median": walnut["median_luma"],
    }
    base["sidewall_geometric_luma_bands"] = {
        "orientation": "local_upper_has_greater_screen_y",
        "upper": {**upper, "coverage_fraction": len(upper_points) / side_count},
        "lower": {**lower, "coverage_fraction": len(lower_points) / side_count},
        "brightness_order": [name for name, _ in sorted(
            (("geometric_upper", upper["median_luma"]),
             ("geometric_lower", lower["median_luma"])),
            key=lambda item: item[1], reverse=True)],
        "central_column_coverage": column_coverage,
        "central_columns_sampled": len(sampled_columns),
    }
    return base


def canonicalize_metrics(raw):
    required = ("renderer", "backend", "width", "height", "quality",
                "msaa_samples", "camera", "warmup_frames", "sample_frames",
                "pocket_only_ui_pass_draw_calls",
                "full_frame_ui_delta_nonzero_pixels", "p95_total_frame_ms",
                "draw_calls_max", "triangles_max", "texture_resident_mib",
                "measurement_method")
    missing = [key for key in required if key not in raw]
    if missing:
        raise RuntimeError("raw smoke metrics are incomplete: " + ",".join(missing))
    result = {key: raw[key] for key in required}
    result.update({
        "schema": "phase2_pocket_v10g_actual_gpu_metrics_v1",
        "evidence_scope": EVIDENCE_SCOPE,
        "visual_review_status": "NOT_RUN",
        "machine_status_before_execution": "NOT_RUN",
        "actual_gpu": True,
        "actual_gpu_capture_count": 1,
        "formal_pass_claimed": False,
        "golden_promotion_claimed": False,
        "ta_review_authorized": False,
        "formal_six_scene_evidence_authorized": False,
        "legacy_smoke_metadata_disposition": "discarded_and_not_inherited",
    })
    serialized = json.dumps(result)
    for token in ("phase2_pocket_v10c_single_scene_non_formal", "PENDING_TA",
                  "PENDING_ROOT_AND_TA"):
        if token in serialized:
            raise RuntimeError("canonical metrics retained stale metadata: " + token)
    return result


def _verify_freeze_receipt(path):
    receipt = json.loads(path.read_text(encoding="utf-8"))
    if receipt.get("status") != "BYTE_IDENTICAL_OFFLINE_PREFLIGHT_PASS_GPU_NOT_AUTHORIZED" or \
            receipt.get("binding_count") != 11 or \
            receipt.get("all_original_and_snapshot_sha256_match") is not True or \
            receipt.get("gpu_capture_authorized") is not False:
        raise RuntimeError("offline source freeze is not the authorized 11/11 package")
    freeze_root = path.parent
    for binding in receipt["bindings"]:
        original = ROOT / binding["original_path"]
        snapshot = freeze_root / binding["snapshot_path"]
        if original.read_bytes() != snapshot.read_bytes() or \
                sha256(original) != binding["original_sha256"] or \
                sha256(snapshot) != binding["snapshot_sha256"]:
            raise RuntimeError("offline freeze binding drift: " + binding["id"])
    return receipt


def pretrigger_checks(asset_root, executable, output, freeze_root):
    if output.exists() or freeze_root.exists():
        raise RuntimeError("single-use GPU evidence or freeze directory already exists")
    paths = {
        "gate": GATE, "offline_report": OFFLINE_REPORT,
        "offline_freeze": OFFLINE_FREEZE,
        "gltf": asset_root / ASSET_NAME,
        "asset_manifest": asset_root / MANIFEST_NAME,
        "renderer_source": ROOT / "src/Billiards/render/modern/modern_renderer.cpp",
        "smoke_source": ROOT / "spikes/render_core_profile/modern_renderer_slice_smoke.mm",
    }
    checks = {}
    for name, path in paths.items():
        checks[name + "_sha_exact"] = path.is_file() and \
            sha256(path) == "sha256:" + EXPECTED[name]
    if not all(checks.values()):
        raise RuntimeError("pre-trigger frozen SHA mismatch: " +
                           ",".join(name for name, passed in checks.items() if not passed))
    gate = json.loads(GATE.read_text(encoding="utf-8"))
    report = json.loads(OFFLINE_REPORT.read_text(encoding="utf-8"))
    if gate.get("GPU_CAPTURE_AUTHORIZED") is not False or \
            report.get("status") != "OFFLINE_PREFLIGHT_PASS_GPU_NOT_AUTHORIZED" or \
            report.get("machine_failures") != [] or \
            sum(report.get("machine_checks", {}).values()) != 30 or \
            len(report.get("machine_checks", {})) != 30 or \
            report.get("evidence_scope") != "phase2_pocket_v10g_offline_only" or \
            report.get("visual_review_status") != "NOT_RUN":
        raise RuntimeError("authorized offline PASS metadata is invalid")
    _verify_freeze_receipt(OFFLINE_FREEZE)
    document = json.loads(paths["gltf"].read_text(encoding="utf-8"))
    manifest = json.loads(paths["asset_manifest"].read_text(encoding="utf-8"))
    errors, validator_report = validate(document, manifest, asset_root,
                                        SHARED_ROOT, verify_hashes=True)
    if errors:
        raise RuntimeError("v10g asset validator failed: " + "; ".join(errors))
    if not executable.is_file() or not os.access(executable, os.X_OK):
        raise RuntimeError("fresh renderer executable is missing or not executable")
    newest_source = max(paths["renderer_source"].stat().st_mtime_ns,
                        paths["smoke_source"].stat().st_mtime_ns)
    if executable.stat().st_mtime_ns < newest_source:
        raise RuntimeError("renderer executable predates frozen renderer/smoke source")
    checks.update({
        "offline_status_exact": True, "offline_checks_30_of_30": True,
        "offline_freeze_11_of_11": True, "asset_validator_pass": True,
        "fresh_executable_exists": True, "fresh_executable_newer_than_sources": True,
        "output_single_use_absent": True, "freeze_single_use_absent": True,
        "metrics_scope_expected": EVIDENCE_SCOPE ==
            "phase2_pocket_v10g_single_scene_non_formal",
        "visual_review_pretrigger_not_run": True,
        "gpu_authorized_count_exactly_one_by_root": True,
    })
    return checks, validator_report, document, manifest, paths


def _runtime_asset_root(directory, asset_root):
    runtime = Path(directory)
    for source in SHARED_ROOT.iterdir():
        if not source.is_file():
            continue
        target = runtime / source.name
        if source.name in (ASSET_NAME, MANIFEST_NAME):
            continue
        target.symlink_to(source)
    shutil.copyfile(asset_root / ASSET_NAME, runtime / ASSET_NAME)
    shutil.copyfile(asset_root / MANIFEST_NAME, runtime / MANIFEST_NAME)
    return runtime


def _write_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n",
                    encoding="utf-8")


def _freeze_evidence(freeze_root, output, bindings, status):
    entries = list(bindings.items()) + [
        ("gpu_manifest", output / MANIFEST_NAME_OUT),
        ("gpu_capture", output / CAPTURE_NAME),
        ("gpu_metrics", output / METRICS_NAME),
        ("gpu_authorization_consumed", output / "gpu_capture_authorization_consumed.json"),
    ]
    receipt_bindings = []
    for identifier, original in entries:
        original = original.resolve()
        relative = original.relative_to(ROOT) if original.is_relative_to(ROOT) else \
            Path("external") / original.name
        snapshot_relative = Path("snapshot") / relative
        snapshot = freeze_root / snapshot_relative
        snapshot.parent.mkdir(parents=True, exist_ok=True)
        snapshot.write_bytes(original.read_bytes())
        receipt_bindings.append({
            "id": identifier,
            "original_path": str(relative),
            "original_sha256": sha256(original),
            "snapshot_path": str(snapshot_relative),
            "snapshot_sha256": sha256(snapshot),
            "byte_identical": original.read_bytes() == snapshot.read_bytes(),
        })
    receipt = {
        "schema": "phase2_pocket_v10g_gpu_source_freeze_receipt_v1",
        "status": "BYTE_IDENTICAL_" + status,
        "evidence_scope": EVIDENCE_SCOPE,
        "actual_gpu_capture_count": 1,
        "gpu_capture_authorized_for_rerun": False,
        "visual_review_status": "NOT_RUN",
        "ta_review_authorized": False,
        "formal_six_scene_evidence_authorized": False,
        "binding_count": len(receipt_bindings),
        "bindings": receipt_bindings,
        "all_original_and_snapshot_sha256_match": all(
            item["byte_identical"] and item["original_sha256"] == item["snapshot_sha256"]
            for item in receipt_bindings),
        "decision": "NO_RERUN; WAIT_FOR_ROOT",
    }
    _write_json(freeze_root / "receipt.json", receipt)
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--freeze-dir", type=Path, default=DEFAULT_FREEZE)
    parser.add_argument("--build-dir", type=Path,
                        default=ROOT / "build-phase2-v10g-gpu-session")
    parser.add_argument("--asset-root", type=Path, default=ASSET_ROOT)
    args = parser.parse_args()
    output = args.output_dir.resolve()
    freeze_root = args.freeze_dir.resolve()
    asset_root = args.asset_root.resolve()
    executable = (args.build_dir.resolve() / "BilliardsModernRendererSliceSmoke")
    prechecks, validator_report, document, asset_manifest, frozen_paths = \
        pretrigger_checks(asset_root, executable, output, freeze_root)

    # Atomically consume the one-shot authorization before starting the GPU
    # process.  A failed process still consumes the authorization.
    output.mkdir(parents=True, exist_ok=False)
    authorization_path = output / "gpu_capture_authorization_consumed.json"
    with authorization_path.open("x", encoding="utf-8") as destination:
        json.dump({"schema": "phase2_pocket_v10g_gpu_authorization_consumed_v1",
                   "authorized_capture_count": 1, "consumed_capture_count": 1,
                   "additional_capture_authorized": False,
                   "scene_id": "pocket_closeup", "width": 1920, "height": 1080,
                   "quality": "medium", "msaa_samples": 4},
                  destination, indent=2, sort_keys=True)
        destination.write("\n")

    with tempfile.TemporaryDirectory(prefix="billiardgl-v10g-runtime-") as directory:
        runtime_root = _runtime_asset_root(directory, asset_root)
        environment = os.environ.copy()
        environment["BILLIARDGL_POCKET_ONLY"] = "1"
        environment["BILLIARDGL_MODERN_PHASE2_ROOT"] = str(runtime_root)
        completed = subprocess.run([str(executable), str(output)], cwd=ROOT,
                                   env=environment, text=True, capture_output=True)
    if completed.returncode:
        raise RuntimeError("the single authorized actual-GPU process failed: " +
                           completed.stdout + completed.stderr)

    ppm_path = output / "pocket_closeup.ppm"
    object_id_path = output / "pocket_object_ids.ppm"
    width, height, pixels = read_ppm(ppm_path)
    id_width, id_height, id_pixels = read_ppm(object_id_path)
    if [width, height] != [1920, 1080] or [id_width, id_height] != [width, height]:
        raise RuntimeError("actual-GPU capture dimensions are not 1920x1080")
    object_id_sha = sha256(object_id_path)
    image = object_id_measurements_v10g(width, height, id_pixels, pixels)
    object_id_path.unlink()
    png_path = output / CAPTURE_NAME
    subprocess.run(["sips", "-s", "format", "png", str(ppm_path),
                    "--out", str(png_path)], check=True, capture_output=True, text=True)
    ppm_path.unlink()

    metrics_path = output / METRICS_NAME
    raw_metrics_bytes = metrics_path.read_bytes()
    raw_metrics = json.loads(raw_metrics_bytes)
    gpu_metrics = canonicalize_metrics(raw_metrics)
    gpu_metrics["legacy_raw_metrics_sha256_before_canonicalization"] = \
        "sha256:" + sha256_bytes(raw_metrics_bytes)
    _write_json(metrics_path, gpu_metrics)

    camera = gpu_metrics["camera"]
    distance_m = math.sqrt(sum((eye - target) ** 2 for eye, target in
                               zip(camera["eye_cm"], camera["target_cm"]))) / 100.0
    focal_pixels = height / (2.0 * math.tan(
        math.radians(camera["vertical_fov_degrees"] * 0.5)))
    projected_ball = focal_pixels * BALL_DIAMETER_M / distance_m
    projected_rim = projected_ball * POCKET_RIM_WIDTH_M / BALL_DIAMETER_M
    black = largest_black_component(width, height, pixels)
    black["projected_ball_diameter_px"] = projected_ball
    black["inward_extension_projected_ball_diameter"] = \
        black["inward_extension_px"] / projected_ball
    geometry = geometry_measurements(document)
    marker_indices = [index for index, node in enumerate(document["nodes"])
                      if node.get("name", "").startswith("SOCKET_")]
    marker_sha = marker_binding_sha256(document, marker_indices)
    luma_values = image["luma"]
    bands = image["sidewall_geometric_luma_bands"]
    checks = {
        **{"pretrigger_" + name: passed for name, passed in prechecks.items()},
        "actual_gpu_capture_count_exactly_one":
            gpu_metrics["actual_gpu_capture_count"] == 1,
        "viewport_1920x1080": [width, height] == [1920, 1080],
        "medium_4x_actual_gpu": gpu_metrics["quality"] == "medium" and
            gpu_metrics["msaa_samples"] == 4 and
            gpu_metrics["measurement_method"] == "gpu_timer_query_plus_cpu_frame",
        "warmup_120_and_samples_300": gpu_metrics["warmup_frames"] >= 120 and
            gpu_metrics["sample_frames"] >= 300,
        "luma_rim_40_to_52": 40 <= luma_values["rim_median"] <= 52,
        "luma_sidewall_upper_27_to_31":
            27 <= luma_values["sidewall_upper_median"] <= 31,
        "luma_sidewall_lower_8_to_11":
            8 <= luma_values["sidewall_lower_median"] <= 11,
        "luma_void_1_to_3_p95_at_most_4": 1 <= luma_values["void_median"] <= 3 and
            luma_values["void_p95"] <= 4,
        "ordered_luma_chain": luma_values["rim_median"] >
            luma_values["sidewall_upper_median"] >
            luma_values["sidewall_lower_median"] > luma_values["void_median"],
        "rim_minus_upper_at_least_10": luma_values["rim_median"] -
            luma_values["sidewall_upper_median"] >= 10,
        "upper_minus_lower_at_least_12": luma_values["sidewall_upper_median"] -
            luma_values["sidewall_lower_median"] >= 12,
        "lower_minus_void_at_least_5": luma_values["sidewall_lower_median"] -
            luma_values["void_median"] >= 5,
        "sidewall_neutral_channel_spread_at_most_12":
            bands["upper"]["maximum_channel_spread"] <= 12 and
            bands["lower"]["maximum_channel_spread"] <= 12,
        "sidewall_bands_each_128px_15percent_and_9_of_11":
            min(bands["upper"]["sample_count"], bands["lower"]["sample_count"]) >= 128 and
            min(bands["upper"]["coverage_fraction"], bands["lower"]["coverage_fraction"]) >= 0.15 and
            bands["central_columns_sampled"] == 11 and bands["central_column_coverage"] >= 9,
        "void_span_exact_1_20d": abs(geometry["void_minimum_span_m"] -
            BALL_DIAMETER_M * 1.20) <= 1e-6,
        "void_front_exact_1_00d": max(abs(value - POCKET_FRONT_Z_M)
            for value in geometry["void_front_z_range_m"]) <= 1e-6,
        "void_back_exact_2_20d": max(abs(value - POCKET_BACK_Z_M)
            for value in geometry["void_back_z_range_m"]) <= 1e-6,
        "rim_sidewall_drop_position_matched_0_25mm":
            geometry["rim_sidewall_drop_boundary_error_max_m"] <= 0.00025,
        "rim_width_frozen": abs(geometry["rim_width_m"] - 0.010287) <= 1e-9,
        "sidewall_drop_frozen": abs(geometry["sidewall_vertical_drop_m"] - 0.02286) <= 1e-9,
        "rounded_arc_segments_by_lod":
            geometry["minimum_arc_segments_per_half_by_lod"] == {"0": 24, "1": 16, "2": 10},
        "drop_has_five_distinct_widths": all(row["distinct_widths_0_5mm"] >= 5
            for row in geometry["drop_width_profiles"]),
        "drop_bottom_mouth_ratio_0_55_to_0_85": all(
            0.55 <= row["bottom_to_mouth_width_ratio"] <= 0.85
            for row in geometry["drop_width_profiles"]),
        "walnut_does_not_enter_cavity": geometry["walnut_apron_maximum_cavity_z_m"] <= 1e-6,
        "connected_stage_height_at_least_72px":
            image["connected_stage_component"]["height_px"] >= 72,
        "rim_geometry_11_to_13_all_10_to_14px":
            11 <= image["rim_visible_run_px"]["median"] <= 13 and
            all(10 <= value <= 14 for value in image["rim_visible_run_px"]["samples"]),
        "sidewall_geometry_median_18_to_24_p10_16_min_14px":
            18 <= image["sidewall_visible_run_px"]["median"] <= 24 and
            image["sidewall_visible_run_px"]["p10"] >= 16 and
            image["sidewall_visible_run_px"]["minimum"] >= 14,
        "ordered_stage_columns_11_of_11":
            image["ordered_stage_columns"]["sampled"] == 11 and
            image["ordered_stage_columns"]["passing"] == 11,
        "void_is_not_rectangular_slot": image["void_rectangularity"] < 0.85,
        "walnut_area_inside_envelope_at_most_10_percent":
            image["walnut_area_inside_mouth_envelope_fraction"] <= 0.10,
        "walnut_intrusion_max_8px_median_0px":
            image["walnut_intrusion_px"]["maximum"] <= 8 and
            image["walnut_intrusion_px"]["median"] <= 0,
        "pocket_ui_and_full_frame_delta_zero":
            gpu_metrics["pocket_only_ui_pass_draw_calls"] == 0 and
            gpu_metrics["full_frame_ui_delta_nonzero_pixels"] == 0,
        "gold_bar_absent": image["gold_component_pixels_bottom_band"] == 0,
        "physics_marker_binding_frozen": marker_sha ==
            "sha256:1b8f8cdb03c88e81721525aa0a134c4e4efa564975c5e2307835514f03df2b7a" ==
            document["asset"]["extras"]["physics_marker_binding_sha256"] ==
            asset_manifest["alignment_binding"]["physics_marker_binding_sha256"],
        "visual_marker_z_mapping_exact":
            asset_manifest["alignment_binding"]["visual_to_marker_frame"]["z_visual"] == "-z_marker",
        "p95_frame_under_16_667ms": gpu_metrics["p95_total_frame_ms"] < 16.667,
        "metadata_scope_exact_and_no_review_claim":
            gpu_metrics["evidence_scope"] == EVIDENCE_SCOPE and
            gpu_metrics["visual_review_status"] == "NOT_RUN" and
            gpu_metrics["ta_review_authorized"] is False,
    }
    failures = [name for name, passed in checks.items() if not passed]
    status = "MACHINE_PASS_PENDING_ROOT_AND_TA" if not failures else "REJECTED_MACHINE"

    binding_files = {
        "production_gltf": asset_root / ASSET_NAME,
        "production_asset_manifest": asset_root / MANIFEST_NAME,
        "shared_pbr_parameters": SHARED_ROOT / "focused_practice_hall_pbr_v1.params.json",
        "shared_lighting_rig": SHARED_ROOT / "focused_practice_hall_lighting_rig_v1.json",
        "shared_venue_gltf": SHARED_ROOT / "focused_practice_hall_venue_v1.gltf",
        "renderer_executable": executable,
        "renderer_smoke_source": frozen_paths["smoke_source"],
        "renderer_source": frozen_paths["renderer_source"],
        "generator": ROOT / "scripts/generate_phase2_modular_gltf_v10g.py",
        "validator": ROOT / "scripts/validate_phase2_modular_gltf_v10g.py",
        "calibrator": ROOT / "scripts/calibrate_phase2_pocket_v10g.py",
        "gpu_preflight_runner": Path(__file__).resolve(),
        "gpu_runner_tests": ROOT / "tests/visual/test_phase2_pocket_v10g_gpu_runner.py",
        "design_gate_v2": GATE,
        "offline_report": OFFLINE_REPORT,
        "offline_freeze_receipt": OFFLINE_FREEZE,
    }
    manifest = {
        "schema": "phase2_pocket_v10g_actual_gpu_preflight_manifest_v1",
        "status": status,
        "evidence_scope": EVIDENCE_SCOPE,
        "machine_status_before_execution": "NOT_RUN",
        "visual_review_status": "NOT_RUN",
        "actual_gpu_capture_count": 1,
        "additional_gpu_capture_authorized": False,
        "formal_pass_claimed": False,
        "golden_promotion_claimed": False,
        "ta_review_authorized": False,
        "formal_six_scene_evidence_authorized": False,
        "production_binding": {name: {"path": str(path.relative_to(ROOT))
            if path.is_relative_to(ROOT) else str(path), "sha256": sha256(path)}
            for name, path in binding_files.items()},
        "pretrigger_checks": prechecks,
        "capture": {"scene_id": "pocket_closeup", "file": CAPTURE_NAME,
                    "sha256": sha256(png_path), "width": width, "height": height,
                    "quality": "medium", "msaa_samples": 4, "actual_gpu": True},
        "object_id_diagnostic": {"actual_gpu": True, "retained": False,
            "sha256_before_discard": object_id_sha,
            "purpose": "machine ownership and luma masks only"},
        "gpu_metrics_file": {"file": METRICS_NAME, "sha256": sha256(metrics_path)},
        "gpu_metrics": gpu_metrics,
        "physics_marker_binding_sha256": marker_sha,
        "geometry_measurements": geometry,
        "image_measurements": {"connected_pure_black_cavity": black,
            "authored_rim_scale_equivalent_projection_px": projected_rim,
            "object_id": image},
        "machine_checks": checks,
        "machine_failures": failures,
        "offline_validator_report": validator_report,
        "review_instruction": "Machine rejection blocks TA and all further capture. Retained PNG is diagnostic only."
            if failures else "Machine PASS permits only root review; TA remains NOT_AUTHORIZED until separately granted.",
        "decision": "NO_RERUN; WAIT_FOR_ROOT",
    }
    manifest_path = output / MANIFEST_NAME_OUT
    _write_json(manifest_path, manifest)
    freeze_root.mkdir(parents=True, exist_ok=False)
    receipt = _freeze_evidence(freeze_root, output, binding_files, status)
    if not receipt["all_original_and_snapshot_sha256_match"]:
        raise RuntimeError("GPU evidence freeze is not byte-identical")
    for root in (output, freeze_root):
        for path in sorted(root.rglob("*"), reverse=True):
            if path.is_file():
                path.chmod(0o444)
            elif path.is_dir():
                path.chmod(0o555)
        root.chmod(0o555)
    print(status)
    print("manifest=" + str(manifest_path))
    print("manifest_sha256=" + sha256(manifest_path))
    print("capture_sha256=" + sha256(png_path))
    print("metrics_sha256=" + sha256(metrics_path))
    print("freeze_receipt_sha256=" + sha256(freeze_root / "receipt.json"))
    print("machine_failures=" + json.dumps(failures))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError, RuntimeError,
            subprocess.SubprocessError, json.JSONDecodeError) as error:
        print("phase2 pocket v10g actual-GPU preflight failed: " + str(error),
              file=sys.stderr)
        raise SystemExit(1)
