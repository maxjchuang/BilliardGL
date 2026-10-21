#!/usr/bin/env python3
"""Generate the original, deterministic Focused Practice Hall M1 graybox."""

import argparse
import base64
import hashlib
import json
import math
from pathlib import Path
import struct


PLAY_SURFACE_Y_M = 0.86483976
BALL_RADIUS_M = 0.028575
NOSE_HEIGHT_M = BALL_RADIUS_M * 1.4
HALF_WIDTH_M = 0.635
HALF_LENGTH_M = 1.27
CORNER_MOUTH_WIDTH_M = 0.132
SIDE_MOUTH_WIDTH_M = 0.086


POCKETS = (
    (0, "corner_nx_nz", "corner", (-HALF_WIDTH_M, -HALF_LENGTH_M),
     (math.sqrt(0.5), math.sqrt(0.5)), (-math.sqrt(0.5), math.sqrt(0.5)),
     CORNER_MOUTH_WIDTH_M),
    (1, "corner_px_nz", "corner", (HALF_WIDTH_M, -HALF_LENGTH_M),
     (-math.sqrt(0.5), math.sqrt(0.5)), (-math.sqrt(0.5), -math.sqrt(0.5)),
     CORNER_MOUTH_WIDTH_M),
    (2, "corner_nx_pz", "corner", (-HALF_WIDTH_M, HALF_LENGTH_M),
     (math.sqrt(0.5), -math.sqrt(0.5)), (math.sqrt(0.5), math.sqrt(0.5)),
     CORNER_MOUTH_WIDTH_M),
    (3, "corner_px_pz", "corner", (HALF_WIDTH_M, HALF_LENGTH_M),
     (-math.sqrt(0.5), -math.sqrt(0.5)), (math.sqrt(0.5), -math.sqrt(0.5)),
     CORNER_MOUTH_WIDTH_M),
    (4, "side_nx", "side", (-HALF_WIDTH_M, 0.0),
     (1.0, 0.0), (0.0, -1.0), SIDE_MOUTH_WIDTH_M),
    (5, "side_px", "side", (HALF_WIDTH_M, 0.0),
     (-1.0, 0.0), (0.0, 1.0), SIDE_MOUTH_WIDTH_M),
)


MATERIALS = (
    ("MAT_graybox_table_felt", [0.0044, 0.0835, 0.1128, 1.0], 0.82, 0.0),
    ("MAT_graybox_table_body", [0.035, 0.035, 0.04, 1.0], 0.72, 0.0),
    ("MAT_graybox_rail", [0.095, 0.105, 0.115, 1.0], 0.58, 0.0),
    ("MAT_graybox_pocket", [0.003, 0.0025, 0.002, 1.0], 0.68, 0.0),
    ("MAT_graybox_cue", [0.38, 0.19, 0.055, 1.0], 0.42, 0.0),
    ("MAT_graybox_lamp", [0.02, 0.024, 0.03, 1.0], 0.48, 0.8),
    ("MAT_graybox_lamp_diffuser", [0.78, 0.72, 0.62, 1.0], 0.5, 0.0),
    ("MAT_graybox_background", [0.006, 0.007, 0.008, 1.0], 0.85, 0.0),
    ("MAT_graybox_ball", [0.82, 0.78, 0.66, 1.0], 0.12, 0.0),
)


