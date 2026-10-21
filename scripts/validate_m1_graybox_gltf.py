#!/usr/bin/env python3
"""Offline validation for the original M1 graybox glTF asset."""

import argparse
import base64
import hashlib
import json
import math
from pathlib import Path
import struct
import sys


PLAY_SURFACE_Y_M = 0.86483976
BALL_RADIUS_M = 0.028575
NOSE_Y_M = PLAY_SURFACE_Y_M + BALL_RADIUS_M * 1.4
HALF_WIDTH_M = 0.635
HALF_LENGTH_M = 1.27

POCKETS = (
    (0, (-0.635, -1.27), (math.sqrt(0.5), math.sqrt(0.5)), (-math.sqrt(0.5), math.sqrt(0.5)), 0.132),
    (1, (0.635, -1.27), (-math.sqrt(0.5), math.sqrt(0.5)), (-math.sqrt(0.5), -math.sqrt(0.5)), 0.132),
    (2, (-0.635, 1.27), (math.sqrt(0.5), -math.sqrt(0.5)), (math.sqrt(0.5), math.sqrt(0.5)), 0.132),
    (3, (0.635, 1.27), (-math.sqrt(0.5), -math.sqrt(0.5)), (math.sqrt(0.5), -math.sqrt(0.5)), 0.132),
    (4, (-0.635, 0.0), (1.0, 0.0), (0.0, -1.0), 0.086),
    (5, (0.635, 0.0), (-1.0, 0.0), (0.0, 1.0), 0.086),
)

REQUIRED_VISIBLE_NODES = {
    "NODE_table_body_proxy", "NODE_table_felt_proxy",
    "NODE_rail_x_neg_z_neg", "NODE_rail_x_neg_z_pos",
    "NODE_rail_x_pos_z_neg", "NODE_rail_x_pos_z_pos",
    "NODE_rail_z_neg", "NODE_rail_z_pos",
    "NODE_cue_proxy_root", "NODE_ball_proxy_shared_instance",
    "NODE_rect_lamp_housing_proxy", "NODE_rect_lamp_diffuser_proxy",
    "NODE_room_floor_proxy", "NODE_room_wall_x_neg_proxy",
    "NODE_room_wall_x_pos_proxy", "NODE_room_wall_z_neg_proxy",
    "NODE_room_wall_z_pos_proxy",
} | {"NODE_pocket_proxy_%d_%s" % (item[0], name) for item, name in zip(POCKETS, (
    "corner_nx_nz", "corner_px_nz", "corner_nx_pz", "corner_px_pz", "side_nx", "side_px"))}


def _finite_vector(value, size):
    return isinstance(value, list) and len(value) == size and all(
        isinstance(item, (int, float)) and not isinstance(item, bool) and math.isfinite(item)
        for item in value)


def _close_vector(first, second, tolerance=1e-7):
    return _finite_vector(first, len(second)) and all(
        abs(first[index] - second[index]) <= tolerance for index in range(len(second)))


def _decode_buffer(document, errors):
    buffers = document.get("buffers")
    if not isinstance(buffers, list) or len(buffers) != 1:
        errors.append("exactly one embedded buffer is required")
        return b""
    uri = buffers[0].get("uri", "")
    prefix = "data:application/octet-stream;base64,"
    if not uri.startswith(prefix):
        errors.append("buffer must use an embedded application/octet-stream data URI")
        return b""
    try:
        payload = base64.b64decode(uri[len(prefix):], validate=True)
    except ValueError:
        errors.append("buffer data URI is not valid base64")
        return b""
    if buffers[0].get("byteLength") != len(payload):
        errors.append("buffer byteLength does not match embedded payload")
    return payload


def _accessor_values(document, payload, accessor_index, errors):
    accessors = document.get("accessors", [])
    views = document.get("bufferViews", [])
    if not isinstance(accessor_index, int) or not 0 <= accessor_index < len(accessors):
        errors.append("primitive references an invalid accessor")
        return []
    accessor = accessors[accessor_index]
    view_index = accessor.get("bufferView")
    if not isinstance(view_index, int) or not 0 <= view_index < len(views):
        errors.append("accessor references an invalid bufferView")
        return []
    view = views[view_index]
    components = {"SCALAR": 1, "VEC3": 3}.get(accessor.get("type"))
    formats = {5123: ("H", 2), 5125: ("I", 4), 5126: ("f", 4)}
    if components is None or accessor.get("componentType") not in formats:
        errors.append("unsupported accessor format")
        return []
    code, component_size = formats[accessor["componentType"]]
    count = accessor.get("count")
    if not isinstance(count, int) or count <= 0:
        errors.append("accessor count must be positive")
        return []
    offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    size = count * components * component_size
    if offset < 0 or offset + size > len(payload):
        errors.append("accessor exceeds embedded buffer")
        return []
    unpacked = struct.unpack_from("<%d%s" % (count * components, code), payload, offset)
    if components == 1:
        return list(unpacked)
    return [list(unpacked[index:index + components])
            for index in range(0, len(unpacked), components)]


