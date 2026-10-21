#!/usr/bin/env python3
"""Create one NON_FORMAL actual-GPU pocket v10c review candidate.

This runner deliberately emits only ``pocket_closeup``.  It performs offline
asset validation before rendering, measures the authored cavity directly from
the production glTF, and records image-space black-cavity and liner-projection
gates without making a TA or formal-pass claim.
"""

import argparse
import base64
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import sys

from generate_m1_graybox_gltf import PLAY_SURFACE_Y_M, POCKETS
from generate_phase2_modular_gltf import (
    ASSET_NAME,
    BALL_DIAMETER_M,
    MANIFEST_NAME,
    POCKET_BACK_Z_M,
    POCKET_FRONT_Z_M,
    POCKET_LINER_WIDTH_M,
    POCKET_MIN_SPAN_M,
    POCKET_RIM_WIDTH_M,
    POCKET_SIDEWALL_DROP_M,
    POCKET_SIDE_X_MARGIN_M,
    POCKET_Z_MARGIN_M,
    marker_binding_sha256,
)
from validate_phase2_modular_gltf import validate


ROOT = Path(__file__).resolve().parents[1]
METRICS_NAME = "pocket_gpu_metrics.json"
CAPTURE_NAME = "pocket_closeup.png"
MANIFEST_OUTPUT_NAME = "phase2-pocket-v10c-preflight-manifest-v1.json"


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return "sha256:" + digest.hexdigest()


def read_ppm(path):
    header, dimensions, maximum, pixels = path.read_bytes().split(b"\n", 3)
    if header != b"P6" or maximum != b"255":
        raise RuntimeError("capture is not an 8-bit binary PPM")
    width, height = map(int, dimensions.split())
    if len(pixels) != width * height * 3:
        raise RuntimeError("capture PPM payload length differs from dimensions")
    return width, height, pixels


def largest_black_component(width, height, pixels, threshold=4):
    x0, x1 = int(width * 0.28), int(width * 0.58)
    y0, y1 = int(height * 0.70), int(height * 0.94)
    roi_width = x1 - x0
    visited = bytearray(roi_width * (y1 - y0))
    largest = None
    for y in range(y0, y1):
        for x in range(x0, x1):
            local = (y - y0) * roi_width + x - x0
            pixel = (y * width + x) * 3
            if visited[local] or max(pixels[pixel:pixel + 3]) > threshold:
                continue
            visited[local] = 1
            pending = [(x, y)]
            count = 0
            minimum_x = maximum_x = x
            minimum_y = maximum_y = y
            while pending:
                current_x, current_y = pending.pop()
                count += 1
                minimum_x, maximum_x = min(minimum_x, current_x), max(maximum_x, current_x)
                minimum_y, maximum_y = min(minimum_y, current_y), max(maximum_y, current_y)
                for next_x, next_y in ((current_x - 1, current_y),
                                       (current_x + 1, current_y),
                                       (current_x, current_y - 1),
                                       (current_x, current_y + 1)):
                    if not (x0 <= next_x < x1 and y0 <= next_y < y1):
                        continue
                    next_local = (next_y - y0) * roi_width + next_x - x0
                    next_pixel = (next_y * width + next_x) * 3
                    if visited[next_local] or \
                            max(pixels[next_pixel:next_pixel + 3]) > threshold:
                        continue
                    visited[next_local] = 1
                    pending.append((next_x, next_y))
            component = {
                "pixel_count": count,
                "bbox_px": [minimum_x, minimum_y, maximum_x, maximum_y],
                "width_px": maximum_x - minimum_x + 1,
                "inward_extension_px": maximum_y - minimum_y + 1,
            }
            if largest is None or count > largest["pixel_count"]:
                largest = component
    if largest is None:
        raise RuntimeError("no connected pure-black pocket component found")
    largest["threshold_max_rgb"] = threshold
    largest["connectivity"] = 4
    largest["roi_px"] = [x0, y0, x1, y1]
    return largest


