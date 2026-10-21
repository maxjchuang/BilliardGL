#!/usr/bin/env python3
"""Offline validation for the original Phase-2 modular glTF candidate."""

import argparse
import base64
import hashlib
import json
import math
from pathlib import Path
import struct
import sys
import zlib

from generate_m1_graybox_gltf import (
    BALL_RADIUS_M, CORNER_MOUTH_WIDTH_M, HALF_LENGTH_M, HALF_WIDTH_M,
    NOSE_HEIGHT_M, PLAY_SURFACE_Y_M, POCKETS, SIDE_MOUTH_WIDTH_M,
)
from generate_phase2_modular_gltf_v10e import (
    ASSET_NAME, ATLAS_COLUMNS, ATLAS_HEIGHT, ATLAS_NAME, ATLAS_ROWS,
    ATLAS_SOURCE_NAME, ATLAS_WIDTH, BALL_ATLAS_INSET_PX, BALL_CLOSEUP_NAME,
    BALL_DIAMETER_M, FELT_CORNER_CUTOUT_RADIUS_M,
    FELT_SIDE_CUTOUT_RADIUS_M, MANIFEST_NAME, MATERIAL_DEFINITIONS,
    MATERIAL_TEXTURE_SETS, PBR_TEXTURE_KEYS, POCKET_BACK_Z_M,
    POCKET_CENTER_Z_M, POCKET_FRONT_Z_M, POCKET_LINER_WIDTH_M,
    POCKET_MIN_SPAN_M, POCKET_RIM_INNER_BACK_Z_M, POCKET_RIM_WIDTH_M,
    POCKET_SIDEWALL_DROP_M,
    POCKET_SIDEWALL_LOWER_VERTEX_LUMA_SCALE,
    POCKET_SIDE_X_MARGIN_M, POCKET_Z_MARGIN_M,
    _rgba8_full_mip_bytes, marker_binding_sha256,
)
from generate_phase2_pbr_package import (
    LIGHTING_RIG_NAME, PARAMETERS_NAME, PREVIEW_NAME, TEXTURE_SPECS,
    texture_filename,
)


EXPECTED_MARKER_COUNT = 23
EXPECTED_MATERIAL_NAMES = {item[0] for item in MATERIAL_DEFINITIONS} | {
    "MAT_ball_identity_%02d" % identity for identity in range(16)
}
REQUIRED_ATTRIBUTES = {"POSITION", "NORMAL", "TANGENT", "TEXCOORD_0"}
REQUIRED_LOD_SETS = {"table", "ball", "cue", "lamp"}
FORBIDDEN_MARKER_TOKENS = ("_throat", "_capture", "_jaw_")


def _sha256(payload):
    return "sha256:" + hashlib.sha256(payload).hexdigest()


def _finite_vector(value, size):
    return isinstance(value, list) and len(value) == size and all(
        isinstance(item, (int, float)) and not isinstance(item, bool) and math.isfinite(item)
        for item in value)


def _close_vector(first, second, tolerance=1e-6):
    return _finite_vector(first, len(second)) and all(
        abs(first[index] - second[index]) <= tolerance for index in range(len(second)))


def _decode_buffer(document, errors):
    buffers = document.get("buffers")
    if not isinstance(buffers, list) or len(buffers) != 1:
        errors.append("exactly one embedded geometry buffer is required")
        return b""
    uri = buffers[0].get("uri", "")
    prefix = "data:application/octet-stream;base64,"
    if not isinstance(uri, str) or not uri.startswith(prefix):
        errors.append("geometry buffer must be an embedded application/octet-stream data URI")
        return b""
    try:
        payload = base64.b64decode(uri[len(prefix):], validate=True)
    except (ValueError, TypeError):
        errors.append("geometry buffer data URI is not valid base64")
        return b""
    if buffers[0].get("byteLength") != len(payload):
        errors.append("geometry buffer byteLength does not match payload")
    return payload


def _accessor_values(document, payload, accessor_index, errors, label):
    accessors = document.get("accessors", [])
    views = document.get("bufferViews", [])
    if not isinstance(accessor_index, int) or not 0 <= accessor_index < len(accessors):
        errors.append(label + " references an invalid accessor")
        return []
    accessor = accessors[accessor_index]
    view_index = accessor.get("bufferView")
    if not isinstance(view_index, int) or not 0 <= view_index < len(views):
        errors.append(label + " accessor references an invalid bufferView")
        return []
    view = views[view_index]
    components = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}.get(accessor.get("type"))
    formats = {5123: ("H", 2), 5125: ("I", 4), 5126: ("f", 4)}
    component = formats.get(accessor.get("componentType"))
    count = accessor.get("count")
    if components is None or component is None or not isinstance(count, int) or count <= 0:
        errors.append(label + " accessor format is unsupported")
        return []
    code, component_size = component
    offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    size = count * components * component_size
    if not isinstance(offset, int) or offset < 0 or offset + size > len(payload):
        errors.append(label + " accessor exceeds embedded geometry buffer")
        return []
    unpacked = struct.unpack_from("<%d%s" % (count * components, code), payload, offset)
    if components == 1:
        return list(unpacked)
    return [list(unpacked[index:index + components])
            for index in range(0, len(unpacked), components)]


def _expected_markers():
    result = {
        "SOCKET_play_surface_origin": [0.0, PLAY_SURFACE_Y_M, 0.0],
        "SOCKET_rail_nose_x_neg": [-HALF_WIDTH_M, PLAY_SURFACE_Y_M + NOSE_HEIGHT_M, 0.0],
        "SOCKET_rail_nose_x_pos": [HALF_WIDTH_M, PLAY_SURFACE_Y_M + NOSE_HEIGHT_M, 0.0],
        "SOCKET_rail_nose_z_neg": [0.0, PLAY_SURFACE_Y_M + NOSE_HEIGHT_M, -HALF_LENGTH_M],
        "SOCKET_rail_nose_z_pos": [0.0, PLAY_SURFACE_Y_M + NOSE_HEIGHT_M, HALF_LENGTH_M],
    }
    for pocket_id, _, _, center, _, tangent, width in POCKETS:
        result["SOCKET_pocket_%d_mouth" % pocket_id] = [
            center[0], PLAY_SURFACE_Y_M, center[1]]
        for suffix, sign in (("neg_t", -1.0), ("pos_t", 1.0)):
            result["SOCKET_pocket_%d_mouth_%s" % (pocket_id, suffix)] = [
                center[0] + tangent[0] * sign * width * 0.5,
                PLAY_SURFACE_Y_M,
                center[1] + tangent[1] * sign * width * 0.5]
    return result


