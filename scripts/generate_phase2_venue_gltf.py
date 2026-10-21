#!/usr/bin/env python3
"""Generate the original Focused Practice Hall venue shell and camera preview."""

import argparse
import base64
import hashlib
import json
import math
from pathlib import Path

from generate_phase2_modular_gltf import Builder, box, beveled_box, cylinder_z
from generate_phase2_pbr_package import _png_rgba, texture_filename


ASSET_NAME = "focused_practice_hall_venue_v1.gltf"
MANIFEST_NAME = "focused_practice_hall_venue_v1.asset.json"
PREVIEW_NAME = "focused_practice_hall_venue_four_camera_v1.png"
ROOM_HALF_X = 4.0
ROOM_HALF_Z = 5.0
ROOM_HEIGHT = 4.0

SHARED_TEXTURES = tuple(
    (texture_set, channel) for texture_set in ("rubber", "gunmetal", "walnut")
    for channel in ("basecolor", "normal", "orm")
)

CAMERAS = {
    "player_shot": {"projection": "perspective", "eye_m": [0.30, 1.12483976, 1.62],
                    "target_m": [0.0, 0.91, 0.28], "vertical_fov_degrees": 50.0},
    "low_aim": {"projection": "perspective", "eye_m": [0.10, 0.98483976, 1.17],
                "target_m": [0.0, 0.91, -0.05], "vertical_fov_degrees": 42.0},
    "tactical_overhead": {"projection": "orthographic", "eye_m": [0.0, 3.92, 0.02],
                          "target_m": [0.0, 0.86483976, 0.0],
                          "orthographic_vertical_span_m": 3.15},
    "broadcast_oblique": {"projection": "perspective", "eye_m": [2.65, 3.11483976, 2.25],
                          "target_m": [0.0, 0.82, 0.0], "vertical_fov_degrees": 42.0},
}


def _sha256(payload):
    return "sha256:" + hashlib.sha256(payload).hexdigest()


def _texture_info(index, repeat):
    return {"index": index, "texCoord": 0,
            "extensions": {"KHR_texture_transform": {
                "offset": [0.0, 0.0], "scale": list(repeat)}}}


def build_materials():
    texture_index = {key: index for index, key in enumerate(SHARED_TEXTURES)}

    def material(name, color, roughness, metallic, relative_ev, texture_set=None,
                 use_base=True, clearcoat=None):
        result = {"name": name, "pbrMetallicRoughness": {
            "baseColorFactor": list(color), "roughnessFactor": roughness,
            "metallicFactor": metallic},
            "extras": {"background_relative_to_table_ev": relative_ev,
                       "base_color_factor_space": "linear_srgb_d65",
                       "shared_texture_reuse": texture_set}}
        if texture_set:
            repeat = {"rubber": (4.0, 4.0), "gunmetal": (3.0, 3.0),
                      "walnut": (3.0, 1.0)}[texture_set]
            if use_base:
                result["pbrMetallicRoughness"]["baseColorTexture"] = _texture_info(
                    texture_index[(texture_set, "basecolor")], repeat)
            result["normalTexture"] = _texture_info(texture_index[(texture_set, "normal")], repeat)
            result["normalTexture"]["scale"] = 0.55 if texture_set == "rubber" else 0.75
            result["pbrMetallicRoughness"]["metallicRoughnessTexture"] = _texture_info(
                texture_index[(texture_set, "orm")], repeat)
            result["occlusionTexture"] = _texture_info(texture_index[(texture_set, "orm")], repeat)
            result["extras"]["normal_convention"] = "opengl_positive_y"
            result["extras"]["orm_channels"] = "r_ao_g_roughness_b_metallic"
        if clearcoat:
            result["extensions"] = {"KHR_materials_clearcoat": {
                "clearcoatFactor": clearcoat[0], "clearcoatRoughnessFactor": clearcoat[1]}}
            result["extras"]["clearcoat_fallback"] = "energy_conserving_specular_lobe"
        return result

    return [
        material("MAT_venue_floor", [0.42, 0.46, 0.49, 1.0], 1.0, 1.0, -2.15, "rubber"),
        material("MAT_venue_wall", [0.018, 0.021, 0.024, 1.0], 0.92, 1.0, -2.55,
                 "rubber", use_base=False),
        material("MAT_venue_ceiling", [0.010, 0.012, 0.014, 1.0], 0.88, 0.0, -2.85),
        material("MAT_venue_gunmetal", [0.72, 0.74, 0.78, 1.0], 1.0, 1.0, -2.35,
                 "gunmetal"),
        material("MAT_venue_acoustic", [0.020, 0.024, 0.027, 1.0], 0.96, 1.0, -2.65,
                 "rubber", use_base=False),
        material("MAT_venue_walnut_trim", [0.62, 0.58, 0.54, 1.0], 1.0, 1.0, -2.40,
                 "walnut", clearcoat=(0.22, 0.24)),
    ]


