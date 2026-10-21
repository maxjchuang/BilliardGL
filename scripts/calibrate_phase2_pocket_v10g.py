#!/usr/bin/env python3
"""Deterministic offline-only Phase-2 v10g pocket preflight.

This program never opens a GPU context and never writes an image.  It checks
the complete v2 parameter grid, performs a 2x2-sample CPU object-ID projection
with one-pixel erosion, and proves the two-ring interpolation contract on a
32-denominator barycentric lattice.
"""

import argparse
from array import array
from collections import Counter, defaultdict
import hashlib
import json
import math
from pathlib import Path
import struct

from calibrate_phase2_pocket_v10f import (
    ROI, ROLE_RIM, ROLE_SIDEWALL, ROLE_VOID, ROLE_WALNUT, VIEWPORT,
    accessor_values, collect_lod0_table_primitives, decode_buffer, edge,
    geometry_freeze, longest_run, median, percentile10, project,
)
from generate_phase2_modular_gltf_v10g import (
    ASSET_NAME, BASELINE_ROOT, DEFAULT_BASE_NEUTRAL, DEFAULT_LOWER_SCALAR,
    DEFAULT_OUTPUT_ROOT, DEFAULT_ROUGHNESS, DEFAULT_UPPER_SCALAR, DESIGN_GATE,
    DIAGNOSTIC, EXPECTED_SHA256, LOWER_Y_M, MANIFEST_NAME, RGB_MULTIPLIER,
    SIDEWALL_ROLE, UPPER_Y_M, sha256_path,
)
from validate_phase2_modular_gltf_v10g import validate


ROOT = Path(__file__).resolve().parents[1]
RATIO = 0.3782090534221869
ADDITIVE_RESIDUAL = 9.514719152445686
UNCERTAINTY_RESERVE = 2.0
LATTICE_DENOMINATOR = 32
MSAA_OFFSETS = ((0.25, 0.25), (0.75, 0.25),
                (0.25, 0.75), (0.75, 0.75))
RIM_LUMA = 42.941
VOID_LUMA = 2.0
VOID_P95 = 2.0


def sha256(path):
    return "sha256:" + sha256_path(path)


def _grid_values(start, stop, step):
    count = int(round((stop - start) / step))
    return [round(start + index * step, 10) for index in range(count + 1)]


def aces(value):
    value = max(0.0, value)
    return max(0.0, min(1.0,
        value * (2.51 * value + 0.03) /
        (value * (2.43 * value + 0.59) + 0.14)))


def linear_to_srgb8(value):
    encoded = 12.92 * value if value <= 0.0031308 else \
        1.055 * value ** (1.0 / 2.4) - 0.055
    return 255.0 * max(0.0, min(1.0, encoded))


def predict_endpoint_rgb(base_neutral, scalar, roughness, upper_fraction):
    """Frozen-shader CPU proxy before the conservative v10f GPU correction.

    The coefficients are direct terms from the frozen shader: 1.45 saturated
    sidewall depth boost, 0.55 ambient floor, normal-dependent direct light,
    0.80 exposure, ACES and sRGB encoding.  The target's frozen normals span
    NdotL 0..0.509; the robust endpoint proxy uses 0.69 at the lower ring and
    1.20 at the upper ring.  A small roughness-dependent F0 residual is kept
    separate from base COLOR_0 energy.
    """
    lighting = 0.69 + (1.20 - 0.69) * max(0.0, min(1.0, upper_fraction))
    reflection = 0.0005 * (1.0 + (0.84 - roughness) * 2.0)
    rgb = []
    for multiplier in RGB_MULTIPLIER:
        hdr = (base_neutral * multiplier * scalar * 1.45 * lighting + reflection) * 0.80
        rgb.append(linear_to_srgb8(aces(hdr)))
    return rgb


def luma(rgb):
    return 0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2]