def _node_mesh_positions(document, payload, node, errors):
    mesh_index = node.get("mesh")
    meshes = document.get("meshes", [])
    if not isinstance(mesh_index, int) or not 0 <= mesh_index < len(meshes):
        return []
    primitive = meshes[mesh_index].get("primitives", [{}])[0]
    return _accessor_values(document, payload,
                            primitive.get("attributes", {}).get("POSITION"), errors,
                            node.get("name", "node") + " POSITION")


def _descendant_triangles(document, root_index):
    nodes = document.get("nodes", [])
    meshes = document.get("meshes", [])
    if not isinstance(root_index, int) or not 0 <= root_index < len(nodes):
        return None
    pending = [root_index]
    visited = set()
    total = 0
    while pending:
        index = pending.pop()
        if index in visited or not 0 <= index < len(nodes):
            continue
        visited.add(index)
        node = nodes[index]
        pending.extend(node.get("children", []))
        mesh_index = node.get("mesh")
        if isinstance(mesh_index, int) and 0 <= mesh_index < len(meshes):
            total += meshes[mesh_index].get("extras", {}).get("triangle_count", 0)
    return total


def _validate_png(payload, errors, expected_size=None, label="ball atlas"):
    if len(payload) < 33 or payload[:8] != b"\x89PNG\r\n\x1a\n":
        errors.append(label + " is not a PNG")
        return None
    length, kind = struct.unpack(">I4s", payload[8:16])
    if length != 13 or kind != b"IHDR":
        errors.append(label + " has no canonical IHDR")
        return None
    width, height, depth, color_type, compression, filtering, interlace = struct.unpack(
        ">IIBBBBB", payload[16:29])
    expected = expected_size or (ATLAS_WIDTH, ATLAS_HEIGHT)
    if (width, height, depth, color_type, compression, filtering, interlace) != (
            expected[0], expected[1], 8, 6, 0, 0, 0):
        errors.append("%s must be %dx%d RGBA8 non-interlaced PNG" %
                      (label, expected[0], expected[1]))
        return None
    return width, height


def _decode_png_rgba(payload, errors, label):
    offset = 8
    width = height = None
    compressed = bytearray()
    while offset + 12 <= len(payload):
        length = struct.unpack_from(">I", payload, offset)[0]
        kind = payload[offset + 4:offset + 8]
        chunk = payload[offset + 8:offset + 8 + length]
        if kind == b"IHDR":
            width, height = struct.unpack_from(">II", chunk)
        elif kind == b"IDAT":
            compressed.extend(chunk)
        elif kind == b"IEND":
            break
        offset += 12 + length
    try:
        raw = zlib.decompress(compressed)
    except zlib.error:
        errors.append(label + " has invalid compressed pixels")
        return None
    if width is None or height is None or len(raw) != height * (width * 4 + 1):
        errors.append(label + " decoded byte count is invalid")
        return None
    pixels = bytearray(width * height * 4)
    for y in range(height):
        source = y * (width * 4 + 1)
        if raw[source] != 0:
            errors.append(label + " must use deterministic PNG filter 0")
            return None
        target = y * width * 4
        pixels[target:target + width * 4] = raw[source + 1:source + 1 + width * 4]
    return width, height, pixels


def _validate_ball_atlas_pixels(payload, errors):
    decoded = _decode_png_rgba(payload, errors, "ball atlas")
    if decoded is None:
        return
    width, height, pixels = decoded
    tile_width, tile_height = width // ATLAS_COLUMNS, height // ATLAS_ROWS
    ink = bytes((17, 18, 20, 255))
    for identity in (1, 8, 11, 15):
        x0 = identity % ATLAS_COLUMNS * tile_width
        y0 = identity // ATLAS_COLUMNS * tile_height
        ink_count = 0
        gutter_ink = 0
        for y in range(y0, y0 + tile_height):
            for x in range(x0, x0 + tile_width):
                offset = (y * width + x) * 4
                if pixels[offset:offset + 4] == ink:
                    ink_count += 1
                    if x < x0 + BALL_ATLAS_INSET_PX or \
                            x >= x0 + tile_width - BALL_ATLAS_INSET_PX or \
                            y < y0 + BALL_ATLAS_INSET_PX or \
                            y >= y0 + tile_height - BALL_ATLAS_INSET_PX:
                        gutter_ink += 1
        if ink_count < 250:
            errors.append("ball identity %d does not have a high-resolution readable digit" % identity)
        if gutter_ink:
            errors.append("ball identity %d digit leaks into the mip-safe gutter" % identity)


def _validate_felt_topology(positions, indices, label, errors):
    if len(indices) % 3:
        errors.append(label + " index count must be divisible by three")
        return
    edges = {}
    for start in range(0, len(indices), 3):
        triangle = indices[start:start + 3]
        if len(set(triangle)) != 3 or any(not 0 <= index < len(positions) for index in triangle):
            errors.append(label + " has invalid or degenerate indices")
            return
        a, b, c = (positions[index] for index in triangle)
        winding_y = (b[2] - a[2]) * (c[0] - a[0]) - (b[0] - a[0]) * (c[2] - a[2])
        if winding_y <= 1e-9:
            errors.append(label + " triangle winding is not consistently +Y")
            return
        for first, second in ((triangle[0], triangle[1]), (triangle[1], triangle[2]),
                              (triangle[2], triangle[0])):
            edge = tuple(sorted((first, second)))
            edges[edge] = edges.get(edge, 0) + 1
    if any(count not in (1, 2) for count in edges.values()):
        errors.append(label + " is not a 2-manifold with boundary")
        return
    boundary = [edge for edge, count in edges.items() if count == 1]
    adjacency = {}
    for first, second in boundary:
        adjacency.setdefault(first, set()).add(second)
        adjacency.setdefault(second, set()).add(first)
    if not adjacency or any(len(neighbours) != 2 for neighbours in adjacency.values()):
        errors.append(label + " boundary is open, branched, or cracked")
        return
    pending = [next(iter(adjacency))]
    visited = set()
    while pending:
        current = pending.pop()
        if current in visited:
            continue
        visited.add(current)
        pending.extend(adjacency[current] - visited)
    if visited != set(adjacency):
        errors.append(label + " must have one continuous outer/pocket boundary loop")
    for pocket_id, _, kind, center, _, _, _ in POCKETS:
        radius = FELT_CORNER_CUTOUT_RADIUS_M if kind == "corner" else \
            FELT_SIDE_CUTOUT_RADIUS_M
        samples = sum(abs(math.hypot(position[0] - center[0], position[2] - center[1]) - radius)
                      <= 2e-6 for index, position in enumerate(positions) if index in adjacency)
        if samples < 5:
            errors.append("%s pocket %d has no smooth analytic boundary samples" %
                          (label, pocket_id))