def build_gltf():
    builder = Builder()
    materials = build_materials()
    material = {entry["name"]: index for index, entry in enumerate(materials)}
    lod_roots = {}
    rendered_triangles = {}
    draw_counts = {}
    scene_nodes = []

    def make_mesh(name, geometry, material_name, role, lod):
        return builder.mesh("MESH_" + name, geometry, material[material_name], role, lod)

    def instance(name, mesh, translation, role, lod, rotation=None, extra=None):
        return builder.node("NODE_" + name, mesh=mesh, translation=translation, rotation=rotation,
                            extras={"asset_role": role, "lod": lod, **(extra or {})})

    for lod in range(3):
        children = []
        floor_mesh = make_mesh("venue_floor_LOD%d" % lod,
                               box((0, 0, 0), (8.0, 0.08, 10.0)),
                               "MAT_venue_floor", "venue_floor", lod)
        wall_x_mesh = make_mesh("venue_wall_x_LOD%d" % lod,
                                box((0, 0, 0), (0.10, 4.0, 10.0)),
                                "MAT_venue_wall", "venue_wall", lod)
        wall_z_mesh = make_mesh("venue_wall_z_LOD%d" % lod,
                                box((0, 0, 0), (8.0, 4.0, 0.10)),
                                "MAT_venue_wall", "venue_wall", lod)
        ceiling_mesh = make_mesh("venue_ceiling_LOD%d" % lod,
                                 box((0, 0, 0), (8.0, 0.10, 10.0)),
                                 "MAT_venue_ceiling", "venue_ceiling", lod)
        children.append(instance("venue_floor_LOD%d" % lod, floor_mesh, (0, -0.04, 0),
                                 "venue_floor", lod))
        for side, x in (("neg", -4.05), ("pos", 4.05)):
            children.append(instance("venue_wall_x_%s_LOD%d" % (side, lod), wall_x_mesh,
                                     (x, 2.0, 0), "venue_wall", lod))
        for side, z in (("neg", -5.05), ("pos", 5.05)):
            children.append(instance("venue_wall_z_%s_LOD%d" % (side, lod), wall_z_mesh,
                                     (0, 2.0, z), "venue_wall", lod))
        children.append(instance("venue_ceiling_LOD%d" % lod, ceiling_mesh, (0, 4.05, 0),
                                 "venue_ceiling", lod,
                                 extra={"visibility_group": "hide_tactical_overhead"}))

        if lod < 2:
            panel_geometry = (beveled_box((0, 0, 0), (0.72, 1.15, 0.055), 0.025)
                              if lod == 0 else box((0, 0, 0), (0.72, 1.15, 0.055)))
            panel_mesh = make_mesh("venue_acoustic_panel_LOD%d" % lod, panel_geometry,
                                   "MAT_venue_acoustic", "venue_acoustic_panel", lod)
            for wall, fixed, positions, rotation in (
                    ("z_neg", -4.97, (-2.55, -0.85, 0.85, 2.55), None),
                    ("z_pos", 4.97, (-2.55, -0.85, 0.85, 2.55), None),
                    ("x_neg", -3.97, (-3.0, -1.0, 1.0, 3.0),
                     [0.0, math.sin(math.pi / 4), 0.0, math.cos(math.pi / 4)]),
                    ("x_pos", 3.97, (-3.0, -1.0, 1.0, 3.0),
                     [0.0, math.sin(math.pi / 4), 0.0, math.cos(math.pi / 4)])):
                for panel_index, position in enumerate(positions):
                    translation = ((position, 2.15, fixed) if wall.startswith("z")
                                   else (fixed, 2.15, position))
                    children.append(instance("acoustic_%s_%d_LOD%d" %
                                             (wall, panel_index, lod), panel_mesh,
                                             translation, "venue_acoustic_panel", lod,
                                             rotation=rotation))
        else:
            band_x = make_mesh("venue_acoustic_band_x_LOD2",
                               box((0, 0, 0), (5.8, 0.72, 0.045)),
                               "MAT_venue_acoustic", "venue_acoustic_band", lod)
            band_z = make_mesh("venue_acoustic_band_z_LOD2",
                               box((0, 0, 0), (0.045, 0.72, 6.8)),
                               "MAT_venue_acoustic", "venue_acoustic_band", lod)
            for side, z in (("neg", -4.97), ("pos", 4.97)):
                children.append(instance("acoustic_band_z_%s_LOD2" % side, band_x,
                                         (0, 2.15, z), "venue_acoustic_band", lod))
            for side, x in (("neg", -3.97), ("pos", 3.97)):
                children.append(instance("acoustic_band_x_%s_LOD2" % side, band_z,
                                         (x, 2.15, 0), "venue_acoustic_band", lod))

        if lod < 2:
            segments = 12 if lod == 0 else 6
            cable_mesh = make_mesh("venue_lamp_cable_LOD%d" % lod,
                                   cylinder_z(0.006, 0.006, 0.0, 1.48, segments),
                                   "MAT_venue_gunmetal", "venue_lamp_cable", lod)
            rotate_z_to_y = [-math.sin(math.pi / 4), 0.0, 0.0, math.cos(math.pi / 4)]
            for index, (x, z) in enumerate(((-0.27, -0.82), (0.27, -0.82),
                                            (-0.27, 0.82), (0.27, 0.82))):
                children.append(instance("lamp_cable_%d_LOD%d" % (index, lod), cable_mesh,
                                         (x, 2.26, z), "venue_lamp_cable", lod,
                                         rotation=rotate_z_to_y,
                                         extra={"visibility_group": "hide_tactical_overhead"}))
            rack_geometry = (beveled_box((0, 0, 0), (0.045, 0.045, 2.05), 0.008)
                             if lod == 0 else box((0, 0, 0), (0.045, 0.045, 2.05)))
            rack_z = make_mesh("venue_lamp_rack_z_LOD%d" % lod, rack_geometry,
                               "MAT_venue_gunmetal", "venue_lamp_rack", lod)
            rack_x = make_mesh("venue_lamp_rack_x_LOD%d" % lod,
                               (beveled_box((0, 0, 0), (0.78, 0.045, 0.045), 0.008)
                                if lod == 0 else box((0, 0, 0), (0.78, 0.045, 0.045))),
                               "MAT_venue_gunmetal", "venue_lamp_rack", lod)
            for index, x in enumerate((-0.32, 0.32)):
                children.append(instance("lamp_rack_z_%d_LOD%d" % (index, lod), rack_z,
                                         (x, 3.78, 0), "venue_lamp_rack", lod,
                                         extra={"visibility_group": "hide_tactical_overhead"}))
            for index, z in enumerate((-0.94, 0.94)):
                children.append(instance("lamp_rack_x_%d_LOD%d" % (index, lod), rack_x,
                                         (0, 3.78, z), "venue_lamp_rack", lod,
                                         extra={"visibility_group": "hide_tactical_overhead"}))

        trim_x = make_mesh("venue_trim_x_LOD%d" % lod,
                           box((0, 0, 0), (7.8, 0.12, 0.055)),
                           "MAT_venue_walnut_trim", "venue_wall_trim", lod)
        trim_z = make_mesh("venue_trim_z_LOD%d" % lod,
                           box((0, 0, 0), (0.055, 0.12, 9.8)),
                           "MAT_venue_walnut_trim", "venue_wall_trim", lod)
        for side, z in (("neg", -4.96), ("pos", 4.96)):
            children.append(instance("wall_trim_z_%s_LOD%d" % (side, lod), trim_x,
                                     (0, 0.78, z), "venue_wall_trim", lod))
        for side, x in (("neg", -3.96), ("pos", 3.96)):
            children.append(instance("wall_trim_x_%s_LOD%d" % (side, lod), trim_z,
                                     (x, 0.78, 0), "venue_wall_trim", lod))

        root = builder.node("NODE_venue_LOD%d" % lod, children=children,
                            extras={"lod_set": "venue", "lod": lod,
                                    "production_candidate": True,
                                    "room_dimensions_m": [8.0, 4.0, 10.0]})
        lod_roots[str(lod)] = root
        rendered_triangles[str(lod)] = sum(
            builder.meshes[builder.nodes[index]["mesh"]]["extras"]["triangle_count"]
            for index in children if "mesh" in builder.nodes[index])
        draw_counts[str(lod)] = sum(1 for index in children if "mesh" in builder.nodes[index])
        if lod == 0:
            scene_nodes.append(root)

    camera_nodes = {}
    for name, camera in CAMERAS.items():
        position_node = builder.node("SOCKET_camera_%s" % name,
                                     translation=camera["eye_m"],
                                     extras={"camera_composition_socket": True, "camera_id": name})
        target_node = builder.node("SOCKET_camera_%s_target" % name,
                                   translation=camera["target_m"],
                                   extras={"camera_target_socket": True, "camera_id": name})
        camera_nodes[name] = {"position": position_node, "target": target_node}
        scene_nodes.extend((position_node, target_node))

    images = []
    textures = []
    for index, (texture_set, channel) in enumerate(SHARED_TEXTURES):
        images.append({"name": "IMG_shared_%s_%s" % (texture_set, channel),
                       "uri": texture_filename(texture_set, channel), "mimeType": "image/png",
                       "extras": {"shared_from_package": "focused_practice_hall_pbr_v1",
                                  "original_procedural": True}})
        textures.append({"name": "T_shared_%s_%s" % (texture_set, channel),
                         "sampler": 0, "source": index})
    document = {
        "asset": {"version": "2.0", "generator": "BilliardGL venue generator v1",
                  "extras": {"asset_id": "focused_practice_hall_venue_v1",
                             "authorship": "original_procedural_billiardgl",
                             "external_source_assets": False,
                             "production_candidate": True, "final_high_mesh": False,
                             "unit": "metre", "up_axis": "+Y", "forward_axis": "+Z",
                             "handedness": "right", "remote_furniture_in_scope": False,
                             "brand_text_present": False}},
        "extensionsUsed": ["KHR_texture_transform", "KHR_materials_clearcoat"],
        "scene": 0,
        "scenes": [{"name": "SCENE_focused_practice_hall_venue_LOD0", "nodes": scene_nodes}],
        "nodes": builder.nodes, "meshes": builder.meshes, "materials": materials,
        "images": images, "textures": textures,
        "samplers": [{"magFilter": 9729, "minFilter": 9987,
                      "wrapS": 10497, "wrapT": 10497}],
        "accessors": builder.accessors, "bufferViews": builder.views,
        "buffers": [{"byteLength": len(builder.binary),
                     "uri": "data:application/octet-stream;base64," +
                            base64.b64encode(builder.binary).decode("ascii")}],
    }
    return document, lod_roots, rendered_triangles, draw_counts, camera_nodes