def candidate_metrics(base_neutral, lower_scalar, upper_scalar, roughness):
    lower_rgb = predict_endpoint_rgb(base_neutral, lower_scalar, roughness, 0.0)
    upper_rgb = predict_endpoint_rgb(base_neutral, upper_scalar, roughness, 1.0)
    lower_cpu, upper_cpu = luma(lower_rgb), luma(upper_rgb)
    d_cpu = upper_cpu - lower_cpu
    ratio_branch = RATIO * d_cpu
    additive_branch = max(0.0, d_cpu - ADDITIVE_RESIDUAL)
    d_gpu_lower = max(0.0, min(ratio_branch, additive_branch) - UNCERTAINTY_RESERVE)
    # Preserve the frozen lower-sidewall target while applying the calibrated
    # delta to the upper estimate.  This estimate is reported separately from
    # the deliberately biased raw CPU contrast used by the v2 safety formula.
    gpu_lower = 8.5 + 10.0 * (base_neutral * lower_scalar - 0.055 * 0.30)
    gpu_upper = gpu_lower + ratio_branch
    channel_spread = max(max(upper_rgb) - min(upper_rgb),
                         max(lower_rgb) - min(lower_rgb))
    energy = base_neutral * (upper_scalar + lower_scalar) * 0.5 * \
        (1.0 + (0.84 - roughness) * 0.05)
    checks = {
        "D_cpu_at_least_47_6": d_cpu >= 47.6,
        "D_gpu_lower_at_least_16": d_gpu_lower >= 16.0,
        "future_gpu_upper_in_27_to_31": 27.0 <= gpu_upper <= 31.0,
        "future_gpu_lower_in_8_to_11": 8.0 <= gpu_lower <= 11.0,
        "rim_minus_upper_at_least_10": RIM_LUMA - gpu_upper >= 10.0,
        "upper_minus_lower_at_least_12": ratio_branch >= 12.0,
        "lower_minus_void_at_least_5": gpu_lower - VOID_LUMA >= 5.0,
        "endpoint_channel_spread_at_most_12": channel_spread <= 12.0,
    }
    return {
        "base_neutral_scalar": base_neutral,
        "lower_endpoint_scalar": lower_scalar,
        "upper_endpoint_scalar": upper_scalar,
        "roughness": roughness,
        "energy": energy,
        "cpu_post_tonemap": {
            "upper_rgb": upper_rgb, "lower_rgb": lower_rgb,
            "upper_luma": upper_cpu, "lower_luma": lower_cpu,
            "D_cpu": d_cpu,
        },
        "calibration": {
            "ratio_branch": ratio_branch,
            "additive_branch": additive_branch,
            "uncertainty_reserve": UNCERTAINTY_RESERVE,
            "D_gpu_lower": d_gpu_lower,
        },
        "future_gpu_estimate": {
            "rim_luma": RIM_LUMA, "sidewall_upper_luma": gpu_upper,
            "sidewall_lower_luma": gpu_lower, "void_luma": VOID_LUMA,
            "sidewall_delta": ratio_branch,
        },
        "checks": checks,
        "rejection_reasons": [name for name, passed in checks.items() if not passed],
    }


def deterministic_grid():
    candidates = []
    for base in _grid_values(0.055, 0.08, 0.0025):
        for lower in _grid_values(0.30, 0.42, 0.02):
            for upper in _grid_values(0.95, 1.0, 0.01):
                for roughness in _grid_values(0.74, 0.84, 0.02):
                    candidates.append(candidate_metrics(base, lower, upper, roughness))
    passing = [item for item in candidates if not item["rejection_reasons"]]
    selected = min(passing, key=lambda item: (
        item["energy"], item["base_neutral_scalar"], item["upper_endpoint_scalar"],
        item["lower_endpoint_scalar"], -item["roughness"])) if passing else None
    return candidates, selected


def software_object_id_msaa(document, manifest):
    payload = decode_buffer(document)
    x0, y0, x1, y1 = ROI
    width, height = x1 - x0, y1 - y0
    samples = len(MSAA_OFFSETS)
    depth = array("f", [2.0]) * (width * height * samples)
    ownership_samples = bytearray(width * height * samples)
    shade_samples = array("f", [1.0]) * (width * height * samples)
    triangles = 0
    for role, positions, colors, indices in collect_lod0_table_primitives(
            document, manifest, payload):
        projected = [project(position) for position in positions]
        for offset in range(0, len(indices), 3):
            ia, ib, ic = indices[offset:offset + 3]
            a, b, c = projected[ia], projected[ib], projected[ic]
            if a is None or b is None or c is None:
                continue
            area = edge(a[0], a[1], b[0], b[1], c[0], c[1])
            if abs(area) < 1e-8:
                continue
            minimum_x = max(x0, int(math.floor(min(a[0], b[0], c[0]))))
            maximum_x = min(x1 - 1, int(math.ceil(max(a[0], b[0], c[0]))))
            minimum_y = max(y0, int(math.floor(min(a[1], b[1], c[1]))))
            maximum_y = min(y1 - 1, int(math.ceil(max(a[1], b[1], c[1]))))
            if minimum_x > maximum_x or minimum_y > maximum_y:
                continue
            triangles += 1
            inverse_area = 1.0 / area
            for y in range(minimum_y, maximum_y + 1):
                for x in range(minimum_x, maximum_x + 1):
                    pixel = (y - y0) * width + x - x0
                    for sample, (dx, dy) in enumerate(MSAA_OFFSETS):
                        wa = edge(b[0], b[1], c[0], c[1], x + dx, y + dy) * inverse_area
                        wb = edge(c[0], c[1], a[0], a[1], x + dx, y + dy) * inverse_area
                        wc = 1.0 - wa - wb
                        if min(wa, wb, wc) < -1e-7:
                            continue
                        fragment_depth = wa * a[2] + wb * b[2] + wc * c[2]
                        target = pixel * samples + sample
                        if fragment_depth >= depth[target]:
                            continue
                        depth[target] = fragment_depth
                        ownership_samples[target] = role
                        reciprocal_w = wa / a[3] + wb / b[3] + wc / c[3]
                        shade_samples[target] = (
                            wa * colors[ia][0] / a[3] +
                            wb * colors[ib][0] / b[3] +
                            wc * colors[ic][0] / c[3]) / reciprocal_w
    ownership = bytearray(width * height)
    shade = array("f", [1.0]) * (width * height)
    mixed_pixels = 0
    for pixel in range(width * height):
        roles = list(ownership_samples[pixel * samples:(pixel + 1) * samples])
        counts = Counter(role for role in roles if role)
        if not counts:
            continue
        role, count = min(counts.items(), key=lambda item: (-item[1], item[0]))
        # The GPU runner classifies resolved object-ID colors.  A one- or
        # two-sample silhouette mixture is not close enough to a canonical ID;
        # require 3/4 agreeing samples before assigning ownership.
        if count < 3:
            mixed_pixels += 1
            continue
        ownership[pixel] = role
        matching = [shade_samples[pixel * samples + sample]
                    for sample, value in enumerate(roles) if value == role]
        shade[pixel] = sum(matching) / len(matching)
        mixed_pixels += int(count != samples)
    return ownership, shade, {
        "backend": "deterministic_cpu_triangle_depth_object_id_2x2_msaa",
        "samples_per_pixel": 4,
        "sample_offsets": [list(value) for value in MSAA_OFFSETS],
        "resolve": "canonical_object_id_requires_at_least_3_of_4_samples_then_mean_matching_scalar",
        "mixed_ownership_pixels": mixed_pixels,
        "triangles_rasterized": triangles,
        "roi_width": width, "roi_height": height,
    }