class GltfBuilder:
    def __init__(self):
        self.binary = bytearray()
        self.buffer_views = []
        self.accessors = []
        self.meshes = []
        self.nodes = []

    def _append_view(self, payload, target):
        while len(self.binary) % 4:
            self.binary.append(0)
        offset = len(self.binary)
        self.binary.extend(payload)
        view = {"buffer": 0, "byteLength": len(payload), "byteOffset": offset,
                "target": target}
        self.buffer_views.append(view)
        return len(self.buffer_views) - 1

    def _float_vec3_accessor(self, values):
        payload = struct.pack("<%df" % (len(values) * 3),
                              *(component for value in values for component in value))
        view = self._append_view(payload, 34962)
        minimum = [min(value[axis] for value in values) for axis in range(3)]
        maximum = [max(value[axis] for value in values) for axis in range(3)]
        self.accessors.append({
            "bufferView": view, "byteOffset": 0, "componentType": 5126,
            "count": len(values), "type": "VEC3", "min": minimum, "max": maximum})
        return len(self.accessors) - 1

    def _index_accessor(self, values):
        component_type = 5123 if max(values, default=0) < 65536 else 5125
        code = "H" if component_type == 5123 else "I"
        payload = struct.pack("<%d%s" % (len(values), code), *values)
        view = self._append_view(payload, 34963)
        self.accessors.append({
            "bufferView": view, "byteOffset": 0, "componentType": component_type,
            "count": len(values), "type": "SCALAR",
            "min": [min(values)], "max": [max(values)]})
        return len(self.accessors) - 1

    def add_mesh(self, name, positions, normals, indices, material, extras=None):
        position_accessor = self._float_vec3_accessor(positions)
        normal_accessor = self._float_vec3_accessor(normals)
        index_accessor = self._index_accessor(indices)
        mesh = {
            "name": name,
            "primitives": [{
                "attributes": {"POSITION": position_accessor, "NORMAL": normal_accessor},
                "indices": index_accessor, "material": material, "mode": 4,
            }],
            "extras": {"triangle_count": len(indices) // 3, "graybox": True},
        }
        if extras:
            mesh["extras"].update(extras)
        self.meshes.append(mesh)
        return len(self.meshes) - 1

    def add_node(self, name, mesh=None, translation=None, rotation=None,
                 children=None, extras=None):
        node = {"name": name}
        if mesh is not None:
            node["mesh"] = mesh
        if translation is not None:
            node["translation"] = list(translation)
        if rotation is not None:
            node["rotation"] = list(rotation)
        if children:
            node["children"] = list(children)
        if extras:
            node["extras"] = extras
        self.nodes.append(node)
        return len(self.nodes) - 1


def box_geometry(center, size):
    cx, cy, cz = center
    sx, sy, sz = (component * 0.5 for component in size)
    faces = (
        ((1, 0, 0), ((sx, -sy, -sz), (sx, sy, -sz), (sx, sy, sz), (sx, -sy, sz))),
        ((-1, 0, 0), ((-sx, -sy, sz), (-sx, sy, sz), (-sx, sy, -sz), (-sx, -sy, -sz))),
        ((0, 1, 0), ((-sx, sy, -sz), (-sx, sy, sz), (sx, sy, sz), (sx, sy, -sz))),
        ((0, -1, 0), ((-sx, -sy, sz), (-sx, -sy, -sz), (sx, -sy, -sz), (sx, -sy, sz))),
        ((0, 0, 1), ((-sx, -sy, sz), (sx, -sy, sz), (sx, sy, sz), (-sx, sy, sz))),
        ((0, 0, -1), ((sx, -sy, -sz), (-sx, -sy, -sz), (-sx, sy, -sz), (sx, sy, -sz))),
    )
    positions = []
    normals = []
    indices = []
    for normal, corners in faces:
        start = len(positions)
        positions.extend((cx + x, cy + y, cz + z) for x, y, z in corners)
        normals.extend([normal] * 4)
        indices.extend((start, start + 1, start + 2, start, start + 2, start + 3))
    return positions, normals, indices


def felt_geometry(columns=32, rows=64):
    positions = []
    normals = []
    for row in range(rows + 1):
        z = -HALF_LENGTH_M + 2.0 * HALF_LENGTH_M * row / rows
        for column in range(columns + 1):
            x = -HALF_WIDTH_M + 2.0 * HALF_WIDTH_M * column / columns
            positions.append((x, PLAY_SURFACE_Y_M, z))
            normals.append((0.0, 1.0, 0.0))
    indices = []

    def cutout(x, z):
        for _, _, kind, center, _, _, width in POCKETS:
            radius = width * 0.52
            if kind == "side":
                radius *= 1.15
            if math.hypot(x - center[0], z - center[1]) <= radius:
                return True
        return False

    for row in range(rows):
        for column in range(columns):
            x0 = -HALF_WIDTH_M + 2.0 * HALF_WIDTH_M * column / columns
            x1 = -HALF_WIDTH_M + 2.0 * HALF_WIDTH_M * (column + 1) / columns
            z0 = -HALF_LENGTH_M + 2.0 * HALF_LENGTH_M * row / rows
            z1 = -HALF_LENGTH_M + 2.0 * HALF_LENGTH_M * (row + 1) / rows
            if cutout((x0 + x1) * 0.5, (z0 + z1) * 0.5):
                continue
            a = row * (columns + 1) + column
            b = a + 1
            c = a + columns + 2
            d = a + columns + 1
            indices.extend((a, d, c, a, c, b))
    return positions, normals, indices


def vertical_funnel_geometry(top_radius, lower_radius, depth, segments=12):
    positions = []
    normals = []
    indices = []
    slope = (top_radius - lower_radius) / depth
    normal_y = slope / math.sqrt(1.0 + slope * slope)
    normal_r = 1.0 / math.sqrt(1.0 + slope * slope)
    for ring, (radius, y) in enumerate(((top_radius, 0.0), (lower_radius, -depth))):
        for segment in range(segments):
            angle = 2.0 * math.pi * segment / segments
            positions.append((radius * math.cos(angle), y, radius * math.sin(angle)))
            normals.append((-normal_r * math.cos(angle), normal_y,
                            -normal_r * math.sin(angle)))
    for segment in range(segments):
        next_segment = (segment + 1) % segments
        indices.extend((segment, next_segment, segments + next_segment,
                        segment, segments + next_segment, segments + segment))
    bottom_center = len(positions)
    positions.append((0.0, -depth, 0.0))
    normals.append((0.0, 1.0, 0.0))
    for segment in range(segments):
        next_segment = (segment + 1) % segments
        indices.extend((bottom_center, segments + segment, segments + next_segment))
    return positions, normals, indices


def z_frustum_geometry(radius0, radius1, z0, z1, segments=12):
    positions = []
    normals = []
    indices = []
    for z, radius in ((z0, radius0), (z1, radius1)):
        for segment in range(segments):
            angle = 2.0 * math.pi * segment / segments
            positions.append((radius * math.cos(angle), radius * math.sin(angle), z))
            normals.append((math.cos(angle), math.sin(angle), 0.0))
    for segment in range(segments):
        nxt = (segment + 1) % segments
        indices.extend((segment, nxt, segments + nxt,
                        segment, segments + nxt, segments + segment))
    for z, ring, normal in ((z0, 0, (0.0, 0.0, -1.0)),
                            (z1, segments, (0.0, 0.0, 1.0))):
        center = len(positions)
        positions.append((0.0, 0.0, z))
        normals.append(normal)
        for segment in range(segments):
            nxt = (segment + 1) % segments
            if ring == 0:
                indices.extend((center, nxt, segment))
            else:
                indices.extend((center, ring + segment, ring + nxt))
    return positions, normals, indices


def sphere_geometry(radius, columns=16, rows=12):
    positions = []
    normals = []
    for row in range(rows + 1):
        latitude = math.pi * row / rows
        y = math.cos(latitude)
        ring = math.sin(latitude)
        for column in range(columns + 1):
            longitude = 2.0 * math.pi * column / columns
            normal = (ring * math.cos(longitude), y, ring * math.sin(longitude))
            normals.append(normal)
            positions.append(tuple(component * radius for component in normal))
    indices = []
    for row in range(rows):
        for column in range(columns):
            a = row * (columns + 1) + column
            b = a + 1
            c = a + columns + 2
            d = a + columns + 1
            indices.extend((a, d, c, a, c, b))
    return positions, normals, indices


def yaw_quaternion_for_local_z(direction_xz):
    angle = math.atan2(direction_xz[0], direction_xz[1])
    return (0.0, math.sin(angle * 0.5), 0.0, math.cos(angle * 0.5))


def build_document():
    builder = GltfBuilder()
    material_index = {item[0]: index for index, item in enumerate(MATERIALS)}
    top_nodes = []

    def mesh_node(name, geometry, material, extras=None):
        mesh = builder.add_mesh("MESH_" + name, *geometry,
                                material_index[material], extras)
        node = builder.add_node("NODE_" + name, mesh=mesh,
                                extras={"graybox": True, **(extras or {})})
        top_nodes.append(node)
        return mesh, node

    mesh_node("table_body_proxy",
              box_geometry((0.0, 0.39, 0.0), (1.55, 0.78, 2.82)),
              "MAT_graybox_table_body", {"asset_role": "table_body_proxy"})
    mesh_node("table_felt_proxy", felt_geometry(), "MAT_graybox_table_felt",
              {"asset_role": "play_surface_proxy"})

    rail_thickness = 0.08
    rail_height = 0.09
    rail_y = PLAY_SURFACE_Y_M + 0.035
    long_length = HALF_LENGTH_M - 0.105 - 0.055
    long_center = 0.055 + long_length * 0.5
    rails = (
        ("rail_x_neg_z_neg", (-HALF_WIDTH_M - rail_thickness * 0.5, rail_y, -long_center),
         (rail_thickness, rail_height, long_length)),
        ("rail_x_neg_z_pos", (-HALF_WIDTH_M - rail_thickness * 0.5, rail_y, long_center),
         (rail_thickness, rail_height, long_length)),
        ("rail_x_pos_z_neg", (HALF_WIDTH_M + rail_thickness * 0.5, rail_y, -long_center),
         (rail_thickness, rail_height, long_length)),
        ("rail_x_pos_z_pos", (HALF_WIDTH_M + rail_thickness * 0.5, rail_y, long_center),
         (rail_thickness, rail_height, long_length)),
        ("rail_z_neg", (0.0, rail_y, -HALF_LENGTH_M - rail_thickness * 0.5),
         (1.06, rail_height, rail_thickness)),
        ("rail_z_pos", (0.0, rail_y, HALF_LENGTH_M + rail_thickness * 0.5),
         (1.06, rail_height, rail_thickness)),
    )
    for name, center, size in rails:
        mesh_node(name, box_geometry(center, size), "MAT_graybox_rail",
                  {"asset_role": "cushion_segment_proxy", "visual_collision": False})

    corner_funnel = builder.add_mesh(
        "MESH_pocket_proxy_corner",
        *vertical_funnel_geometry(CORNER_MOUTH_WIDTH_M * 0.5,
                                  CORNER_MOUTH_WIDTH_M * 0.3, 0.07),
        material_index["MAT_graybox_pocket"],
        {"asset_role": "visual_pocket_proxy", "visual_only_non_solid": True})
    side_funnel = builder.add_mesh(
        "MESH_pocket_proxy_side",
        *vertical_funnel_geometry(SIDE_MOUTH_WIDTH_M * 0.5,
                                  SIDE_MOUTH_WIDTH_M * 0.3, 0.07),
        material_index["MAT_graybox_pocket"],
        {"asset_role": "visual_pocket_proxy", "visual_only_non_solid": True})
    for pocket_id, name, kind, center, inward, _, width in POCKETS:
        outward = (-inward[0], -inward[1])
        offset = width * 0.2
        node = builder.add_node(
            "NODE_pocket_proxy_%d_%s" % (pocket_id, name),
            mesh=corner_funnel if kind == "corner" else side_funnel,
            translation=(center[0] + outward[0] * offset,
                         PLAY_SURFACE_Y_M - 0.002,
                         center[1] + outward[1] * offset),
            extras={"graybox": True, "visual_only_non_solid": True,
                    "pocket_id": pocket_id, "profile_binding": "mouth_only"})
        top_nodes.append(node)

    cue_shaft = builder.add_mesh(
        "MESH_cue_proxy_shaft", *z_frustum_geometry(0.006, 0.015, 0.012, 1.45),
        material_index["MAT_graybox_cue"], {"asset_role": "cue_proxy"})
    cue_tip = builder.add_mesh(
        "MESH_cue_proxy_tip", *z_frustum_geometry(0.0062, 0.006, 0.0, 0.012),
        material_index["MAT_graybox_cue"], {"asset_role": "cue_tip_proxy"})
    cue_shaft_node = builder.add_node("NODE_cue_proxy_shaft", mesh=cue_shaft,
                                     extras={"graybox": True})
    cue_tip_node = builder.add_node("NODE_cue_proxy_tip", mesh=cue_tip,
                                   extras={"graybox": True})
    cue_root = builder.add_node(
        "NODE_cue_proxy_root", translation=(0.0, PLAY_SURFACE_Y_M + BALL_RADIUS_M, 0.233575),
        children=(cue_tip_node, cue_shaft_node),
        extras={"graybox": True, "origin": "tip_contact_face_center", "forward_axis": "+Z"})
    top_nodes.append(cue_root)

    sphere = builder.add_mesh("MESH_ball_proxy_shared", *sphere_geometry(BALL_RADIUS_M),
                              material_index["MAT_graybox_ball"],
                              {"asset_role": "shared_ball_proxy", "radius_m": BALL_RADIUS_M})
    ball_node = builder.add_node(
        "NODE_ball_proxy_shared_instance", mesh=sphere,
        translation=(0.0, PLAY_SURFACE_Y_M + BALL_RADIUS_M, 0.2),
        extras={"graybox": True, "shared_mesh_preview_instance": True})
    top_nodes.append(ball_node)

    lamp_y = PLAY_SURFACE_Y_M + 1.35
    mesh_node("rect_lamp_housing_proxy",
              box_geometry((0.0, lamp_y + 0.035, 0.0), (0.65, 0.07, 1.85)),
              "MAT_graybox_lamp", {"asset_role": "rect_lamp_proxy"})
    mesh_node("rect_lamp_diffuser_proxy",
              box_geometry((0.0, lamp_y - 0.003, 0.0), (0.61, 0.006, 1.81)),
              "MAT_graybox_lamp_diffuser",
              {"asset_role": "emissive_diffuser_proxy", "emissive_visual_only": True})

    background_boxes = (
        ("room_floor_proxy", (0.0, -0.025, 0.0), (10.0, 0.05, 10.0)),
        ("room_wall_x_neg_proxy", (-5.0, 2.0, 0.0), (0.05, 4.0, 10.0)),
        ("room_wall_x_pos_proxy", (5.0, 2.0, 0.0), (0.05, 4.0, 10.0)),
        ("room_wall_z_neg_proxy", (0.0, 2.0, -5.0), (10.0, 4.0, 0.05)),
        ("room_wall_z_pos_proxy", (0.0, 2.0, 5.0), (10.0, 4.0, 0.05)),
    )
    for name, center, size in background_boxes:
        mesh_node(name, box_geometry(center, size), "MAT_graybox_background",
                  {"asset_role": "room_shell_proxy"})

    marker_nodes = []

    def marker(name, translation, rotation=None, extras=None):
        node = builder.add_node(
            name, translation=translation, rotation=rotation,
            extras={"alignment_marker": True, **(extras or {})})
        marker_nodes.append(node)
        top_nodes.append(node)

    marker("SOCKET_play_surface_origin", (0.0, PLAY_SURFACE_Y_M, 0.0),
           extras={"marker_kind": "play_surface"})
    rail_markers = (
        ("SOCKET_rail_nose_x_neg", (-HALF_WIDTH_M, PLAY_SURFACE_Y_M + NOSE_HEIGHT_M, 0.0), (1.0, 0.0)),
        ("SOCKET_rail_nose_x_pos", (HALF_WIDTH_M, PLAY_SURFACE_Y_M + NOSE_HEIGHT_M, 0.0), (-1.0, 0.0)),
        ("SOCKET_rail_nose_z_neg", (0.0, PLAY_SURFACE_Y_M + NOSE_HEIGHT_M, -HALF_LENGTH_M), (0.0, 1.0)),
        ("SOCKET_rail_nose_z_pos", (0.0, PLAY_SURFACE_Y_M + NOSE_HEIGHT_M, HALF_LENGTH_M), (0.0, -1.0)),
    )
    for name, translation, inward in rail_markers:
        marker(name, translation, yaw_quaternion_for_local_z(inward),
               {"marker_kind": "rail_nose", "local_positive_z": "inward"})

    for pocket_id, name, _, center, inward, tangent, width in POCKETS:
        outward = (-inward[0], -inward[1])
        rotation = yaw_quaternion_for_local_z(outward)
        marker("SOCKET_pocket_%d_mouth" % pocket_id,
               (center[0], PLAY_SURFACE_Y_M, center[1]), rotation,
               {"marker_kind": "pocket_mouth", "pocket_id": pocket_id,
                "pocket_name": name, "mouth_width_m": width,
                "local_positive_x": "tangent", "local_positive_z": "outward_depth"})
        for suffix, sign in (("neg_t", -1.0), ("pos_t", 1.0)):
            endpoint = (center[0] + tangent[0] * sign * width * 0.5,
                        PLAY_SURFACE_Y_M,
                        center[1] + tangent[1] * sign * width * 0.5)
            marker("SOCKET_pocket_%d_mouth_%s" % (pocket_id, suffix), endpoint,
                   extras={"marker_kind": "pocket_mouth_endpoint",
                           "pocket_id": pocket_id, "tangent_sign": int(sign)})

    gltf_materials = []
    for name, color, roughness, metallic in MATERIALS:
        material = {
            "name": name,
            "pbrMetallicRoughness": {
                "baseColorFactor": color, "roughnessFactor": roughness,
                "metallicFactor": metallic,
            },
            "doubleSided": False,
            "extras": {"graybox": True, "base_color_factor_space": "linear_srgb_d65"},
        }
        if name == "MAT_graybox_lamp_diffuser":
            material["emissiveFactor"] = [1.0, 0.79, 0.59]
            material["extras"]["emissive_strength_target"] = 8.0
        gltf_materials.append(material)

    data_uri = "data:application/octet-stream;base64," + base64.b64encode(builder.binary).decode("ascii")
    return {
        "asset": {
            "version": "2.0",
            "generator": "BilliardGL generate_m1_graybox_gltf.py v1",
            "extras": {
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
            },
        },
        "scene": 0,
        "scenes": [{"name": "SCENE_focused_practice_hall_m1_graybox", "nodes": top_nodes}],
        "nodes": builder.nodes,
        "meshes": builder.meshes,
        "materials": gltf_materials,
        "accessors": builder.accessors,
        "bufferViews": builder.buffer_views,
        "buffers": [{"byteLength": len(builder.binary), "uri": data_uri}],
    }


def build_alignment_manifest(gltf_bytes):
    return {
        "schema_version": 1,
        "alignment_id": "focused_practice_hall_m1_alignment_v1",
        "asset_id": "focused_practice_hall_m1_graybox",
        "asset_path": "assets/modern/graybox/focused_practice_hall_m1_graybox.gltf",
        "asset_sha256": "sha256:" + hashlib.sha256(gltf_bytes).hexdigest(),
        "decision_status": "approved_for_m1_m2_compatibility",
        "binding": {
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
            "no_visual_solid_clearance_cm": 0.10
        },
        "capture_presentation": {
            "m1_mode": "simulation_driven",
            "presentation_detach_enabled": False,
            "future_detach_requires_snapshot_fields": ["pocket_id", "capture_sequence"]
        },
        "prohibitions": [
            "production_throat_marker",
            "production_jaw_marker",
            "production_capture_marker",
            "renderer_physics_offset",
            "capture_detach_without_snapshot_event"
        ],
        "approval_source": "project_manager_decision_2026_07_27"
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--alignment-output", type=Path)
    arguments = parser.parse_args()
    document = build_document()
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    gltf_bytes = (json.dumps(document, indent=2, sort_keys=True) + "\n").encode("utf-8")
    arguments.output.write_bytes(gltf_bytes)
    alignment_output = arguments.alignment_output or arguments.output.with_name(
        "focused_practice_hall_m1_alignment_v1.json")
    alignment_output.parent.mkdir(parents=True, exist_ok=True)
    alignment_output.write_text(
        json.dumps(build_alignment_manifest(gltf_bytes), indent=2, sort_keys=True) + "\n",
        encoding="utf-8")
    unique_triangles = sum(mesh["extras"]["triangle_count"] for mesh in document["meshes"])
    print("generated %s and %s (%d unique triangles, %d materials)" %
          (arguments.output, alignment_output, unique_triangles, len(document["materials"])))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