def _validate_mouth_plane_profile(document, payload, nodes, errors):
    """Enforce the TA-approved v10e connected pocket topology contract."""
    profile = document.get("asset", {}).get("extras", {}).get("pocket_visual_profile")
    if profile is None:
        return
    if profile != "connected_cavity_v10e":
        errors.append("unsupported pocket_visual_profile")
        return
    asset_extras = document["asset"]["extras"]
    expected_frame = {"x": "physics_tangent", "y": "world_up",
                      "z": "cavity_inward",
                      "marker_z_mapping": "z_visual=-z_marker",
                      "handedness": "right"}
    if asset_extras.get("pocket_visual_frame") != expected_frame:
        errors.append("v10b visual/marker frame mapping or handedness differs")
    expected_profile = {
        "ball_diameter_m": BALL_DIAMETER_M,
        "front_z_m": POCKET_FRONT_Z_M,
        "center_z_m": POCKET_CENTER_Z_M,
        "back_z_m": POCKET_BACK_Z_M,
        "minimum_span_m": POCKET_MIN_SPAN_M,
        "side_x_margin_m": POCKET_SIDE_X_MARGIN_M,
        "z_margin_m": POCKET_Z_MARGIN_M,
        "rim_width_m": POCKET_RIM_WIDTH_M,
        "rim_inner_back_z_m": POCKET_RIM_INNER_BACK_Z_M,
        "sidewall_drop_m": POCKET_SIDEWALL_DROP_M,
        "sidewall_lower_vertex_luma_scale":
            POCKET_SIDEWALL_LOWER_VERTEX_LUMA_SCALE,
    }
    if asset_extras.get("pocket_v10e_profile") != expected_profile:
        errors.append("v10e authored pocket profile differs from TA design gate")
    material_names = [item.get("name") for item in document.get("materials", [])]
    expected_materials = {
        "pocket_apron_integrated_wedge": "MAT_table_walnut",
        "pocket_liner_rim_visual_only": "MAT_pocket_liner",
        "pocket_liner_sidewall_visual_only": "MAT_pocket_liner",
        "pocket_void_visual_only": "MAT_pocket_void",
    }
    expected_roles = {
        "pocket_void_visual_only": "inward_or_plane",
        "pocket_liner_rim_visual_only": "inward_or_plane",
        "pocket_liner_sidewall_visual_only": "inward_or_plane",
        "pocket_apron_integrated_wedge": "outward_or_plane",
    }
    role_counts = {role: 0 for role in expected_roles}
    pocket_by_id = {pocket[0]: pocket for pocket in POCKETS}
    tolerance = 1e-6
    boundaries = {}
    for node in nodes:
        role = node.get("extras", {}).get("asset_role")
        if role not in expected_roles:
            continue
        role_counts[role] += 1
        extras = node.get("extras", {})
        if extras.get("mouth_plane_profile") != "connected_cavity_v10e" or \
                extras.get("halfspace") != expected_roles[role]:
            errors.append(node.get("name", "pocket node") +
                          " has an invalid mouth-plane halfspace contract")
            continue
        if role == "pocket_liner_rim_visual_only" and \
                (extras.get("shape") != "rounded_u_rim" or
                 abs(extras.get("authored_rim_width_m", 0.0) -
                     POCKET_RIM_WIDTH_M) > tolerance):
            errors.append(node.get("name", "pocket rim") +
                          " differs from the v10e rim contract")
        if role == "pocket_liner_sidewall_visual_only" and \
                (extras.get("shape") != "rounded_tapered_u_sidewall" or
                 abs(extras.get("authored_vertical_drop_m", 0.0) -
                     POCKET_SIDEWALL_DROP_M) > tolerance):
            errors.append(node.get("name", "pocket sidewall") +
                          " differs from the v10e sidewall contract")
        if role in {"pocket_liner_rim_visual_only",
                    "pocket_liner_sidewall_visual_only"}:
            minimum_segments = {0: 24, 1: 16, 2: 10}.get(extras.get("lod"), 10)
            if extras.get("arc_segments_per_half", 0) < minimum_segments:
                errors.append(node.get("name", "pocket liner") +
                              " has insufficient rounded arc tessellation")
        if role == "pocket_apron_integrated_wedge" and \
                extras.get("body_contiguous") is not True:
            errors.append(node.get("name", "pocket apron") +
                          " must be contiguous with the table body")
        positions = _node_mesh_positions(document, payload, node, errors)
        mesh = document.get("meshes", [])[node.get("mesh")]
        material_index = mesh.get("primitives", [{}])[0].get("material")
        actual_material = material_names[material_index] \
            if isinstance(material_index, int) and 0 <= material_index < len(material_names) else None
        if actual_material != expected_materials[role]:
            errors.append(node.get("name", "pocket node") +
                          " breaks rubber/facing/liner/void material order")
        ranges = extras.get("local_ranges")
        if not isinstance(ranges, list) or len(ranges) != len(POCKETS):
            errors.append(node.get("name", "pocket node") +
                          " must define one local range per pocket")
            continue
        covered = 0
        for item in ranges:
            pocket = pocket_by_id.get(item.get("pocket_id")) if isinstance(item, dict) else None
            start = item.get("vertex_start") if isinstance(item, dict) else None
            count = item.get("vertex_count") if isinstance(item, dict) else None
            if pocket is None or not isinstance(start, int) or not isinstance(count, int) or \
                    start != covered or count <= 0 or start + count > len(positions):
                errors.append(node.get("name", "pocket node") +
                              " has invalid pocket vertex ranges")
                break
            pocket_id, _, _, center, inward, tangent, width = pocket
            local = []
            for position in positions[start:start + count]:
                dx, dz = position[0] - center[0], position[2] - center[1]
                local.append((dx * tangent[0] + dz * tangent[1],
                              position[1] - PLAY_SURFACE_Y_M,
                              dx * inward[0] + dz * inward[1]))
            depths = [position[2] for position in local]
            if expected_roles[role] == "inward_or_plane" and min(depths) < -tolerance:
                errors.append("%s pocket %d has throat/bottom geometry outside mouth plane" %
                              (node.get("name"), pocket_id))
            if expected_roles[role] == "outward_or_plane" and max(depths) > tolerance:
                errors.append("%s pocket %d has apron geometry inside mouth plane" %
                              (node.get("name"), pocket_id))
            if role == "pocket_apron_integrated_wedge" and \
                    min(position[1] for position in positions[start:start + count]) > 0.780001:
                errors.append("%s pocket %d apron wedge floats above the table body" %
                              (node.get("name"), pocket_id))
            if role == "pocket_void_visual_only":
                front, back = min(depths), max(depths)
                center_present = any(abs(value - POCKET_CENTER_Z_M) <= tolerance
                                     for value in depths)
                width_span = max(value[0] for value in local) - \
                    min(value[0] for value in local)
                if abs(front - POCKET_FRONT_Z_M) > tolerance or \
                        abs(back - POCKET_BACK_Z_M) > tolerance or \
                        not center_present or back - front + tolerance < POCKET_MIN_SPAN_M:
                    errors.append("%s pocket %d fails v10b front/center/back depth" %
                                  (node.get("name"), pocket_id))
                if width_span <= 0 or extras.get("rounded_tapered") is not True:
                    errors.append("%s pocket %d is not a rounded tapered drop" %
                                  (node.get("name"), pocket_id))
            key = (extras.get("lod"), pocket_id)
            if role == "pocket_liner_rim_visual_only":
                boundaries.setdefault(key, {})["rim_inner"] = item.get("inner_boundary")
            elif role == "pocket_liner_sidewall_visual_only":
                boundaries.setdefault(key, {})["side_upper"] = item.get("upper_boundary")
                boundaries.setdefault(key, {})["side_lower"] = item.get("lower_boundary")
            elif role == "pocket_void_visual_only":
                boundaries.setdefault(key, {})["drop"] = item.get("boundary")
            covered += count
        if covered != len(positions):
            errors.append(node.get("name", "pocket node") +
                          " pocket vertex ranges do not cover the mesh")
    for role, count in role_counts.items():
        if count != 3:
            errors.append("pocket profile requires three LOD nodes for role " + role)

    def boundary_error(first, second):
        if not isinstance(first, list) or not isinstance(second, list) or \
                len(first) != len(second) or not first:
            return math.inf
        return max(math.hypot(a[0] - b[0], a[1] - b[1])
                   for a, b in zip(first, second))

    for key, stages in boundaries.items():
        if boundary_error(stages.get("rim_inner"), stages.get("side_upper")) > 0.00025 or \
                boundary_error(stages.get("side_lower"), stages.get("drop")) > 0.00025:
            errors.append("pocket %s rim-sidewall-drop boundaries are not position matched" %
                          (key,))
        lower = stages.get("side_lower")
        if isinstance(lower, list) and len(lower) >= 10:
            widths = [abs(point[0]) * 2.0 for point in lower]
            distinct = len({round(value / 0.0005) for value in widths})
            positive = [value for value in widths if value > 0.0005]
            ratio = min(positive) / max(positive) if positive else 0.0
            if distinct < 5 or ratio >= 0.85:
                errors.append("pocket %s drop width profile is slot-like" % (key,))

    if not (BALL_DIAMETER_M * 0.14 <= POCKET_RIM_WIDTH_M <=
            BALL_DIAMETER_M * 0.18):
        errors.append("v10e rim width is outside the authored D gate")
    if not (BALL_DIAMETER_M * 0.40 <= POCKET_SIDEWALL_DROP_M <=
            BALL_DIAMETER_M * 0.65):
        errors.append("v10e sidewall drop is outside the authored D gate")
    for node in nodes:
        lowered = node.get("name", "").lower()
        if ("slot" in lowered or "plate" in lowered) and \
                "pocket" in lowered:
            errors.append(node.get("name") + " is a forbidden independent slot/plate")

    sequence = ["MAT_table_cushion_rubber", "MAT_pocket_leather",
                "MAT_pocket_liner", "MAT_pocket_liner", "MAT_pocket_void"]
    if asset_extras.get("pocket_material_sequence") != sequence:
        errors.append("pocket material sequence must be rubber/facing/dark liner/black void")
    role_material = {}
    for node in nodes:
        role = node.get("extras", {}).get("asset_role")
        if role not in {"cushion_segment", "pocket_facing_visual_only"}:
            continue
        mesh = document["meshes"][node["mesh"]]
        index = mesh["primitives"][0]["material"]
        role_material.setdefault(role, set()).add(material_names[index])
    if role_material.get("cushion_segment") != {sequence[0]} or \
            role_material.get("pocket_facing_visual_only") != {sequence[1]}:
        errors.append("rubber/facing material stages are not explicit")

    authored_materials = {item[0]: item for item in MATERIAL_DEFINITIONS}
    material_gates = {
        "MAT_pocket_leather": ((0.018, 0.010, 0.007), (0.035, 0.022, 0.016), (0.54, 0.70)),
        "MAT_pocket_liner": ((0.015, 0.018, 0.020), (0.026, 0.032, 0.035), (0.68, 0.82)),
        "MAT_pocket_void": ((0.0003, 0.0003, 0.0004), (0.0012, 0.0014, 0.0016), (0.88, 1.0)),
        "MAT_table_walnut": ((0.040, 0.020, 0.012), (0.065, 0.035, 0.022), (0.32, 0.48)),
    }
    for name, (minimum, maximum, roughness_range) in material_gates.items():
        definition = authored_materials.get(name)
        color = definition[1] if definition else []
        roughness = definition[2] if definition else -1
        metalness = definition[3] if definition else -1
        if len(color) < 3 or any(not minimum[i] <= color[i] <= maximum[i]
                                 for i in range(3)) or \
                not roughness_range[0] <= roughness <= roughness_range[1] or \
                metalness != 0.0:
            errors.append(name + " fails the v10e material parameter gate")