def validate(document, alignment):
    errors = []
    asset = document.get("asset", {})
    extras = asset.get("extras", {})
    expected_metadata = {
        "asset_id": "focused_practice_hall_m1_graybox",
        "schema_version": 1,
        "authorship": "original_procedural_billiardgl",
        "external_source_assets": False,
        "graybox": True,
        "production_high_mesh": False,
        "unit": "metre",
        "up_axis": "+Y",
        "forward_axis": "+Z",
        "handedness": "right",
        "play_surface_height_m": PLAY_SURFACE_Y_M,
        "physics_boundary": "legacy_opening_band",
        "pocket_marker_scope": "mouth_only",
        "alignment_manifest_id": "focused_practice_hall_m1_alignment_v1",
        "alignment_manifest": "assets/modern/graybox/focused_practice_hall_m1_alignment_v1.json",
        "source_of_truth": "scripts/generate_m1_graybox_gltf.py",
    }
    if asset.get("version") != "2.0":
        errors.append("asset.version must be glTF 2.0")
    for key, expected in expected_metadata.items():
        if extras.get(key) != expected:
            errors.append("asset.extras.%s must be %r" % (key, expected))
    for forbidden in ("images", "textures"):
        if document.get(forbidden):
            errors.append(forbidden + " are forbidden in the self-contained graybox")

    payload = _decode_buffer(document, errors)
    views = document.get("bufferViews", [])
    for index, view in enumerate(views):
        offset = view.get("byteOffset", 0)
        length = view.get("byteLength")
        if not isinstance(offset, int) or offset % 4 != 0:
            errors.append("bufferView[%d] is not four-byte aligned" % index)
        if not isinstance(length, int) or length <= 0 or offset + length > len(payload):
            errors.append("bufferView[%d] exceeds the embedded buffer" % index)

    materials = document.get("materials")
    if not isinstance(materials, list) or len(materials) != 9:
        errors.append("graybox must contain exactly 9 material slots")
        materials = []
    for index, material in enumerate(materials):
        pbr = material.get("pbrMetallicRoughness", {})
        color = pbr.get("baseColorFactor")
        if not _finite_vector(color, 4) or any(value < 0 or value > 1 for value in color):
            errors.append("material[%d] baseColorFactor must be finite linear RGBA" % index)
        for field in ("roughnessFactor", "metallicFactor"):
            value = pbr.get(field)
            if not isinstance(value, (int, float)) or not 0 <= value <= 1:
                errors.append("material[%d].%s must be in [0,1]" % (index, field))
        if material.get("extras", {}).get("base_color_factor_space") != "linear_srgb_d65":
            errors.append("material[%d] must label its factor color space" % index)

    meshes = document.get("meshes")
    if not isinstance(meshes, list) or not meshes:
        errors.append("meshes must be non-empty")
        meshes = []
    unique_triangles = 0
    mesh_triangles = {}
    for mesh_index, mesh in enumerate(meshes):
        primitives = mesh.get("primitives", [])
        if len(primitives) != 1:
            errors.append("mesh[%d] must contain exactly one primitive" % mesh_index)
            continue
        primitive = primitives[0]
        if primitive.get("mode", 4) != 4:
            errors.append("mesh[%d] must use TRIANGLES mode" % mesh_index)
        material = primitive.get("material")
        if not isinstance(material, int) or not 0 <= material < len(materials):
            errors.append("mesh[%d] has an invalid material reference" % mesh_index)
        attributes = primitive.get("attributes", {})
        positions = _accessor_values(document, payload, attributes.get("POSITION"), errors)
        normals = _accessor_values(document, payload, attributes.get("NORMAL"), errors)
        indices = _accessor_values(document, payload, primitive.get("indices"), errors)
        if len(positions) != len(normals):
            errors.append("mesh[%d] position/normal counts differ" % mesh_index)
        if len(indices) % 3:
            errors.append("mesh[%d] index count is not divisible by three" % mesh_index)
        if indices and max(indices) >= len(positions):
            errors.append("mesh[%d] index exceeds vertex count" % mesh_index)
        position_accessor = document.get("accessors", [])[attributes.get("POSITION", 0)]
        if positions:
            expected_min = [min(value[axis] for value in positions) for axis in range(3)]
            expected_max = [max(value[axis] for value in positions) for axis in range(3)]
            if not _close_vector(position_accessor.get("min"), expected_min, 1e-6) or \
                    not _close_vector(position_accessor.get("max"), expected_max, 1e-6):
                errors.append("mesh[%d] POSITION bounds do not match buffer data" % mesh_index)
        triangles = len(indices) // 3
        if mesh.get("extras", {}).get("triangle_count") != triangles:
            errors.append("mesh[%d] triangle_count metadata is wrong" % mesh_index)
        mesh_triangles[mesh_index] = triangles
        unique_triangles += triangles

    nodes = document.get("nodes")
    if not isinstance(nodes, list):
        errors.append("nodes must be an array")
        nodes = []
    names = [node.get("name") for node in nodes]
    if len(names) != len(set(names)):
        errors.append("node names must be unique")
    missing_visible = REQUIRED_VISIBLE_NODES - set(names)
    if missing_visible:
        errors.append("missing visible nodes: " + ", ".join(sorted(missing_visible)))
    for forbidden_token in ("_throat", "_capture", "_jaw_"):
        forbidden_names = [name for name in names if isinstance(name, str) and
                           name.startswith("SOCKET_") and forbidden_token in name]
        if forbidden_names:
            errors.append("unpromoted pocket markers are forbidden: " + ", ".join(forbidden_names))
    for index, node in enumerate(nodes):
        if "scale" in node and node["scale"] != [1.0, 1.0, 1.0]:
            errors.append("node[%d] has non-unit scale" % index)
        if "translation" in node and not _finite_vector(node["translation"], 3):
            errors.append("node[%d] translation is invalid" % index)
        if "rotation" in node:
            rotation = node["rotation"]
            if not _finite_vector(rotation, 4) or \
                    abs(math.sqrt(sum(value * value for value in rotation)) - 1.0) > 1e-6:
                errors.append("node[%d] rotation must be a unit quaternion" % index)
        mesh_index = node.get("mesh")
        if mesh_index is not None and (not isinstance(mesh_index, int) or
                                       not 0 <= mesh_index < len(meshes)):
            errors.append("node[%d] has an invalid mesh reference" % index)

    by_name = {node.get("name"): node for node in nodes}
    expected_markers = {
        "SOCKET_play_surface_origin": [0.0, PLAY_SURFACE_Y_M, 0.0],
        "SOCKET_rail_nose_x_neg": [-HALF_WIDTH_M, NOSE_Y_M, 0.0],
        "SOCKET_rail_nose_x_pos": [HALF_WIDTH_M, NOSE_Y_M, 0.0],
        "SOCKET_rail_nose_z_neg": [0.0, NOSE_Y_M, -HALF_LENGTH_M],
        "SOCKET_rail_nose_z_pos": [0.0, NOSE_Y_M, HALF_LENGTH_M],
    }
    for pocket_id, center, _, tangent, width in POCKETS:
        expected_markers["SOCKET_pocket_%d_mouth" % pocket_id] = [
            center[0], PLAY_SURFACE_Y_M, center[1]]
        for suffix, sign in (("neg_t", -1.0), ("pos_t", 1.0)):
            expected_markers["SOCKET_pocket_%d_mouth_%s" % (pocket_id, suffix)] = [
                center[0] + tangent[0] * sign * width * 0.5,
                PLAY_SURFACE_Y_M,
                center[1] + tangent[1] * sign * width * 0.5]
    for name, translation in expected_markers.items():
        node = by_name.get(name)
        if node is None:
            errors.append("missing alignment marker " + name)
        elif not _close_vector(node.get("translation"), translation, 5e-7):
            errors.append(name + " translation differs from the alignment contract")
    actual_socket_names = {name for name in names if isinstance(name, str) and name.startswith("SOCKET_")}
    if actual_socket_names != set(expected_markers):
        errors.append("alignment marker set contains missing or unexpected names")

    ball_meshes = [mesh for mesh in meshes if mesh.get("name") == "MESH_ball_proxy_shared"]
    if len(ball_meshes) != 1:
        errors.append("exactly one shared ball mesh is required")
    ball_instances = [node for node in nodes if node.get("name") == "NODE_ball_proxy_shared_instance"]
    if len(ball_instances) != 1:
        errors.append("the asset must contain one shared-ball preview instance")
    pocket_instances = [node for node in nodes if str(node.get("name", "")).startswith("NODE_pocket_proxy_")]
    if len(pocket_instances) != 6:
        errors.append("exactly six visible pocket proxy instances are required")
    rail_instances = [node for node in nodes if str(node.get("name", "")).startswith("NODE_rail_")]
    if len(rail_instances) != 6:
        errors.append("exactly six visible rail segments are required")

    expected_binding = {
        "runtime_profile_id": "chinese_pool_interactive_120hz_v5",
        "formula_version": "legacy_v1",
        "geometry_id": "legacy_opening_band",
        "length_unit": "centimetre",
        "surface_y_cm": 86.483976,
        "ball_radius_cm": 2.8575,
        "ball_diameter_cm": 5.715,
        "playfield_width_cm": 127.0,
        "playfield_length_cm": 254.0,
        "cushion_nose_height_ratio": 1.4,
        "cushion_nose_height_above_surface_cm": 4.0005,
        "corner_mouth_width_cm": 13.2,
        "side_mouth_width_cm": 8.6,
        "pocket_marker_scope": "mouth_only",
        "marker_position_tolerance_cm": 0.05,
        "no_visual_solid_clearance_cm": 0.10,
    }
    if not isinstance(alignment, dict):
        errors.append("alignment manifest must be an object")
        alignment = {}
    if alignment.get("schema_version") != 1 or \
            alignment.get("alignment_id") != "focused_practice_hall_m1_alignment_v1" or \
            alignment.get("asset_id") != "focused_practice_hall_m1_graybox":
        errors.append("alignment manifest identity is invalid")
    if alignment.get("decision_status") != "approved_for_m1_m2_compatibility":
        errors.append("alignment decision must be approved for M1/M2 compatibility")
    binding = alignment.get("binding", {})
    for key, expected in expected_binding.items():
        if binding.get(key) != expected:
            errors.append("alignment binding %s must be %r" % (key, expected))
    capture = alignment.get("capture_presentation", {})
    if capture.get("m1_mode") != "simulation_driven" or \
            capture.get("presentation_detach_enabled") is not False or \
            capture.get("future_detach_requires_snapshot_fields") != ["pocket_id", "capture_sequence"]:
        errors.append("M1 capture presentation must remain simulation-driven")
    required_prohibitions = {
        "production_throat_marker", "production_jaw_marker",
        "production_capture_marker", "renderer_physics_offset",
        "capture_detach_without_snapshot_event",
    }
    if set(alignment.get("prohibitions", [])) != required_prohibitions:
        errors.append("alignment prohibitions are incomplete")
    canonical_bytes = (json.dumps(document, indent=2, sort_keys=True) + "\n").encode("utf-8")
    expected_hash = "sha256:" + hashlib.sha256(canonical_bytes).hexdigest()
    if alignment.get("asset_sha256") != expected_hash:
        errors.append("alignment asset_sha256 does not bind the glTF document")

    rendered_triangles = sum(mesh_triangles.get(node.get("mesh"), 0) for node in nodes)
    return errors, {
        "unique_triangles": unique_triangles,
        "rendered_triangles": rendered_triangles,
        "materials": len(materials),
        "meshes": len(meshes),
        "nodes": len(nodes),
        "markers": len(expected_markers),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("gltf", type=Path)
    parser.add_argument("--alignment", type=Path)
    arguments = parser.parse_args()
    alignment_path = arguments.alignment or arguments.gltf.with_name(
        "focused_practice_hall_m1_alignment_v1.json")
    try:
        document = json.loads(arguments.gltf.read_text(encoding="utf-8"))
        alignment = json.loads(alignment_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        print("graybox validation error: " + str(error), file=sys.stderr)
        return 2
    errors, report = validate(document, alignment)
    if errors:
        for error in errors:
            print("graybox validation error: " + error, file=sys.stderr)
        return 1
    print("graybox glTF valid: unique_triangles={unique_triangles} "
          "rendered_triangles={rendered_triangles} materials={materials} "
          "meshes={meshes} nodes={nodes} markers={markers}".format(**report))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