def _median(values):
    ordered = sorted(values)
    if not ordered:
        return 0.0
    middle = len(ordered) // 2
    return ordered[middle] if len(ordered) % 2 else \
        (ordered[middle - 1] + ordered[middle]) * 0.5


def object_id_measurements(width, height, id_pixels, color_pixels):
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

    roi = (537, 756, 1113, 1015)
    x0, y0, x1, y1 = roi
    points = {}
    for name, mask in roles.items():
        points[name] = [(x, y) for y in range(y0, y1) for x in range(x0, x1)
                        if mask[y * width + x]]
    if any(not points[name] for name in ("rim", "sidewall", "void")):
        raise RuntimeError("object-ID pocket stages are not all visible")

    combined = bytearray(width * height)
    for index in range(width * height):
        combined[index] = roles["rim"][index] or roles["sidewall"][index] or \
            roles["void"][index] or roles["facing"][index]
    component = largest_mask_component(width, height, combined, roi)
    stage_points = points["rim"] + points["sidewall"] + points["void"]
    stage_x = [point[0] for point in stage_points]
    stage_bbox = [min(stage_x), min(point[1] for point in stage_points),
                  max(stage_x), max(point[1] for point in stage_points)]
    stage_width = stage_bbox[2] - stage_bbox[0] + 1
    central_x0 = stage_bbox[0] + int(stage_width * 0.20)
    central_x1 = stage_bbox[2] - int(stage_width * 0.20)

    def vertical_runs(mask):
        runs = []
        for x in range(central_x0, central_x1 + 1):
            current = best = 0
            for y in range(y0, y1):
                if mask[y * width + x]:
                    current += 1
                    best = max(best, current)
                else:
                    current = 0
            if best:
                runs.append(best)
        return runs

    rim_runs = vertical_runs(roles["rim"])
    side_runs = vertical_runs(roles["sidewall"])
    ordered_columns = 0
    sampled_columns = 0
    for x in range(central_x0, central_x1 + 1):
        stage_rows = [[y for y in range(y0, y1) if roles[name][y * width + x]]
                      for name in ("rim", "sidewall", "void")]
        if not all(stage_rows):
            continue
        sampled_columns += 1
        gaps = []
        for first, second in zip(stage_rows, stage_rows[1:]):
            gaps.append(min(abs(a - b) for a in first for b in second) - 1)
        if max(gaps) <= 1:
            ordered_columns += 1

    def lumas(mask, y_range=None):
        values = []
        minimum_y, maximum_y = y_range or (y0, y1)
        for y in range(max(y0, minimum_y), min(y1, maximum_y)):
            for x in range(x0, x1):
                index = y * width + x
                if not mask[index]:
                    continue
                # One-pixel erosion excludes MSAA boundaries.
                if any(not mask[(y + dy) * width + x + dx]
                       for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1))
                       if 0 <= x + dx < width and 0 <= y + dy < height):
                    continue
                r, g, b = color_pixels[index * 3:index * 3 + 3]
                values.append(0.2126 * r + 0.7152 * g + 0.0722 * b)
        return values

    side_y = [point[1] for point in points["sidewall"]]
    side_min, side_max = min(side_y), max(side_y)
    third = max(1, (side_max - side_min + 1) // 3)
    side_top = lumas(roles["sidewall"], (side_min, side_min + third))
    side_bottom = lumas(roles["sidewall"], (side_max - third + 1, side_max + 1))
    void_lumas = lumas(roles["void"])
    rim_lumas = lumas(roles["rim"])
    walnut_lumas = lumas(roles["walnut"])
    upper_median, lower_median = _median(side_top), _median(side_bottom)
    if upper_median < lower_median:
        upper_median, lower_median = lower_median, upper_median
    envelope_pixels = component["width_px"] * component["height_px"]
    walnut_inside = sum(1 for x, y in points["walnut"]
                        if component["bbox_px"][0] <= x <= component["bbox_px"][2] and
                        component["bbox_px"][1] <= y <= component["bbox_px"][3])
    walnut_intrusions = []
    for y in range(component["bbox_px"][1], component["bbox_px"][3] + 1):
        stage_x = [x for x in range(component["bbox_px"][0],
                                    component["bbox_px"][2] + 1)
                   if combined[y * width + x]]
        if not stage_x:
            continue
        left, right = min(stage_x), max(stage_x)
        walnut_intrusions.extend((
            sum(1 for x in range(left, right + 1)
                if roles["walnut"][y * width + x] and x < (left + right) // 2),
            sum(1 for x in range(left, right + 1)
                if roles["walnut"][y * width + x] and x >= (left + right) // 2),
        ))
    gold_pixels = 0
    for y in range(1000, 1080):
        for x in range(width):
            r, g, b = color_pixels[(y * width + x) * 3:(y * width + x) * 3 + 3]
            if r >= 150 and g >= 95 and b <= 100 and r > 1.15 * g and g > 1.20 * b:
                gold_pixels += 1
    return {
        "roi_px": list(roi),
        "stage_pixel_counts": {name: len(points[name]) for name in points},
        "connected_stage_component": component,
        "all_stage_bbox_px": stage_bbox,
        "rim_visible_run_px": {"median": _median(rim_runs),
                                "minimum": min(rim_runs or [0]),
                                "maximum": max(rim_runs or [0])},
        "sidewall_visible_run_px": {"median": _median(side_runs),
                                     "p10": sorted(side_runs)[len(side_runs) // 10]
                                        if side_runs else 0,
                                     "minimum": min(side_runs or [0]),
                                     "maximum": max(side_runs or [0])},
        "ordered_stage_columns": {"passing": ordered_columns,
                                  "sampled": sampled_columns,
                                  "fraction": ordered_columns / max(1, sampled_columns)},
        "void_rectangularity": len(points["void"]) /
            max(1, ((max(x for x, _ in points["void"]) - min(x for x, _ in points["void"]) + 1) *
                    (max(y for _, y in points["void"]) - min(y for _, y in points["void"]) + 1))),
        "walnut_area_inside_mouth_envelope_fraction": walnut_inside / max(1, envelope_pixels),
        "walnut_intrusion_px": {"maximum": max(walnut_intrusions or [0]),
                                 "median": _median(walnut_intrusions)},
        "luma": {"rim_median": _median(rim_lumas),
                 "sidewall_upper_median": upper_median,
                 "sidewall_lower_median": lower_median,
                 "void_median": _median(void_lumas),
                 "void_p95": sorted(void_lumas)[int(len(void_lumas) * 0.95)]
                    if void_lumas else 0.0,
                 "walnut_median": _median(walnut_lumas)},
        "gold_component_pixels_bottom_band": gold_pixels,
    }


def largest_mask_component(width, height, mask, roi):
    x0, y0, x1, y1 = roi
    visited = bytearray(mask)
    largest = []
    for y in range(y0, y1):
        for x in range(x0, x1):
            index = y * width + x
            if not visited[index]:
                continue
            visited[index] = 0
            pending = [(x, y)]
            component = []
            while pending:
                px, py = pending.pop()
                component.append((px, py))
                for nx, ny in ((px - 1, py), (px + 1, py),
                               (px, py - 1), (px, py + 1)):
                    if x0 <= nx < x1 and y0 <= ny < y1:
                        next_index = ny * width + nx
                        if visited[next_index]:
                            visited[next_index] = 0
                            pending.append((nx, ny))
            if len(component) > len(largest):
                largest = component
    if not largest:
        raise RuntimeError("no connected pocket object-ID component")
    minimum_x = min(x for x, _ in largest)
    maximum_x = max(x for x, _ in largest)
    minimum_y = min(y for _, y in largest)
    maximum_y = max(y for _, y in largest)
    return {"pixel_count": len(largest),
            "bbox_px": [minimum_x, minimum_y, maximum_x, maximum_y],
            "width_px": maximum_x - minimum_x + 1,
            "height_px": maximum_y - minimum_y + 1}


def position_values(document, payload, node):
    mesh = document["meshes"][node["mesh"]]
    accessor = document["accessors"][
        mesh["primitives"][0]["attributes"]["POSITION"]]
    view = document["bufferViews"][accessor["bufferView"]]
    offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    stride = view.get("byteStride", 12)
    return [struct.unpack_from("<3f", payload, offset + index * stride)
            for index in range(accessor["count"])]


def geometry_measurements(document):
    encoded = document["buffers"][0]["uri"]
    prefix = "data:application/octet-stream;base64,"
    if not encoded.startswith(prefix):
        raise RuntimeError("production glTF buffer is not embedded")
    payload = base64.b64decode(encoded[len(prefix):], validate=True)
    pockets = {item[0]: item for item in POCKETS}
    samples = []
    apron_maximum_z = -math.inf
    liner_minimum_screen_px = math.inf
    for node in document["nodes"]:
        role = node.get("extras", {}).get("asset_role")
        if role not in {"pocket_void_visual_only",
                        "pocket_liner_rim_visual_only",
                        "pocket_liner_sidewall_visual_only",
                        "pocket_apron_integrated_wedge"}:
            continue
        positions = position_values(document, payload, node)
        for item in node["extras"]["local_ranges"]:
            pocket = pockets[item["pocket_id"]]
            center, inward, tangent, mouth_width = pocket[3], pocket[4], pocket[5], pocket[6]
            local = []
            start, end = item["vertex_start"], item["vertex_start"] + item["vertex_count"]
            for x, y, z in positions[start:end]:
                dx, dz = x - center[0], z - center[1]
                local.append((dx * tangent[0] + dz * tangent[1],
                              y - PLAY_SURFACE_Y_M,
                              dx * inward[0] + dz * inward[1]))
            row = {"lod": node["extras"]["lod"], "pocket_id": item["pocket_id"],
                   "role": role}
            depths = [value[2] for value in local]
            if role == "pocket_void_visual_only":
                width = max(value[0] for value in local) - min(value[0] for value in local)
                row.update({"front_z_m": min(depths), "back_z_m": max(depths),
                            "span_m": max(depths) - min(depths), "width_m": width,
                            "depth_width_ratio": (max(depths) - min(depths)) / width})
            elif role == "pocket_liner_rim_visual_only":
                row.update({"minimum_z_m": min(depths),
                            "maximum_z_m": max(depths),
                            "authored_rim_width_m":
                                node["extras"]["authored_rim_width_m"],
                            "arc_segments_per_half":
                                node["extras"]["arc_segments_per_half"]})
            elif role == "pocket_liner_sidewall_visual_only":
                row.update({"minimum_z_m": min(depths),
                            "maximum_z_m": max(depths),
                            "authored_vertical_drop_m":
                                node["extras"]["authored_vertical_drop_m"],
                            "arc_segments_per_half":
                                node["extras"]["arc_segments_per_half"]})
            else:
                row["maximum_z_m"] = max(depths)
                apron_maximum_z = max(apron_maximum_z, row["maximum_z_m"])
            samples.append(row)
    void_rows = [row for row in samples if row["role"] == "pocket_void_visual_only"]
    rim_rows = [row for row in samples if row["role"] ==
                "pocket_liner_rim_visual_only"]
    sidewall_rows = [row for row in samples if row["role"] ==
                     "pocket_liner_sidewall_visual_only"]
    boundary_errors = []
    width_profiles = []
    by_key = {}
    for node in document["nodes"]:
        role = node.get("extras", {}).get("asset_role")
        if role not in {"pocket_liner_rim_visual_only",
                        "pocket_liner_sidewall_visual_only",
                        "pocket_void_visual_only"}:
            continue
        lod = node["extras"]["lod"]
        for item in node["extras"]["local_ranges"]:
            entry = by_key.setdefault((lod, item["pocket_id"]), {})
            if role == "pocket_liner_rim_visual_only":
                entry["rim_inner"] = item["inner_boundary"]
            elif role == "pocket_liner_sidewall_visual_only":
                entry["side_upper"] = item["upper_boundary"]
                entry["side_lower"] = item["lower_boundary"]
            else:
                entry["drop"] = item["boundary"]
    for entry in by_key.values():
        for first, second in ((entry["rim_inner"], entry["side_upper"]),
                              (entry["side_lower"], entry["drop"])):
            boundary_errors.append(max(math.hypot(a[0] - b[0], a[1] - b[1])
                                       for a, b in zip(first, second)))
        widths = [abs(point[0]) * 2.0 for point in entry["drop"]]
        mouth_width = max(abs(point[0]) * 2.0 for point in entry["side_upper"])
        width_profiles.append({
            "distinct_widths_0_5mm": len({round(value / 0.0005)
                                           for value in widths}),
            "rectangularity_proxy_min_max_width_ratio":
                min(value for value in widths if value > 0.0005) / max(widths),
            "bottom_to_mouth_width_ratio": max(widths) / mouth_width,
        })
    return {
        "coordinate_frame": {
            "x_visual": "physics_tangent", "y_visual": "world_up",
            "z_visual": "cavity_inward", "z_visual_to_marker": "-z_marker",
            "visual_handedness": "right",
        },
        "sample_count": len(samples),
        "void_sample_count": len(void_rows),
        "rim_sample_count": len(rim_rows),
        "sidewall_sample_count": len(sidewall_rows),
        "void_minimum_span_m": min(row["span_m"] for row in void_rows),
        "void_minimum_span_d": min(row["span_m"] for row in void_rows) / BALL_DIAMETER_M,
        "void_minimum_depth_width_ratio": min(row["depth_width_ratio"] for row in void_rows),
        "void_front_z_range_m": [min(row["front_z_m"] for row in void_rows),
                                  max(row["front_z_m"] for row in void_rows)],
        "void_back_z_range_m": [min(row["back_z_m"] for row in void_rows),
                                 max(row["back_z_m"] for row in void_rows)],
        "rim_width_m": POCKET_RIM_WIDTH_M,
        "sidewall_vertical_drop_m": POCKET_SIDEWALL_DROP_M,
        "rim_sidewall_drop_boundary_error_max_m": max(boundary_errors),
        "minimum_arc_segments_per_half_by_lod": {
            str(lod): min(row["arc_segments_per_half"] for row in rim_rows
                          if row["lod"] == lod) for lod in (0, 1, 2)},
        "drop_width_profiles": width_profiles,
        "walnut_apron_maximum_cavity_z_m": apron_maximum_z,
        "samples": samples,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build-product-session")
    parser.add_argument("--asset-root", type=Path, default=ROOT / "assets/modern/phase2")
    parser.add_argument("--resume-existing-capture", action="store_true",
                        help="Finish a failed analysis without issuing another GPU capture")
    arguments = parser.parse_args()
    output = arguments.output_dir.resolve()
    if output.exists() and not arguments.resume_existing_capture:
        raise RuntimeError("output directory already exists; use a new evidence directory")
    if not output.exists():
        output.mkdir(parents=True)

    asset_root = arguments.asset_root.resolve()
    gltf_path = asset_root / ASSET_NAME
    asset_manifest_path = asset_root / MANIFEST_NAME
    document = json.loads(gltf_path.read_text(encoding="utf-8"))
    asset_manifest = json.loads(asset_manifest_path.read_text(encoding="utf-8"))
    errors, validator_report = validate(document, asset_manifest, asset_root)
    if errors:
        raise RuntimeError("offline production asset validation failed: " + "; ".join(errors))

    executable = (arguments.build_dir / "BilliardsModernRendererSliceSmoke").resolve()
    if not executable.is_file():
        raise RuntimeError("missing renderer smoke executable: " + str(executable))
    environment = os.environ.copy()
    environment["BILLIARDGL_POCKET_ONLY"] = "1"
    environment["BILLIARDGL_MODERN_PHASE2_ROOT"] = str(asset_root)
    if not arguments.resume_existing_capture:
        completed = subprocess.run([str(executable), str(output)], cwd=ROOT,
                                   env=environment, text=True, capture_output=True)
        if completed.returncode:
            raise RuntimeError("actual-GPU capture failed: " + completed.stdout + completed.stderr)
    elif not (output / "pocket_closeup.ppm").is_file() or \
            not (output / "pocket_object_ids.ppm").is_file():
        raise RuntimeError("resume requires the original closeup and object-ID PPM files")

    ppm_path = output / "pocket_closeup.ppm"
    width, height, pixels = read_ppm(ppm_path)
    object_id_path = output / "pocket_object_ids.ppm"
    id_width, id_height, id_pixels = read_ppm(object_id_path)
    object_id_sha256 = sha256(object_id_path)
    if [id_width, id_height] != [width, height]:
        raise RuntimeError("object-ID capture dimensions differ from closeup")
    image_measurements = object_id_measurements(
        width, height, id_pixels, pixels)
    object_id_path.unlink()
    png_path = output / CAPTURE_NAME
    subprocess.run(["sips", "-s", "format", "png", str(ppm_path), "--out", str(png_path)],
                   check=True, text=True, capture_output=True)
    ppm_path.unlink()
    if width != 1920 or height != 1080 or not png_path.is_file():
        raise RuntimeError("pocket capture is not the required 1920x1080 PNG")

    metrics_path = output / METRICS_NAME
    gpu_metrics = json.loads(metrics_path.read_text(encoding="utf-8"))
    camera = gpu_metrics["camera"]
    distance_m = math.sqrt(sum((eye - target) ** 2 for eye, target in
                               zip(camera["eye_cm"], camera["target_cm"]))) / 100.0
    focal_pixels = height / (2.0 * math.tan(
        math.radians(camera["vertical_fov_degrees"] * 0.5)))
    projected_ball_diameter = focal_pixels * BALL_DIAMETER_M / distance_m
    projected_liner_occlusion = projected_ball_diameter * \
        POCKET_RIM_WIDTH_M / BALL_DIAMETER_M
    black_component = largest_black_component(width, height, pixels)
    black_component["projected_ball_diameter_px"] = projected_ball_diameter
    black_component["inward_extension_projected_ball_diameter"] = \
        black_component["inward_extension_px"] / projected_ball_diameter

    geometry = geometry_measurements(document)
    marker_indices = [index for index, node in enumerate(document["nodes"])
                      if node.get("name", "").startswith("SOCKET_")]
    actual_marker_binding = marker_binding_sha256(document, marker_indices)
    machine_checks = {
        "viewport_1920x1080": [width, height] == [1920, 1080],
        "medium_4x_actual_gpu": gpu_metrics.get("quality") == "medium" and
            gpu_metrics.get("msaa_samples") == 4 and
            gpu_metrics.get("measurement_method") == "gpu_timer_query_plus_cpu_frame",
        "warmup_and_sample_floor": gpu_metrics.get("warmup_frames", 0) >= 120 and
            gpu_metrics.get("sample_frames", 0) >= 300,
        "void_span_exact_1_20d": abs(geometry["void_minimum_span_m"] -
            BALL_DIAMETER_M * 1.20) <= 1e-6,
        "void_front_exact_1_00d": max(abs(value - POCKET_FRONT_Z_M)
            for value in geometry["void_front_z_range_m"]) <= 1e-6,
        "void_back_exact_2_20d": max(abs(value - POCKET_BACK_Z_M)
            for value in geometry["void_back_z_range_m"]) <= 1e-6,
        "rim_sidewall_drop_position_matched_0_25mm":
            geometry["rim_sidewall_drop_boundary_error_max_m"] <= 0.00025,
        "rim_width_0_14_to_0_18d": BALL_DIAMETER_M * 0.14 <=
            geometry["rim_width_m"] <= BALL_DIAMETER_M * 0.18,
        "sidewall_drop_0_40_to_0_65d": BALL_DIAMETER_M * 0.40 <=
            geometry["sidewall_vertical_drop_m"] <= BALL_DIAMETER_M * 0.65,
        "rounded_arc_segments_by_lod":
            geometry["minimum_arc_segments_per_half_by_lod"] ==
            {"0": 24, "1": 16, "2": 10},
        "drop_has_five_distinct_widths": all(
            row["distinct_widths_0_5mm"] >= 5
            for row in geometry["drop_width_profiles"]),
        "drop_bottom_mouth_ratio_0_55_to_0_85": all(
            0.55 <= row["bottom_to_mouth_width_ratio"] <= 0.85
            for row in geometry["drop_width_profiles"]),
        "walnut_does_not_enter_cavity":
            geometry["walnut_apron_maximum_cavity_z_m"] <= 1e-6,
        "connected_object_id_coverage_at_least_72px":
            image_measurements["connected_stage_component"]["height_px"] >= 72,
        "rim_projected_visible_10_to_14px": 10 <=
            image_measurements["rim_visible_run_px"]["median"] <= 14,
        "sidewall_projected_visible_14_to_28px": 14 <=
            image_measurements["sidewall_visible_run_px"]["median"] <= 28 and
            image_measurements["sidewall_visible_run_px"]["p10"] >= 12 and
            image_measurements["sidewall_visible_run_px"]["minimum"] >= 10,
        "ordered_stage_columns_at_least_95_percent":
            image_measurements["ordered_stage_columns"]["fraction"] >= 0.95,
        "void_is_not_rectangular_slot":
            image_measurements["void_rectangularity"] < 0.85,
        "walnut_area_inside_envelope_at_most_10_percent":
            image_measurements["walnut_area_inside_mouth_envelope_fraction"] <= 0.10,
        "walnut_intrusion_max_8px_median_6px":
            image_measurements["walnut_intrusion_px"]["maximum"] <= 8 and
            image_measurements["walnut_intrusion_px"]["median"] <= 6,
        "post_tonemap_luma_chain":
            26 <= image_measurements["luma"]["rim_median"] <= 52 and
            image_measurements["luma"]["sidewall_upper_median"] >= 22 and
            9 <= image_measurements["luma"]["sidewall_lower_median"] <= 24 and
            image_measurements["luma"]["void_median"] <= 6 and
            image_measurements["luma"]["void_p95"] <= 10 and
            image_measurements["luma"]["rim_median"] -
                image_measurements["luma"]["void_median"] >= 18 and
            image_measurements["luma"]["sidewall_upper_median"] -
                image_measurements["luma"]["sidewall_lower_median"] >= 8 and
            image_measurements["luma"]["sidewall_lower_median"] -
                image_measurements["luma"]["void_median"] >= 5,
        "pocket_only_ui_draws_zero":
            gpu_metrics.get("pocket_only_ui_pass_draw_calls") == 0,
        "full_frame_ui_delta_zero":
            gpu_metrics.get("full_frame_ui_delta_nonzero_pixels") == 0,
        "gold_bar_absent":
            image_measurements["gold_component_pixels_bottom_band"] == 0,
        "physics_marker_binding_unchanged": actual_marker_binding ==
            "sha256:1b8f8cdb03c88e81721525aa0a134c4e4efa564975c5e2307835514f03df2b7a" ==
            document["asset"]["extras"]["physics_marker_binding_sha256"] ==
            asset_manifest["alignment_binding"]["physics_marker_binding_sha256"],
        "visual_marker_z_mapping_exact":
            asset_manifest["alignment_binding"]["visual_to_marker_frame"]["z_visual"] ==
            "-z_marker",
    }
    failed = [name for name, passed in machine_checks.items() if not passed]

    binding_files = {
        "production_gltf": gltf_path,
        "production_asset_manifest": asset_manifest_path,
        "pbr_parameters": asset_root / "focused_practice_hall_pbr_v1.params.json",
        "lighting_rig": asset_root / "focused_practice_hall_lighting_rig_v1.json",
        "venue_gltf": asset_root / "focused_practice_hall_venue_v1.gltf",
        "renderer_executable": executable,
        "renderer_smoke_source": ROOT / "spikes/render_core_profile/modern_renderer_slice_smoke.mm",
        "renderer_source": ROOT / "src/Billiards/render/modern/modern_renderer.cpp",
        "generator": ROOT / "scripts/generate_phase2_modular_gltf.py",
        "validator": ROOT / "scripts/validate_phase2_modular_gltf.py",
        "preflight_runner": ROOT / "scripts/run_phase2_pocket_v10b_preflight.py",
        "design_gate": ROOT / "docs/art/phase2-pocket-v10c-design-gate-v1.json",
    }
    manifest = {
        "schema_version": 1,
        "evidence_id": output.name,
        "status": "NON_FORMAL_MACHINE_CHECKS_SATISFIED_PENDING_TA"
            if not failed else "REJECTED_MACHINE",
        "evidence_scope": "one_actual_gpu_pocket_closeup_only",
        "formal_pass_claimed": False,
        "formal_six_scene_evidence_authorized": False,
        "visual_review_status": "PENDING_TA",
        "golden_promotion_claimed": False,
        "production_binding": {
            name: {"path": str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path),
                   "sha256": sha256(path)}
            for name, path in binding_files.items()
        },
        "physics_marker_binding_sha256": actual_marker_binding,
        "capture": {
            "scene_id": "pocket_closeup",
            "file": CAPTURE_NAME,
            "sha256": sha256(png_path),
            "width": width,
            "height": height,
            "quality": "medium",
            "msaa_samples": 4,
            "actual_gpu": True,
        },
        "object_id_diagnostic": {
            "actual_gpu": True,
            "retained_as_review_image": False,
            "sha256_before_discard": object_id_sha256,
            "purpose": "machine ownership masks only; the sole review image is pocket_closeup.png",
        },
        "gpu_metrics": gpu_metrics,
        "gpu_metrics_file": {"file": METRICS_NAME, "sha256": sha256(metrics_path)},
        "geometry_measurements": geometry,
        "image_measurements": {
            "connected_pure_black_cavity": black_component,
            "authored_rim_scale_equivalent_projection_px": projected_liner_occlusion,
            "object_id": image_measurements,
        },
        "machine_checks": machine_checks,
        "machine_failures": failed,
        "offline_validator_report": validator_report,
        "review_instruction": "Machine rejection blocks TA promotion; the PNG is retained only for diagnosing the listed visual roots. This package makes no visual PASS claim."
            if failed else "Independent TA must judge the PNG; this package makes no visual PASS claim.",
    }
    manifest_path = output / MANIFEST_OUTPUT_NAME
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n",
                             encoding="utf-8")
    print("phase2 pocket v10c NON_FORMAL candidate generated")
    print("manifest=" + str(manifest_path))
    print("manifest_sha256=" + sha256(manifest_path))
    print("capture_sha256=" + manifest["capture"]["sha256"])
    print("pure_black_inward_px=%.3f rim_scale_equivalent_px=%.3f" %
          (black_component["inward_extension_px"], projected_liner_occlusion))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as error:
        print("phase2 pocket v10c preflight failed: " + str(error), file=sys.stderr)
        raise SystemExit(1)