def validate(document, manifest, asset_dir, verify_hashes=True):
    errors = []
    asset_dir = Path(asset_dir)
    asset = document.get("asset", {})
    extras = asset.get("extras", {})
    expected_metadata = {
        "asset_id": "focused_practice_hall_modular_v1",
        "authorship": "original_procedural_billiardgl",
        "external_source_assets": False,
        "production_candidate": True,
        "final_high_mesh": False,
        "unit": "metre",
        "up_axis": "+Y",
        "forward_axis": "+Z",
        "handedness": "right",
        "physics_boundary": "legacy_opening_band",
        "pocket_marker_scope": "mouth_only",
    }
    if asset.get("version") != "2.0":
        errors.append("asset.version must be glTF 2.0")
    for key, expected in expected_metadata.items():
        if extras.get(key) != expected:
            errors.append("asset.extras.%s must be %r" % (key, expected))
    if document.get("extensionsUsed") != ["KHR_materials_clearcoat", "KHR_texture_transform"]:
        errors.append("extensionsUsed must declare the two approved optional extensions")
    if document.get("extensionsRequired"):
        errors.append("optional material extensions must not be required")

    payload = _decode_buffer(document, errors)
    for index, view in enumerate(document.get("bufferViews", [])):
        offset = view.get("byteOffset", 0)
        length = view.get("byteLength")
        if not isinstance(offset, int) or offset % 4:
            errors.append("bufferView[%d] is not four-byte aligned" % index)
        if not isinstance(length, int) or length <= 0 or offset + length > len(payload):
            errors.append("bufferView[%d] exceeds the geometry buffer" % index)

    images = document.get("images", [])
    textures = document.get("textures", [])
    expected_image_uris = [ATLAS_NAME] + [
        texture_filename(texture_set, channel)
        for texture_set, channel in PBR_TEXTURE_KEYS]
    if len(images) != len(expected_image_uris) or \
            [image.get("uri") for image in images] != expected_image_uris or \
            any(image.get("mimeType") != "image/png" for image in images):
        errors.append("images must be the generated atlas plus 18 generated PBR maps")
    if len(textures) != len(expected_image_uris) or any(
            texture.get("source") != index for index, texture in enumerate(textures)):
        errors.append("textures must map one-to-one to the 19 generated images")
    for index, image in enumerate(images):
        uri = image.get("uri", "")
        if not isinstance(uri, str) or uri not in expected_image_uris or "/" in uri or "\\" in uri:
            errors.append("external or nested image URI is forbidden")
        if image.get("extras", {}).get("original_procedural") is not True:
            errors.append("image[%d] must be labelled original procedural" % index)
    samplers = document.get("samplers", [])
    if len(samplers) != 2 or samplers[0].get("wrapS") != 33071 or \
            samplers[1].get("wrapS") != 10497:
        errors.append("atlas clamp and PBR repeat samplers are required")

    materials = document.get("materials", [])
    names = [item.get("name") for item in materials]
    if len(materials) != len(EXPECTED_MATERIAL_NAMES) or \
            set(names) != EXPECTED_MATERIAL_NAMES or len(names) != len(set(names)):
        errors.append("candidate must contain exactly the approved unique material slots")
    ball_transforms = set()
    for index, material in enumerate(materials):
        pbr = material.get("pbrMetallicRoughness", {})
        color = pbr.get("baseColorFactor")
        if not _finite_vector(color, 4) or any(value < 0.0 or value > 1.0 for value in color):
            errors.append("material[%d] baseColorFactor must be finite RGBA" % index)
        for field in ("roughnessFactor", "metallicFactor"):
            value = pbr.get(field)
            if not isinstance(value, (int, float)) or not 0.0 <= value <= 1.0:
                errors.append("material[%d].%s must be in [0,1]" % (index, field))
        clearcoat = material.get("extensions", {}).get("KHR_materials_clearcoat")
        if clearcoat is not None:
            if material.get("extras", {}).get("clearcoat_fallback") != \
                    "energy_conserving_specular_lobe":
                errors.append("material[%d] clearcoat has no documented fallback" % index)
            if not all(isinstance(clearcoat.get(field), (int, float)) and
                       0.0 <= clearcoat[field] <= 1.0
                       for field in ("clearcoatFactor", "clearcoatRoughnessFactor")):
                errors.append("material[%d] clearcoat factors are invalid" % index)
        identity = material.get("extras", {}).get("ball_identity")
        if identity is not None:
            if not isinstance(identity, int) or not 0 <= identity < 16:
                errors.append("ball identity must be an integer in [0,15]")
                continue
            texture_info = pbr.get("baseColorTexture", {})
            transform = texture_info.get("extensions", {}).get("KHR_texture_transform", {})
            offset, scale = transform.get("offset"), transform.get("scale")
            expected_offset = [identity % ATLAS_COLUMNS / ATLAS_COLUMNS +
                               BALL_ATLAS_INSET_PX / ATLAS_WIDTH,
                               identity // ATLAS_COLUMNS / ATLAS_ROWS +
                               BALL_ATLAS_INSET_PX / ATLAS_HEIGHT]
            expected_scale = [1 / ATLAS_COLUMNS - 2 * BALL_ATLAS_INSET_PX / ATLAS_WIDTH,
                              1 / ATLAS_ROWS - 2 * BALL_ATLAS_INSET_PX / ATLAS_HEIGHT]
            if texture_info.get("index") != 0 or not _close_vector(offset, expected_offset) or \
                    not _close_vector(scale, expected_scale):
                errors.append("ball identity %d atlas transform is invalid" % identity)
            ball_transforms.add((identity, tuple(offset or ()), tuple(scale or ())))
        expected_texture_set = MATERIAL_TEXTURE_SETS.get(material.get("name"))
        if expected_texture_set:
            if pbr.get("baseColorFactor") != [1.0, 1.0, 1.0, 1.0]:
                errors.append("material[%d] textured baseColorFactor must remain neutral to avoid double tint" % index)
            if material.get("extras", {}).get("pbr_texture_set") != expected_texture_set:
                errors.append("material[%d] PBR texture set binding is invalid" % index)
            repeat = next(spec[6] for spec in TEXTURE_SPECS if spec[0] == expected_texture_set)
            texture_infos = {
                "basecolor": pbr.get("baseColorTexture", {}),
                "normal": material.get("normalTexture", {}),
                "orm_mr": pbr.get("metallicRoughnessTexture", {}),
                "orm_ao": material.get("occlusionTexture", {}),
            }
            expected_indices = {
                "basecolor": 1 + PBR_TEXTURE_KEYS.index((expected_texture_set, "basecolor")),
                "normal": 1 + PBR_TEXTURE_KEYS.index((expected_texture_set, "normal")),
                "orm_mr": 1 + PBR_TEXTURE_KEYS.index((expected_texture_set, "orm")),
                "orm_ao": 1 + PBR_TEXTURE_KEYS.index((expected_texture_set, "orm")),
            }
            for role, texture_info in texture_infos.items():
                transform = texture_info.get("extensions", {}).get("KHR_texture_transform", {})
                if texture_info.get("index") != expected_indices[role] or \
                        transform.get("offset") != [0.0, 0.0] or \
                        transform.get("scale") != repeat:
                    errors.append("material[%d] %s texture binding is invalid" % (index, role))
            if material.get("extras", {}).get("normal_convention") != "opengl_positive_y" or \
                    material.get("extras", {}).get("orm_channels") != \
                    "r_ao_g_roughness_b_metallic":
                errors.append("material[%d] PBR channel metadata is invalid" % index)
            if not isinstance(material.get("extras", {}).get("untextured_fallback_factors"), dict):
                errors.append("material[%d] must retain untextured fallback factors" % index)
    if len(ball_transforms) != 16:
        errors.append("all 16 unique ball atlas transforms are required")
    expected_atlas_contract = {
        "tile_size_px": [ATLAS_WIDTH // ATLAS_COLUMNS, ATLAS_HEIGHT // ATLAS_ROWS],
        "safe_inset_px": BALL_ATLAS_INSET_PX,
        "padding": "edge_dilated_identity_base",
        "review_identities": [1, 8, 11, 15],
        "sphere_uv_u_orientation": "decreases_with_positive_longitude",
        "glyph_storage_orientation": "upright_source",
    }
    if manifest.get("ball_atlas_contract") != expected_atlas_contract:
        errors.append("ball atlas tile/mip-safe contract is invalid")
    variants = manifest.get("ball_material_variants", {})
    if any(variants.get(str(identity), {}).get("safe_inset_px") != BALL_ATLAS_INSET_PX
           for identity in range(16)):
        errors.append("all ball material variants must declare the safe atlas inset")

    meshes = document.get("meshes", [])
    unique_triangles = 0
    for mesh_index, mesh in enumerate(meshes):
        primitives = mesh.get("primitives", [])
        if len(primitives) != 1:
            errors.append("mesh[%d] must contain one primitive" % mesh_index)
            continue
        primitive = primitives[0]
        attributes = primitive.get("attributes", {})
        role = mesh.get("extras", {}).get("asset_role")
        expected_attributes = REQUIRED_ATTRIBUTES | ({"COLOR_0"} if role ==
            "pocket_liner_sidewall_visual_only" else set())
        if set(attributes) != expected_attributes:
            errors.append("mesh[%d] has an invalid v10e vertex attribute contract" % mesh_index)
        values = {semantic: _accessor_values(
            document, payload, attributes.get(semantic), errors,
            "mesh[%d] %s" % (mesh_index, semantic)) for semantic in REQUIRED_ATTRIBUTES}
        indices = _accessor_values(document, payload, primitive.get("indices"), errors,
                                   "mesh[%d] indices" % mesh_index)
        count = len(values["POSITION"])
        if any(len(values[semantic]) != count for semantic in REQUIRED_ATTRIBUTES):
            errors.append("mesh[%d] vertex attribute counts differ" % mesh_index)
        if len(indices) % 3 or (indices and max(indices) >= count):
            errors.append("mesh[%d] triangle indices are invalid" % mesh_index)
        if primitive.get("mode", 4) != 4:
            errors.append("mesh[%d] must use TRIANGLES" % mesh_index)
        material_index = primitive.get("material")
        if not isinstance(material_index, int) or not 0 <= material_index < len(materials):
            errors.append("mesh[%d] has an invalid material" % mesh_index)
        for normal in values["NORMAL"]:
            if not _finite_vector(normal, 3) or abs(math.sqrt(sum(v * v for v in normal)) - 1) > 2e-5:
                errors.append("mesh[%d] contains a non-unit normal" % mesh_index)
                break
        for tangent in values["TANGENT"]:
            if not _finite_vector(tangent, 4) or abs(math.sqrt(sum(v * v for v in tangent[:3])) - 1) > 2e-5 \
                    or tangent[3] not in (-1.0, 1.0):
                errors.append("mesh[%d] contains an invalid tangent" % mesh_index)
                break
        if any(not _finite_vector(uv, 2) for uv in values["TEXCOORD_0"]):
            errors.append("mesh[%d] contains an invalid UV0" % mesh_index)
        if role == "pocket_liner_sidewall_visual_only":
            colors = _accessor_values(document, payload, attributes.get("COLOR_0"),
                                      errors, "mesh[%d] COLOR_0" % mesh_index)
            if len(colors) != count or any(not _finite_vector(color, 4) or
                    min(color[:3]) < POCKET_SIDEWALL_LOWER_VERTEX_LUMA_SCALE - 1e-6 or
                    max(color[:3]) > 1.0 + 1e-6 or
                    max(color[:3]) - min(color[:3]) > 1e-6 or
                    abs(color[3] - 1.0) > 1e-6 for color in colors):
                errors.append("mesh[%d] sidewall COLOR_0 ramp is invalid" % mesh_index)
        position_accessor = document.get("accessors", [])[attributes.get("POSITION", 0)]
        if values["POSITION"]:
            minimum = [min(value[axis] for value in values["POSITION"]) for axis in range(3)]
            maximum = [max(value[axis] for value in values["POSITION"]) for axis in range(3)]
            if not _close_vector(position_accessor.get("min"), minimum) or \
                    not _close_vector(position_accessor.get("max"), maximum):
                errors.append("mesh[%d] POSITION bounds differ from buffer data" % mesh_index)
        triangles = len(indices) // 3
        if mesh.get("extras", {}).get("triangle_count") != triangles:
            errors.append("mesh[%d] triangle metadata is invalid" % mesh_index)
        unique_triangles += triangles
        if mesh.get("extras", {}).get("asset_role") == "table_felt":
            _validate_felt_topology(values["POSITION"], indices,
                                    mesh.get("name", "table felt"), errors)

    nodes = document.get("nodes", [])
    node_names = [node.get("name") for node in nodes]
    if len(node_names) != len(set(node_names)):
        errors.append("node names must be unique")
    for index, node in enumerate(nodes):
        if "scale" in node and node.get("scale") != [1.0, 1.0, 1.0]:
            errors.append("node[%d] has non-unit scale" % index)
        if "translation" in node and not _finite_vector(node["translation"], 3):
            errors.append("node[%d] translation is invalid" % index)
        for child in node.get("children", []):
            if not isinstance(child, int) or not 0 <= child < len(nodes):
                errors.append("node[%d] has invalid child" % index)
        mesh_index = node.get("mesh")
        if mesh_index is not None and (not isinstance(mesh_index, int) or not 0 <= mesh_index < len(meshes)):
            errors.append("node[%d] has invalid mesh" % index)

    by_name = {node.get("name"): node for node in nodes}
    _validate_mouth_plane_profile(document, payload, nodes, errors)
    expected_markers = _expected_markers()
    actual_markers = {name for name in node_names
                      if isinstance(name, str) and name.startswith("SOCKET_")}
    if actual_markers != set(expected_markers):
        errors.append("alignment marker set must be the approved 23 mouth-only markers")
    for name, expected in expected_markers.items():
        node = by_name.get(name)
        if node is None or not _close_vector(node.get("translation"), expected, 5e-7):
            errors.append(name + " differs from the approved alignment")
    for name in actual_markers:
        if any(token in name for token in FORBIDDEN_MARKER_TOKENS):
            errors.append("unpromoted pocket marker is forbidden: " + name)
    marker_indices = [index for index, node in enumerate(nodes)
                      if node.get("name") in expected_markers]
    actual_marker_binding = marker_binding_sha256(document, marker_indices)
    if document.get("asset", {}).get("extras", {}).get(
            "physics_marker_binding_sha256") != actual_marker_binding:
        errors.append("asset physics marker binding SHA-256 differs from signed marker data")

    lod_sets = manifest.get("lod_sets", {})
    if set(lod_sets) != REQUIRED_LOD_SETS:
        errors.append("manifest must define table/ball/cue/lamp LOD sets")
    marker_name_list = [nodes[index].get("name") for index in marker_indices]
    for set_name in REQUIRED_LOD_SETS:
        lod_set = lod_sets.get(set_name, {})
        levels, triangle_budget = lod_set.get("nodes", {}), lod_set.get("triangles", {})
        if set(levels) != {"0", "1", "2"} or set(triangle_budget) != {"0", "1", "2"}:
            errors.append(set_name + " must contain LOD0/1/2")
            continue
        actual_counts = []
        for lod in ("0", "1", "2"):
            root = levels[lod]
            count = _descendant_triangles(document, root)
            actual_counts.append(count)
            if count != triangle_budget[lod]:
                errors.append("%s LOD%s triangle budget differs from glTF" % (set_name, lod))
            if not isinstance(root, int) or not 0 <= root < len(nodes) or \
                    nodes[root].get("extras", {}).get("alignment_marker_nodes") != marker_indices:
                errors.append("%s LOD%s does not reference the identical marker list" % (set_name, lod))
        if not actual_counts[0] > actual_counts[1] > actual_counts[2] > 0:
            errors.append(set_name + " LOD triangles must strictly decrease")
        if lod_set.get("alignment_markers") != marker_name_list:
            errors.append(set_name + " manifest marker names differ")

    # Enforce the approved 0.10 cm no-visual-solid clearance at each mouth.
    clearance = 0.001
    for node in nodes:
        role = node.get("extras", {}).get("asset_role")
        if role not in {"cushion_segment", "pocket_facing_visual_only"}:
            continue
        positions = _node_mesh_positions(document, payload, node, errors)
        for pocket_id, _, _, center, inward, tangent, width in POCKETS:
            for position in positions:
                delta = (position[0] - center[0], position[2] - center[1])
                offset = delta[0] * tangent[0] + delta[1] * tangent[1]
                depth = -(delta[0] * inward[0] + delta[1] * inward[1])
                near_mouth_plane = -0.055 <= depth <= 0.080
                if near_mouth_plane and abs(offset) < width * 0.5 + clearance - 1e-7:
                    errors.append("%s intrudes pocket %d NO_VISUAL_SOLID clearance" %
                                  (node.get("name"), pocket_id))
                    positions = []
                    break
            if not positions:
                break

    ball_nodes = [node for node in nodes if node.get("extras", {}).get("asset_role") == "shared_ball"]
    if len(ball_nodes) != 3:
        errors.append("one shared sphere is required for each of three ball LODs")
    for node in ball_nodes:
        positions = _node_mesh_positions(document, payload, node, errors)
        if positions and max(abs(math.sqrt(sum(v * v for v in p)) - BALL_RADIUS_M)
                             for p in positions) > 2e-6:
            errors.append(node.get("name", "ball") + " radius differs from the approved ball")

    constraints = manifest.get("constraints", {})
    required_constraints = {
        "unpromoted_pocket_parameters_present": False,
        "all_lod_marker_lists_identical": True,
        "pocket_leather_no_visual_solid_checked": True,
        "clearcoat_extensions_required": False,
        "metallic_roughness_fallback_present": True,
        "pbr_textures_original_procedural": True,
        "basecolor_srgb_normal_orm_linear": True,
        "normal_convention": "opengl_positive_y",
        "lighting_rig_has_no_external_hdr": True,
        "remote_furniture_in_scope": False,
        "felt_index_bounds_winding_boundary_validated": True,
        "ball_atlas_mip_safe_inset_validated": True,
    }
    for key, expected in required_constraints.items():
        if constraints.get(key) != expected:
            errors.append("manifest constraint %s must be %r" % (key, expected))
    expected_topology = {
        "surface": "single_manifold_with_boundary", "boundary_loops": 1,
        "pocket_openings": 6,
        "winding": "counter_clockwise_when_viewed_from_positive_y",
        "max_edge_incidence": 2, "nondegenerate_triangles": True,
    }
    if manifest.get("felt_topology") != expected_topology:
        errors.append("felt topology evidence contract is invalid")
    binding = manifest.get("alignment_binding", {})
    expected_binding = {
        "alignment_id": "focused_practice_hall_m1_alignment_v1",
        "runtime_profile_id": "chinese_pool_interactive_120hz_v5",
        "geometry_id": "legacy_opening_band",
        "surface_y_cm": 86.483976,
        "nose_height_ratio": 1.4,
        "pocket_marker_scope": "mouth_only",
        "marker_tolerance_cm": 0.05,
        "no_visual_solid_clearance_cm": 0.10,
        "capture_presentation": "simulation_driven_no_detach",
        "physics_marker_binding_sha256": actual_marker_binding,
        "visual_to_marker_frame": {
            "x_visual": "x_marker",
            "y_visual": "y_marker",
            "z_visual": "-z_marker",
            "visual_handedness": "right",
        },
    }
    for key, expected in expected_binding.items():
        if binding.get(key) != expected:
            errors.append("alignment binding %s must remain frozen" % key)

    expected_v10b_contract = {
        "status": "TA_DESIGN_PASS_IMPLEMENTATION_CANDIDATE",
        "ball_diameter_m": BALL_DIAMETER_M,
        "front_center_back_d": [1.0, 1.6, 2.2],
        "front_center_back_m": [
            POCKET_FRONT_Z_M, POCKET_CENTER_Z_M, POCKET_BACK_Z_M],
        "authored_span_d": 1.2,
        "authored_span_m": POCKET_BACK_Z_M - POCKET_FRONT_Z_M,
        "minimum_span_d": 1.1,
        "minimum_span_m": POCKET_MIN_SPAN_M,
        "side_x_margin_m": POCKET_SIDE_X_MARGIN_M,
        "z_margin_m": POCKET_Z_MARGIN_M,
        "liner_width_m": POCKET_LINER_WIDTH_M,
        "liner_occlusion_target_px": [8, 30],
        "pure_black_inward_min_projected_ball_diameter": 0.5,
        "pure_black_inward_min_px": 36,
        "black_depth_width_ratio_min": 0.32,
        "material_sequence": ["rubber", "facing", "dark_liner", "black_void"],
        "formal_six_scene_evidence_authorized": False,
        "manual_visual_status": "PENDING_TA",
    }
    if manifest.get("pocket_v10b_contract") != expected_v10b_contract:
        errors.append("manifest pocket_v10b_contract differs from TA design pass")

    budgets = manifest.get("budgets", {})
    if budgets.get("unique_triangles") != unique_triangles:
        errors.append("unique triangle budget differs from glTF")
    if budgets.get("materials_total") != len(materials) or \
            budgets.get("textures") != len(expected_image_uris):
        errors.append("material or texture budget differs from glTF")
    if budgets.get("functional_material_slots") != {
            "table": 7, "cue": 2, "lamp": 2,
            "background": 1, "ball_variants": 16}:
        errors.append("functional material-slot budget differs from v10b package")
    if budgets.get("embedded_geometry_buffer_bytes") != len(payload):
        errors.append("geometry buffer budget differs from glTF")

    atlas_path = asset_dir / ATLAS_NAME
    svg_path = asset_dir / ATLAS_SOURCE_NAME
    closeup_path = asset_dir / BALL_CLOSEUP_NAME
    gltf_path = asset_dir / ASSET_NAME
    pbr_paths = [asset_dir / texture_filename(texture_set, channel)
                 for texture_set, channel in PBR_TEXTURE_KEYS]
    support_paths = [asset_dir / PARAMETERS_NAME, asset_dir / LIGHTING_RIG_NAME,
                     asset_dir / PREVIEW_NAME]
    for path in (atlas_path, svg_path, closeup_path, gltf_path, *pbr_paths, *support_paths):
        if not path.is_file():
            errors.append("missing candidate file " + path.name)
    atlas_bytes = atlas_path.read_bytes() if atlas_path.is_file() else b""
    svg_bytes = svg_path.read_bytes() if svg_path.is_file() else b""
    _validate_png(atlas_bytes, errors)
    _validate_ball_atlas_pixels(atlas_bytes, errors)
    closeup_bytes = closeup_path.read_bytes() if closeup_path.is_file() else b""
    _validate_png(closeup_bytes, errors,
                  (ATLAS_WIDTH // ATLAS_COLUMNS * 4, ATLAS_HEIGHT // ATLAS_ROWS),
                  "1/8/11/15 identity closeup")
    if b"Original BilliardGL procedural" not in svg_bytes or b"href=" in svg_bytes:
        errors.append("SVG must be an original procedural source without external references")
    if budgets.get("atlas_disk_bytes") != len(atlas_bytes) or \
            budgets.get("atlas_gpu_bytes_rgba8_with_full_mips") != \
            _rgba8_full_mip_bytes(ATLAS_WIDTH, ATLAS_HEIGHT):
        errors.append("atlas disk or GPU budget is invalid")
    pbr_disk_bytes = sum(path.stat().st_size for path in pbr_paths if path.is_file())
    expected_pbr_gpu_bytes = sum(
        3 * (4 * ((4 * size * size - 1) // 3)) for _, size, *_ in TEXTURE_SPECS)
    if budgets.get("pbr_texture_disk_bytes") != pbr_disk_bytes or \
            budgets.get("pbr_texture_gpu_bytes_rgba8_with_full_mips") != expected_pbr_gpu_bytes or \
            budgets.get("runtime_texture_gpu_bytes_rgba8_with_full_mips") != \
            _rgba8_full_mip_bytes(ATLAS_WIDTH, ATLAS_HEIGHT) + expected_pbr_gpu_bytes:
        errors.append("PBR texture disk or full-mip GPU budget is invalid")
    if verify_hashes:
        files = manifest.get("files", {})
        expected_files = {
            "gltf": gltf_path,
            "ball_atlas_png": atlas_path,
            "ball_atlas_vector_source": svg_path,
            "ball_identity_closeup": closeup_path,
            "pbr_parameters": asset_dir / PARAMETERS_NAME,
            "lighting_rig": asset_dir / LIGHTING_RIG_NAME,
            "material_review_preview": asset_dir / PREVIEW_NAME,
        }
        for key, path in expected_files.items():
            if path.is_file() and files.get(key, {}).get("sha256") != _sha256(path.read_bytes()):
                errors.append(key + " SHA-256 differs from manifest")
        texture_entries = files.get("pbr_textures", {})
        for texture_set, channel in PBR_TEXTURE_KEYS:
            path = asset_dir / texture_filename(texture_set, channel)
            if path.is_file() and texture_entries.get(texture_set, {}).get(channel, {}).get("sha256") != \
                    _sha256(path.read_bytes()):
                errors.append("%s %s SHA-256 differs from manifest" % (texture_set, channel))

    report = {
        "unique_triangles": unique_triangles,
        "materials": len(materials),
        "textures": len(textures),
        "markers": len(actual_markers),
        "geometry_bytes": len(payload),
        "atlas_disk_bytes": len(atlas_bytes),
        "lod_triangles": {name: lod_sets.get(name, {}).get("triangles", {})
                          for name in sorted(REQUIRED_LOD_SETS)},
    }
    return errors, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("asset", type=Path,
                        nargs="?", default=Path("assets/modern/phase2-v10e") / ASSET_NAME)
    parser.add_argument("--manifest", type=Path)
    arguments = parser.parse_args()
    manifest_path = arguments.manifest or arguments.asset.with_name(MANIFEST_NAME)
    try:
        document = json.loads(arguments.asset.read_text(encoding="utf-8"))
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        print("phase2 validation failed: " + str(error), file=sys.stderr)
        return 2
    errors, report = validate(document, manifest, arguments.asset.parent)
    if errors:
        for error in errors:
            print("error: " + error, file=sys.stderr)
        return 1
    print("phase2 modular glTF valid: unique_triangles=%d materials=%d textures=%d "
          "markers=%d geometry_bytes=%d atlas_disk_bytes=%d" %
          (report["unique_triangles"], report["materials"], report["textures"],
           report["markers"], report["geometry_bytes"], report["atlas_disk_bytes"]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
