#!/usr/bin/env python3
"""Deterministic CPU screen-space preflight for the Phase-2 v10f pocket.

This tool never creates a GPU context.  It decodes the production glTF,
projects the selected LOD0 table through the frozen pocket camera, performs a
software depth/object-ownership pass in the frozen review ROI, and evaluates
the same rim/sidewall/order/walnut axes used by the actual-GPU preflight.
"""

import argparse
from array import array
import base64
import hashlib
import json
import math
from pathlib import Path
import struct

from generate_phase2_modular_gltf_v10f import (
    ASSET_NAME, BALL_DIAMETER_M, MANIFEST_NAME, POCKET_BACK_Z_M,
    POCKET_FRONT_Z_M, POCKET_RIM_WIDTH_M, POCKET_SIDEWALL_DROP_M,
    marker_binding_sha256,
)
from validate_phase2_modular_gltf_v10f import validate


ROOT = Path(__file__).resolve().parents[1]
VIEWPORT = (1920, 1080)
ROI = (537, 756, 1113, 1015)
CAMERA_EYE_CM = (115.0, 215.0, 175.0)
CAMERA_TARGET_CM = (50.0, 89.0, 95.0)
VERTICAL_FOV_DEGREES = 30.0
FROZEN_MARKER_SHA256 = \
    "sha256:1b8f8cdb03c88e81721525aa0a134c4e4efa564975c5e2307835514f03df2b7a"
V10D_BASELINE_SIDEWALL_LUMA = 25.293999999999997
V10D_BASELINE_VOID_LUMA = 2.0
LUMA_MODEL_EXPONENT = 0.55
LUMA_MODEL_UNCERTAINTY = 1.5

ROLE_OTHER = 1
ROLE_WALNUT = 2
ROLE_FACING = 3
ROLE_CUSHION = 4
ROLE_RIM = 5
ROLE_SIDEWALL = 6
ROLE_VOID = 7


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return "sha256:" + digest.hexdigest()


def median(values):
    ordered = sorted(values)
    if not ordered:
        return 0.0
    middle = len(ordered) // 2
    return ordered[middle] if len(ordered) % 2 else \
        (ordered[middle - 1] + ordered[middle]) * 0.5