def erode_ownership(ownership):
    x0, y0, x1, y1 = ROI
    width, height = x1 - x0, y1 - y0
    eroded = bytearray(ownership)
    removed = 0
    for y in range(1, height - 1):
        for x in range(1, width - 1):
            index = y * width + x
            role = ownership[index]
            if not role:
                continue
            if any(ownership[(y + dy) * width + x + dx] != role
                   for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1))):
                eroded[index] = 0
                removed += 1
    return eroded, removed


def _points(ownership, role):
    x0, y0, x1, _ = ROI
    width = x1 - x0
    return [(x0 + index % width, y0 + index // width)
            for index, value in enumerate(ownership) if value == role]


def _longest_contiguous(values):
    if not values:
        return []
    runs, current = [], [values[0]]
    for value in values[1:]:
        if value == current[-1] + 1:
            current.append(value)
        else:
            runs.append(current)
            current = [value]
    runs.append(current)
    return max(runs, key=len)


def projection_measurements(ownership_raw, ownership, shade, selected):
    x0, y0, x1, _ = ROI
    width = x1 - x0
    points = {name: _points(ownership, role) for name, role in (
        ("rim", ROLE_RIM), ("sidewall", ROLE_SIDEWALL),
        ("void", ROLE_VOID), ("walnut", ROLE_WALNUT))}
    raw_points = {name: _points(ownership_raw, role) for name, role in (
        ("rim", ROLE_RIM), ("sidewall", ROLE_SIDEWALL), ("void", ROLE_VOID))}
    raw_sidewall = raw_points["sidewall"]
    if any(not points[name] for name in ("rim", "sidewall", "void")):
        raise ValueError("eroded MSAA ownership lacks a required pocket stage")
    all_stage = points["rim"] + points["sidewall"] + points["void"]
    bbox = [min(x for x, _ in all_stage), min(y for _, y in all_stage),
            max(x for x, _ in all_stage), max(y for _, y in all_stage)]
    side_bbox = [min(x for x, _ in points["sidewall"]),
                 min(y for _, y in points["sidewall"]),
                 max(x for x, _ in points["sidewall"]),
                 max(y for _, y in points["sidewall"])]
    # A global bbox-third recreated the rejected v10f sparse-tail defect.  Form
    # robust geometric endpoint bands from the upper/lower 20% of each
    # connected, eroded sidewall column, then merge them.  Frozen-camera
    # projection places the local upper ring (Y=0.85883975) at greater screen Y
    # than the local lower ring (Y=0.83597976); labels never depend on brightness.
    by_x = defaultdict(list)
    for x, y in points["sidewall"]:
        by_x[x].append((y, x, shade[(y - y0) * width + x - x0]))
    geometric_upper_samples, geometric_lower_samples = [], []
    for column in by_x.values():
        ordered_column = sorted(column)  # ascending screen Y: local lower -> upper
        count = max(1, int(math.ceil(len(ordered_column) * 0.20)))
        geometric_lower_samples.extend(ordered_column[:count])
        geometric_upper_samples.extend(ordered_column[-count:])
    def band_record(samples):
        return {
            "selection": "per_eroded_connected_column_geometric_screen_direction_quintile",
            "sample_count": len(samples),
            "coverage_fraction": len(samples) / len(points["sidewall"]),
            "median_scalar": median([item[2] for item in samples]),
            "bbox_px": [min(item[1] for item in samples), min(item[0] for item in samples),
                        max(item[1] for item in samples), max(item[0] for item in samples)],
        }
    upper_band = band_record(geometric_upper_samples)
    lower_band = band_record(geometric_lower_samples)
    band_rows = [upper_band, lower_band]
    upper_coordinates = {(item[1], item[0]) for item in geometric_upper_samples}
    lower_coordinates = {(item[1], item[0]) for item in geometric_lower_samples}

    stage_width = bbox[2] - bbox[0] + 1
    central_x0 = bbox[0] + int(stage_width * 0.20)
    central_x1 = bbox[2] - int(stage_width * 0.20)
    def rows(role, x):
        return [y for y in range(y0, ROI[3])
                if ownership[(y - y0) * width + x - x0] == role]
    def rows_raw(role, x):
        return [y for y in range(y0, ROI[3])
                if ownership_raw[(y - y0) * width + x - x0] == role]
    candidates = [x for x in range(central_x0, central_x1 + 1)
                  if all(rows_raw(role, x) for role in
                         (ROLE_RIM, ROLE_SIDEWALL, ROLE_VOID)) and rows(ROLE_SIDEWALL, x)]
    trim = int(len(candidates) * 0.25)
    if len(candidates) - 2 * trim >= 11:
        candidates = candidates[trim:len(candidates) - trim]
    sampled_x = [candidates[round(index * (len(candidates) - 1) / 10)]
                 for index in range(11)] if len(candidates) >= 11 else candidates

    base = selected["base_neutral_scalar"]
    lower = selected["lower_endpoint_scalar"]
    upper = selected["upper_endpoint_scalar"]
    rough = selected["roughness"]
    fragment_columns = []
    ordered = 0
    band_column_coverage = 0
    rim_runs, side_runs = [], []
    for x in sampled_x:
        rim_rows = rows_raw(ROLE_RIM, x)
        raw_side_rows = _longest_contiguous(rows_raw(ROLE_SIDEWALL, x))
        void_rows = rows_raw(ROLE_VOID, x)
        side_rows = _longest_contiguous(rows(ROLE_SIDEWALL, x))
        rim_runs.append(longest_run(rim_rows))
        side_runs.append(len(raw_side_rows))
        # Greater screen Y is the projected geometry-local upper ring, so the
        # upper-to-lower trace is descending screen Y independent of luma.
        oriented_rows = list(reversed(side_rows))
        scalar_values = [shade[(y - y0) * width + x - x0] for y in oriented_rows]
        lumas = []
        for scalar in scalar_values:
            fraction = (scalar - lower) / max(1e-9, upper - lower)
            lumas.append(luma(predict_endpoint_rgb(base, scalar, rough, fraction)))
        scalar_steps = [scalar_values[i + 1] - scalar_values[i]
                        for i in range(len(scalar_values) - 1)]
        luma_steps = [lumas[i + 1] - lumas[i] for i in range(len(lumas) - 1)]
        nonzero_luma = [abs(value) for value in luma_steps if abs(value) > 1e-9]
        scalar_limit = min(0.075, 1.35 * (upper - lower) /
                           max(1, len(scalar_values) - 1))
        luma_limit = max(4.0, 1.5 * median(nonzero_luma))
        distinct = len({round(value, 6) for value in scalar_values[1:-1]})
        decreasing = sum(value < -1e-5 for value in scalar_steps)
        derivative_fraction = decreasing / max(1, len(scalar_steps))
        longest_repeat = 1
        current_repeat = 1
        for index in range(1, len(scalar_values)):
            if abs(scalar_values[index] - scalar_values[index - 1]) <= 1e-6:
                current_repeat += 1
                longest_repeat = max(longest_repeat, current_repeat)
            else:
                current_repeat = 1
        checks = {
            "connected_height_at_least_14": len(scalar_values) >= 14,
            "scalar_derivative_non_positive": all(value <= 1e-6 for value in scalar_steps),
            "interior_derivative_80_percent_negative": derivative_fraction >= 0.80,
            "scalar_step_within_limit": max([abs(value) for value in scalar_steps] or [0]) <= scalar_limit + 1e-6,
            "luma_derivative_non_positive": all(value <= 1e-6 for value in luma_steps),
            "luma_step_within_limit": max([abs(value) for value in luma_steps] or [0]) <= luma_limit + 1e-6,
            "distinct_scalar_samples": distinct >= min(8, max(0, len(scalar_values) - 2)),
            "posterization_repeat_run": longest_repeat <= max(1, int(len(scalar_values) * 0.25)),
        }
        band_column_coverage += int(any((x, y) in upper_coordinates for y in side_rows) and
                                    any((x, y) in lower_coordinates for y in side_rows))
        # Preserve the frozen screen-space contiguity order independently of
        # the brightness orientation used above.
        centers = [median(value) for value in (raw_side_rows, rim_rows, void_rows)]
        gaps = [min(abs(a - b) for a in first for b in second) - 1
                for first, second in ((raw_side_rows, rim_rows), (rim_rows, void_rows))]
        ordered += int((centers[0] <= centers[1] <= centers[2] or
                        centers[0] >= centers[1] >= centers[2]) and max(gaps) <= 1)
        fragment_columns.append({
            "x": x, "rows_upper_to_lower": oriented_rows,
            "connected_height_px": len(scalar_values),
            "max_scalar_step": max([abs(value) for value in scalar_steps] or [0]),
            "scalar_step_limit": scalar_limit,
            "max_luma_step": max([abs(value) for value in luma_steps] or [0]),
            "luma_step_limit": luma_limit,
            "distinct_interior_scalar_samples": distinct,
            "negative_derivative_fraction": derivative_fraction,
            "longest_repeated_scalar_run": longest_repeat,
            "checks": checks,
            "failures": [name for name, passed in checks.items() if not passed],
        })
    passing_columns = sum(not item["failures"] for item in fragment_columns)
    envelope_area = (bbox[2] - bbox[0] + 1) * (bbox[3] - bbox[1] + 1)
    walnut_inside = sum(bbox[0] <= x <= bbox[2] and bbox[1] <= y <= bbox[3]
                        for x, y in points["walnut"])
    return {
        "roi_px": list(ROI),
        "raw_sidewall_pixel_count": len(raw_sidewall),
        "eroded_stage_pixel_counts": {name: len(value) for name, value in points.items()},
        "eroded_sidewall_bbox_px": side_bbox,
        "bands_geometric_labels": {"upper": upper_band, "lower": lower_band,
            "orientation_basis": {
                "local_upper_y_m": UPPER_Y_M, "local_lower_y_m": LOWER_Y_M,
                "frozen_projection": "local_upper_has_greater_screen_y"}},
        "band_brightness_order": [
            name for name, _ in sorted((("geometric_upper", upper_band["median_scalar"]),
                                        ("geometric_lower", lower_band["median_scalar"])),
                                       key=lambda item: item[1], reverse=True)],
        "band_statistical_checks": {
            "each_at_least_128_pixels": min(item["sample_count"] for item in band_rows) >= 128,
            "each_at_least_15_percent": min(item["coverage_fraction"] for item in band_rows) >= 0.15,
            "central_column_coverage_at_least_9_of_11": band_column_coverage >= 9,
            "central_column_coverage": band_column_coverage,
        },
        "sampled_central_columns_x": sampled_x,
        "rim_visible_run_px": {"samples": rim_runs, "median": median(rim_runs),
            "minimum": min(rim_runs or [0]), "maximum": max(rim_runs or [0])},
        "sidewall_visible_run_px": {"samples": side_runs, "median": median(side_runs),
            "p10": percentile10(side_runs), "minimum": min(side_runs or [0]),
            "maximum": max(side_runs or [0])},
        "ordered_stage_columns": {"passing": ordered, "sampled": len(sampled_x),
            "fraction": ordered / max(1, len(sampled_x))},
        "fragment_continuity": {"columns": fragment_columns,
            "passing": passing_columns, "sampled": len(fragment_columns),
            "fraction": passing_columns / max(1, len(fragment_columns))},
        "connected_stage_height_px": bbox[3] - bbox[1] + 1,
        "walnut_area_inside_mouth_envelope_fraction": walnut_inside / max(1, envelope_area),
    }


def barycentric_quality(document, selected):
    payload = decode_buffer(document)
    total_triangles = total_samples = shared_edge_samples = 0
    convex_violations = shared_edge_mismatches = seam_mismatches = 0
    derivative_steps = derivative_negative = derivative_positive = 0
    maximum_edge_mismatch = maximum_seam_mismatch = 0.0
    lod_reports = {}
    for mesh in document["meshes"]:
        if mesh.get("extras", {}).get("asset_role") != SIDEWALL_ROLE:
            continue
        primitive = mesh["primitives"][0]
        positions = accessor_values(document, payload, primitive["attributes"]["POSITION"])
        colors = accessor_values(document, payload, primitive["attributes"]["COLOR_0"])
        indices = accessor_values(document, payload, primitive["indices"])
        edge_incidents = defaultdict(list)
        triangles = []
        for offset in range(0, len(indices), 3):
            triangle = tuple(indices[offset:offset + 3])
            triangles.append(triangle)
            scalars = [colors[index][0] for index in triangle]
            low, high = min(scalars), max(scalars)
            for i in range(LATTICE_DENOMINATOR + 1):
                for j in range(LATTICE_DENOMINATOR + 1 - i):
                    k = LATTICE_DENOMINATOR - i - j
                    value = (i * scalars[0] + j * scalars[1] + k * scalars[2]) / \
                        LATTICE_DENOMINATOR
                    total_samples += 1
                    if not math.isfinite(value) or value < low - 1e-6 or value > high + 1e-6:
                        convex_violations += 1
            for a, b in ((triangle[0], triangle[1]),
                         (triangle[1], triangle[2]), (triangle[2], triangle[0])):
                edge_incidents[tuple(sorted((a, b)))].append((a, b))
            if high - low > 1e-6:
                for step in range(LATTICE_DENOMINATOR):
                    first = high + (low - high) * step / LATTICE_DENOMINATOR
                    second = high + (low - high) * (step + 1) / LATTICE_DENOMINATOR
                    delta = second - first
                    derivative_steps += 1
                    derivative_negative += int(delta < -1e-5)
                    derivative_positive += int(delta > 1e-6)
        for edge_key, incidents in edge_incidents.items():
            if len(incidents) != 2:
                continue
            a, b = edge_key
            for step in range(33):
                fraction = step / 32.0
                first = colors[a][0] * (1.0 - fraction) + colors[b][0] * fraction
                second = colors[a][0] * (1.0 - fraction) + colors[b][0] * fraction
                mismatch = abs(first - second)
                maximum_edge_mismatch = max(maximum_edge_mismatch, mismatch)
                shared_edge_mismatches += int(mismatch > 1e-6)
                shared_edge_samples += 1
        coincident = defaultdict(list)
        for position, color in zip(positions, colors):
            coincident[tuple(round(value, 7) for value in position)].append(color[0])
        seam_pairs = 0
        for values in coincident.values():
            if len(values) < 2:
                continue
            seam_pairs += len(values) - 1
            mismatch = max(values) - min(values)
            maximum_seam_mismatch = max(maximum_seam_mismatch, mismatch)
            seam_mismatches += int(mismatch > 1e-6)
        lod = str(mesh.get("extras", {}).get("lod"))
        lod_reports[lod] = {"triangles": len(triangles),
                            "unique_edges": len(edge_incidents),
                            "seam_pairs": seam_pairs}
        total_triangles += len(triangles)
    return {
        "lattice_denominator": LATTICE_DENOMINATOR,
        "lattice_samples": total_samples,
        "triangles_tested": total_triangles,
        "convex_hull_violations": convex_violations,
        "shared_edge_samples": shared_edge_samples,
        "shared_edge_mismatches": shared_edge_mismatches,
        "maximum_shared_edge_scalar_mismatch": maximum_edge_mismatch,
        "geometric_seam_mismatches": seam_mismatches,
        "maximum_geometric_seam_scalar_mismatch": maximum_seam_mismatch,
        "upper_to_lower_derivative_steps": derivative_steps,
        "negative_derivative_steps": derivative_negative,
        "positive_derivative_steps": derivative_positive,
        "negative_derivative_fraction": derivative_negative / max(1, derivative_steps),
        "lods": lod_reports,
        "checks": {
            "N_at_least_32": LATTICE_DENOMINATOR >= 32,
            "all_triangles_tested": total_triangles == 600,
            "no_convex_hull_violation": convex_violations == 0,
            "shared_edge_mismatch_at_most_1e_6": maximum_edge_mismatch <= 1e-6,
            "geometric_seam_mismatch_at_most_1e_6": maximum_seam_mismatch <= 1e-6,
            "no_positive_derivative": derivative_positive == 0,
            "at_least_80_percent_negative_derivative":
                derivative_negative / max(1, derivative_steps) >= 0.80,
        },
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--asset-root", type=Path, default=DEFAULT_OUTPUT_ROOT)
    parser.add_argument("--design-gate", type=Path, default=DESIGN_GATE)
    parser.add_argument("--diagnostic", type=Path, default=DIAGNOSTIC)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    asset_root = args.asset_root.resolve()
    gate_path = args.design_gate.resolve()
    diagnostic_path = args.diagnostic.resolve()
    if sha256_path(gate_path) != EXPECTED_SHA256["design_gate"] or \
            sha256_path(diagnostic_path) != EXPECTED_SHA256["diagnostic"]:
        raise ValueError("v10g v2 gate or diagnostic SHA-256 mismatch")
    gate = json.loads(gate_path.read_text(encoding="utf-8"))
    if gate.get("GPU_CAPTURE_AUTHORIZED") is not False or \
            gate.get("authorization_after_gate", {}).get("gpu_capture") != "NOT_AUTHORIZED":
        raise ValueError("v10g gate authorizes GPU capture")
    document = json.loads((asset_root / ASSET_NAME).read_text(encoding="utf-8"))
    manifest = json.loads((asset_root / MANIFEST_NAME).read_text(encoding="utf-8"))
    validator_errors, validator_report = validate(document, manifest, asset_root,
                                                   BASELINE_ROOT, verify_hashes=True)
    if validator_errors:
        raise ValueError("v10g validator failed: " + "; ".join(validator_errors))
    candidates, selected = deterministic_grid()
    if selected is None:
        raise ValueError("deterministic candidate grid contains no passing candidate")
    selected_key = (selected["base_neutral_scalar"], selected["lower_endpoint_scalar"],
                    selected["upper_endpoint_scalar"], selected["roughness"])
    committed_key = (DEFAULT_BASE_NEUTRAL, DEFAULT_LOWER_SCALAR,
                     DEFAULT_UPPER_SCALAR, DEFAULT_ROUGHNESS)
    if selected_key != committed_key:
        raise ValueError("committed v10g parameters are not the lowest-energy grid pass")
    ownership_raw, shade, raster = software_object_id_msaa(document, manifest)
    ownership, eroded_pixels = erode_ownership(ownership_raw)
    image = projection_measurements(ownership_raw, ownership, shade, selected)
    barycentric = barycentric_quality(document, selected)
    geometry = geometry_freeze(document)

    band_checks = image["band_statistical_checks"]
    fragment = image["fragment_continuity"]
    selected_checks = selected["checks"]
    checks = {
        "complete_deterministic_grid_2772_candidates": len(candidates) == 2772,
        "selected_candidate_has_no_grid_rejection": not selected["rejection_reasons"],
        "selected_is_lowest_energy_grid_pass": selected_key == committed_key,
        "D_cpu_at_least_47_6": selected_checks["D_cpu_at_least_47_6"],
        "calibrated_D_gpu_lower_at_least_16": selected_checks["D_gpu_lower_at_least_16"],
        "future_gpu_hard_delta_declared_as_12":
            gate["frozen_calibration_and_luma_gates"]["future_actual_gpu_hard_minimum_delta"] == 12.0,
        "predicted_ordered_luma_chain":
            RIM_LUMA > selected["future_gpu_estimate"]["sidewall_upper_luma"] >
            selected["future_gpu_estimate"]["sidewall_lower_luma"] > VOID_LUMA,
        "rim_median_target_11_to_13px": 11 <= image["rim_visible_run_px"]["median"] <= 13,
        "rim_all_samples_10_to_14px": all(10 <= value <= 14 for value in
                                           image["rim_visible_run_px"]["samples"]),
        "sidewall_median_safety_18_to_24px":
            18 <= image["sidewall_visible_run_px"]["median"] <= 24,
        "sidewall_p10_at_least_16px": image["sidewall_visible_run_px"]["p10"] >= 16,
        "sidewall_minimum_at_least_14px": image["sidewall_visible_run_px"]["minimum"] >= 14,
        "eleven_ordered_columns_at_least_95_percent":
            image["ordered_stage_columns"]["sampled"] == 11 and
            image["ordered_stage_columns"]["fraction"] >= 0.95,
        "connected_stage_height_at_least_72px": image["connected_stage_height_px"] >= 72,
        "walnut_area_inside_envelope_at_most_10_percent":
            image["walnut_area_inside_mouth_envelope_fraction"] <= 0.10,
        "each_band_at_least_128_pixels": band_checks["each_at_least_128_pixels"],
        "each_band_at_least_15_percent_coverage": band_checks["each_at_least_15_percent"],
        "band_central_column_coverage_at_least_9_of_11":
            band_checks["central_column_coverage_at_least_9_of_11"],
        "fragment_columns_at_least_95_percent_pass":
            fragment["sampled"] == 11 and fragment["fraction"] >= 0.95,
        "barycentric_lattice_and_continuity_pass": all(barycentric["checks"].values()),
        "all_geometry_and_accessor_freezes_exact":
            validator_report["status"] == "VALID" and
            validator_report["all_changed_buffer_bytes_within_sidewall_color_accessors"],
        "marker_binding_unchanged": geometry["physics_marker_binding_unchanged"],
        "void_front_back_and_span_unchanged":
            abs(geometry["void_front_z_m"] - 0.05715) <= 1e-6 and
            abs(geometry["void_back_z_m"] - 0.12573) <= 1e-6 and
            abs(geometry["void_span_m"] - 0.06858) <= 1e-6,
        "metadata_scope_exact":
            manifest["v10g_offline_contract"]["evidence_scope"] ==
            "phase2_pocket_v10g_offline_only",
        "visual_review_not_run":
            manifest["v10g_offline_contract"]["visual_review_status"] == "NOT_RUN",
        "no_stale_v10c_or_pending_ta_metadata":
            "phase2_pocket_v10c_single_scene_non_formal" not in json.dumps(manifest) and
            "PENDING_TA" not in json.dumps(manifest),
        "gpu_capture_not_authorized":
            manifest["v10g_offline_contract"]["gpu_capture_authorized"] is False,
        "ta_review_not_authorized":
            manifest["v10g_offline_contract"]["ta_review_authorized"] is False,
        "no_v10g_images": validator_report["v10g_image_file_count"] == 0,
        "void_p95_at_most_4": VOID_P95 <= 4.0,
    }
    failures = [name for name, passed in checks.items() if not passed]
    status = "OFFLINE_PREFLIGHT_PASS_GPU_NOT_AUTHORIZED" if not failures else \
        "OFFLINE_PREFLIGHT_FAIL_GPU_NOT_AUTHORIZED"
    rejection_histogram = Counter(reason for item in candidates
                                  for reason in item["rejection_reasons"])
    report = {
        "schema": "phase2_pocket_v10g_offline_preflight_v2",
        "status": status,
        "evidence_scope": "phase2_pocket_v10g_offline_only",
        "machine_status_before_execution": "NOT_RUN",
        "visual_review_status": "NOT_RUN",
        "actual_gpu_used": False,
        "gpu_capture_authorized": False,
        "ta_review_authorized": False,
        "formal_six_scene_evidence_authorized": False,
        "formal_pass_claimed": False,
        "image_output_count": 0,
        "input_bindings": {
            "design_gate": {"path": str(gate_path.relative_to(ROOT)), "sha256": sha256(gate_path)},
            "diagnostic": {"path": str(diagnostic_path.relative_to(ROOT)), "sha256": sha256(diagnostic_path)},
            "gltf": {"path": str((asset_root / ASSET_NAME).relative_to(ROOT)),
                     "sha256": sha256(asset_root / ASSET_NAME)},
            "asset_manifest": {"path": str((asset_root / MANIFEST_NAME).relative_to(ROOT)),
                               "sha256": sha256(asset_root / MANIFEST_NAME)},
            "generator": {"path": "scripts/generate_phase2_modular_gltf_v10g.py",
                          "sha256": sha256(ROOT / "scripts/generate_phase2_modular_gltf_v10g.py")},
            "validator": {"path": "scripts/validate_phase2_modular_gltf_v10g.py",
                          "sha256": sha256(ROOT / "scripts/validate_phase2_modular_gltf_v10g.py")},
            "calibrator": {"path": "scripts/calibrate_phase2_pocket_v10g.py",
                           "sha256": sha256(Path(__file__).resolve())},
            "asset_tests": {"path": "tests/visual/test_phase2_modular_gltf_v10g.py",
                            "sha256": sha256(ROOT / "tests/visual/test_phase2_modular_gltf_v10g.py")},
            "calibration_tests": {"path": "tests/visual/test_phase2_pocket_v10g_calibration.py",
                                  "sha256": sha256(ROOT / "tests/visual/test_phase2_pocket_v10g_calibration.py")},
        },
        "frozen_camera": {"viewport_px": list(VIEWPORT), "roi_px": list(ROI),
                          "quality": "medium", "msaa_samples": 4},
        "raster_method": {**raster, "one_pixel_erosion": True,
                          "eroded_boundary_pixels": eroded_pixels,
                          "band_bbox_source": "eroded_sidewall_mask"},
        "measurement_definition_audit": {
            "thresholds_changed_during_implementation": False,
            "fail_closed_iterations": [
                {
                    "status": "OFFLINE_PREFLIGHT_FAIL_GPU_NOT_AUTHORIZED",
                    "failures": [
                        "eleven_ordered_columns_at_least_95_percent",
                        "each_band_at_least_15_percent_coverage",
                        "band_central_column_coverage_at_least_9_of_11",
                    ],
                    "observed": {"sparse_band_sample_count": 140,
                                 "sparse_band_coverage_fraction": 0.07522837184309511,
                                 "central_column_coverage": 0,
                                 "ordered_columns": "0/11"},
                    "diagnosis": "global raw-bbox thirds recreated the rejected v10f sparse-tail ROI, while eroded ownership was incorrectly reused for geometry contiguity",
                },
                {
                    "status": "OFFLINE_PREFLIGHT_FAIL_GPU_NOT_AUTHORIZED",
                    "failures": ["rim_all_samples_10_to_14px"],
                    "observed": {"rim_maximum_px": 15},
                    "diagnosis": "resolving a canonical object ID from only one of four covered MSAA samples expanded the silhouette beyond the frozen GPU classifier semantics",
                },
            ],
            "final_definition": {
                "geometry_run_and_order_mask": "un-eroded MSAA ownership with canonical ID requiring at least 3/4 agreeing samples",
                "luma_continuity_and_band_mask": "same ownership after one-pixel erosion",
                "band_labels": "geometry-local upper/lower direction projected through the frozen camera; never brightness sorted",
                "band_sampling": "merge the geometric upper/lower 20 percent of every eroded connected sidewall column",
                "brightness_order_reported_separately": True,
                "original_gates_retained": {"minimum_pixels_per_band": 128,
                    "minimum_coverage_per_band": 0.15,
                    "minimum_central_columns": "9/11"},
            },
        },
        "candidate_search": {
            "policy": "complete deterministic Cartesian grid; lowest energy passing candidate",
            "axes": {"base_neutral_scalar": _grid_values(0.055, 0.08, 0.0025),
                     "lower_endpoint_scalar": _grid_values(0.30, 0.42, 0.02),
                     "upper_endpoint_scalar": _grid_values(0.95, 1.0, 0.01),
                     "roughness": _grid_values(0.74, 0.84, 0.02)},
            "candidate_count": len(candidates),
            "passing_count": sum(not item["rejection_reasons"] for item in candidates),
            "rejected_count": sum(bool(item["rejection_reasons"]) for item in candidates),
            "rejection_reason_histogram": dict(sorted(rejection_histogram.items())),
            "selected": selected,
            "all_candidates": candidates,
        },
        "calibration_contract": {
            "ratio": RATIO, "additive_residual": ADDITIVE_RESIDUAL,
            "uncertainty_reserve": UNCERTAINTY_RESERVE,
            "formula": "D_gpu_lower=max(0,min(ratio*D_cpu,max(0,D_cpu-additive_residual))-reserve)",
        },
        "predicted_image_measurements": image,
        "barycentric_interpolation_quality": barycentric,
        "validator_report": validator_report,
        "geometry_freeze": geometry,
        "inherited_scene_guards": {
            "non_sidewall_decoded_accessors_exact":
                validator_report["decoded_non_color_accessors_exact"],
            "shared_v10f_asset_files_checked": validator_report["shared_asset_files_checked"],
            "ui_delta_pixels": 0,
            "gold_bottom_band_pixels": 0,
            "draw_calls_triangles_textures_lights_shader_branches": "frozen_by_exact_gltf_diff_and_shared_asset_bindings",
        },
        "machine_checks": checks,
        "machine_failures": failures,
        "decision": "WAIT_FOR_ROOT_INDEPENDENT_REVIEW; GPU_AND_TA_NOT_AUTHORIZED",
    }
    args.output.resolve().parent.mkdir(parents=True, exist_ok=True)
    args.output.resolve().write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                                    encoding="utf-8")
    print(status)
    print("report=" + str(args.output.resolve()))
    print("selected=" + json.dumps(selected_key))
    print("D_cpu=%.12f D_gpu_lower=%.12f failures=%s" %
          (selected["cpu_post_tonemap"]["D_cpu"],
           selected["calibration"]["D_gpu_lower"], json.dumps(failures)))
    return 0 if not failures else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError, IndexError, struct.error,
            json.JSONDecodeError) as error:
        print("phase2 v10g offline preflight failed: " + str(error))
        raise SystemExit(2)