def _normalise(value):
    length = math.sqrt(sum(component * component for component in value))
    return tuple(component / length for component in value)


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0])


def _box_faces(center, size, color, yaw=0.0):
    hx, hy, hz = (value * 0.5 for value in size)
    local = [(-hx, -hy, -hz), (hx, -hy, -hz), (hx, hy, -hz), (-hx, hy, -hz),
             (-hx, -hy, hz), (hx, -hy, hz), (hx, hy, hz), (-hx, hy, hz)]
    cosine, sine = math.cos(yaw), math.sin(yaw)
    points = [(center[0] + x * cosine + z * sine, center[1] + y,
               center[2] - x * sine + z * cosine) for x, y, z in local]
    return [(tuple(points[index] for index in face), color) for face in
            ((0, 1, 2, 3), (5, 4, 7, 6), (4, 0, 3, 7),
             (1, 5, 6, 2), (3, 2, 6, 7), (4, 5, 1, 0))]


def _fill_polygon(pixels, width, height, points, color, clip=None):
    clip = clip or (0, 0, width, height)
    minimum_y = max(clip[1], int(math.floor(min(point[1] for point in points))))
    maximum_y = min(clip[3] - 1, int(math.ceil(max(point[1] for point in points))))
    for y in range(minimum_y, maximum_y + 1):
        scan_y = y + 0.5
        intersections = []
        for index, first in enumerate(points):
            second = points[(index + 1) % len(points)]
            if (first[1] <= scan_y < second[1]) or (second[1] <= scan_y < first[1]):
                t = (scan_y - first[1]) / (second[1] - first[1])
                intersections.append(first[0] + (second[0] - first[0]) * t)
        intersections.sort()
        for index in range(0, len(intersections) - 1, 2):
            x0 = max(clip[0], int(math.floor(intersections[index])))
            x1 = min(clip[2] - 1, int(math.ceil(intersections[index + 1])))
            for x in range(x0, x1 + 1):
                offset = (y * width + x) * 4
                pixels[offset:offset + 4] = bytes(color + (255,))