def percentile10(values):
    return sorted(values)[len(values) // 10] if values else 0


def vector_subtract(first, second):
    return tuple(a - b for a, b in zip(first, second))


def dot(first, second):
    return sum(a * b for a, b in zip(first, second))


def cross(first, second):
    return (first[1] * second[2] - first[2] * second[1],
            first[2] * second[0] - first[0] * second[2],
            first[0] * second[1] - first[1] * second[0])


def normalize(value):
    length = math.sqrt(dot(value, value))
    if not math.isfinite(length) or length <= 1e-9:
        raise ValueError("camera basis is degenerate")
    return tuple(component / length for component in value)


def project(position_m):
    """Match buildSharedCameraProjection and clipToPhysicalScreen."""
    world = tuple(value * 100.0 for value in position_m)
    forward = normalize(vector_subtract(CAMERA_TARGET_CM, CAMERA_EYE_CM))
    side = normalize(cross(forward, (0.0, 1.0, 0.0)))
    up = cross(side, forward)
    relative = vector_subtract(world, CAMERA_EYE_CM)
    camera_x = dot(side, relative)
    camera_y = dot(up, relative)
    camera_forward = dot(forward, relative)
    if camera_forward <= 1e-5:
        return None
    q = 1.0 / math.tan(math.radians(VERTICAL_FOV_DEGREES) * 0.5)
    ndc_x = (q / (VIEWPORT[0] / VIEWPORT[1])) * camera_x / camera_forward
    ndc_y = q * camera_y / camera_forward
    near_plane, far_plane = 2.0, 1800.0
    ndc_z = ((far_plane + near_plane) / (far_plane - near_plane) -
             2.0 * far_plane * near_plane /
             ((far_plane - near_plane) * camera_forward))
    return ((ndc_x * 0.5 + 0.5) * VIEWPORT[0],
            (1.0 - (ndc_y * 0.5 + 0.5)) * VIEWPORT[1], ndc_z,
            camera_forward)


def decode_buffer(document):
    prefix = "data:application/octet-stream;base64,"
    buffers = document.get("buffers", [])
    if len(buffers) != 1 or not buffers[0].get("uri", "").startswith(prefix):
        raise ValueError("v10f calibrator requires one embedded glTF buffer")
    return base64.b64decode(buffers[0]["uri"][len(prefix):], validate=True)


def accessor_values(document, payload, accessor_index):
    if not isinstance(accessor_index, int):
        raise ValueError("missing required glTF accessor")
    accessor = document["accessors"][accessor_index]
    view = document["bufferViews"][accessor["bufferView"]]
    component_format = {5126: "f", 5125: "I", 5123: "H"}.get(
        accessor["componentType"])
    component_count = {"SCALAR": 1, "VEC2": 2, "VEC3": 3,
                       "VEC4": 4}.get(accessor["type"])
    if not component_format or not component_count:
        raise ValueError("unsupported glTF accessor in v10f calibration")
    component_size = struct.calcsize("<" + component_format)
    packed_size = component_size * component_count
    stride = view.get("byteStride", packed_size)
    offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    values = []
    for index in range(accessor["count"]):
        unpacked = struct.unpack_from("<" + component_format * component_count,
                                      payload, offset + index * stride)
        values.append(unpacked[0] if component_count == 1 else unpacked)
    return values


def role_code(role):
    if role in ("table_body", "pocket_apron_integrated_wedge"):
        return ROLE_WALNUT
    if role == "pocket_facing_visual_only":
        return ROLE_FACING
    if role == "cushion_segment":
        return ROLE_CUSHION
    if role == "pocket_liner_rim_visual_only":
        return ROLE_RIM
    if role == "pocket_liner_sidewall_visual_only":
        return ROLE_SIDEWALL
    if role == "pocket_void_visual_only":
        return ROLE_VOID
    return ROLE_OTHER


def collect_lod0_table_primitives(document, manifest, payload):
    table_root = manifest["lod_sets"]["table"]["nodes"]["0"]
    nodes = document["nodes"]
    meshes = document["meshes"]
    primitive_rows = []
    pending = [table_root]
    while pending:
        node_index = pending.pop()
        node = nodes[node_index]
        pending.extend(reversed(node.get("children", [])))
        if "mesh" not in node:
            continue
        mesh = meshes[node["mesh"]]
        role = node.get("extras", {}).get("asset_role",
            mesh.get("extras", {}).get("asset_role", ""))
        primitive = mesh["primitives"][0]
        attributes = primitive["attributes"]
        positions = accessor_values(document, payload, attributes["POSITION"])
        colors = accessor_values(document, payload, attributes["COLOR_0"]) \
            if "COLOR_0" in attributes else [(1.0, 1.0, 1.0, 1.0)] * len(positions)
        indices = accessor_values(document, payload, primitive["indices"])
        primitive_rows.append((role_code(role), positions, colors, indices))
    return primitive_rows


def edge(first_x, first_y, second_x, second_y, x, y):
    return ((x - first_x) * (second_y - first_y) -
            (y - first_y) * (second_x - first_x))


def software_object_id(document, manifest):
    """Rasterize one center sample per pixel with OpenGL-like depth order."""
    payload = decode_buffer(document)
    x0, y0, x1, y1 = ROI
    roi_width, roi_height = x1 - x0, y1 - y0
    depth = array("f", [2.0]) * (roi_width * roi_height)
    ownership = bytearray(roi_width * roi_height)
    shade = array("f", [1.0]) * (roi_width * roi_height)
    triangles_rasterized = 0
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
            triangles_rasterized += 1
            inverse_area = 1.0 / area
            for y in range(minimum_y, maximum_y + 1):
                py = y + 0.5
                row = (y - y0) * roi_width
                for x in range(minimum_x, maximum_x + 1):
                    px = x + 0.5
                    wa = edge(b[0], b[1], c[0], c[1], px, py) * inverse_area
                    wb = edge(c[0], c[1], a[0], a[1], px, py) * inverse_area
                    wc = 1.0 - wa - wb
                    if min(wa, wb, wc) < -1e-7:
                        continue
                    fragment_depth = wa * a[2] + wb * b[2] + wc * c[2]
                    index = row + x - x0
                    if fragment_depth >= depth[index]:
                        continue
                    depth[index] = fragment_depth
                    ownership[index] = role
                    reciprocal_w = (wa / a[3] + wb / b[3] + wc / c[3])
                    shade[index] = ((wa * colors[ia][0] / a[3] +
                                     wb * colors[ib][0] / b[3] +
                                     wc * colors[ic][0] / c[3]) /
                                    reciprocal_w if reciprocal_w else 1.0)
    return ownership, shade, {"roi_width": roi_width,
                              "roi_height": roi_height,
                              "triangles_rasterized": triangles_rasterized}


def mask_points(ownership, role):
    x0, y0, x1, y1 = ROI
    width = x1 - x0
    return [(x0 + index % width, y0 + index // width)
            for index, value in enumerate(ownership) if value == role]


def connected_component(ownership):
    x0, y0, x1, y1 = ROI
    width, height = x1 - x0, y1 - y0
    stage = bytearray(value in (ROLE_RIM, ROLE_SIDEWALL, ROLE_VOID)
                      for value in ownership)
    dilated = bytearray(stage)
    for index, value in enumerate(stage):
        if not value:
            continue
        x, y = index % width, index // width
        for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            if 0 <= x + dx < width and 0 <= y + dy < height:
                dilated[(y + dy) * width + x + dx] = 1
    visited = bytearray(dilated)
    largest = []
    for start, value in enumerate(visited):
        if not value:
            continue
        visited[start] = 0
        pending, component = [start], []
        while pending:
            current = pending.pop()
            component.append(current)
            x, y = current % width, current // width
            for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < width and 0 <= ny < height:
                    candidate = ny * width + nx
                    if visited[candidate]:
                        visited[candidate] = 0
                        pending.append(candidate)
        if len(component) > len(largest):
            largest = component
    if not largest:
        raise ValueError("CPU object-ID pass contains no connected pocket stage")
    xs = [x0 + index % width for index in largest]
    ys = [y0 + index // width for index in largest]
    return {"pixel_count": len(largest),
            "bbox_px": [min(xs), min(ys), max(xs), max(ys)],
            "width_px": max(xs) - min(xs) + 1,
            "height_px": max(ys) - min(ys) + 1,
            "connectivity_dilation_px": 1}


def longest_run(rows):
    best = current = 0
    previous = None
    for value in sorted(rows):
        current = current + 1 if previous is not None and value == previous + 1 else 1
        best = max(best, current)
        previous = value
    return best


def predict_luma(scale):
    clamped = max(0.0, min(1.0, scale))
    return (V10D_BASELINE_VOID_LUMA +
            (V10D_BASELINE_SIDEWALL_LUMA - V10D_BASELINE_VOID_LUMA) *
            clamped ** LUMA_MODEL_EXPONENT)


def measurements(ownership, shade):
    x0, y0, x1, y1 = ROI
    width = x1 - x0
    points = {name: mask_points(ownership, role) for name, role in (
        ("rim", ROLE_RIM), ("sidewall", ROLE_SIDEWALL),
        ("void", ROLE_VOID), ("walnut", ROLE_WALNUT))}
    if any(not points[name] for name in ("rim", "sidewall", "void")):
        raise ValueError("CPU object-ID pass does not expose all three pocket stages")
    stage_points = points["rim"] + points["sidewall"] + points["void"]
    stage_bbox = [min(x for x, _ in stage_points), min(y for _, y in stage_points),
                  max(x for x, _ in stage_points), max(y for _, y in stage_points)]
    stage_width = stage_bbox[2] - stage_bbox[0] + 1
    central_x0 = stage_bbox[0] + int(stage_width * 0.20)
    central_x1 = stage_bbox[2] - int(stage_width * 0.20)

    def rows(role, x):
        local_x = x - x0
        return [y for y in range(y0, y1)
                if ownership[(y - y0) * width + local_x] == role]

    candidates = [x for x in range(central_x0, central_x1 + 1)
                  if all(rows(role, x) for role in
                         (ROLE_RIM, ROLE_SIDEWALL, ROLE_VOID))]
    # Use the central 60% of the common-ownership band.  This is the stable
    # throat cross-section; the extreme U-arc shoulders are silhouette samples,
    # not vertical stage-height samples.
    trim = int(len(candidates) * 0.25)
    if len(candidates) - 2 * trim >= 11:
        candidates = candidates[trim:len(candidates) - trim]
    if len(candidates) >= 11:
        sampled_x = [candidates[round(index * (len(candidates) - 1) / 10)]
                     for index in range(11)]
    else:
        sampled_x = candidates
    rim_runs, side_runs, ordered = [], [], 0
    for x in sampled_x:
        rim_rows = rows(ROLE_RIM, x)
        sidewall_rows = rows(ROLE_SIDEWALL, x)
        void_rows = rows(ROLE_VOID, x)
        rim_runs.append(longest_run(rim_rows))
        side_runs.append(longest_run(sidewall_rows))
        # In this frozen exterior corner camera, lower cavity geometry projects
        # upward.  The measured contiguous screen order is therefore
        # sidewall -> rim -> void, not world-space rim -> sidewall -> void.
        stage_rows = [sidewall_rows, rim_rows, void_rows]
        centers = [median(values) for values in stage_rows]
        gaps = [min(abs(a - b) for a in first for b in second) - 1
                for first, second in zip(stage_rows, stage_rows[1:])]
        monotonic = (centers[0] <= centers[1] <= centers[2] or
                     centers[0] >= centers[1] >= centers[2])
        if monotonic and max(gaps) <= 1:
            ordered += 1

    side_ys = [y for _, y in points["sidewall"]]
    minimum_y, maximum_y = min(side_ys), max(side_ys)
    third = max(1, (maximum_y - minimum_y + 1) // 3)

    def luma_values(start_y, end_y):
        result = []
        for x, y in points["sidewall"]:
            if start_y <= y < end_y:
                index = (y - y0) * width + x - x0
                result.append(predict_luma(shade[index]))
        return result

    first = median(luma_values(minimum_y, minimum_y + third))
    last = median(luma_values(maximum_y - third + 1, maximum_y + 1))
    upper_luma, lower_luma = max(first, last), min(first, last)
    component = connected_component(ownership)
    envelope_area = component["width_px"] * component["height_px"]
    walnut_inside = sum(component["bbox_px"][0] <= x <= component["bbox_px"][2] and
                        component["bbox_px"][1] <= y <= component["bbox_px"][3]
                        for x, y in points["walnut"])
    return {
        "roi_px": list(ROI),
        "stage_pixel_counts": {name: len(value) for name, value in points.items()},
        "connected_stage_component": component,
        "all_stage_bbox_px": stage_bbox,
        "sampled_central_columns_x": sampled_x,
        "rim_visible_run_px": {"samples": rim_runs, "median": median(rim_runs),
            "minimum": min(rim_runs or [0]), "maximum": max(rim_runs or [0])},
        "sidewall_visible_run_px": {"samples": side_runs,
            "median": median(side_runs), "p10": percentile10(side_runs),
            "minimum": min(side_runs or [0]), "maximum": max(side_runs or [0])},
        "ordered_stage_columns": {"passing": ordered, "sampled": len(sampled_x),
            "fraction": ordered / max(1, len(sampled_x)),
            "frozen_screen_order": ["sidewall", "rim", "void"]},
        "walnut_area_inside_mouth_envelope_fraction":
            walnut_inside / max(1, envelope_area),
        "tone_mapped_luma_prediction": {
            "model": "v10d_anchor_power_curve_over_interpolated_sidewall_COLOR_0",
            "baseline_sidewall_luma": V10D_BASELINE_SIDEWALL_LUMA,
            "baseline_void_luma": V10D_BASELINE_VOID_LUMA,
            "exponent": LUMA_MODEL_EXPONENT,
            "uncertainty_plus_minus_luma": LUMA_MODEL_UNCERTAINTY,
            "sidewall_upper_median": upper_luma,
            "sidewall_lower_median": lower_luma,
            "upper_minus_lower": upper_luma - lower_luma,
            "conservative_delta_lower_bound":
                upper_luma - lower_luma - 2.0 * LUMA_MODEL_UNCERTAINTY,
        },
    }


def geometry_freeze(document):
    nodes = document["nodes"]
    marker_indices = [index for index, node in enumerate(nodes)
                      if node.get("name", "").startswith("SOCKET_")]
    marker_sha = marker_binding_sha256(document, marker_indices)
    extras = document["asset"]["extras"]["pocket_v10f_profile"]
    ratios = []
    for node in nodes:
        if node.get("extras", {}).get("asset_role") != \
                "pocket_liner_sidewall_visual_only":
            continue
        for item in node["extras"]["local_ranges"]:
            upper = max(abs(point[0]) * 2.0 for point in item["upper_boundary"])
            lower = max(abs(point[0]) * 2.0 for point in item["lower_boundary"])
            ratios.append(lower / upper)
    return {
        "physics_marker_binding_sha256": marker_sha,
        "physics_marker_binding_unchanged": marker_sha == FROZEN_MARKER_SHA256,
        "void_front_z_m": extras["front_z_m"],
        "void_back_z_m": extras["back_z_m"],
        "void_span_m": extras["back_z_m"] - extras["front_z_m"],
        "rim_width_m": POCKET_RIM_WIDTH_M,
        "sidewall_drop_m": POCKET_SIDEWALL_DROP_M,
        "drop_bottom_to_mouth_width_ratio": {
            "minimum": min(ratios), "maximum": max(ratios)},
    }


def frozen_v10d_calibration():
    asset_root = ROOT / "assets/modern/phase2-v10d"
    evidence_path = ROOT / "docs/art/phase2-pocket-v10d-preflight-v1" / \
        "phase2-pocket-v10d-preflight-manifest-v1.json"
    document = json.loads((asset_root / ASSET_NAME).read_text(encoding="utf-8"))
    manifest = json.loads((asset_root / MANIFEST_NAME).read_text(encoding="utf-8"))
    evidence = json.loads(evidence_path.read_text(encoding="utf-8"))
    ownership, shade, _ = software_object_id(document, manifest)
    predicted = measurements(ownership, shade)
    actual = evidence["image_measurements"]["object_id"]
    predicted_values = {
        "rim_median_px": predicted["rim_visible_run_px"]["median"],
        "sidewall_median_px": predicted["sidewall_visible_run_px"]["median"],
        "sidewall_p10_px": predicted["sidewall_visible_run_px"]["p10"],
        "sidewall_minimum_px": predicted["sidewall_visible_run_px"]["minimum"],
        "ordered_sampled_columns": predicted["ordered_stage_columns"]["sampled"],
        "ordered_passing_columns": predicted["ordered_stage_columns"]["passing"],
        "connected_height_px": predicted["connected_stage_component"]["height_px"],
        "walnut_fraction": predicted["walnut_area_inside_mouth_envelope_fraction"],
        "sidewall_luma": V10D_BASELINE_SIDEWALL_LUMA,
    }
    actual_values = {
        "rim_median_px": actual["rim_visible_run_px"]["median"],
        "sidewall_median_px": actual["sidewall_visible_run_px"]["median"],
        "sidewall_p10_px": actual["sidewall_visible_run_px"]["p10"],
        "sidewall_minimum_px": actual["sidewall_visible_run_px"]["minimum"],
        "ordered_sampled_columns": actual["ordered_stage_columns"]["sampled"],
        "ordered_passing_columns": actual["ordered_stage_columns"]["passing"],
        "connected_height_px": actual["connected_stage_component"]["height_px"],
        "walnut_fraction": actual["walnut_area_inside_mouth_envelope_fraction"],
        "sidewall_luma": actual["luma"]["sidewall_upper_median"],
    }
    residuals = {name: abs(predicted_values[name] - actual_values[name])
                 for name in predicted_values}
    return {
        "source_manifest": {"path": str(evidence_path.relative_to(ROOT)),
                            "sha256": sha256(evidence_path)},
        "predicted": predicted_values,
        "actual_gpu": actual_values,
        "absolute_residual": residuals,
        "pixel_axes_within_1px": all(residuals[name] <= 1.0 for name in (
            "rim_median_px", "sidewall_median_px", "sidewall_p10_px",
            "sidewall_minimum_px", "ordered_sampled_columns",
            "ordered_passing_columns", "connected_height_px")),
        "walnut_fraction_residual_at_most_0_005":
            residuals["walnut_fraction"] <= 0.005,
        "luma_anchor_exact": residuals["sidewall_luma"] <= 1e-9,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--asset-root", type=Path,
                        default=ROOT / "assets/modern/phase2-v10f")
    parser.add_argument("--design-gate", type=Path,
                        default=ROOT / "docs/art/phase2-pocket-v10f-design-gate-v1.json")
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    asset_root = arguments.asset_root.resolve()
    gltf_path, manifest_path = asset_root / ASSET_NAME, asset_root / MANIFEST_NAME
    document = json.loads(gltf_path.read_text(encoding="utf-8"))
    asset_manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    design_gate = json.loads(arguments.design_gate.read_text(encoding="utf-8"))
    if design_gate.get("schema") != "phase2_pocket_v10f_design_gate_v1" or \
            design_gate.get("actual_gpu_authorized") is not False:
        raise ValueError("v10f design gate is missing or authorizes GPU capture")
    errors, validator_report = validate(document, asset_manifest, asset_root)
    if errors:
        raise ValueError("offline asset validator failed: " + "; ".join(errors))
    ownership, shade, raster = software_object_id(document, asset_manifest)
    image = measurements(ownership, shade)
    geometry = geometry_freeze(document)
    baseline_calibration = frozen_v10d_calibration()
    luma = image["tone_mapped_luma_prediction"]
    checks = {
        "rim_median_target_11_to_13px":
            11 <= image["rim_visible_run_px"]["median"] <= 13,
        "rim_all_samples_10_to_14px": all(10 <= value <= 14
            for value in image["rim_visible_run_px"]["samples"]),
        "sidewall_median_safety_18_to_24px":
            18 <= image["sidewall_visible_run_px"]["median"] <= 24,
        "sidewall_p10_at_least_16px":
            image["sidewall_visible_run_px"]["p10"] >= 16,
        "sidewall_minimum_at_least_14px":
            image["sidewall_visible_run_px"]["minimum"] >= 14,
        "eleven_ordered_columns_at_least_95_percent":
            image["ordered_stage_columns"]["sampled"] == 11 and
            image["ordered_stage_columns"]["fraction"] >= 0.95,
        "connected_stage_height_at_least_72px":
            image["connected_stage_component"]["height_px"] >= 72,
        "walnut_area_inside_envelope_at_most_10_percent":
            image["walnut_area_inside_mouth_envelope_fraction"] <= 0.10,
        "luma_delta_prediction_at_least_12": luma["upper_minus_lower"] >= 12,
        "luma_delta_conservative_lower_bound_at_least_9":
            luma["conservative_delta_lower_bound"] >= 9,
        "luma_lower_in_9_to_22": 9 <= luma["sidewall_lower_median"] <= 22,
        "marker_binding_unchanged": geometry["physics_marker_binding_unchanged"],
        "void_front_back_and_span_unchanged":
            abs(geometry["void_front_z_m"] - BALL_DIAMETER_M) <= 1e-6 and
            abs(geometry["void_back_z_m"] - BALL_DIAMETER_M * 2.2) <= 1e-6 and
            abs(geometry["void_span_m"] - BALL_DIAMETER_M * 1.2) <= 1e-6,
        "drop_ratio_0_55_to_0_85":
            0.55 <= geometry["drop_bottom_to_mouth_width_ratio"]["minimum"] and
            geometry["drop_bottom_to_mouth_width_ratio"]["maximum"] <= 0.85,
        "frozen_v10d_cpu_model_calibrated":
            baseline_calibration["pixel_axes_within_1px"] and
            baseline_calibration["walnut_fraction_residual_at_most_0_005"] and
            baseline_calibration["luma_anchor_exact"],
    }
    failed = [name for name, passed in checks.items() if not passed]
    report = {
        "schema": "phase2_pocket_v10f_cpu_projection_preflight_v1",
        "status": "OFFLINE_PREFLIGHT_PASS_GPU_NOT_AUTHORIZED" if not failed else
                  "OFFLINE_PREFLIGHT_FAIL_GPU_NOT_AUTHORIZED",
        "actual_gpu_used": False,
        "actual_gpu_authorized": False,
        "formal_pass_claimed": False,
        "production_binding": {
            "gltf": {"path": str(gltf_path.relative_to(ROOT)), "sha256": sha256(gltf_path)},
            "asset_manifest": {"path": str(manifest_path.relative_to(ROOT)),
                               "sha256": sha256(manifest_path)},
            "generator": {"path": "scripts/generate_phase2_modular_gltf_v10f.py",
                "sha256": sha256(ROOT / "scripts/generate_phase2_modular_gltf_v10f.py")},
            "validator": {"path": "scripts/validate_phase2_modular_gltf_v10f.py",
                "sha256": sha256(ROOT / "scripts/validate_phase2_modular_gltf_v10f.py")},
            "calibrator": {"path": "scripts/calibrate_phase2_pocket_v10f.py",
                "sha256": sha256(Path(__file__).resolve())},
            "design_gate": {"path": str(arguments.design_gate.resolve().relative_to(ROOT)),
                            "sha256": sha256(arguments.design_gate.resolve())},
            "asset_tests": {"path": "tests/visual/test_phase2_modular_gltf_v10f.py",
                "sha256": sha256(ROOT / "tests/visual/test_phase2_modular_gltf_v10f.py")},
            "calibration_tests": {
                "path": "tests/visual/test_phase2_pocket_v10f_calibration.py",
                "sha256": sha256(ROOT /
                    "tests/visual/test_phase2_pocket_v10f_calibration.py")},
        },
        "frozen_camera": {"eye_cm": list(CAMERA_EYE_CM),
            "target_cm": list(CAMERA_TARGET_CM),
            "vertical_fov_degrees": VERTICAL_FOV_DEGREES,
            "viewport_px": list(VIEWPORT), "roi_px": list(ROI)},
        "raster_method": {"backend": "deterministic_cpu_triangle_depth_object_id",
                          "pixel_sample": "center", **raster},
        "validator_report": validator_report,
        "geometry_freeze": geometry,
        "frozen_v10d_model_calibration": baseline_calibration,
        "predicted_image_measurements": image,
        "machine_checks": checks,
        "machine_failures": failed,
        "decision": "WAIT_FOR_ROOT_REVIEW; DO_NOT_CAPTURE_GPU",
    }
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                                encoding="utf-8")
    print(report["status"])
    print("report=" + str(arguments.output.resolve()))
    print("failures=" + json.dumps(failed))
    return 0 if not failed else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError, IndexError, struct.error,
            json.JSONDecodeError) as error:
        print("phase2 v10f CPU preflight failed: " + str(error))
        raise SystemExit(2)