def _fill_triangle_depth(pixels, depth_buffer, width, height, points, color, clip):
    minimum_x = max(clip[0], int(math.floor(min(point[0] for point in points))))
    maximum_x = min(clip[2] - 1, int(math.ceil(max(point[0] for point in points))))
    minimum_y = max(clip[1], int(math.floor(min(point[1] for point in points))))
    maximum_y = min(clip[3] - 1, int(math.ceil(max(point[1] for point in points))))
    a, b, c = points
    denominator = ((b[1] - c[1]) * (a[0] - c[0]) +
                   (c[0] - b[0]) * (a[1] - c[1]))
    if abs(denominator) < 1e-8:
        return
    for y in range(minimum_y, maximum_y + 1):
        for x in range(minimum_x, maximum_x + 1):
            px, py = x + 0.5, y + 0.5
            wa = ((b[1] - c[1]) * (px - c[0]) +
                  (c[0] - b[0]) * (py - c[1])) / denominator
            wb = ((c[1] - a[1]) * (px - c[0]) +
                  (a[0] - c[0]) * (py - c[1])) / denominator
            wc = 1.0 - wa - wb
            if wa < -1e-6 or wb < -1e-6 or wc < -1e-6:
                continue
            depth = wa * a[2] + wb * b[2] + wc * c[2]
            index = y * width + x
            if depth < depth_buffer[index]:
                depth_buffer[index] = depth
                offset = index * 4
                pixels[offset:offset + 4] = bytes(color + (255,))


def build_preview():
    panel_width, panel_height = 768, 432
    width, height = panel_width * 2, panel_height * 2
    pixels = bytearray([12, 14, 16, 255] * (width * height))
    depth_buffer = [float("inf")] * (width * height)
    faces = []
    overhead_hidden_faces = []
    for center, size, color, yaw in (
            ((0, -0.04, 0), (8, 0.08, 10), (30, 34, 37), 0),
            ((-4.05, 2, 0), (0.1, 4, 10), (25, 28, 32), 0),
            ((4.05, 2, 0), (0.1, 4, 10), (25, 28, 32), 0),
            ((0, 2, -5.05), (8, 4, 0.1), (22, 25, 29), 0),
            ((0, 2, 5.05), (8, 4, 0.1), (22, 25, 29), 0),
            ((0, 4.05, 0), (8, 0.1, 10), (16, 18, 21), 0),
            ((0, 0.39, 0), (1.55, 0.78, 2.82), (73, 37, 23), 0),
            ((0, 0.87, 0), (1.27, 0.04, 2.54), (20, 118, 132), 0),
            ((-0.69, 0.93, 0), (0.10, 0.10, 2.72), (84, 43, 25), 0),
            ((0.69, 0.93, 0), (0.10, 0.10, 2.72), (84, 43, 25), 0),
            ((0, 0.93, -1.34), (1.38, 0.10, 0.10), (84, 43, 25), 0),
            ((0, 0.93, 1.34), (1.38, 0.10, 0.10), (84, 43, 25), 0),
            ((0.18, 0.92, 1.18), (0.025, 0.025, 1.25), (177, 111, 52), 0.12)):
        faces.extend(_box_faces(center, size, color, yaw))
    for x, z in ((-0.635, -1.27), (0.635, -1.27), (-0.635, 1.27),
                 (0.635, 1.27), (-0.635, 0.0), (0.635, 0.0)):
        faces.extend(_box_faces((x, 0.925, z), (0.13, 0.035, 0.13), (3, 5, 6)))
    ball_colors = ((235, 231, 213), (239, 184, 28), (38, 91, 184),
                   (201, 47, 39), (31, 34, 37), (121, 52, 139))
    for index, (x, z) in enumerate(((0.12, 0.72), (-0.14, 0.13), (0.18, -0.08),
                                    (-0.05, -0.42), (0.24, -0.58), (-0.25, -0.78))):
        faces.extend(_box_faces((x, 0.90, z), (0.058, 0.058, 0.058),
                                ball_colors[index]))
    overhead_hidden_faces.extend(_box_faces((0, 2.25, 0), (0.66, 0.08, 1.86),
                                            (62, 66, 72)))
    overhead_hidden_faces.extend(_box_faces((0, 2.20, 0), (0.60, 0.02, 1.80),
                                            (224, 194, 148)))
    for z in (-4.97, 4.97):
        for x in (-2.55, -0.85, 0.85, 2.55):
            faces.extend(_box_faces((x, 2.15, z), (0.72, 1.15, 0.055), (34, 39, 44)))
    for x in (-3.97, 3.97):
        for z in (-3.0, -1.0, 1.0, 3.0):
            faces.extend(_box_faces((x, 2.15, z), (0.055, 1.15, 0.72), (34, 39, 44)))
    for x, z in ((-0.27, -0.82), (0.27, -0.82), (-0.27, 0.82), (0.27, 0.82)):
        overhead_hidden_faces.extend(_box_faces((x, 3.0, z), (0.012, 1.48, 0.012),
                                                (67, 72, 78)))

    panel_order = ("player_shot", "low_aim", "tactical_overhead", "broadcast_oblique")
    header_colors = ((30, 111, 125), (61, 93, 111), (79, 73, 105), (106, 69, 48))
    for panel_index, camera_name in enumerate(panel_order):
        camera = CAMERAS[camera_name]
        origin_x = (panel_index % 2) * panel_width
        origin_y = (panel_index // 2) * panel_height
        eye = tuple(camera["eye_m"])
        forward = _normalise(tuple(camera["target_m"][i] - eye[i] for i in range(3)))
        reference_up = (0, 0, -1) if abs(forward[1]) > 0.94 else (0, 1, 0)
        right = _normalise(_cross(forward, reference_up))
        up = _normalise(_cross(right, forward))
        projected = []
        visible_faces = faces if camera_name == "tactical_overhead" else faces + overhead_hidden_faces
        for polygon, color in visible_faces:
            points = []
            depths = []
            for world in polygon:
                delta = tuple(world[i] - eye[i] for i in range(3))
                depth = sum(delta[i] * forward[i] for i in range(3))
                depths.append(depth)
                if camera["projection"] == "orthographic":
                    scale = panel_height / camera["orthographic_vertical_span_m"]
                    px = origin_x + panel_width * 0.5 + sum(delta[i] * right[i] for i in range(3)) * scale
                    py = origin_y + panel_height * 0.5 - sum(delta[i] * up[i] for i in range(3)) * scale
                else:
                    if depth <= 0.03:
                        break
                    tangent = math.tan(math.radians(camera["vertical_fov_degrees"]) * 0.5)
                    px = origin_x + panel_width * 0.5 + \
                        sum(delta[i] * right[i] for i in range(3)) / depth * panel_height / (2 * tangent)
                    py = origin_y + panel_height * 0.5 - \
                        sum(delta[i] * up[i] for i in range(3)) / depth * panel_height / (2 * tangent)
                points.append((px, py, depth))
            if len(points) == 4 and min(depths) > 0.03:
                projected.append((sum(depths) / 4.0, points, color))
        clip = (origin_x, origin_y, origin_x + panel_width, origin_y + panel_height)
        for _, points, color in projected:
            _fill_triangle_depth(pixels, depth_buffer, width, height,
                                 (points[0], points[1], points[2]), color, clip)
            _fill_triangle_depth(pixels, depth_buffer, width, height,
                                 (points[0], points[2], points[3]), color, clip)
        for y in range(origin_y + 8, origin_y + 18):
            for x in range(origin_x + 8, origin_x + panel_width - 8):
                offset = (y * width + x) * 4
                pixels[offset:offset + 4] = bytes(header_colors[panel_index] + (255,))
    return _png_rgba(width, height, pixels)


def build_manifest(gltf_bytes, preview_bytes, document, lod_roots, rendered_triangles,
                   draw_counts, camera_nodes, shared_texture_dir):
    texture_files = {}
    standalone_gpu_bytes = 0
    for texture_set, channel in SHARED_TEXTURES:
        path = shared_texture_dir / texture_filename(texture_set, channel)
        payload = path.read_bytes()
        texture_files.setdefault(texture_set, {})[channel] = {
            "path": "assets/modern/phase2/" + path.name, "sha256": _sha256(payload)}
        size = 512 if texture_set == "walnut" else 256
        standalone_gpu_bytes += 4 * ((4 * size * size - 1) // 3)
    unique_triangles = sum(mesh["extras"]["triangle_count"] for mesh in document["meshes"])
    camera_layout = {}
    for name, camera in CAMERAS.items():
        camera_layout[name] = {**camera, "nodes": camera_nodes[name],
                               "background_relative_ev_target": [-3.0, -2.0]}
    return {
        "schema_version": 1,
        "asset_id": "focused_practice_hall_venue_v1",
        "status": "phase2_venue_production_candidate_not_final_high_mesh",
        "authorship": "original_procedural_billiardgl",
        "files": {
            "gltf": {"path": "assets/modern/phase2/" + ASSET_NAME,
                     "sha256": _sha256(gltf_bytes)},
            "four_camera_preview": {"path": "assets/modern/phase2/" + PREVIEW_NAME,
                                    "sha256": _sha256(preview_bytes), "runtime": False,
                                    "panel_layout": {
                                        "top_left": "player_shot",
                                        "top_right": "low_aim",
                                        "bottom_left": "tactical_overhead",
                                        "bottom_right": "broadcast_oblique"}},
            "shared_pbr_textures": texture_files,
        },
        "room": {"dimensions_m": [8.0, 4.0, 10.0], "clearance_around_table_m": 3.2,
                 "modules": ["floor", "walls", "ceiling", "lamp_rack_and_cables",
                             "acoustic_panels", "wall_trim"],
                 "remote_furniture": False, "brand_text": False},
        "lod_set": {"roots": lod_roots, "rendered_triangles": rendered_triangles,
                    "draw_calls_unbatched": draw_counts},
        "cameras": camera_layout,
        "visibility_overrides": {
            "tactical_overhead": ["hide_tactical_overhead"],
            "reason": "keep the approved orthographic playfield unobstructed while retaining light emission"
        },
        "protected_visibility_anchors_m": {
            "table": [[0, 0.86483976, 0], [-0.635, 0.86483976, -1.27],
                      [0.635, 0.86483976, 1.27]],
            "lamp": [[0, 2.21483976, 0], [-0.30, 2.21483976, -0.90],
                     [0.30, 2.21483976, 0.90]],
            "cue": [[0.18, 0.92, 0.08], [0.18, 0.92, 1.58]],
        },
        "budgets": {
            "unique_triangles_all_lods": unique_triangles,
            "rendered_triangles_by_lod": rendered_triangles,
            "draw_calls_unbatched_by_lod": draw_counts,
            "materials": len(document["materials"]), "texture_references": len(document["textures"]),
            "incremental_texture_gpu_bytes_when_core_pbr_loaded": 0,
            "standalone_shared_texture_gpu_bytes_full_mips": standalone_gpu_bytes,
            "geometry_buffer_bytes": document["buffers"][0]["byteLength"],
        },
        "constraints": {
            "background_relative_to_table_ev_range": [-3.0, -2.0],
            "four_camera_occlusion_check_required": True,
            "shared_original_pbr_only": True,
            "new_texture_assets": 0,
            "remote_furniture_present": False,
            "brand_or_competitor_content_present": False,
        },
        "generator": "scripts/generate_phase2_venue_gltf.py",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--shared-texture-dir", type=Path)
    arguments = parser.parse_args()
    shared_texture_dir = arguments.shared_texture_dir or arguments.output_dir
    arguments.output_dir.mkdir(parents=True, exist_ok=True)
    document, roots, triangles, draws, camera_nodes = build_gltf()
    gltf_bytes = (json.dumps(document, indent=2, sort_keys=True) + "\n").encode("utf-8")
    preview_bytes = build_preview()
    manifest = build_manifest(gltf_bytes, preview_bytes, document, roots, triangles,
                              draws, camera_nodes, shared_texture_dir)
    outputs = {ASSET_NAME: gltf_bytes, PREVIEW_NAME: preview_bytes,
               MANIFEST_NAME: (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode("utf-8")}
    for name, payload in outputs.items():
        (arguments.output_dir / name).write_bytes(payload)
    print("generated venue candidate: unique_triangles=%d LOD0_triangles=%d LOD0_draws=%d" %
          (manifest["budgets"]["unique_triangles_all_lods"],
           triangles["0"], draws["0"]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
