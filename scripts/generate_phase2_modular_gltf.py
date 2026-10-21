#!/usr/bin/env python3
"""Generate the original Phase-2 modular glTF, ball atlas, and asset manifest."""

import argparse
import base64
import hashlib
import json
import math
from pathlib import Path
import struct
import zlib

from generate_m1_graybox_gltf import (
    BALL_RADIUS_M, CORNER_MOUTH_WIDTH_M, HALF_LENGTH_M, HALF_WIDTH_M,
    NOSE_HEIGHT_M, PLAY_SURFACE_Y_M, POCKETS, SIDE_MOUTH_WIDTH_M,
    yaw_quaternion_for_local_z,
)
from generate_phase2_pbr_package import (
    LIGHTING_RIG_NAME, PARAMETERS_NAME, PREVIEW_NAME, TEXTURE_SPECS,
    build_package, texture_filename,
)


ATLAS_WIDTH = 4096
ATLAS_HEIGHT = 512
ATLAS_COLUMNS = 8
ATLAS_ROWS = 2
BALL_ATLAS_INSET_PX = 24
ATLAS_NAME = "focused_practice_hall_ball_identity_atlas_v1.png"
ATLAS_SOURCE_NAME = "focused_practice_hall_ball_identity_atlas_v1.svg"
BALL_CLOSEUP_NAME = "focused_practice_hall_ball_identity_closeup_1_8_11_15_v2.png"
ASSET_NAME = "focused_practice_hall_modular_v1.gltf"
MANIFEST_NAME = "focused_practice_hall_modular_v1.asset.json"

# Phase-2 v10c remains the default asset profile.  The bounded split-facing
# candidate instead uses the shipping pocket frame directly: +X=tangent,
# +Y=world up and +Z=physics outward depth.
BALL_DIAMETER_M = BALL_RADIUS_M * 2.0
POCKET_FRONT_Z_M = BALL_DIAMETER_M * 1.00
POCKET_CENTER_Z_M = BALL_DIAMETER_M * 1.60
POCKET_BACK_Z_M = BALL_DIAMETER_M * 2.20
POCKET_MIN_SPAN_M = BALL_DIAMETER_M * 1.10
POCKET_SIDE_X_MARGIN_M = 0.004425
POCKET_Z_MARGIN_M = 0.005715
POCKET_LINER_WIDTH_M = 0.007
POCKET_RIM_WIDTH_M = BALL_DIAMETER_M * 0.16
POCKET_SIDEWALL_DROP_M = BALL_DIAMETER_M * 0.50
POCKET_PHYSICS_THROAT_DEPTH_M = 0.060
POCKET_PHYSICS_CAVITY_DROP_M = 0.055
POCKET_PHYSICS_VOID_BOTTOM_M = 0.145
POCKET_BACK_LIP_DROP_M = PLAY_SURFACE_Y_M - 0.780
POCKET_BACK_LIP_WIDTH_RATIO = 0.62
POCKET_BACK_LIP_RECESS_RATIO = 0.12
POCKET_CLOSED_BOWL_CONTRACTION = 0.72
POCKET_BODY_TOP_Y_M = 0.780
POCKET_SHOULDER_TOP_Y_M = PLAY_SURFACE_Y_M - 0.006
POCKET_SHOULDER_WIDTH_M = 0.200
POCKET_SHOULDER_PLATFORM_RADIUS_M = 0.160
POCKET_BODY_UNDERCUT_M = PLAY_SURFACE_Y_M - POCKET_BODY_TOP_Y_M
# Keep the cabinet aperture on the authored cloth seam.  Earlier 40 mm and
# 84.84 mm reliefs exposed the deep bag as an exterior black tube.
POCKET_RECESSED_BODY_APERTURE_UNDERCUT_M = 0.0
POCKET_RECESSED_DEEP_RING_Y_M = -0.220
POCKET_RECESSED_BAG_BOTTOM_M = 0.300
POCKET_VISUAL_SOLID_CLEARANCE_M = 0.001
POCKET_RECESSED_BACK_LIP_Y_M = -0.006
POCKET_RECESSED_CAPTURE_WALL_Y_M = -0.120
POCKET_RECESSED_BACK_WALL_Y_M = -0.135
# Keep the visible throat lip transition compact: this is a visual Y-only
# transition and must not stretch across the 60 mm shipping capture band.
POCKET_RECESSED_LIP_DESCENT_M = 0.012
POCKET_RECESSED_JAW_THICKNESS_M = 0.002
POCKET_UPPER_ROLL_RADIUS_M = 0.005
POCKET_UPPER_ROLL_STEPS = 6


MATERIAL_DEFINITIONS = [
    ("MAT_table_felt_teal", [0.0144, 0.1070, 0.1248, 1.0], 0.82, 0.0, None),
    ("MAT_table_cushion_rubber", [0.006, 0.02, 0.022, 1.0], 0.66, 0.0, None),
    ("MAT_table_walnut", [0.0482, 0.0232, 0.0130, 1.0], 0.33, 0.0, (0.42, 0.18)),
    ("MAT_table_gunmetal", [0.0272, 0.0331, 0.0395, 1.0], 0.28, 1.0, None),
    ("MAT_pocket_leather", [0.026, 0.016, 0.011, 1.0], 0.62, 0.0, None),
    ("MAT_pocket_liner", [0.021, 0.025, 0.027, 1.0], 0.76, 0.0, None),
    ("MAT_pocket_void", [0.0005, 0.0005, 0.0006, 1.0], 0.90, 0.0, None),
    ("MAT_pocket_void_depth", [0.0005, 0.0005, 0.0006, 1.0], 0.90, 0.0, None),
    ("MAT_cue_shaft", [0.477, 0.243, 0.078, 1.0], 0.34, 0.0, None),
    ("MAT_cue_tip", [0.0134, 0.010, 0.0072, 1.0], 0.74, 0.0, None),
    ("MAT_lamp_housing", [0.010, 0.0116, 0.0134, 1.0], 0.42, 0.88, None),
    ("MAT_lamp_diffuser", [0.78, 0.72, 0.62, 1.0], 0.50, 0.0, None),
    ("MAT_background", [0.0044, 0.0052, 0.0056, 1.0], 0.84, 0.0, None),
]

MATERIAL_TEXTURE_SETS = {
    "MAT_table_felt_teal": "felt",
    "MAT_table_cushion_rubber": "rubber",
    "MAT_table_walnut": "walnut",
    "MAT_table_gunmetal": "gunmetal",
    "MAT_pocket_leather": "leather",
    "MAT_pocket_void_depth": "pocket_void",
    "MAT_cue_shaft": "cue",
    "MAT_cue_tip": "leather",
    "MAT_lamp_housing": "gunmetal",
}

PBR_TEXTURE_KEYS = tuple(
    (spec[0], channel) for spec in TEXTURE_SPECS
    for channel in ("basecolor", "normal", "orm")
)

BALL_SRGB = [
    (239, 234, 211), (242, 190, 30), (35, 85, 190), (205, 42, 35),
    (126, 48, 144), (238, 114, 24), (35, 139, 73), (133, 38, 48),
    (19, 20, 23), (242, 190, 30), (35, 85, 190), (205, 42, 35),
    (126, 48, 144), (238, 114, 24), (35, 139, 73), (133, 38, 48),
]

DIGIT_GLYPHS = {
    "0": ("0111110", "1100011", "1100011", "1100011", "1100011", "1100011", "1100011", "1100011", "0111110"),
    "1": ("0011000", "0111000", "1111000", "0011000", "0011000", "0011000", "0011000", "0011000", "1111111"),
    "2": ("0111110", "1100011", "0000011", "0000110", "0001100", "0011000", "0110000", "1100000", "1111111"),
    "3": ("1111110", "0000011", "0000011", "0000011", "0111110", "0000011", "0000011", "0000011", "1111110"),
    "4": ("0000110", "0001110", "0011110", "0110110", "1100110", "1111111", "0000110", "0000110", "0000110"),
    "5": ("1111111", "1100000", "1100000", "1100000", "1111110", "0000011", "0000011", "0000011", "1111110"),
    "6": ("0011110", "0110000", "1100000", "1100000", "1111110", "1100011", "1100011", "1100011", "0111110"),
    "7": ("1111111", "0000011", "0000110", "0001100", "0011000", "0110000", "0110000", "0110000", "0110000"),
    "8": ("0111110", "1100011", "1100011", "1100011", "0111110", "1100011", "1100011", "1100011", "0111110"),
    "9": ("0111110", "1100011", "1100011", "1100011", "0111111", "0000011", "0000011", "0000110", "0111100"),
}


def _sha256(data):
    return "sha256:" + hashlib.sha256(data).hexdigest()


def marker_binding_sha256(document, marker_nodes):
    """Hash only frozen signed-physics marker identity/transforms/extras."""
    payload = [{key: document["nodes"][index][key]
                for key in ("name", "translation", "rotation", "extras")
                if key in document["nodes"][index]}
               for index in marker_nodes]
    canonical = json.dumps(payload, separators=(",", ":"),
                           sort_keys=True).encode("utf-8")
    return _sha256(canonical)


def _pack_png_rgba(width, height, pixels):
    def chunk(kind, payload):
        return (struct.pack(">I", len(payload)) + kind + payload +
                struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF))
    raw = b"".join(b"\x00" + bytes(pixels[y * width * 4:(y + 1) * width * 4])
                   for y in range(height))
    return (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def _paint_rect(pixels, x0, y0, x1, y1, color):
    for y in range(max(0, y0), min(ATLAS_HEIGHT, y1)):
        for x in range(max(0, x0), min(ATLAS_WIDTH, x1)):
            offset = (y * ATLAS_WIDTH + x) * 4
            pixels[offset:offset + 4] = color


def _paint_circle(pixels, cx, cy, radius, color):
    radius_squared = radius * radius
    for y in range(cy - radius, cy + radius + 1):
        for x in range(cx - radius, cx + radius + 1):
            if (x - cx) ** 2 + (y - cy) ** 2 <= radius_squared:
                _paint_rect(pixels, x, y, x + 1, y + 1, color)


def _digit_rectangles(digit, x, y, scale):
    return [(x + column * scale, y + row * scale,
             x + (column + 1) * scale, y + (row + 1) * scale)
            for row, line in enumerate(DIGIT_GLYPHS[digit])
            for column, filled in enumerate(line) if filled == "1"]


def build_ball_atlas():
    pixels = bytearray(ATLAS_WIDTH * ATLAS_HEIGHT * 4)
    svg = [
        '<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">' %
        (ATLAS_WIDTH, ATLAS_HEIGHT, ATLAS_WIDTH, ATLAS_HEIGHT),
        '<!-- Original BilliardGL procedural seven-segment ball identity atlas; '
        '24 px tile-safe inset and edge-dilated padding. -->',
    ]
    tile_width = ATLAS_WIDTH // ATLAS_COLUMNS
    tile_height = ATLAS_HEIGHT // ATLAS_ROWS
    ivory = (239, 234, 211, 255)
    ink = (17, 18, 20, 255)
    for identity in range(16):
        column = identity % ATLAS_COLUMNS
        row = identity // ATLAS_COLUMNS
        x0 = column * tile_width
        y0 = row * tile_height
        color = BALL_SRGB[identity] + (255,)
        base = ivory if identity >= 9 or identity == 0 else color
        _paint_rect(pixels, x0, y0, x0 + tile_width, y0 + tile_height, base)
        svg.append('<rect x="%d" y="%d" width="%d" height="%d" fill="rgb(%d,%d,%d)"/>' %
                   (x0, y0, tile_width, tile_height, base[0], base[1], base[2]))
        content_x0 = x0 + BALL_ATLAS_INSET_PX
        content_y0 = y0 + BALL_ATLAS_INSET_PX
        content_width = tile_width - 2 * BALL_ATLAS_INSET_PX
        content_height = tile_height - 2 * BALL_ATLAS_INSET_PX
        if identity >= 9:
            band_y0 = content_y0 + int(content_height * 0.32)
            band_y1 = content_y0 + int(content_height * 0.68)
            _paint_rect(pixels, x0, band_y0, x0 + tile_width, band_y1, color)
            svg.append('<rect x="%d" y="%d" width="%d" height="%d" fill="rgb(%d,%d,%d)"/>' %
                       (x0, band_y0, tile_width, band_y1 - band_y0,
                        color[0], color[1], color[2]))
        if identity == 0:
            continue
        for patch_x in (content_x0 + content_width // 4,
                        content_x0 + 3 * content_width // 4):
            patch_y = content_y0 + content_height // 2
            _paint_circle(pixels, patch_x, patch_y, 38, ivory)
            svg.append('<circle cx="%d" cy="%d" r="38" fill="rgb(%d,%d,%d)"/>' %
                       (patch_x, patch_y, ivory[0], ivory[1], ivory[2]))
            digits = str(identity)
            scale = 4
            glyph_width = len(DIGIT_GLYPHS["0"][0])
            glyph_height = len(DIGIT_GLYPHS["0"])
            advance = (glyph_width + 1) * scale
            total_width = len(digits) * advance - scale
            start_x = patch_x - total_width // 2
            for digit_index, digit in enumerate(digits):
                for rectangle in _digit_rectangles(
                        digit, start_x + digit_index * advance,
                        patch_y - glyph_height * scale // 2, scale):
                    _paint_rect(pixels, *rectangle, ink)
                    svg.append('<rect x="%d" y="%d" width="%d" height="%d" fill="rgb(%d,%d,%d)"/>' %
                               (rectangle[0], rectangle[1], rectangle[2] - rectangle[0],
                                rectangle[3] - rectangle[1], ink[0], ink[1], ink[2]))
    svg.append("</svg>")
    return _pack_png_rgba(ATLAS_WIDTH, ATLAS_HEIGHT, pixels), ("\n".join(svg) + "\n").encode("utf-8")


def build_ball_identity_closeup(atlas_bytes):
    """Create a lossless native-resolution review strip for the four hard identities."""
    compressed = bytearray()
    offset = 8
    while offset + 12 <= len(atlas_bytes):
        length = struct.unpack_from(">I", atlas_bytes, offset)[0]
        kind = atlas_bytes[offset + 4:offset + 8]
        if kind == b"IDAT":
            compressed.extend(atlas_bytes[offset + 8:offset + 8 + length])
        offset += 12 + length
    raw = zlib.decompress(compressed)
    atlas_pixels = bytearray()
    stride = ATLAS_WIDTH * 4
    for y in range(ATLAS_HEIGHT):
        atlas_pixels.extend(raw[y * (stride + 1) + 1:y * (stride + 1) + 1 + stride])
    tile_width = ATLAS_WIDTH // ATLAS_COLUMNS
    tile_height = ATLAS_HEIGHT // ATLAS_ROWS
    identities = (1, 8, 11, 15)
    preview = bytearray(tile_width * len(identities) * tile_height * 4)
    preview_width = tile_width * len(identities)
    for panel, identity in enumerate(identities):
        source_x = (identity % ATLAS_COLUMNS) * tile_width
        source_y = (identity // ATLAS_COLUMNS) * tile_height
        for y in range(tile_height):
            source = ((source_y + y) * ATLAS_WIDTH + source_x) * 4
            target = (y * preview_width + panel * tile_width) * 4
            preview[target:target + tile_width * 4] = atlas_pixels[source:source + tile_width * 4]
    return _pack_png_rgba(preview_width, tile_height, preview)


class Builder:
    def __init__(self):
        self.binary = bytearray()
        self.views = []
        self.accessors = []
        self.meshes = []
        self.nodes = []

    def _view(self, payload, target):
        while len(self.binary) % 4:
            self.binary.append(0)
        offset = len(self.binary)
        self.binary.extend(payload)
        self.views.append({"buffer": 0, "byteOffset": offset,
                           "byteLength": len(payload), "target": target})
        return len(self.views) - 1

    def _float_accessor(self, values, accessor_type):
        components = {"VEC2": 2, "VEC3": 3, "VEC4": 4}[accessor_type]
        payload = struct.pack("<%df" % (len(values) * components),
                              *(item for value in values for item in value))
        result = {"bufferView": self._view(payload, 34962), "byteOffset": 0,
                  "componentType": 5126, "count": len(values), "type": accessor_type}
        if accessor_type == "VEC3":
            result["min"] = [min(value[axis] for value in values) for axis in range(3)]
            result["max"] = [max(value[axis] for value in values) for axis in range(3)]
        self.accessors.append(result)
        return len(self.accessors) - 1

    def _indices(self, values):
        code, component = ("H", 5123) if max(values) < 65536 else ("I", 5125)
        payload = struct.pack("<%d%s" % (len(values), code), *values)
        self.accessors.append({"bufferView": self._view(payload, 34963), "byteOffset": 0,
                               "componentType": component, "count": len(values),
                               "type": "SCALAR", "min": [min(values)], "max": [max(values)]})
        return len(self.accessors) - 1

    def mesh(self, name, geometry, material, role, lod):
        positions, normals, tangents, uvs, indices = geometry
        primitive = {"attributes": {
            "POSITION": self._float_accessor(positions, "VEC3"),
            "NORMAL": self._float_accessor(normals, "VEC3"),
            "TANGENT": self._float_accessor(tangents, "VEC4"),
            "TEXCOORD_0": self._float_accessor(uvs, "VEC2")},
            "indices": self._indices(indices), "material": material, "mode": 4}
        self.meshes.append({"name": name, "primitives": [primitive],
                            "extras": {"triangle_count": len(indices) // 3,
                                       "asset_role": role, "lod": lod}})
        return len(self.meshes) - 1

    def node(self, name, mesh=None, translation=None, rotation=None, children=None, extras=None):
        value = {"name": name}
        if mesh is not None:
            value["mesh"] = mesh
        if translation is not None:
            value["translation"] = list(translation)
        if rotation is not None:
            value["rotation"] = list(rotation)
        if children:
            value["children"] = list(children)
        if extras:
            value["extras"] = extras
        self.nodes.append(value)
        return len(self.nodes) - 1


def _normalise(vector):
    # Keep the operation order explicit.  ``sum`` starts with an integer zero;
    # on near-orthogonal pocket tangents that changed the last bit of the
    # normalised result relative to the original asset generator.  The direct
    # three-component expression is stable and reproduces the committed glTF.
    length = math.sqrt(vector[0] * vector[0] +
                       vector[1] * vector[1] +
                       vector[2] * vector[2])
    return tuple(value / length for value in vector)


def _orthogonal_tangent(normal, preferred):
    dot = sum(normal[i] * preferred[i] for i in range(3))
    tangent = tuple(preferred[i] - normal[i] * dot for i in range(3))
    if sum(value * value for value in tangent) < 1e-12:
        tangent = (1.0, 0.0, 0.0)
    tangent = _normalise(tangent)
    return tangent + (1.0,)


def box(center, size):
    cx, cy, cz = center
    sx, sy, sz = (value * 0.5 for value in size)
    faces = (
        ((1, 0, 0), (0, 0, 1), ((sx, -sy, -sz), (sx, -sy, sz), (sx, sy, sz), (sx, sy, -sz))),
        ((-1, 0, 0), (0, 0, -1), ((-sx, -sy, sz), (-sx, -sy, -sz), (-sx, sy, -sz), (-sx, sy, sz))),
        ((0, 1, 0), (1, 0, 0), ((-sx, sy, -sz), (sx, sy, -sz), (sx, sy, sz), (-sx, sy, sz))),
        ((0, -1, 0), (1, 0, 0), ((-sx, -sy, sz), (sx, -sy, sz), (sx, -sy, -sz), (-sx, -sy, -sz))),
        ((0, 0, 1), (1, 0, 0), ((-sx, -sy, sz), (sx, -sy, sz), (sx, sy, sz), (-sx, sy, sz))),
        ((0, 0, -1), (-1, 0, 0), ((sx, -sy, -sz), (-sx, -sy, -sz), (-sx, sy, -sz), (sx, sy, -sz))),
    )
    positions, normals, tangents, uvs, indices = [], [], [], [], []
    for normal, tangent, corners in faces:
        start = len(positions)
        positions.extend((cx + x, cy + y, cz + z) for x, y, z in corners)
        normals.extend([normal] * 4)
        tangents.extend([tangent + (1.0,)] * 4)
        uvs.extend(((0, 0), (1, 0), (1, 1), (0, 1)))
        indices.extend((start, start + 1, start + 2, start, start + 2, start + 3))
    return positions, normals, tangents, uvs, indices


def beveled_box(center, size, bevel):
    cx, cy, cz = center
    hx, hy, hz = (value * 0.5 for value in size)
    core = (max(0.0001, hx - bevel), max(0.0001, hy - bevel), max(0.0001, hz - bevel))
    faces = (
        ((hx, -hy, -hz), (hx, -hy, hz), (hx, hy, hz), (hx, hy, -hz)),
        ((-hx, -hy, hz), (-hx, -hy, -hz), (-hx, hy, -hz), (-hx, hy, hz)),
        ((-hx, hy, -hz), (hx, hy, -hz), (hx, hy, hz), (-hx, hy, hz)),
        ((-hx, -hy, hz), (hx, -hy, hz), (hx, -hy, -hz), (-hx, -hy, -hz)),
        ((-hx, -hy, hz), (hx, -hy, hz), (hx, hy, hz), (-hx, hy, hz)),
        ((hx, -hy, -hz), (-hx, -hy, -hz), (-hx, hy, -hz), (hx, hy, -hz)),
    )
    positions, normals, tangents, uvs, indices = [], [], [], [], []
    for corners in faces:
        axis_u = tuple(corners[1][i] - corners[0][i] for i in range(3))
        u_length = math.sqrt(sum(value * value for value in axis_u))
        v_axis = tuple(corners[3][i] - corners[0][i] for i in range(3))
        v_length = math.sqrt(sum(value * value for value in v_axis))
        u_values = (0.0, min(0.49, bevel / u_length), max(0.51, 1.0 - bevel / u_length), 1.0)
        v_values = (0.0, min(0.49, bevel / v_length), max(0.51, 1.0 - bevel / v_length), 1.0)
        start = len(positions)
        for v in v_values:
            for u in u_values:
                raw = tuple(corners[0][i] + axis_u[i] * u + v_axis[i] * v for i in range(3))
                nearest = (max(-core[0], min(core[0], raw[0])),
                           max(-core[1], min(core[1], raw[1])),
                           max(-core[2], min(core[2], raw[2])))
                normal = _normalise(tuple(raw[i] - nearest[i] for i in range(3)))
                rounded = tuple(nearest[i] + normal[i] * bevel for i in range(3))
                positions.append((rounded[0] + cx, rounded[1] + cy, rounded[2] + cz))
                normals.append(normal)
                tangents.append(_orthogonal_tangent(normal, _normalise(axis_u)))
                uvs.append((u, v))
        for row in range(3):
            for column in range(3):
                a = start + row * 4 + column
                b = a + 1
                c = a + 5
                d = a + 4
                indices.extend((a, b, c, a, c, d))
    return positions, normals, tangents, uvs, indices


def _flat_top_polygon(points, height):
    """Triangulate one x/z polygon as a +Y table-body surface."""
    compact = []
    for point in points:
        if not compact or math.hypot(point[0] - compact[-1][0],
                                     point[1] - compact[-1][1]) > 1e-12:
            compact.append(point)
    if len(compact) > 1 and math.hypot(compact[0][0] - compact[-1][0],
                                       compact[0][1] - compact[-1][1]) <= 1e-12:
        compact.pop()
    if len(compact) < 3:
        return ([], [], [], [], [])
    area = sum(compact[index][0] * compact[(index + 1) % len(compact)][1] -
               compact[(index + 1) % len(compact)][0] * compact[index][1]
               for index in range(len(compact)))
    if abs(area) <= 1e-14:
        return ([], [], [], [], [])
    if area < 0.0:
        compact.reverse()
    indices = []
    for index in range(1, len(compact) - 1):
        indices.extend((0, index + 1, index))
    return ([(x, height, z) for x, z in compact],
            [(0.0, 1.0, 0.0)] * len(compact),
            [(1.0, 0.0, 0.0, 1.0)] * len(compact),
            [((x + 0.775) / 1.55, (z + 1.41) / 2.82)
             for x, z in compact], indices)


def _convex_hull_xz(points):
    """Return the strict CCW hull while retaining the authored D arc."""
    ordered = sorted(set(points))
    if len(ordered) < 3:
        raise ValueError("table-body pocket opening has no area")

    def cross(origin, first, second):
        return ((first[0] - origin[0]) * (second[1] - origin[1]) -
                (first[1] - origin[1]) * (second[0] - origin[0]))

    lower = []
    for point in ordered:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], point) <= 1e-14:
            lower.pop()
        lower.append(point)
    upper = []
    for point in reversed(ordered):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], point) <= 1e-14:
            upper.pop()
        upper.append(point)
    return lower[:-1] + upper[:-1]


def _top_rectangle(x0, z0, x1, z1, height):
    if x1 - x0 <= 1e-12 or z1 - z0 <= 1e-12:
        return ([], [], [], [], [])
    return _flat_top_polygon(((x0, z0), (x1, z0),
                              (x1, z1), (x0, z1)), height)


def _top_rectangle_minus_convex_opening(bounds, opening, height):
    """Exactly tile an axis-aligned patch outside one convex opening."""
    x0, z0, x1, z1 = bounds
    hull = _convex_hull_xz(opening)
    hole_x0 = min(point[0] for point in hull)
    hole_x1 = max(point[0] for point in hull)
    pieces = [
        _top_rectangle(x0, z0, hole_x0, z1, height),
        _top_rectangle(hole_x1, z0, x1, z1, height),
    ]
    # A strict CCW convex hull traverses its lower x-monotone chain from
    # left to right and its upper chain from right to left.  Tiling each
    # non-vertical edge against the corresponding patch boundary leaves the
    # actual curved D opening untouched, without a Boolean-mesh dependency.
    for first, second in zip(hull, hull[1:] + hull[:1]):
        dx = second[0] - first[0]
        if abs(dx) <= 1e-12:
            continue
        left, right = ((first, second) if dx > 0.0 else (second, first))
        boundary_z = z0 if dx > 0.0 else z1
        pieces.append(_flat_top_polygon((
            left, right, (right[0], boundary_z),
            (left[0], boundary_z)), height))
    return merge_geometries(pieces)


def _pocket_body_opening_seam(width, pocket_type, segments,
                              recessed_funnel_candidate=False):
    """Return the cloth seam projected onto the walnut body's top opening.

    The cloth sits 84.84 mm above the cabinet top.  The candidate's narrow
    sloped liner finishes this cut-face without enlarging the lower opening
    beyond the cloth seam and exposing the bag exterior.
    """
    cloth = [(x, -depth) for x, depth in _pocket_opening_boundary(
        width, pocket_type, segments, physics_aligned=True)]
    if not recessed_funnel_candidate:
        return cloth
    return [(x, depth - POCKET_RECESSED_BODY_APERTURE_UNDERCUT_M)
            for x, depth in cloth]


def table_body_with_pocket_openings(rounded, segments,
                                    recessed_funnel_candidate=False):
    """Table body whose walnut top has six real physics-aligned openings."""
    # Preserve the complete cabinet shell.  Only its upper face is retiled
    # around the actual cloth-cut -> mouth -> 6 cm capture footprint.  Cutting
    # onward to the cabinet edge produced a visible exterior chute even though
    # the recessed pocket shell already hides the cabinet floor.
    body = (beveled_box((0, 0.39, 0), (1.55, 0.78, 2.82), 0.035)
            if rounded else box((0, 0.39, 0), (1.55, 0.78, 2.82)))
    x_limit, z_limit = ((0.740, 1.375) if rounded else (0.775, 1.410))
    positions, normals, tangents, uvs, source_indices = body
    indices = []
    for offset in range(0, len(source_indices), 3):
        triangle = source_indices[offset:offset + 3]
        if all(normals[index][1] > 0.999999 for index in triangle):
            continue
        indices.extend(triangle)
    body_without_top = (positions, normals, tangents, uvs, indices)

    openings = []
    physical_openings = []
    decorative_cuts = []
    for _, _, pocket_type, center, inward, tangent, width in POCKETS:
        _, _, _, _, cavity_upper, _, _, _ = profiled_physics_pocket_cavity(
            width, segments, pocket_type)
        body_seam = _pocket_body_opening_seam(
            width, pocket_type, segments,
            recessed_funnel_candidate=recessed_funnel_candidate)
        opening = [
            (center[0] + tangent[0] * x - inward[0] * depth,
             center[1] + tangent[1] * x - inward[1] * depth)
            for x, depth in (cavity_upper + body_seam)]
        hull = _convex_hull_xz(opening)
        physical_openings.append(((min(p[0] for p in hull),min(p[1] for p in hull),
                                   max(p[0] for p in hull),max(p[1] for p in hull)),hull))
        if recessed_funnel_candidate:
            rim=profiled_physics_pocket_cavity(width,segments,pocket_type,True)[0]
            outer=[(p[0],p[2]) for p in rim[0][segments+1:2*(segments+1)]]
            for index in range(segments):
                for tri in ((cavity_upper[index],cavity_upper[index+1],outer[index+1]),
                            (cavity_upper[index],outer[index+1],outer[index])):
                    points=[(center[0]+tangent[0]*x-inward[0]*d,
                             center[1]+tangent[1]*x-inward[1]*d) for x,d in tri]
                    a,b,c=points
                    if abs((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]))>1e-14:
                        decorative_cuts.append(_convex_hull_xz(points))
            hull=_convex_hull_xz([
                (center[0]+tangent[0]*x-inward[0]*depth,
                 center[1]+tangent[1]*x-inward[1]*depth)
                for x,depth in outer+body_seam])
        openings.append((
            (min(point[0] for point in hull),
             min(point[1] for point in hull),
             max(point[0] for point in hull),
             max(point[1] for point in hull)), hull))

    partition_openings = physical_openings if recessed_funnel_candidate else openings
    x_breaks = sorted({-x_limit, x_limit} |
                      {value for bounds, _ in partition_openings
                       for value in (bounds[0], bounds[2])})
    z_breaks = sorted({-z_limit, z_limit} |
                      {value for bounds, _ in partition_openings
                       for value in (bounds[1], bounds[3])})
    if recessed_funnel_candidate:
        x_breaks = sorted(set(x_breaks) | {
            max(-x_limit, min(x_limit, edge + offset))
            for bounds, _ in partition_openings for edge in (bounds[0], bounds[2])
            for offset in (-POCKET_SHOULDER_WIDTH_M, POCKET_SHOULDER_WIDTH_M)})
        z_breaks = sorted(set(z_breaks) | {
            max(-z_limit, min(z_limit, edge + offset))
            for bounds, _ in partition_openings for edge in (bounds[1], bounds[3])
            for offset in (-POCKET_SHOULDER_WIDTH_M, POCKET_SHOULDER_WIDTH_M)})
        # Bound the local height field with a regular sampling cadence rather
        # than leaving a 78 mm fall on one long, arbitrarily placed triangle.
        cadence = 0.040 if segments == 24 else (0.045 if segments == 16 else 0.050)
        steps = math.ceil(0.360 / cadence)
        offsets = {max(-0.360,min(0.360,step*cadence))
                   for step in range(-steps,steps+1)}
        x_breaks = sorted(set(x_breaks) | {
            max(-x_limit, min(x_limit, pocket[3][0] + offset))
            for pocket in POCKETS for offset in offsets})
        z_breaks = sorted(set(z_breaks) | {
            max(-z_limit, min(z_limit, pocket[3][1] + offset))
            for pocket in POCKETS for offset in offsets})
    top_pieces = []
    cuts=[hull for _,hull in physical_openings]+decorative_cuts
    def halfplane(poly,a,b,inside):
        result=[]
        for p,q in zip(poly,poly[1:]+poly[:1]):
            dp=(b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0])
            dq=(b[0]-a[0])*(q[1]-a[1])-(b[1]-a[1])*(q[0]-a[0])
            if not inside:dp,dq=-dp,-dq
            if dp>=-1e-13:result.append(p)
            if (dp>1e-13 and dq< -1e-13) or (dp< -1e-13 and dq>1e-13):
                t=dp/(dp-dq)
                result.append((p[0]+t*(q[0]-p[0]),p[1]+t*(q[1]-p[1])))
        return result
    def polygon_area(poly):
        return abs(sum(a[0]*b[1]-a[1]*b[0] for a,b in zip(poly,poly[1:]+poly[:1])))*.5
    for x0, x1 in zip(x_breaks, x_breaks[1:]):
        for z0, z1 in zip(z_breaks, z_breaks[1:]):
            if recessed_funnel_candidate:
                pieces=[[(x0,z0),(x1,z0),(x1,z1),(x0,z1)]]
                for cut in cuts:
                    if max(p[0] for p in cut)<x0 or min(p[0] for p in cut)>x1 or \
                            max(p[1] for p in cut)<z0 or min(p[1] for p in cut)>z1:continue
                    remaining=[]
                    for polygon in pieces:
                        inside=polygon
                        for a,b in zip(cut,cut[1:]+cut[:1]):
                            outside=halfplane(inside,a,b,False)
                            if len(outside)>=3 and polygon_area(outside)>1e-13:
                                remaining.append(outside)
                            inside=halfplane(inside,a,b,True)
                            if len(inside)<3 or polygon_area(inside)<1e-13:break
                    pieces=remaining
                top_pieces.extend(_flat_top_polygon(poly,.78) for poly in pieces)
                continue
            center = ((x0 + x1) * 0.5, (z0 + z1) * 0.5)
            if any(bounds[0] - 1e-12 <= center[0] <= bounds[2] + 1e-12 and
                   bounds[1] - 1e-12 <= center[1] <= bounds[3] + 1e-12
                   for bounds, _ in openings):
                continue
            top_pieces.append(_top_rectangle(x0, z0, x1, z1, 0.78))
    if not recessed_funnel_candidate:
        for bounds, opening in openings:
            top_pieces.append(_top_rectangle_minus_convex_opening(
                bounds, opening, 0.78))

    top = merge_geometries(top_pieces)
    if recessed_funnel_candidate:
        positions, _, _, uvs, indices = top
        # Conform every patch edge before displacement.  A vertex lying on
        # another patch's unsplit edge was harmless when planar but opens a
        # crack after height displacement.  Reuse one index per X/Z point.
        points = sorted({(round(p[0], 12), round(p[2], 12)) for p in positions})
        point_indices = {p: index for index, p in enumerate(points)}
        conforming = []
        for offset in range(0, len(indices), 3):
            triangle = [(round(positions[i][0], 12), round(positions[i][2], 12))
                        for i in indices[offset:offset+3]]
            boundary = []
            for a, b in zip(triangle, triangle[1:] + triangle[:1]):
                dx, dz = b[0]-a[0], b[1]-a[1]
                length2 = dx*dx+dz*dz
                edge_points = []
                for p in points:
                    t = ((p[0]-a[0])*dx+(p[1]-a[1])*dz)/length2
                    if -1e-10 <= t < 1.0-1e-10 and \
                            abs((p[0]-a[0])*dz-(p[1]-a[1])*dx) < 1e-11:
                        edge_points.append((t, point_indices[p]))
                boundary.extend(index for _, index in sorted(edge_points))
            center = (sum(p[0] for p in triangle)/3,
                      sum(p[1] for p in triangle)/3)
            # Centers are not fed back into edge splitting: original triangle
            # interiors cannot be another patch's boundary.
            conforming.append((center, boundary))
        base_points = list(points)
        indices = []
        for center, boundary in conforming:
            center_index = len(base_points)
            base_points.append(center)
            for first, second in zip(boundary, boundary[1:] + boundary[:1]):
                indices.extend((center_index, first, second))
        positions = [(x, POCKET_BODY_TOP_Y_M, z) for x, z in base_points]
        uvs = [((x+0.775)/1.55, (z+1.41)/2.82) for x,z in base_points]
        def shoulder_height(x, z):
            distance = min(math.hypot(x-pocket[3][0], z-pocket[3][1])
                           for pocket in POCKETS) - POCKET_SHOULDER_PLATFORM_RADIUS_M
            blend = 1.0 - _smoothstep01(distance / POCKET_SHOULDER_WIDTH_M)
            return POCKET_BODY_TOP_Y_M + blend * (POCKET_SHOULDER_TOP_Y_M-POCKET_BODY_TOP_Y_M)
        # Bilinear height per shared grid cell avoids injecting curved center
        # samples into very skinny cut-cell triangles, which amplifies the
        # perpendicular face slope despite a bounded continuous derivative.
        from bisect import bisect_right
        raised = []
        for x, y, z in positions:
            xi=max(0,min(len(x_breaks)-2,bisect_right(x_breaks,x)-1))
            zi=max(0,min(len(z_breaks)-2,bisect_right(z_breaks,z)-1))
            x0,x1=x_breaks[xi:xi+2];z0,z1=z_breaks[zi:zi+2]
            tx=(x-x0)/(x1-x0);tz=(z-z0)/(z1-z0)
            h0=shoulder_height(x0,z0)*(1-tx)+shoulder_height(x1,z0)*tx
            h1=shoulder_height(x0,z1)*(1-tx)+shoulder_height(x1,z1)*tx
            raised.append((x,h0*(1-tz)+h1*tz,z))
        top = _smooth_indexed_geometry(raised, uvs, indices,
                                       [(1.0, 0.0, 0.0)] * len(raised))
        # Continue the raised local upper edge down into the original cabinet
        # at the actual exterior perimeter, not as a ramp across the opening.
        walls = []
        for axis, limit, other in ((0,x_limit,2),(2,z_limit,0)):
            for sign in (-1.0,1.0):
                boundary = sorted({point for point in raised
                    if abs(point[axis]-sign*limit)<1e-9}, key=lambda p:p[other])
                for first, second in zip(boundary,boundary[1:]):
                    lower_first=(first[0],POCKET_BODY_TOP_Y_M,first[2])
                    lower_second=(second[0],POCKET_BODY_TOP_Y_M,second[2])
                    corners=[lower_first,lower_second,second,first]
                    if (axis==0 and sign<0) or (axis==2 and sign>0):
                        corners.reverse()
                    for a,b,c in ((corners[0],corners[1],corners[2]),
                                  (corners[0],corners[2],corners[3])):
                        u=tuple(b[i]-a[i] for i in range(3));v=tuple(c[i]-a[i] for i in range(3))
                        face=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
                        if sum(value*value for value in face)<1e-20:continue
                        normal=_normalise(face)
                        tangent=_orthogonal_tangent(normal,u)
                        walls.append(([a,b,c],[normal]*3,[tangent]*3,
                                      [(0,0),(1,0),(1,1)],[0,1,2]))
        top=merge_geometries((top,*walls))
    return merge_geometries((body_without_top, top))


def transform_geometry(geometry, center, axis_x=(1, 0, 0), axis_z=(0, 0, 1),
                       preserve_winding=False):
    positions, normals, tangents, uvs, indices = geometry
    axis_y = (0, 1, 0)

    def transform3(value, translate):
        result = tuple(axis_x[i] * value[0] + axis_y[i] * value[1] + axis_z[i] * value[2]
                       for i in range(3))
        return tuple(result[i] + (center[i] if translate else 0.0) for i in range(3))
    transformed_tangents = []
    determinant = axis_x[0] * axis_z[2] - axis_x[2] * axis_z[0]
    for tangent in tangents:
        xyz = transform3(tangent[:3], False)
        handedness = (-tangent[3]
                      if preserve_winding and determinant < 0.0 else
                      tangent[3])
        transformed_tangents.append(xyz + (handedness,))
    transformed_indices = list(indices)
    if preserve_winding and determinant < 0.0:
        transformed_indices = []
        for offset in range(0, len(indices), 3):
            transformed_indices.extend((indices[offset], indices[offset + 2],
                                        indices[offset + 1]))
    return ([transform3(value, True) for value in positions],
            [transform3(value, False) for value in normals], transformed_tangents,
            list(uvs), transformed_indices)


def _triangulate_simple_polygon(points):
    """Ear-clip an x/z polygon and return +Y-wound triangle indices."""
    order = list(range(len(points)))
    area = sum(points[i][0] * points[(i + 1) % len(points)][1] -
               points[(i + 1) % len(points)][0] * points[i][1]
               for i in range(len(points)))
    if area < 0.0:
        order.reverse()
    triangles = []
    while len(order) > 3:
        clipped = False
        for slot, current in enumerate(order):
            previous = order[slot - 1]
            following = order[(slot + 1) % len(order)]
            ax, az = points[previous]
            bx, bz = points[current]
            cx, cz = points[following]
            cross = (bx - ax) * (cz - bz) - (bz - az) * (cx - bx)
            if cross <= 1e-12:
                continue

            def inside(index):
                if index in (previous, current, following):
                    return False
                px, pz = points[index]
                signs = []
                for first, second in ((previous, current), (current, following),
                                      (following, previous)):
                    x1, z1 = points[first]
                    x2, z2 = points[second]
                    signs.append((x2 - x1) * (pz - z1) - (z2 - z1) * (px - x1))
                return min(signs) >= -1e-10

            if any(inside(index) for index in order):
                continue
            # Standard x/z CCW becomes +Y when the last two indices are reversed.
            triangles.extend((previous, following, current))
            del order[slot]
            clipped = True
            break
        if not clipped:
            raise ValueError("felt pocket contour is not a triangulable simple polygon")
    triangles.extend((order[0], order[2], order[1]))
    return triangles


def felt_grid(columns, rows, physics_aligned_pockets=False):
    """Build a clean six-pocket cloth contour instead of omitting coarse grid cells."""
    del rows
    corner_radius = (CORNER_MOUTH_WIDTH_M / math.sqrt(2.0)
                     if physics_aligned_pockets else
                     CORNER_MOUTH_WIDTH_M * 0.56)
    side_radius = (SIDE_MOUTH_WIDTH_M * 0.5
                   if physics_aligned_pockets else
                   SIDE_MOUTH_WIDTH_M * 0.56)
    points = []

    def point(x, z):
        candidate = (round(x, 9), round(z, 9))
        if not points or candidate != points[-1]:
            points.append(candidate)

    def arc(cx, cz, radius, start, end, segments):
        for index in range(segments + 1):
            angle = start + (end - start) * index / segments
            point(cx + radius * math.cos(angle), cz + radius * math.sin(angle))

    def corner_opening(cx, cz, radius, start, end):
        # Preserve sampled analytic tangency at each jaw, then bridge the
        # centre with a long chord.  This keeps the frozen smooth-boundary
        # contract while removing the visually pointed cloth wedge produced
        # by a full quarter-circle in the exterior pocket camera.
        edge = (end - start) * 0.04
        arc(cx, cz, radius, start, start + edge, 3)
        arc(cx, cz, radius, end - edge, end, 3)

    # Clockwise boundary.  Corner openings use one clean mouth chord between
    # the jaw lines.  The former quarter-circle pushed a teal wedge between
    # the leather facings in the low camera, despite being mathematically
    # valid.  Side pockets retain their smooth semicircular cloth cut-out.
    point(-HALF_WIDTH_M + corner_radius, HALF_LENGTH_M)
    point(HALF_WIDTH_M - corner_radius, HALF_LENGTH_M)
    corner_opening(HALF_WIDTH_M, HALF_LENGTH_M, corner_radius,
                   math.pi, 1.5 * math.pi)
    point(HALF_WIDTH_M, side_radius)
    arc(HALF_WIDTH_M, 0.0, side_radius, 0.5 * math.pi, 1.5 * math.pi,
        max(8, min(24, columns)))
    point(HALF_WIDTH_M, -HALF_LENGTH_M + corner_radius)
    corner_opening(HALF_WIDTH_M, -HALF_LENGTH_M, corner_radius,
                   0.5 * math.pi, math.pi)
    point(-HALF_WIDTH_M + corner_radius, -HALF_LENGTH_M)
    corner_opening(-HALF_WIDTH_M, -HALF_LENGTH_M, corner_radius,
                   0.0, 0.5 * math.pi)
    point(-HALF_WIDTH_M, -side_radius)
    arc(-HALF_WIDTH_M, 0.0, side_radius, -0.5 * math.pi, 0.5 * math.pi,
        max(8, min(24, columns)))
    point(-HALF_WIDTH_M, HALF_LENGTH_M - corner_radius)
    corner_opening(-HALF_WIDTH_M, HALF_LENGTH_M, corner_radius,
                   -0.5 * math.pi, 0.0)
    if points[-1] == points[0]:
        points.pop()
    positions = [(x, PLAY_SURFACE_Y_M, z) for x, z in points]
    normals = [(0, 1, 0)] * len(points)
    tangents = [(1, 0, 0, 1)] * len(points)
    uvs = [((x + HALF_WIDTH_M) / (2 * HALF_WIDTH_M),
            (z + HALF_LENGTH_M) / HALF_LENGTH_M) for x, z in points]
    return positions, normals, tangents, uvs, _triangulate_simple_polygon(points)


def sphere(radius, columns, rows, outward_winding=True):
    positions, normals, tangents, uvs, indices = [], [], [], [], []
    for row in range(rows + 1):
        v = row / rows
        latitude = math.pi * v
        for column in range(columns + 1):
            u = column / columns
            longitude = 2 * math.pi * u
            normal = (math.sin(latitude) * math.cos(longitude), math.cos(latitude),
                      math.sin(latitude) * math.sin(longitude))
            positions.append(tuple(value * radius for value in normal))
            normals.append(normal)
            tangents.append((-math.sin(longitude), 0.0, math.cos(longitude), 1.0))
            # Decreasing U with positive longitude keeps identity glyphs readable
            # from the +Z presentation camera without a renderer-only mirror.
            uvs.append((1.0 - u, v))
    for row in range(rows):
        for column in range(columns):
            a = row * (columns + 1) + column
            b, d, c = a + 1, a + columns + 1, a + columns + 2
            if outward_winding:
                indices.extend((a, c, d, a, b, c))
            else:
                indices.extend((a, d, c, a, c, b))
    return positions, normals, tangents, uvs, indices


def cylinder_z(radius0, radius1, z0, z1, segments):
    positions, normals, tangents, uvs, indices = [], [], [], [], []
    for ring, (radius, z) in enumerate(((radius0, z0), (radius1, z1))):
        for segment in range(segments + 1):
            u = segment / segments
            angle = 2 * math.pi * u
            normal = (math.cos(angle), math.sin(angle), 0)
            positions.append((radius * normal[0], radius * normal[1], z))
            normals.append(normal)
            tangents.append((-math.sin(angle), math.cos(angle), 0, 1))
            uvs.append((u, ring))
    for segment in range(segments):
        a = segment
        b, d, c = a + 1, a + segments + 1, a + segments + 2
        indices.extend((a, b, c, a, c, d))
    return positions, normals, tangents, uvs, indices


def disk(radius, segments):
    positions = [(0.0, 0.0, 0.0)]
    normals = [(0, 1, 0)]
    tangents = [(1, 0, 0, 1)]
    uvs = [(0.5, 0.5)]
    for segment in range(segments + 1):
        angle = 2 * math.pi * segment / segments
        positions.append((radius * math.cos(angle), 0.0, radius * math.sin(angle)))
        normals.append((0, 1, 0))
        tangents.append((1, 0, 0, 1))
        uvs.append((0.5 + 0.5 * math.cos(angle), 0.5 + 0.5 * math.sin(angle)))
    indices = []
    for segment in range(segments):
        indices.extend((0, segment + 1, segment + 2))
    return positions, normals, tangents, uvs, indices


def ellipse_disk(radius_x, radius_z, center_z, segments):
    """Horizontal recessed bottom constrained to the inward mouth half-space."""
    positions = [(0.0, 0.0, center_z)]
    normals = [(0, 1, 0)]
    tangents = [(1, 0, 0, 1)]
    uvs = [(0.5, 0.5)]
    for segment in range(segments + 1):
        angle = 2 * math.pi * segment / segments
        positions.append((radius_x * math.cos(angle), 0.0,
                          center_z + radius_z * math.sin(angle)))
        normals.append((0, 1, 0))
        tangents.append((1, 0, 0, 1))
        uvs.append((0.5 + 0.5 * math.cos(angle), 0.5 + 0.5 * math.sin(angle)))
    indices = []
    for segment in range(segments):
        indices.extend((0, segment + 1, segment + 2))
    return positions, normals, tangents, uvs, indices


def _geometry_at_y(geometry, y):
    positions, normals, tangents, uvs, indices = geometry
    return ([(position[0], y, position[2]) for position in positions],
            normals, tangents, uvs, indices)


def _pocket_u_boundary(half_x, front_z, back_z, segments):
    """One rounded, inward-tapered U from right mouth to left mouth."""
    boundary = []
    for segment in range(segments + 1):
        theta = math.pi * segment / segments
        inward = math.sin(theta)
        taper = 1.0 - 0.30 * inward
        boundary.append((half_x * math.cos(theta) * taper,
                         front_z + (back_z - front_z) * inward))
    return boundary


def _pocket_opening_boundary(width, pocket_type, segments, inset=0.0,
                             physics_aligned=False):
    """Return the liner seam that matches the actual six-pocket cloth cut.

    Side pockets use the same semicircle as ``felt_grid``.  Corner pockets use
    its two short analytic shoulders and central chord, expressed in the
    pocket's tangent/inward frame.  ``inset`` moves the seam into the opening,
    never underneath intact cloth.
    """
    radius = ((width / math.sqrt(2.0) if pocket_type == "corner"
               else width * 0.5) if physics_aligned else width * 0.56) - inset
    if radius <= 0.0:
        raise ValueError("pocket opening inset consumes the mouth")
    if pocket_type == "side":
        return [(radius * math.cos(math.pi * segment / segments),
                 radius * math.sin(math.pi * segment / segments))
                for segment in range(segments + 1)]
    if pocket_type != "corner":
        raise ValueError("unsupported pocket type")
    endpoint = radius / math.sqrt(2.0)
    shoulder_angle = math.pi * (0.25 + 0.5 * 0.04)
    shoulder_x = radius * math.cos(shoulder_angle)
    shoulder_z = radius * math.sin(shoulder_angle)
    boundary = []
    for segment in range(segments + 1):
        x = endpoint * (1.0 - 2.0 * segment / segments)
        absolute_x = abs(x)
        if absolute_x <= shoulder_x:
            z = shoulder_z
        else:
            z = math.sqrt(max(0.0, radius * radius - x * x))
        boundary.append((x, z))
    return boundary


def _smooth_indexed_geometry(positions, uvs, indices, tangent_hints):
    """Build smooth vertex normals from the actual indexed surface.

    Vertices are shared only inside one visually continuous surface.  Callers
    deliberately keep material changes and real cross-section folds separate.
    """
    accumulated = [[0.0, 0.0, 0.0] for _ in positions]
    for offset in range(0, len(indices), 3):
        triangle = indices[offset:offset + 3]
        a, b, c = (positions[index] for index in triangle)
        first_edge = tuple(b[axis] - a[axis] for axis in range(3))
        second_edge = tuple(c[axis] - a[axis] for axis in range(3))
        face = (
            first_edge[1] * second_edge[2] - first_edge[2] * second_edge[1],
            first_edge[2] * second_edge[0] - first_edge[0] * second_edge[2],
            first_edge[0] * second_edge[1] - first_edge[1] * second_edge[0],
        )
        if sum(value * value for value in face) <= 1e-20:
            raise ValueError("smooth pocket surface contains a degenerate triangle")
        for index in triangle:
            for axis in range(3):
                accumulated[index][axis] += face[axis]
    normals = [_normalise(tuple(value)) for value in accumulated]
    tangents = [_orthogonal_tangent(normal, tangent_hints[index])
                for index, normal in enumerate(normals)]
    return positions, normals, tangents, uvs, indices


def _smooth_strip_between_boundaries(first, second, first_y, second_y):
    """One continuous U strip with shared ring vertices and smooth normals."""
    if len(first) != len(second):
        raise ValueError("pocket boundary cardinality mismatch")
    positions = ([(x, first_y, z) for x, z in first] +
                 [(x, second_y, z) for x, z in second])
    count = len(first)
    uvs = ([(index / (count - 1), 0.0) for index in range(count)] +
           [(index / (count - 1), 1.0) for index in range(count)])
    indices = []
    for index in range(count - 1):
        a, b = index, index + 1
        d, c = count + index, count + index + 1
        indices.extend((a, b, c, a, c, d))

    def boundary_tangent(boundary, index):
        before = boundary[max(0, index - 1)]
        after = boundary[min(len(boundary) - 1, index + 1)]
        return (after[0] - before[0], 0.0, after[1] - before[1])

    tangent_hints = ([boundary_tangent(first, index)
                      for index in range(count)] +
                     [boundary_tangent(second, index)
                      for index in range(count)])
    return _smooth_indexed_geometry(positions, uvs, indices, tangent_hints)


def _smooth_strip_between_rings3d(first, second):
    """One continuous strip between equally sampled authored 3D rings."""
    if len(first) != len(second) or len(first) < 3:
        raise ValueError("pocket 3D ring cardinality mismatch")
    positions = list(first) + list(second)
    count = len(first)
    uvs = ([(index / count, 0.0) for index in range(count)] +
           [(index / count, 1.0) for index in range(count)])
    indices = []
    for index in range(count):
        following = (index + 1) % count
        indices.extend((index, following, count + following,
                        index, count + following, count + index))

    def ring_tangent(ring, index):
        before = ring[(index - 1) % count]
        after = ring[(index + 1) % count]
        return tuple(after[axis] - before[axis] for axis in range(3))

    tangent_hints = ([ring_tangent(first, index) for index in range(count)] +
                     [ring_tangent(second, index) for index in range(count)])
    return _smooth_indexed_geometry(positions, uvs, indices, tangent_hints)


def _smooth_strip_between_open_rings3d(first, second):
    """One continuous strip between open U rings, without a mouth cap."""
    if len(first) != len(second) or len(first) < 2:
        raise ValueError("pocket open 3D ring cardinality mismatch")
    positions = list(first) + list(second)
    count = len(first)
    uvs = ([(index / (count - 1), 0.0) for index in range(count)] +
           [(index / (count - 1), 1.0) for index in range(count)])
    indices = []
    for index in range(count - 1):
        indices.extend((index, index + 1, count + index + 1,
                        index, count + index + 1, count + index))

    def ring_tangent(ring, index):
        before = ring[max(0, index - 1)]
        after = ring[min(count - 1, index + 1)]
        return tuple(after[axis] - before[axis] for axis in range(3))

    tangent_hints = ([ring_tangent(first, index) for index in range(count)] +
                     [ring_tangent(second, index) for index in range(count)])
    return _smooth_indexed_geometry(positions, uvs, indices, tangent_hints)


def _smooth_strip_between_closed_rings3d(first, second):
    """One continuous strip between equal closed rings, without end caps."""
    if len(first) != len(second) or len(first) < 3:
        raise ValueError("pocket closed 3D ring cardinality mismatch")
    positions = list(first) + list(second)
    count = len(first)
    uvs = ([(index / count, 0.0) for index in range(count)] +
           [(index / count, 1.0) for index in range(count)])
    indices = []
    for index in range(count):
        following = (index + 1) % count
        indices.extend((index, following, count + following,
                        index, count + following, count + index))

    def ring_tangent(ring, index):
        before = ring[(index - 1) % count]
        after = ring[(index + 1) % count]
        return tuple(after[axis] - before[axis] for axis in range(3))

    tangent_hints = ([ring_tangent(first, index) for index in range(count)] +
                     [ring_tangent(second, index) for index in range(count)])
    geometry = _smooth_indexed_geometry(positions, uvs, indices, tangent_hints)
    # This is the *inside* of a pocket, not an outward-facing vessel shell.
    # Reverse both winding and the complete tangent frame, leaving positions
    # untouched.  Winding/normal agreement alone missed this semantic error.
    reversed_indices = []
    for offset in range(0, len(indices), 3):
        a, b, c = indices[offset:offset+3]
        reversed_indices.extend((a, c, b))
    return (geometry[0], [tuple(-value for value in n) for n in geometry[1]],
            [t[:3] + (-t[3],) for t in geometry[2]], geometry[3], reversed_indices)


def _smoothstep01(value):
    value = max(0.0, min(1.0, value))
    return value * value * (3.0 - 2.0 * value)


def _pocket_upper_roll(body_seam):
    """True quarter-round leather lip; zero radius at physical jaw endpoints."""
    positions, uvs, hints, indices = [], [], [], []
    lookup, rings = {}, []
    for step in range(POCKET_UPPER_ROLL_STEPS + 1):
        angle = math.pi * 0.5 * step / POCKET_UPPER_ROLL_STEPS
        ring = []
        for index, (x, depth) in enumerate(body_seam):
            fraction = index / (len(body_seam)-1)
            radius = POCKET_UPPER_ROLL_RADIUS_M * math.sin(math.pi*fraction)
            if index in (0,len(body_seam)-1):
                radius = 0.0
            point = (x, -0.002-radius*(1-math.cos(angle)),
                     depth+radius*math.sin(angle))
            key = tuple(round(value,12) for value in point)
            if key not in lookup:
                lookup[key] = len(positions)
                positions.append(point)
                uvs.append((fraction,step/POCKET_UPPER_ROLL_STEPS))
                before=body_seam[max(0,index-1)]
                after=body_seam[min(len(body_seam)-1,index+1)]
                hints.append((after[0]-before[0],0,after[1]-before[1]))
            ring.append(lookup[key])
        rings.append(ring)
    for first,second in zip(rings,rings[1:]):
        for index in range(len(body_seam)-1):
            for tri in ((first[index],first[index+1],second[index+1]),
                        (first[index],second[index+1],second[index])):
                if len(set(tri))==3:
                    indices.extend(tri)
    return _smooth_indexed_geometry(positions,uvs,indices,hints), \
        [positions[index] for index in rings[-1]]


def _carved_u_rim(inner, outer):
    """Replace the buried U skin with a recessed, never-raised leather curve."""
    outer=[(a[0]+(b[0]-a[0])*5/7,b[1],a[2]+(b[2]-a[2])*5/7)
           for a,b in zip(inner,outer)]
    levels=[0,6,1,2,3,4,5]
    positions,uvs,hints=[],[],[]
    for level in levels:
        theta=level*math.pi/12
        for index,(a,b) in enumerate(zip(inner,outer)):
            positions.append(a if level==0 else b if level==6 else
                             (a[0]+(b[0]-a[0])*(1-math.cos(theta)),
                              a[1]+(b[1]-a[1])*math.sin(theta),
                              a[2]+(b[2]-a[2])*(1-math.cos(theta))))
            uvs.append((index/(len(inner)-1),level/6))
            before=inner[max(0,index-1)];after=inner[min(len(inner)-1,index+1)]
            hints.append(tuple(after[i]-before[i] for i in range(3)))
    indices=[];count=len(inner)
    for level in range(6):
        first=levels.index(level)*count;second=levels.index(level+1)*count
        for index in range(count-1):
            indices.extend((first+index,first+index+1,second+index+1,
                            first+index,second+index+1,second+index))
    return _smooth_indexed_geometry(positions,uvs,indices,hints)


def _recessed_funnel_rings(cavity_upper, rim_inner, lower):
    """Keep the opening near cloth height; descend the liner inside its shell.

    The exact-width mouth and 6 cm capture band remain unchanged.  The upper
    lip meets the locally raised shoulder 6 mm below cloth instead of being
    forced down to the cabinet's unrelated 84.84 mm lower top.  Jaw nose
    endpoints retain their shared -18 mm to -6 mm edge.
    """
    if not (len(cavity_upper) == len(rim_inner) == len(lower)):
        raise ValueError("recessed funnel boundary cardinality mismatch")
    maximum_depth = max(depth for _, depth in cavity_upper)
    post_capture_span = maximum_depth - POCKET_PHYSICS_THROAT_DEPTH_M
    body_top_y = POCKET_SHOULDER_TOP_Y_M - PLAY_SURFACE_Y_M
    outer_ring, inner_ring, lower_ring = [], [], []
    for boundary, inset, lower_point in zip(cavity_upper, rim_inner, lower):
        depth = boundary[1]
        # The lower liner can descend, but the opening stays on the raised
        # shoulder.  No independent annulus or cover spans the mouth.
        capture_phase = _smoothstep01(
            depth / POCKET_RECESSED_LIP_DESCENT_M)
        back_phase = (_smoothstep01(
            (depth - POCKET_PHYSICS_THROAT_DEPTH_M) / post_capture_span)
            if post_capture_span > 1e-12 else 0.0)
        outer_y = (-0.006 + (body_top_y + 0.006) * capture_phase +
                   (POCKET_RECESSED_BACK_LIP_Y_M - body_top_y) * back_phase)
        lip_thickness = 0.012
        inner_y = outer_y - lip_thickness
        lower_y = (-POCKET_PHYSICS_CAVITY_DROP_M +
                   (POCKET_RECESSED_CAPTURE_WALL_Y_M +
                    POCKET_PHYSICS_CAVITY_DROP_M) * capture_phase +
                   (POCKET_RECESSED_BACK_WALL_Y_M -
                    POCKET_RECESSED_CAPTURE_WALL_Y_M) * back_phase)
        # The leather lip grows away from the open channel.  Mirroring the
        # legacy inset across the exact cavity boundary keeps the visible
        # inner edge on |x|=W/2 throughout d=0..6 cm instead of narrowing it.
        outer_ring.append((2.0 * boundary[0] - inset[0], outer_y,
                           2.0 * depth - inset[1]))
        inner_ring.append((boundary[0], inner_y, depth))
        # Lateral contraction is legal only after capture.  The full-width
        # shipping corridor remains geometrically clear at every height.
        if depth <= POCKET_PHYSICS_THROAT_DEPTH_M + 1e-12:
            lower_ring.append((boundary[0], lower_y, depth))
        else:
            lower_ring.append((lower_point[0], lower_y, lower_point[1]))
    return outer_ring, inner_ring, lower_ring


def _strip_between_boundaries(first, second, first_y, second_y,
                              smooth=False):
    if len(first) != len(second):
        raise ValueError("pocket boundary cardinality mismatch")
    if smooth:
        return _smooth_strip_between_boundaries(
            first, second, first_y, second_y)
    quads = []
    for index in range(len(first) - 1):
        quads.append(_quad_geometry((
            (first[index][0], first_y, first[index][1]),
            (first[index + 1][0], first_y, first[index + 1][1]),
            (second[index + 1][0], second_y, second[index + 1][1]),
            (second[index][0], second_y, second[index][1]),
        )))
    return merge_geometries(quads)


def profiled_pocket_v10c(width, segments, smooth=False, pocket_type="side"):
    """Connected mouth -> rim -> sidewall -> rounded tapered drop geometry."""
    inner_half = width * 0.5 - POCKET_SIDE_X_MARGIN_M
    lower_half = inner_half - POCKET_RIM_WIDTH_M
    if lower_half <= 0.0:
        raise ValueError("pocket rim consumes the mouth")
    if smooth:
        # The exposed rim must terminate at the real cloth opening.  The old
        # deep U crossed back under intact felt, leaving only disconnected
        # slivers visible through the opening (the apparent "knife blades").
        rim_outer = _pocket_opening_boundary(
            width, pocket_type, segments)
        rim_inner = _pocket_opening_boundary(
            width, pocket_type, segments, inset=POCKET_RIM_WIDTH_M)
    else:
        rim_inner = _pocket_u_boundary(inner_half, 0.0,
                                       POCKET_BACK_Z_M + POCKET_RIM_WIDTH_M,
                                       segments)
        rim_outer = _pocket_u_boundary(inner_half + POCKET_RIM_WIDTH_M,
                                       0.0,
                                       POCKET_BACK_Z_M + 2.0 * POCKET_RIM_WIDTH_M,
                                       segments)
    drop_boundary = _pocket_u_boundary(lower_half, POCKET_FRONT_Z_M,
                                       POCKET_BACK_Z_M, segments)
    rim_y = -0.012
    drop_y = rim_y - POCKET_SIDEWALL_DROP_M
    rim = _strip_between_boundaries(
        rim_inner, rim_outer, rim_y, -0.006, smooth=smooth)
    sidewall = _strip_between_boundaries(drop_boundary, rim_inner,
                                         drop_y, rim_y, smooth=smooth)
    drop_points = [(x, z) for x, z in drop_boundary]
    positions = [(0.0, drop_y, POCKET_CENTER_Z_M)] + [
        (x, drop_y, z) for x, z in drop_points]
    indices = []
    for index in range(len(drop_points) - 1):
        indices.extend((0, index + 1, index + 2))
    indices.extend((0, len(drop_points), 1))
    drop = (positions, [(0, 1, 0)] * len(positions),
            [(1, 0, 0, 1)] * len(positions),
            [(0.5, 0.5)] + [((x / (2.0 * lower_half)) + 0.5,
              (z - POCKET_FRONT_Z_M) /
              (POCKET_BACK_Z_M - POCKET_FRONT_Z_M)) for x, z in drop_points],
            indices)
    return rim, sidewall, drop, rim_inner, drop_boundary


def profiled_physics_pocket_cavity(width, segments, pocket_type="side",
                                   recessed_funnel_candidate=False):
    """Compact bowl bound to the shipping mouth and +outward 6 cm throat.

    The cloth seam is the only portion permitted inward of the physics mouth
    plane.  The free mouth chord is exactly ``width`` and the cavity descends
    immediately from that chord toward the canonical 6 cm outward station.
    """
    cloth_inward = _pocket_opening_boundary(
        width, pocket_type, segments, physics_aligned=True)
    cloth_seam = [(x, -depth) for x, depth in cloth_inward]
    mouth = [(x, 0.0) for x, _ in cloth_seam]
    side_segments = max(1, segments // 4)
    back_segments = segments - 2 * side_segments
    if back_segments < 1:
        raise ValueError("pocket throat needs a back span")
    half_width = width * 0.5
    cavity_upper = []
    for index in range(side_segments + 1):
        cavity_upper.append((
            half_width,
            POCKET_PHYSICS_THROAT_DEPTH_M * index / side_segments))
    # Keep the complete, full-width shipping corridor through the 6 cm
    # capture station, then round the *visible* opening behind it.  A straight
    # chord at 6 cm made the pocket read as a square socket even when the
    # lower shell was rounded.  The arc starts at both full-width capture
    # corners, so it cannot narrow or obstruct the physical passage.
    post_capture_rounding = width * 0.25
    for index in range(1, back_segments + 1):
        x = half_width - width * index / back_segments
        cavity_upper.append((
            x,
            POCKET_PHYSICS_THROAT_DEPTH_M +
            post_capture_rounding * math.sqrt(max(
                0.0, 1.0 - (x / half_width) ** 2))))
    for index in range(1, side_segments + 1):
        cavity_upper.append((
            -half_width,
            POCKET_PHYSICS_THROAT_DEPTH_M *
            (side_segments - index) / side_segments))

    rim_inner = []
    lower = []
    for x, depth in cavity_upper:
        side_phase = min(1.0, depth / POCKET_PHYSICS_THROAT_DEPTH_M)
        back_phase = 1.0 if depth >= POCKET_PHYSICS_THROAT_DEPTH_M - 1e-12 else 0.0
        # Follow the outward cavity arc instead of bridging the whole mouth
        # chord to the inward cloth seam.  The old strip was the solid brown
        # trapezoid visible across every opening.
        rim_inner.append((x - math.copysign(
                              POCKET_LINER_WIDTH_M * side_phase, x)
                          if abs(x) > 1e-12 else 0.0,
                          depth - POCKET_LINER_WIDTH_M * back_phase))
        # The lower boundary follows the same post-capture arc.  This makes
        # the back a vertical recessed wall rather than a horizontal lid;
        # only its below-rim width tapers, after the unobstructed 6 cm band.
        lower.append((x * (1.0 - 0.18 * side_phase),
                      depth))

    if recessed_funnel_candidate:
        # Keep the shallow cloth-cut region genuinely open, and descend the
        # visible lip below the cabinet before the capture station.  The old
        # constant-Y U ring sat 29.84 mm above the cabinet and closed across
        # the mouth, so the exterior camera saw a flat black socket.  These
        # open rings preserve the exact plan-view corridor while presenting a
        # continuous leather lip -> liner wall -> deep void cross-section.
        opening_boundary = list(cavity_upper)
        lip_outer_ring, lip_inner_ring, wall_lower_ring = \
            _recessed_funnel_rings(cavity_upper, rim_inner, lower)
        rim = _carved_u_rim(lip_inner_ring, lip_outer_ring)
        sidewall = _smooth_strip_between_open_rings3d(
            wall_lower_ring, lip_inner_ring)
        # Return the actual shared cavity-edge and lower-wall plan boundaries
        # so metadata/validation describe rendered geometry, not the rejected
        # inward-inset legacy strip.
        rim_inner = [(x, depth) for x, _, depth in lip_inner_ring]
        lower = [(x, depth) for x, _, depth in wall_lower_ring]
        body_seam = _pocket_body_opening_seam(
            width, pocket_type, segments, recessed_funnel_candidate=True)
        body_top_relative_y = POCKET_SHOULDER_TOP_Y_M - PLAY_SURFACE_Y_M
        body_aperture_ring = [
            (x, body_top_relative_y, depth) for x, depth in body_seam]
        # A narrow sloped cut-face, not a broad walnut shelf or a further
        # plan-view excavation.  Its entire surface is below the full ball
        # sweep.  The back wall retains this contour down to the bag floor;
        # contracting it toward the capture station exposed a black tube.
        body_liner_ring = [
            (x, body_top_relative_y - POCKET_LINER_WIDTH_M,
             depth + POCKET_LINER_WIDTH_M *
             math.sin(math.pi * index / (len(body_seam) - 1)))
            for index, (x, depth) in enumerate(body_seam)]
        rim = merge_geometries((rim,
            _smooth_strip_between_open_rings3d(
                list(reversed(body_aperture_ring)),
                [(x, -0.002, depth) for x, depth in reversed(body_seam)])))
        sidewall = merge_geometries((sidewall,
            _smooth_strip_between_open_rings3d(
                list(reversed(body_liner_ring)),
                list(reversed(body_aperture_ring)))))
        # Close the open U wall with the compact, below-cloth cabinet seam.
        # The resulting upper loop is the real entrance to a bag-shaped
        # cavity, not a full-width planar cover.
        bowl_upper_ring = (list(wall_lower_ring) +
                           list(reversed(body_liner_ring[1:-1])))
        maximum_upper_depth = max(point[2] for point in bowl_upper_ring)
        required_radial_clearance = (BALL_DIAMETER_M * 0.5 +
                                     POCKET_VISUAL_SOLID_CLEARANCE_M)
        outward_span = maximum_upper_depth - POCKET_PHYSICS_THROAT_DEPTH_M
        outward_depth_scale = max(
            1.0, required_radial_clearance / outward_span)
        deep_ring = [
            (point[0] * (POCKET_CLOSED_BOWL_CONTRACTION
                         if index < len(wall_lower_ring) else 1.0),
             POCKET_RECESSED_DEEP_RING_Y_M,
             POCKET_PHYSICS_THROAT_DEPTH_M +
             (point[2] - POCKET_PHYSICS_THROAT_DEPTH_M) *
             (outward_depth_scale
              if point[2] >= POCKET_PHYSICS_THROAT_DEPTH_M else
              (1.0 if index >= len(wall_lower_ring) else
               POCKET_CLOSED_BOWL_CONTRACTION)))
            for index, point in enumerate(bowl_upper_ring)]
        bottom_ring = [
            (point[0], -POCKET_RECESSED_BAG_BOTTOM_M, point[2])
            for point in deep_ring]
        closure_parts = [
            _smooth_strip_between_closed_rings3d(
                bowl_upper_ring, deep_ring),
            _smooth_strip_between_closed_rings3d(
                deep_ring, bottom_ring),
        ]
        bottom_boundary = [(point[0], point[2]) for point in bottom_ring]
        bottom_center_depth = sum(point[1] for point in bottom_boundary) / \
            len(bottom_boundary)
        bottom_y = -POCKET_RECESSED_BAG_BOTTOM_M
    else:
        rim = _strip_between_boundaries(
            rim_inner, cavity_upper, -0.012, -0.006, smooth=True)
        sidewall = _strip_between_boundaries(
            lower, rim_inner, -POCKET_PHYSICS_CAVITY_DROP_M, -0.012,
            smooth=True)
        bottom_boundary = cloth_seam + list(reversed(lower))
        bottom_center_depth = 0.0
        bottom_y = -POCKET_PHYSICS_VOID_BOTTOM_M
        inner_wall = _strip_between_boundaries(
            cloth_seam, cloth_seam, -0.006,
            -POCKET_PHYSICS_VOID_BOTTOM_M, smooth=True)
        lower_shell = _strip_between_boundaries(
            lower, lower, -POCKET_PHYSICS_CAVITY_DROP_M,
            -POCKET_PHYSICS_VOID_BOTTOM_M, smooth=True)
        closure_parts = [inner_wall, lower_shell]
    bottom_area = sum(
        bottom_boundary[index][0] *
        bottom_boundary[(index + 1) % len(bottom_boundary)][1] -
        bottom_boundary[(index + 1) % len(bottom_boundary)][0] *
        bottom_boundary[index][1]
        for index in range(len(bottom_boundary)))
    if bottom_area < 0.0:
        bottom_boundary.reverse()
    bottom_positions = [(0.0, bottom_y,
                         bottom_center_depth)] + [
        (x, bottom_y, depth)
        for x, depth in bottom_boundary]
    bottom_indices = []
    for index in range(len(bottom_boundary)):
        bottom_indices.extend((
            0, 1 + (index + 1) % len(bottom_boundary), 1 + index))
    bottom = (
        bottom_positions,
        [(0.0, 1.0, 0.0)] * len(bottom_positions),
        [(1.0, 0.0, 0.0, 1.0)] * len(bottom_positions),
        # Every point of this fan is at the same 145 mm cavity depth, so it
        # must sample the same deepest AO row.  The texture transform scales
        # V=1 to the repeat-safe V=.5 minimum; varying V across this plane
        # would create a false light/dark "lid" pattern.
        [(0.5, 1.0)] + [((x / width) + 0.5, 1.0)
                        for x, _ in bottom_boundary],
        bottom_indices)
    drop = merge_geometries((*closure_parts, bottom))
    return (rim, sidewall, drop, rim_inner, cavity_upper, lower,
            cloth_seam, mouth)


def _quad_geometry(corners):
    """One rendered quad with derived normal/tangent."""
    edge_u = tuple(corners[1][axis] - corners[0][axis] for axis in range(3))
    edge_v = tuple(corners[3][axis] - corners[0][axis] for axis in range(3))
    normal = _normalise((edge_u[1] * edge_v[2] - edge_u[2] * edge_v[1],
                         edge_u[2] * edge_v[0] - edge_u[0] * edge_v[2],
                         edge_u[0] * edge_v[1] - edge_u[1] * edge_v[0]))
    tangent = _orthogonal_tangent(normal, edge_u)
    return (list(corners), [normal] * 4, [tangent] * 4,
            [(0, 0), (1, 0), (1, 1), (0, 1)], [0, 1, 2, 0, 2, 3])


def _triangle_geometry(corners):
    """One rendered triangle with derived normal/tangent."""
    edge_u = tuple(corners[1][axis] - corners[0][axis] for axis in range(3))
    edge_v = tuple(corners[2][axis] - corners[0][axis] for axis in range(3))
    normal = _normalise((edge_u[1] * edge_v[2] - edge_u[2] * edge_v[1],
                         edge_u[2] * edge_v[0] - edge_u[0] * edge_v[2],
                         edge_u[0] * edge_v[1] - edge_u[1] * edge_v[0]))
    tangent = _orthogonal_tangent(normal, edge_u)
    return (list(corners), [normal] * 3, [tangent] * 3,
            [(0, 0), (0.5, 1), (1, 0)], [0, 1, 2])


def mouth_apron_wedge(width, open_top=False):
    """Walnut apron frame terminating exactly at local mouth plane Z=0."""
    inner_x, outer_x = width * 0.52, width * 0.74
    inner_z, outer_z = 0.0, -width * 0.70
    if open_top:
        # Candidate support is fully buried inside the authored table body.
        # Its previous -40 mm upper edge and mouth-to-apron bottom remained
        # visible as square wooden brackets and a brown crossbar.
        lower, upper = 0.740 - PLAY_SURFACE_Y_M, 0.775 - PLAY_SURFACE_Y_M
    else:
        lower, upper = 0.78 - PLAY_SURFACE_Y_M, -0.040
    top = ((-inner_x, upper, inner_z), (inner_x, upper, inner_z),
           (outer_x, upper, outer_z), (-outer_x, upper, outer_z))
    bottom = ((-inner_x, lower, inner_z), (-outer_x, lower, outer_z),
              (outer_x, lower, outer_z), (inner_x, lower, inner_z))
    sides = (
        (top[0], bottom[0], bottom[3], top[1]),
        (top[1], bottom[3], bottom[2], top[2]),
        (top[2], bottom[2], bottom[1], top[3]),
        (top[3], bottom[1], bottom[0], top[0]),
    )
    surfaces = ((_quad_geometry(bottom),) if open_top else
                (_quad_geometry(top), _quad_geometry(bottom)))
    # The first side is the vertical wall on the physics mouth chord.  Keeping
    # it while merely removing the horizontal cap produced the brown block
    # that visually sealed the pocket.  The candidate is a genuine frame:
    # bottom, two cheeks and the outer apron wall, with the mouth left open.
    rendered_sides = sides[1:] if open_top else sides
    return merge_geometries((*surfaces,
                             *(_quad_geometry(side)
                               for side in rendered_sides)))


def pocket_throat_underlay(width, pocket_type, segments):
    """Sloped cavity floor from the deep mouth plane to the cloth cut seam."""
    boundary = _pocket_opening_boundary(width, pocket_type, segments)
    outer_half_width = width * 0.74
    outer_z = -width * 0.70
    # Replace the walnut apron cap with one continuous recessed cavity from
    # its outer edge to the real cloth cut.  The boundary endpoints are
    # omitted because the wider apron corners already close the polygon.
    points = [(-outer_half_width, outer_z),
              (outer_half_width, outer_z)] + boundary[1:-1]
    maximum_z = max(point[1] for point in boundary)
    mouth_y, seam_y = -0.078, -0.040575
    positions = [(x, mouth_y + (seam_y - mouth_y) *
                  (z - outer_z) / (maximum_z - outer_z), z)
                 for x, z in points]
    uvs = [((x / (2.0 * outer_half_width)) + 0.5,
            (z - outer_z) / (maximum_z - outer_z))
           for x, z in points]
    indices = _triangulate_simple_polygon(points)
    return _smooth_indexed_geometry(
        positions, uvs, indices, [(1.0, 0.0, 0.0)] * len(positions))


def inward_u_slot(width, segments):
    """Open U-shaped leather wall and welded facing folds, never a 360 tube."""
    lower, upper = [], []
    for segment in range(segments + 1):
        theta = math.pi * segment / segments
        upper.append((width * 0.50 * math.cos(theta), -0.012,
                      width * 0.58 * math.sin(theta)))
        lower.append((width * 0.30 * math.cos(theta), -0.075,
                      width * (0.18 + 0.34 * math.sin(theta))))
    positions = lower + upper
    normals, tangents, uvs, indices = [], [], [], []
    # Smooth approximate inward-facing normals; exact tangent-space length is
    # part of the existing glTF validator contract.
    for ring, values in enumerate((lower, upper)):
        for segment, position in enumerate(values):
            theta = math.pi * segment / segments
            normal = _normalise((-math.cos(theta), 0.35, -math.sin(theta)))
            normals.append(normal)
            tangents.append(_orthogonal_tangent(normal,
                                                 (-math.sin(theta), 0.0,
                                                  math.cos(theta))))
            uvs.append((segment / segments, ring))
    stride = segments + 1
    for segment in range(segments):
        bottom, top = segment, stride + segment
        indices.extend((bottom, top, top + 1, bottom, top + 1, bottom + 1))
    wall = positions, normals, tangents, uvs, indices

    folds = []
    for sign in (-1.0, 1.0):
        # Both inner coordinates occur in the U wall above; keeping identical
        # values makes the jaw-to-wall join objectively verifiable.
        jaw_upper = (sign * width * 0.56, -0.006, 0.0)
        jaw_lower = (sign * width * 0.56, -0.035, 0.0)
        wall_upper = (sign * width * 0.50, -0.012, 0.0)
        wall_lower = (sign * width * 0.30, -0.075, width * 0.18)
        if sign < 0:
            corners = (jaw_upper, wall_upper, wall_lower, jaw_lower)
        else:
            corners = (wall_upper, jaw_upper, jaw_lower, wall_lower)
        folds.append(_quad_geometry(corners))
    return merge_geometries((wall, *folds))


def annulus_arc(inner_radius, outer_radius, segments,
                start_fraction=0.20, end_fraction=0.80,
                inner_y=0.0, outer_y=0.0):
    """Short inward-facing cloth lip; local +Z points into the table."""
    positions, normals, tangents, uvs, indices = [], [], [], [], []
    for ring, radius in enumerate((inner_radius, outer_radius)):
        for segment in range(segments + 1):
            # Keep the cloth transition between the two jaw noses.  A full
            # half-ring reads as an applied plastic pocket protector once it
            # reaches the apron, especially in the low pocket camera.
            angle = math.pi * (start_fraction +
                               (end_fraction - start_fraction) * segment / segments)
            x, z = radius * math.cos(angle), radius * math.sin(angle)
            positions.append((x, inner_y if ring == 0 else outer_y, z))
            normals.append((0, 1, 0))
            tangents.append((1, 0, 0, 1))
            scale = radius / outer_radius
            uvs.append((0.5 + 0.5 * scale * math.cos(angle),
                        0.5 + 0.5 * scale * math.sin(angle)))
    stride = segments + 1
    for segment in range(segments):
        inner, outer = segment, stride + segment
        indices.extend((inner, outer + 1, outer, inner, inner + 1, outer + 1))
    return positions, normals, tangents, uvs, indices


def eccentric_annulus_arc(inner_radius, outer_radius, inner_offset_z, segments,
                          start_fraction=0.0, end_fraction=1.0,
                          inner_y=-0.035, outer_y=-0.006):
    """Leather mouth transition whose recessed throat sits behind the jaw plane.

    Local +Z points into the playing surface.  A negative ``inner_offset_z``
    therefore moves the throat into the apron instead of leaving a circular
    collar sitting on top of it.
    """
    positions, normals, tangents, uvs, indices = [], [], [], [], []
    for ring, radius in enumerate((inner_radius, outer_radius)):
        for segment in range(segments + 1):
            angle = math.pi * (start_fraction +
                               (end_fraction - start_fraction) * segment / segments)
            z_offset = inner_offset_z if ring == 0 else 0.0
            positions.append((radius * math.cos(angle),
                              inner_y if ring == 0 else outer_y,
                              radius * math.sin(angle) + z_offset))
            normals.append((0, 1, 0))
            tangents.append((1, 0, 0, 1))
            scale = radius / outer_radius
            uvs.append((0.5 + 0.5 * scale * math.cos(angle),
                        0.5 + 0.5 * scale * math.sin(angle)))
    stride = segments + 1
    for segment in range(segments):
        inner, outer = segment, stride + segment
        indices.extend((inner, outer + 1, outer, inner, inner + 1, outer + 1))
    return positions, normals, tangents, uvs, indices


def curved_wall_arc(radius, lower, upper, segments,
                    start_fraction=0.08, end_fraction=0.92):
    """Visible leather inner wall that makes the pocket recess read in depth."""
    positions, normals, tangents, uvs, indices = [], [], [], [], []
    for ring, y in enumerate((lower, upper)):
        for segment in range(segments + 1):
            angle = math.pi * (start_fraction +
                               (end_fraction - start_fraction) * segment / segments)
            x, z = radius * math.cos(angle), radius * math.sin(angle)
            positions.append((x, y, z))
            normals.append((-math.cos(angle), 0, -math.sin(angle)))
            tangents.append((-math.sin(angle), 0, math.cos(angle), 1))
            uvs.append((segment / segments, ring))
    stride = segments + 1
    for segment in range(segments):
        lower_index, upper_index = segment, stride + segment
        indices.extend((lower_index, upper_index, upper_index + 1,
                        lower_index, upper_index + 1, lower_index + 1))
    return positions, normals, tangents, uvs, indices


def tapered_wall_arc(upper_radius, lower_radius, lower, upper, segments,
                     start_fraction=0.0, end_fraction=2.0):
    """Inward-sloping full pocket wall, from leather lip to recessed bottom."""
    positions, normals, tangents, uvs, indices = [], [], [], [], []
    for ring, (radius, y) in enumerate(((lower_radius, lower),
                                        (upper_radius, upper))):
        for segment in range(segments + 1):
            angle = math.pi * (start_fraction +
                               (end_fraction - start_fraction) * segment / segments)
            positions.append((radius * math.cos(angle), y,
                              radius * math.sin(angle)))
            radial_y = (upper_radius - lower_radius) / max(upper - lower, 1e-6)
            normals.append(_normalise((-math.cos(angle), radial_y,
                                       -math.sin(angle))))
            tangents.append((-math.sin(angle), 0, math.cos(angle), 1))
            uvs.append((segment / segments, ring))
    stride = segments + 1
    for segment in range(segments):
        lower_index, upper_index = segment, stride + segment
        indices.extend((lower_index, upper_index, upper_index + 1,
                        lower_index, upper_index + 1, lower_index + 1))
    return positions, normals, tangents, uvs, indices


def disk_sector(radius, segments):
    """Inward half of a recessed pocket void, with local +Z into the table."""
    positions = [(0.0, 0.0, 0.0)]
    normals = [(0, 1, 0)]
    tangents = [(1, 0, 0, 1)]
    uvs = [(0.5, 0.5)]
    for segment in range(segments + 1):
        angle = math.pi * segment / segments
        positions.append((radius * math.cos(angle), 0.0, radius * math.sin(angle)))
        normals.append((0, 1, 0)); tangents.append((1, 0, 0, 1))
        uvs.append((0.5 + 0.5 * math.cos(angle), 0.5 + 0.5 * math.sin(angle)))
    indices = []
    for segment in range(segments):
        indices.extend((0, segment + 1, segment + 2))
    return positions, normals, tangents, uvs, indices


def merge_geometries(geometries):
    merged = ([], [], [], [], [])
    positions, normals, tangents, uvs, indices = merged
    for geometry in geometries:
        source_positions, source_normals, source_tangents, source_uvs, source_indices = geometry
        base = len(positions)
        positions.extend(source_positions); normals.extend(source_normals)
        tangents.extend(source_tangents); uvs.extend(source_uvs)
        indices.extend(base + index for index in source_indices)
    return merged


def _filter_geometry_triangles(geometry, predicate):
    """Return a compact geometry containing exactly the selected triangles."""
    source_positions, source_normals, source_tangents, source_uvs, source_indices = geometry
    positions, normals, tangents, uvs, indices = [], [], [], [], []
    remap = {}
    for offset in range(0, len(source_indices), 3):
        triangle = source_indices[offset:offset + 3]
        if not predicate(triangle, source_positions, source_normals):
            continue
        for source_index in triangle:
            if source_index not in remap:
                remap[source_index] = len(positions)
                positions.append(source_positions[source_index])
                normals.append(source_normals[source_index])
                tangents.append(source_tangents[source_index])
                uvs.append(source_uvs[source_index])
            indices.append(remap[source_index])
    return positions, normals, tangents, uvs, indices


def curved_leather_terminal(length, rail_half_width, throat_half_width,
                            lateral_curve, sections=7, smooth=False,
                            inward_cross_sign=None, surface="full",
                            width_profile="linear",
                            compact_throat_facing=False):
    """Curved cushion terminal ribbon descending continuously into the throat."""
    if width_profile not in ("linear", "quadratic", "cubic"):
        raise ValueError("unsupported terminal width profile")
    if width_profile != "linear" and not smooth:
        raise ValueError("curved terminal width profiles require smooth topology")
    rings = []
    for section in range(sections + 1):
        t = section / sections
        width_t = (t * t * t if width_profile == "cubic" else
                   t * t if width_profile == "quadratic" else t)
        half_width = throat_half_width + \
            (rail_half_width - throat_half_width) * width_t
        center_x = lateral_curve * math.sin(math.pi * t)
        if inward_cross_sign is not None:
            if inward_cross_sign not in (-1.0, 1.0):
                raise ValueError("terminal inward cross sign must be canonical")
            # The rail occupies the outside half-space.  Anchor every terminal
            # section by its inner edge, rather than centring it on the cloth
            # cut and placing half of the leather underneath the playing felt.
            # At t=1 this aligns the 80 mm terminal with the 80 mm rail; at the
            # throat it leaves only the real 4 mm facing outside the opening.
            center_x -= inward_cross_sign * half_width
        # Keep the jaw nose at rail height, then fold its rear half below the
        # cloth plane so it reads as a continuous facing entering the pocket,
        # rather than a flat leather wing spread across the apron.
        lower = ((-0.018 + 0.008 * t * t)
                 if compact_throat_facing else
                 (-0.035 + 0.025 * t * t))
        upper = -0.006 + (NOSE_HEIGHT_M + 0.006) * t * t
        rings.append((
            (center_x - half_width, lower, length * t),
            (center_x + half_width, lower, length * t),
            (center_x + half_width, upper, length * t),
            (center_x - half_width, upper, length * t),
        ))
    if smooth:
        if surface not in ("full", "jaw", "jaw_solid", "facing"):
            raise ValueError("unsupported smooth terminal surface")
        # Each longitudinal side is one smooth ribbon.  The four ribbons do
        # not share vertices with one another: those cross-section corners are
        # intentional leather folds and must remain hard.  End caps are also
        # deliberately split from the ribbons.
        ribbons = []
        for side in range(4):
            following = (side + 1) % 4
            positions, uvs, indices, tangent_hints = [], [], [], []
            for section, ring in enumerate(rings):
                t = section / sections
                across = tuple(ring[following][axis] - ring[side][axis]
                               for axis in range(3))
                positions.extend((ring[side], ring[following]))
                uvs.extend(((0.0, t), (1.0, t)))
                tangent_hints.extend((across, across))
            for section in range(sections):
                start = section * 2
                indices.extend((start, start + 1, start + 3,
                                start, start + 3, start + 2))
            ribbons.append(_smooth_indexed_geometry(
                positions, uvs, indices, tangent_hints))
        start_cap = _quad_geometry((rings[0][0], rings[0][3],
                                    rings[0][2], rings[0][1]))
        end_cap = _quad_geometry(rings[-1])
        if surface in ("jaw", "jaw_solid"):
            if inward_cross_sign is None:
                raise ValueError("split jaw needs a canonical inner edge")
            # The cloth-facing nose is the actual cushion continuation.  The
            # former full-width top ribbon swept from 14 to 80 mm and rendered
            # as a detached triangular wing over the apron.
            nose_side = 1 if inward_cross_sign > 0.0 else 3
            if surface == "jaw":
                return ribbons[nose_side]

            # Give the nose a real, closed cross-section without restoring
            # the rejected 14->80 mm top wing.  The rear surface grows from
            # zero thickness at the shared mouth edge to 2 mm under the rail;
            # two tapered side strips and the hidden rail-end cap close it.
            # There is deliberately no independent mouth cap.
            following = (nose_side + 1) % 4
            front_pairs = [(ring[nose_side], ring[following])
                           for ring in rings]
            back_pairs = []
            for section, pair in enumerate(front_pairs):
                phase = _smoothstep01(section / sections)
                offset = (-inward_cross_sign *
                          POCKET_RECESSED_JAW_THICKNESS_M * phase)
                back_pairs.append(tuple(
                    ((point[0] + offset), point[1], point[2])
                    for point in pair))

            back_positions, back_uvs, back_indices, back_tangents = [], [], [], []
            for section, pair in enumerate(back_pairs):
                t = section / sections
                across = tuple(pair[1][axis] - pair[0][axis]
                               for axis in range(3))
                back_positions.extend(pair)
                back_uvs.extend(((0.0, t), (1.0, t)))
                back_tangents.extend((across, across))
            for section in range(sections):
                start = section * 2
                back_indices.extend((start, start + 3, start + 1,
                                     start, start + 2, start + 3))
            back = _smooth_indexed_geometry(
                back_positions, back_uvs, back_indices, back_tangents)

            edge_parts = []
            for edge in range(2):
                # The zero-thickness mouth terminates in a triangle that is
                # part of the longitudinal shell, not a separate throat cap.
                if edge == 0:
                    edge_parts.append(_triangle_geometry((
                        front_pairs[0][edge],
                        back_pairs[1][edge],
                        front_pairs[1][edge])))
                else:
                    edge_parts.append(_triangle_geometry((
                        front_pairs[0][edge],
                        front_pairs[1][edge],
                        back_pairs[1][edge])))
                for section in range(1, sections):
                    if edge == 0:
                        corners = (front_pairs[section][edge],
                                   back_pairs[section][edge],
                                   back_pairs[section + 1][edge],
                                   front_pairs[section + 1][edge])
                    else:
                        corners = (front_pairs[section][edge],
                                   front_pairs[section + 1][edge],
                                   back_pairs[section + 1][edge],
                                   back_pairs[section][edge])
                    edge_parts.append(_quad_geometry(corners))
            end_cap = _quad_geometry((
                front_pairs[-1][0], front_pairs[-1][1],
                back_pairs[-1][1], back_pairs[-1][0]))
            return merge_geometries(
                (ribbons[nose_side], back, *edge_parts, end_cap))
        if surface == "facing":
            # Pocket facing leather belongs only on the cut end of the jaw,
            # not over the jaw's broad top and outside surfaces.
            return start_cap
        return merge_geometries((*ribbons, start_cap, end_cap))

    if surface != "full":
        raise ValueError("split terminal surfaces require smooth topology")

    faces = []
    for section in range(sections):
        for side in range(4):
            faces.append((section * 4 + side,
                          section * 4 + (side + 1) % 4,
                          (section + 1) * 4 + (side + 1) % 4,
                          (section + 1) * 4 + side))
    faces.extend(((0, 3, 2, 1),
                  (sections * 4, sections * 4 + 1,
                   sections * 4 + 2, sections * 4 + 3)))
    corners = tuple(point for ring in rings for point in ring)
    positions, normals, tangents, uvs, indices = [], [], [], [], []
    for face in faces:
        start = len(positions)
        quad = [corners[index] for index in face]
        edge_u = tuple(quad[1][axis] - quad[0][axis] for axis in range(3))
        edge_v = tuple(quad[3][axis] - quad[0][axis] for axis in range(3))
        normal = _normalise((edge_u[1] * edge_v[2] - edge_u[2] * edge_v[1],
                             edge_u[2] * edge_v[0] - edge_u[0] * edge_v[2],
                             edge_u[0] * edge_v[1] - edge_u[1] * edge_v[0]))
        tangent = _orthogonal_tangent(normal, edge_u)
        positions.extend(quad); normals.extend([normal] * 4); tangents.extend([tangent] * 4)
        uvs.extend(((0, 0), (1, 0), (1, 1), (0, 1)))
        indices.extend((start, start + 1, start + 2, start, start + 2, start + 3))
    return positions, normals, tangents, uvs, indices


def build_materials():
    materials = []
    pbr_texture_indices = {key: index + 1 for index, key in enumerate(PBR_TEXTURE_KEYS)}
    repeats = {spec[0]: spec[6] for spec in TEXTURE_SPECS}

    def texture_info(texture_set, channel):
        return {"index": pbr_texture_indices[(texture_set, channel)], "texCoord": 0,
                "extensions": {"KHR_texture_transform": {
                    "offset": [0.0, 0.0], "scale": repeats[texture_set]}}}

    for name, color, roughness, metallic, coat in MATERIAL_DEFINITIONS:
        material = {"name": name, "pbrMetallicRoughness": {
            "baseColorFactor": color, "roughnessFactor": roughness,
            "metallicFactor": metallic}, "doubleSided": False,
            "extras": {"base_color_factor_space": "linear_srgb_d65",
                       "fallback_workflow": "gltf2_metallic_roughness"}}
        if coat:
            material["extensions"] = {"KHR_materials_clearcoat": {
                "clearcoatFactor": coat[0], "clearcoatRoughnessFactor": coat[1]}}
            material["extras"]["clearcoat_fallback"] = "energy_conserving_specular_lobe"
        if name == "MAT_lamp_diffuser":
            material["emissiveFactor"] = [1.0, 0.79, 0.59]
            material["extras"]["emissive_strength_target"] = 8.0
        texture_set = MATERIAL_TEXTURE_SETS.get(name)
        if texture_set:
            material["extras"]["untextured_fallback_factors"] = {
                "baseColorFactor": color,
                "roughnessFactor": roughness,
                "metallicFactor": metallic,
            }
            # pocket_void is a neutral-white utility texture whose only
            # authored variation is ORM occlusion.  Retain the near-black
            # material factor so the texture cannot turn the cavity white.
            material["pbrMetallicRoughness"]["baseColorFactor"] = \
                (color if name == "MAT_pocket_void_depth" else
                 [1.0, 1.0, 1.0, 1.0])
            material["pbrMetallicRoughness"]["roughnessFactor"] = 1.0
            material["pbrMetallicRoughness"]["metallicFactor"] = 1.0
            material["pbrMetallicRoughness"]["baseColorTexture"] = \
                texture_info(texture_set, "basecolor")
            material["pbrMetallicRoughness"]["metallicRoughnessTexture"] = \
                texture_info(texture_set, "orm")
            material["normalTexture"] = texture_info(texture_set, "normal")
            material["normalTexture"]["scale"] = 1.0
            material["occlusionTexture"] = texture_info(texture_set, "orm")
            material["occlusionTexture"]["strength"] = 1.0
            material["extras"]["pbr_texture_set"] = texture_set
            material["extras"]["normal_convention"] = "opengl_positive_y"
            material["extras"]["orm_channels"] = "r_ao_g_roughness_b_metallic"
        materials.append(material)
    atlas_image = 0
    atlas_texture = 0
    for identity in range(16):
        column, row = identity % 8, identity // 8
        gutter_x, gutter_y = BALL_ATLAS_INSET_PX / ATLAS_WIDTH, BALL_ATLAS_INSET_PX / ATLAS_HEIGHT
        offset = [column / 8 + gutter_x, row / 2 + gutter_y]
        scale = [1 / 8 - 2 * gutter_x, 1 / 2 - 2 * gutter_y]
        materials.append({
            "name": "MAT_ball_identity_%02d" % identity,
            "pbrMetallicRoughness": {
                "baseColorFactor": [1, 1, 1, 1], "roughnessFactor": 0.12,
                "metallicFactor": 0.0,
                "baseColorTexture": {"index": atlas_texture, "texCoord": 0,
                    "extensions": {"KHR_texture_transform": {
                        "offset": offset, "scale": scale}}}},
            "extensions": {"KHR_materials_clearcoat": {
                "clearcoatFactor": 0.9, "clearcoatRoughnessFactor": 0.06}},
            "extras": {"ball_identity": identity,
                       "base_color_texture_encoding": "srgb_iec61966_2_1",
                       "clearcoat_fallback": "energy_conserving_specular_lobe",
                       "atlas_image": atlas_image}})
    return materials


def build_gltf(smooth_pocket_candidate=False,
               split_pocket_facing_candidate=False,
               outward_ball_winding=True,
               recessed_funnel_candidate=False,
               recessed_wall_material="void"):
    if split_pocket_facing_candidate and not smooth_pocket_candidate:
        raise ValueError("split pocket facing requires smooth topology")
    recessed_wall_materials = {
        "void": "MAT_pocket_void",
        "liner": "MAT_pocket_liner",
        "leather": "MAT_pocket_leather",
    }
    if recessed_wall_material not in recessed_wall_materials:
        raise ValueError("unsupported recessed pocket wall material")
    if recessed_wall_material != "void" and not recessed_funnel_candidate:
        raise ValueError("recessed wall material requires recessed funnel candidate")
    selected_void_material = recessed_wall_materials[recessed_wall_material]
    builder = Builder()
    materials = build_materials()
    material = {entry["name"]: index for index, entry in enumerate(materials)}
    marker_nodes = []
    scene_nodes = []

    def marker(name, translation, rotation=None, extras=None):
        index = builder.node(name, translation=translation, rotation=rotation,
                             extras={"alignment_marker": True, **(extras or {})})
        marker_nodes.append(index)
        scene_nodes.append(index)

    marker("SOCKET_play_surface_origin", (0, PLAY_SURFACE_Y_M, 0), extras={"kind": "surface"})
    for name, position, inward in (
        ("SOCKET_rail_nose_x_neg", (-HALF_WIDTH_M, PLAY_SURFACE_Y_M + NOSE_HEIGHT_M, 0), (1, 0)),
        ("SOCKET_rail_nose_x_pos", (HALF_WIDTH_M, PLAY_SURFACE_Y_M + NOSE_HEIGHT_M, 0), (-1, 0)),
        ("SOCKET_rail_nose_z_neg", (0, PLAY_SURFACE_Y_M + NOSE_HEIGHT_M, -HALF_LENGTH_M), (0, 1)),
        ("SOCKET_rail_nose_z_pos", (0, PLAY_SURFACE_Y_M + NOSE_HEIGHT_M, HALF_LENGTH_M), (0, -1))):
        marker(name, position, yaw_quaternion_for_local_z(inward), {"kind": "rail_nose"})
    for pocket_id, name, _, center, inward, tangent, width in POCKETS:
        outward = (-inward[0], -inward[1])
        marker("SOCKET_pocket_%d_mouth" % pocket_id,
               (center[0], PLAY_SURFACE_Y_M, center[1]),
               yaw_quaternion_for_local_z(outward),
               {"kind": "pocket_mouth", "pocket_id": pocket_id,
                "local_positive_x": "tangent", "local_positive_z": "outward_depth"})
        for suffix, sign in (("neg_t", -1), ("pos_t", 1)):
            marker("SOCKET_pocket_%d_mouth_%s" % (pocket_id, suffix),
                   (center[0] + tangent[0] * sign * width * 0.5, PLAY_SURFACE_Y_M,
                    center[1] + tangent[1] * sign * width * 0.5),
                   extras={"kind": "pocket_mouth_endpoint", "pocket_id": pocket_id})

    lod_sets = {"table": {}, "ball": {}, "cue": {}, "lamp": {}}
    triangle_stats = {}

    def add(name, geometry, material_name, role, lod, translation=None, extras=None):
        mesh = builder.mesh("MESH_" + name, geometry, material[material_name], role, lod)
        node = builder.node("NODE_" + name, mesh=mesh, translation=translation,
                            extras={"lod": lod, "asset_role": role, **(extras or {})})
        triangle_stats[name] = builder.meshes[mesh]["extras"]["triangle_count"]
        return node

    for lod, grid, rounded in ((0, (32, 64), True), (1, (16, 32), False), (2, (8, 16), False)):
        children = []
        facing_parts = []
        facing_ranges = []
        facing_vertex_start = 0
        jaw_parts = []
        jaw_ranges = []
        jaw_vertex_start = 0
        apron_parts, apron_ranges = [], []
        body_geometry = (table_body_with_pocket_openings(
                            rounded, 24 if lod == 0 else (16 if lod == 1 else 10),
                            recessed_funnel_candidate=recessed_funnel_candidate)
                         if split_pocket_facing_candidate else
                         ((beveled_box if rounded else box)(
                              (0, 0.39, 0), (1.55, 0.78, 2.82), 0.025)
                          if rounded else
                          box((0, 0.39, 0), (1.55, 0.78, 2.82))))
        children.append(add(
            "table_body_LOD%d" % lod, body_geometry, "MAT_table_walnut",
            "table_body", lod,
            extras=({"pocket_body_aperture":
                         "carved_leather_u_outer_edge_v1"}
                    if recessed_funnel_candidate else None)))
        children.append(add(
            "table_felt_LOD%d" % lod,
            felt_grid(*grid,
                      physics_aligned_pockets=split_pocket_facing_candidate),
            "MAT_table_felt_teal", "table_felt", lod))
        apron_vertex_start = 0
        for pocket_id, _, pocket_type, center, inward, tangent, width in POCKETS:
            if split_pocket_facing_candidate:
                continue
            local_apron = mouth_apron_wedge(
                width, open_top=split_pocket_facing_candidate)
            if max(position[2] for position in local_apron[0]) > 1e-6:
                raise ValueError("pocket apron crosses inward of mouth plane")
            apron_parts.append(transform_geometry(
                local_apron, (center[0], PLAY_SURFACE_Y_M, center[1]),
                (tangent[0], 0, tangent[1]), (inward[0], 0, inward[1])))
            apron_ranges.append({"pocket_id": pocket_id,
                                  "vertex_start": apron_vertex_start,
                                  "vertex_count": len(local_apron[0])})
            apron_vertex_start += len(local_apron[0])
        if apron_parts:
            children.append(add("pocket_apron_wedge_assembly_LOD%d" % lod,
                                merge_geometries(apron_parts), "MAT_table_walnut",
                                "pocket_apron_integrated_wedge", lod,
                                extras={"mouth_plane_profile":
                                            "connected_cavity_v10c",
                                        "halfspace": "outward_or_plane",
                                        "local_ranges": apron_ranges,
                                        "body_contiguous": True}))
        rail_y, thickness, height = PLAY_SURFACE_Y_M + 0.035, 0.08, 0.09
        terminal_length = 0.065
        long_length, long_center = 1.11 - 2 * terminal_length, 0.61
        rails = (
            (-HALF_WIDTH_M - 0.04, rail_y, -long_center, thickness, height, long_length),
            (-HALF_WIDTH_M - 0.04, rail_y, long_center, thickness, height, long_length),
            (HALF_WIDTH_M + 0.04, rail_y, -long_center, thickness, height, long_length),
            (HALF_WIDTH_M + 0.04, rail_y, long_center, thickness, height, long_length),
            (0, rail_y, -HALF_LENGTH_M - 0.04, 1.06 - 2 * terminal_length, height, thickness),
            (0, rail_y, HALF_LENGTH_M + 0.04, 1.06 - 2 * terminal_length, height, thickness))
        for index, values in enumerate(rails):
            center, size = values[:3], values[3:]
            geometry = beveled_box(center, size, 0.008) if rounded else box(center, size)
            children.append(add("table_rail_%d_LOD%d" % (index, lod), geometry,
                                "MAT_table_cushion_rubber", "cushion_segment", lod))
        if lod < 2:
            for pocket_id, _, pocket_type, center, inward, tangent, width in POCKETS:
                for sign in (-1, 1):
                    if pocket_type == "corner":
                        railward = _normalise((tangent[0] * sign + inward[0],
                                               0.0,
                                               tangent[1] * sign + inward[1]))
                    else:
                        railward = (tangent[0] * sign, 0.0, tangent[1] * sign)
                    cross_axis = (-railward[2], 0.0, railward[0])
                    # The physics-aligned candidate terminates at the actual
                    # authored rail gap.  The former 140/70 mm lengths made
                    # the widening shell overshoot that gap and read as a
                    # broad triangular wing around both pocket types.
                    length = ((0.123 if pocket_type == "corner" else 0.076)
                              if split_pocket_facing_candidate else
                              (0.140 if pocket_type == "corner" else 0.070))
                    rail_half_width = 0.040
                    hidden_center = center
                    guard_offset = (
                        width * 0.5
                        if recessed_funnel_candidate else
                        (width * 0.5 + POCKET_VISUAL_SOLID_CLEARANCE_M
                         if split_pocket_facing_candidate else width * 0.56))
                    guard_center = (hidden_center[0] + tangent[0] * sign *
                                    guard_offset,
                                    PLAY_SURFACE_Y_M,
                                     hidden_center[1] + tangent[1] * sign *
                                    guard_offset)
                    lateral_curve = 0.020 * (inward[0] * cross_axis[0] +
                                             inward[1] * cross_axis[2])
                    inward_cross_sign = 1.0 if lateral_curve > 0.0 else -1.0
                    # Keep the playable mouth chord untouched while giving
                    # the visual nose a finite liner-width end.  A 4 mm tip
                    # read as two detached black spikes at close range.
                    throat_half_width = (POCKET_LINER_WIDTH_M
                                         if split_pocket_facing_candidate
                                         else 0.002)
                    terminal_arguments = (
                        length, rail_half_width, throat_half_width,
                        0.0 if smooth_pocket_candidate else lateral_curve,
                    )
                    terminal_keywords = {
                        "smooth": smooth_pocket_candidate,
                        "inward_cross_sign": (inward_cross_sign
                                              if smooth_pocket_candidate else None),
                        "compact_throat_facing":
                            recessed_funnel_candidate,
                    }
                    if split_pocket_facing_candidate:
                        jaw = curved_leather_terminal(
                            *terminal_arguments, **terminal_keywords,
                            surface=("jaw_solid" if recessed_funnel_candidate
                                     else "jaw"),
                            width_profile="quadratic")
                        local = (None if recessed_funnel_candidate else
                                 curved_leather_terminal(
                                     *terminal_arguments, **terminal_keywords,
                                     surface="facing", width_profile="quadratic"))
                        jaw_parts.append(transform_geometry(
                            jaw, guard_center, cross_axis, railward,
                            preserve_winding=recessed_funnel_candidate))
                        jaw_ranges.append({
                            "pocket_id": pocket_id,
                            "terminal_sign": sign,
                            "vertex_start": jaw_vertex_start,
                            "vertex_count": len(jaw[0]),
                            "inward_cross_sign": inward_cross_sign,
                            "cloth_halfspace": "outward_or_boundary",
                            **({"compact_throat_facing": True}
                               if recessed_funnel_candidate else {}),
                            **({"mouth_endpoint_welded_to_liner_lip": True}
                               if recessed_funnel_candidate else {}),
                            **({"tapered_closed_cross_section": True,
                                "rail_end_thickness_m":
                                    POCKET_RECESSED_JAW_THICKNESS_M,
                                "independent_mouth_cap": False}
                               if recessed_funnel_candidate else {}),
                        })
                        jaw_vertex_start += len(jaw[0])
                    else:
                        local = curved_leather_terminal(
                            *terminal_arguments, **terminal_keywords)
                    if local is not None:
                        geometry = transform_geometry(
                            local, guard_center, cross_axis, railward)
                        facing_parts.append(geometry)
                        if smooth_pocket_candidate:
                            facing_ranges.append({
                                "pocket_id": pocket_id,
                                "terminal_sign": sign,
                                "vertex_start": facing_vertex_start,
                                "vertex_count": len(local[0]),
                                "inward_cross_sign": inward_cross_sign,
                                "cloth_halfspace": "outward_or_boundary",
                            })
                        facing_vertex_start += len(local[0])
        if jaw_parts:
            children.append(add(
                "pocket_cushion_jaw_assembly_LOD%d" % lod,
                merge_geometries(jaw_parts), "MAT_table_cushion_rubber",
                "pocket_cushion_jaw_visual_only", lod,
                extras={"no_visual_solid_checked": True,
                        "rail_top_to_throat_section": True,
                        "surface_scope":
                            "inner_nose_without_terminal_caps",
                        "width_profile": "quadratic_throat_to_rail_gap",
                        "local_ranges": jaw_ranges,
                        "cloth_halfspace": "outward_or_boundary"}))
        if facing_parts:
            children.append(add("pocket_leather_facing_assembly_LOD%d" % lod,
                                merge_geometries(facing_parts), "MAT_pocket_leather",
                                "pocket_facing_visual_only", lod,
                                extras={"no_visual_solid_checked": True,
                                        "rail_top_to_throat_section": True,
                                        "material_stage": "facing",
                                        **({"surface_scope": "throat_cap_only"}
                                           if split_pocket_facing_candidate else {}),
                                        **({"local_ranges": facing_ranges,
                                            "cloth_halfspace":
                                                "outward_or_boundary",
                                            **({"compact_throat_facing": True}
                                               if recessed_funnel_candidate else {})}
                                           if smooth_pocket_candidate else {})}))
        segment_count = 24 if lod == 0 else (16 if lod == 1 else 10)
        void_parts, void_bottom_parts, rim_parts, sidewall_parts = [], [], [], []
        inner_parts, inner_ranges = [], []
        inner_vertex_start = 0
        void_ranges, void_bottom_ranges, rim_ranges, sidewall_ranges = [], [], [], []
        void_vertex_start = void_bottom_vertex_start = 0
        rim_vertex_start = sidewall_vertex_start = 0
        for pocket_id, _, pocket_type, center, inward, tangent, width in POCKETS:
            basis_x = (tangent[0], 0, tangent[1])
            if split_pocket_facing_candidate:
                basis_z = (-inward[0], 0, -inward[1])
                local_rim, local_sidewall, local_void, rim_inner, \
                    cavity_upper, drop_boundary, cloth_seam, mouth = \
                    profiled_physics_pocket_cavity(
                        width, segment_count, pocket_type=pocket_type,
                        recessed_funnel_candidate=recessed_funnel_candidate)
                minimum_cloth_depth = min(point[1] for point in cloth_seam)
                minimum_visual_depth = (
                    min(point[1] for point in _pocket_body_opening_seam(
                        width, pocket_type, segment_count,
                        recessed_funnel_candidate=True))
                    if recessed_funnel_candidate else minimum_cloth_depth)
                maximum_visual_depth = (POCKET_PHYSICS_THROAT_DEPTH_M +
                                        width * 0.25)
                if recessed_funnel_candidate:
                    maximum_visual_depth = max(
                        maximum_visual_depth,
                        POCKET_PHYSICS_THROAT_DEPTH_M +
                        BALL_DIAMETER_M * 0.5 +
                        POCKET_VISUAL_SOLID_CLEARANCE_M)
                sidewall_minimum_depth = (minimum_visual_depth
                                         if recessed_funnel_candidate else 0.0)
                if min(position[2] for position in local_sidewall[0]) < sidewall_minimum_depth - 1e-6 or \
                        min(position[2] for position in local_void[0]) < \
                        minimum_visual_depth - 1e-6 or \
                        max(position[2] for position in local_sidewall[0]) > \
                        maximum_visual_depth + (0.005 if recessed_funnel_candidate else 0.0) + 1e-6 or \
                        max(position[2] for position in local_void[0]) > \
                        maximum_visual_depth + 1e-6:
                    raise ValueError("visual pocket shell exceeds its rounded post-capture back")
                local_void_bottom = _filter_geometry_triangles(
                    local_void,
                    lambda triangle, _positions, normals:
                    all(normals[index][1] > 0.999999 for index in triangle))
                local_void = _filter_geometry_triangles(
                    local_void,
                    lambda triangle, _positions, normals:
                    not all(normals[index][1] > 0.999999 for index in triangle))
                if recessed_funnel_candidate:
                    local_inner = _filter_geometry_triangles(
                        local_void, lambda triangle, positions, _normals:
                        max(positions[index][1] for index in triangle) >
                        POCKET_RECESSED_DEEP_RING_Y_M + 1e-9)
                    local_void = _filter_geometry_triangles(
                        local_void, lambda triangle, positions, _normals:
                        max(positions[index][1] for index in triangle) <=
                        POCKET_RECESSED_DEEP_RING_Y_M + 1e-9)
                    inner_parts.append(transform_geometry(
                        local_inner, (center[0], PLAY_SURFACE_Y_M, center[1]),
                        basis_x, basis_z, preserve_winding=True))
                    inner_ranges.append({"pocket_id": pocket_id,
                        "vertex_start": inner_vertex_start,
                        "vertex_count": len(local_inner[0])})
                    inner_vertex_start += len(local_inner[0])
            else:
                basis_z = (inward[0], 0, inward[1])
                local_rim, local_sidewall, local_void, rim_inner, drop_boundary = \
                    profiled_pocket_v10c(
                        width, segment_count, smooth=smooth_pocket_candidate,
                        pocket_type=pocket_type)
                cavity_upper = rim_inner
                cloth_seam = None
                if min(position[2] for position in local_void[0]) < -1e-6:
                    raise ValueError("pocket bottom crosses outward of mouth plane")
                if min(position[2] for position in local_rim[0]) < -1e-6 or \
                        min(position[2] for position in local_sidewall[0]) < -1e-6:
                    raise ValueError("pocket liner crosses outward of mouth plane")
                local_void_bottom = None
            void_parts.append(transform_geometry(
                local_void, (center[0], PLAY_SURFACE_Y_M, center[1]),
                basis_x, basis_z,
                preserve_winding=recessed_funnel_candidate))
            if local_void_bottom is not None:
                void_bottom_parts.append(transform_geometry(
                    local_void_bottom,
                    (center[0], PLAY_SURFACE_Y_M, center[1]),
                    basis_x, basis_z,
                    preserve_winding=recessed_funnel_candidate))
            rim_parts.append(transform_geometry(
                local_rim, (center[0], PLAY_SURFACE_Y_M, center[1]),
                basis_x, basis_z,
                preserve_winding=recessed_funnel_candidate))
            sidewall_parts.append(transform_geometry(
                local_sidewall, (center[0], PLAY_SURFACE_Y_M, center[1]),
                basis_x, basis_z,
                preserve_winding=recessed_funnel_candidate))
            void_ranges.append({"pocket_id": pocket_id,
                                "vertex_start": void_vertex_start,
                                "vertex_count": len(local_void[0]),
                                "boundary_vertex_count": len(drop_boundary),
                                "boundary": drop_boundary,
                                **({"mouth_width_binding": "exact",
                                    "throat_width_m": width,
                                    "post_capture_rounding_depth_m": width * 0.25,
                                    **({"central_closure":
                                           "descending_open_u_bowl_with_tapered_body_undercut",
                                        "cavity_contraction_ratio":
                                           POCKET_CLOSED_BOWL_CONTRACTION,
                                        "contraction_scope":
                                           "below_full_ball_sweep_only",
                                        "under_channel_closure":
                                           "closed_funnel_ring_below_full_ball_sweep_v1",
                                        "minimum_corridor_clearance_m":
                                           BALL_DIAMETER_M +
                                           POCKET_VISUAL_SOLID_CLEARANCE_M,
                                        "mouth_front_cap": False,
                                        "capture_lip_y_m":
                                           POCKET_SHOULDER_TOP_Y_M - PLAY_SURFACE_Y_M,
                                        "post_capture_lip_y_m":
                                           POCKET_RECESSED_BACK_LIP_Y_M,
                                        "lip_descent_depth_m":
                                           POCKET_RECESSED_LIP_DESCENT_M}
                                       if recessed_funnel_candidate else {})}
                                   if split_pocket_facing_candidate else {})})
            if local_void_bottom is not None:
                void_bottom_ranges.append({
                    "pocket_id": pocket_id,
                    "vertex_start": void_bottom_vertex_start,
                    "vertex_count": len(local_void_bottom[0]),
                    "mouth_width_binding": "exact",
                    "throat_width_m": width,
                    "post_capture_rounding_depth_m": width * 0.25,
                    **({"bottom_footprint_binding":
                            "contracted_deep_bag_ring_v1"}
                       if recessed_funnel_candidate else {}),
                    "uv_depth_mapping": "constant_deep_v1",
                })
            rim_range = {"pocket_id": pocket_id,
                         "vertex_start": rim_vertex_start,
                         "vertex_count": len(local_rim[0]),
                         "inner_boundary": rim_inner}
            if smooth_pocket_candidate:
                rim_range["outer_boundary"] = (
                    [(point[0], point[2])
                     for point in local_rim[0][segment_count + 1:2*(segment_count + 1)]]
                    if recessed_funnel_candidate else
                    cavity_upper if split_pocket_facing_candidate else
                    _pocket_opening_boundary(
                        width, pocket_type, segment_count))
                rim_range["cloth_opening_seam"] = True
                if split_pocket_facing_candidate:
                    rim_range["cloth_boundary"] = cloth_seam
                    rim_range["mouth_chord"] = mouth
                    if recessed_funnel_candidate:
                        rim_range.update({
                            "three_dimensional_descending_lip": True,
                            "mouth_outer_y_m": -0.006,
                            "mouth_inner_y_m": -0.018,
                            "capture_outer_y_m":
                                POCKET_SHOULDER_TOP_Y_M - PLAY_SURFACE_Y_M,
                            "post_capture_outer_y_m":
                                POCKET_RECESSED_BACK_LIP_Y_M,
                            "lip_descent_depth_m":
                                POCKET_RECESSED_LIP_DESCENT_M,
                            "jaw_endpoint_welded": True,
                        })
            rim_ranges.append(rim_range)
            sidewall_ranges.append({
                "pocket_id": pocket_id,
                "vertex_start": sidewall_vertex_start,
                "vertex_count": len(local_sidewall[0]),
                "upper_boundary": rim_inner,
                "lower_boundary": drop_boundary,
                **({"capture_lower_y_m":
                        POCKET_RECESSED_CAPTURE_WALL_Y_M,
                    "post_capture_lower_y_m":
                        POCKET_RECESSED_BACK_WALL_Y_M,
                    "open_mouth": True}
                   if recessed_funnel_candidate else {})})
            void_vertex_start += len(local_void[0])
            if local_void_bottom is not None:
                void_bottom_vertex_start += len(local_void_bottom[0])
            rim_vertex_start += len(local_rim[0])
            sidewall_vertex_start += len(local_sidewall[0])
        children.append(add("pocket_void_assembly_LOD%d" % lod, merge_geometries(void_parts),
                            selected_void_material, "pocket_void_visual_only", lod,
                            extras={"visual_only_non_solid": True,
                                    "mouth_only_assembly": True,
                                    "mouth_plane_profile": (
                                        "physics_outward_cavity_v1"
                                        if split_pocket_facing_candidate else
                                        "connected_cavity_v10c"),
                                    "halfspace": (
                                        "cloth_seam_to_outward_throat"
                                        if split_pocket_facing_candidate else
                                        "inward_or_plane"),
                                    "local_ranges": void_ranges,
                                    **({"throat_depth_m":
                                            POCKET_PHYSICS_THROAT_DEPTH_M,
                                        "vertical_capture_back":
                                            not recessed_funnel_candidate,
                                        "inward_cloth_seam_wall":
                                            not recessed_funnel_candidate,
                                        **({"recessed_open_well": True}
                                           if recessed_funnel_candidate else {}),
                                        **({"candidate_wall_material":
                                                selected_void_material}
                                           if recessed_funnel_candidate else {}),
                                        "void_bottom_drop_m":
                                            (POCKET_RECESSED_BAG_BOTTOM_M
                                             if recessed_funnel_candidate else
                                             POCKET_PHYSICS_VOID_BOTTOM_M)}
                                       if split_pocket_facing_candidate else
                                       {"profile_stations_d": [1.0, 1.6, 2.2],
                                        "depth_span_d": 1.20}),
                                    "rounded_tapered": True,
                                    "pure_black": True}))
        if recessed_funnel_candidate:
            children.append(add("pocket_inner_assembly_LOD%d" % lod,
                merge_geometries(inner_parts), "MAT_pocket_liner",
                "pocket_inner_visual_only", lod,
                extras={"visual_only_non_solid": True,
                        "mouth_plane_profile": "physics_outward_cavity_v1",
                        "halfspace": "cloth_seam_to_outward_throat",
                        "local_ranges": inner_ranges,
                        "normals_face_cavity": True,
                        "lower_y_relative_m": POCKET_RECESSED_DEEP_RING_Y_M}))
        if split_pocket_facing_candidate:
            children.append(add(
                "pocket_void_bottom_assembly_LOD%d" % lod,
                merge_geometries(void_bottom_parts),
                "MAT_pocket_void_depth", "pocket_void_bottom_visual_only", lod,
                extras={"visual_only_non_solid": True,
                        "mouth_only_assembly": True,
                        "mouth_plane_profile": "physics_outward_cavity_v1",
                        "halfspace": "cloth_seam_to_outward_throat",
                        "local_ranges": void_bottom_ranges,
                        "throat_depth_m": POCKET_PHYSICS_THROAT_DEPTH_M,
                        **({"recessed_open_well": True}
                           if recessed_funnel_candidate else {}),
                        "post_capture_only_occlusion": True,
                        "occlusion_texture_set": "pocket_void",
                        "uv_depth_mapping": "constant_deep_v1",
                        "rounded_tapered": True,
                        "pure_black": True}))
        liner_material = ("MAT_pocket_void" if smooth_pocket_candidate else
                          "MAT_pocket_liner")
        rim_material = ("MAT_pocket_leather"
                        if recessed_funnel_candidate else liner_material)
        sidewall_material = ("MAT_pocket_liner"
                             if recessed_funnel_candidate else liner_material)
        children.append(add("pocket_liner_rim_assembly_LOD%d" % lod,
                            merge_geometries(rim_parts), rim_material,
                            "pocket_liner_rim_visual_only", lod,
                            extras={"visual_only_non_solid": True,
                                    "mouth_only_assembly": True,
                                    "shape": "rounded_u_rim",
                                    "mouth_plane_profile": (
                                        "physics_outward_cavity_v1"
                                        if split_pocket_facing_candidate else
                                        "connected_cavity_v10c"),
                                    "halfspace": (
                                        "outward_or_plane"
                                        if split_pocket_facing_candidate else
                                        "inward_or_plane"),
                                    "local_ranges": rim_ranges,
                                    **({"halfspace": "cloth_seam_to_outward_throat"}
                                       if recessed_funnel_candidate else {}),
                                    "arc_segments_per_half": segment_count,
                                    "authored_rim_width_m": POCKET_RIM_WIDTH_M}))
        children.append(add("pocket_liner_sidewall_assembly_LOD%d" % lod,
                            merge_geometries(sidewall_parts), sidewall_material,
                            "pocket_liner_sidewall_visual_only", lod,
                            extras={"visual_only_non_solid": True,
                                    "mouth_only_assembly": True,
                                    "shape": "rounded_tapered_u_sidewall",
                                    "mouth_plane_profile": (
                                        "physics_outward_cavity_v1"
                                        if split_pocket_facing_candidate else
                                        "connected_cavity_v10c"),
                                    "halfspace": (
                                        "outward_or_plane"
                                        if split_pocket_facing_candidate else
                                        "inward_or_plane"),
                                    **({"halfspace": "cloth_seam_to_outward_throat"}
                                       if recessed_funnel_candidate else {}),
                                    "local_ranges": sidewall_ranges,
                                    "arc_segments_per_half": segment_count,
                                    "authored_vertical_drop_m": POCKET_SIDEWALL_DROP_M}))
        root = builder.node("NODE_table_LOD%d" % lod, children=children,
                            extras={"lod_set": "table", "lod": lod,
                                    "alignment_marker_nodes": marker_nodes,
                                    "production_candidate": True})
        lod_sets["table"][str(lod)] = root
        if lod == 0:
            scene_nodes.append(root)

    for lod, columns, rows in ((0, 32, 24), (1, 20, 14), (2, 12, 8)):
        node = add("ball_shared_LOD%d" % lod,
                   sphere(BALL_RADIUS_M, columns, rows, outward_ball_winding),
                   "MAT_ball_identity_00", "shared_ball", lod,
                   translation=(0, PLAY_SURFACE_Y_M + BALL_RADIUS_M, 0.2),
                   extras={"material_variant_set": "ball_identity_00_15",
                           "alignment_marker_nodes": marker_nodes})
        lod_sets["ball"][str(lod)] = node
        if lod == 0:
            scene_nodes.append(node)

    for lod, segments in ((0, 24), (1, 12), (2, 8)):
        shaft = add("cue_shaft_LOD%d" % lod, cylinder_z(0.006, 0.015, 0.012, 1.45, segments),
                    "MAT_cue_shaft", "cue_shaft", lod)
        tip = add("cue_tip_LOD%d" % lod, cylinder_z(0.0062, 0.006, 0, 0.012, segments),
                  "MAT_cue_tip", "cue_tip", lod)
        root = builder.node("NODE_cue_LOD%d" % lod,
                            translation=(0, PLAY_SURFACE_Y_M + BALL_RADIUS_M, 0.233575),
                            children=(tip, shaft), extras={"lod_set": "cue", "lod": lod,
                                                          "origin": "tip_contact_face_center",
                                                          "forward_axis": "+Z",
                                                          "alignment_marker_nodes": marker_nodes})
        lod_sets["cue"][str(lod)] = root
        if lod == 0:
            scene_nodes.append(root)

    lamp_y = PLAY_SURFACE_Y_M + 1.35
    for lod in range(3):
        housing_geometry = beveled_box((0, 0.035, 0), (0.65, 0.07, 1.85), 0.015) if lod == 0 else box((0, 0.035, 0), (0.65, 0.07, 1.85))
        housing = add("lamp_housing_LOD%d" % lod, housing_geometry,
                      "MAT_lamp_housing", "lamp_housing", lod)
        children = [housing]
        if lod < 2:
            children.append(add("lamp_diffuser_LOD%d" % lod,
                                box((0, -0.003, 0), (0.61, 0.006, 1.81)),
                                "MAT_lamp_diffuser", "lamp_diffuser", lod))
        root = builder.node("NODE_lamp_LOD%d" % lod, translation=(0, lamp_y, 0),
                            children=children, extras={"lod_set": "lamp", "lod": lod,
                                                       "alignment_marker_nodes": marker_nodes})
        lod_sets["lamp"][str(lod)] = root
        if lod == 0:
            scene_nodes.append(root)

    background = []
    for name, center, size in (
        ("floor", (0, -0.025, 0), (10, 0.05, 10)),
        ("wall_nx", (-5, 2, 0), (0.05, 4, 10)),
        ("wall_px", (5, 2, 0), (0.05, 4, 10)),
        ("wall_nz", (0, 2, -5), (10, 4, 0.05)),
        ("wall_pz", (0, 2, 5), (10, 4, 0.05))):
        background.append(add("background_%s" % name, box(center, size),
                              "MAT_background", "background_shell", 0))
    background_root = builder.node("NODE_background_shell", children=background,
                                   extras={"production_candidate": True,
                                           "remote_furniture_excluded": True})
    scene_nodes.append(background_root)

    binary_uri = "data:application/octet-stream;base64," + base64.b64encode(builder.binary).decode("ascii")
    images = [{"name": "IMG_ball_identity_atlas_v1", "uri": ATLAS_NAME,
               "mimeType": "image/png", "extras": {"encoding": "srgb_iec61966_2_1",
                                                      "original_procedural": True}}]
    textures = [{"name": "T_ball_identity_atlas_v1", "sampler": 0, "source": 0}]
    for texture_set, channel in PBR_TEXTURE_KEYS:
        encoding = {"basecolor": "srgb_iec61966_2_1",
                    "normal": "linear_opengl_positive_y",
                    "orm": "linear_r_ao_g_roughness_b_metallic"}[channel]
        images.append({"name": "IMG_%s_%s_v1" % (texture_set, channel),
                       "uri": texture_filename(texture_set, channel),
                       "mimeType": "image/png",
                       "extras": {"encoding": encoding, "original_procedural": True,
                                  "runtime_texture": True}})
        textures.append({"name": "T_%s_%s_v1" % (texture_set, channel),
                         "sampler": 1, "source": len(images) - 1})
    document = {
        "asset": {"version": "2.0", "generator": "BilliardGL generate_phase2_modular_gltf.py v1",
                  "extras": {"asset_id": "focused_practice_hall_modular_v1",
                             "authorship": "original_procedural_billiardgl",
                             "external_source_assets": False,
                             "production_candidate": True,
                             "final_high_mesh": False,
                             "unit": "metre", "up_axis": "+Y", "forward_axis": "+Z",
                             "handedness": "right", "physics_boundary": "legacy_opening_band",
                             "pocket_visual_profile": (
                                 "physics_outward_cavity_v1"
                                 if split_pocket_facing_candidate else
                                 "connected_cavity_v10c"),
                             "pocket_marker_scope": "mouth_only",
                             "pocket_visual_frame": ({
                                 "x": "physics_tangent",
                                 "y": "world_up",
                                 "z": "physics_outward_depth",
                                 "marker_z_mapping": "z_visual=z_marker",
                                 "handedness": "right"}
                                 if split_pocket_facing_candidate else {
                                 "x": "physics_tangent",
                                 "y": "world_up",
                                 "z": "cavity_inward",
                                 "marker_z_mapping": "z_visual=-z_marker",
                                 "handedness": "right"}),
                             "pocket_material_sequence": ([
                                 "MAT_table_cushion_rubber",
                                 "MAT_pocket_leather",
                                 "MAT_pocket_liner",
                                 selected_void_material,
                                 "MAT_pocket_void_depth"]
                                if recessed_funnel_candidate else ([
                                 "MAT_table_cushion_rubber",
                                 "MAT_pocket_leather",
                                 "MAT_pocket_void",
                                 "MAT_pocket_void",
                                 selected_void_material,
                                 "MAT_pocket_void_depth"]
                                if split_pocket_facing_candidate else ([
                                 "MAT_table_cushion_rubber",
                                 "MAT_pocket_leather",
                                 "MAT_pocket_void",
                                 "MAT_pocket_void",
                                 "MAT_pocket_void"] if smooth_pocket_candidate else [
                                 "MAT_table_cushion_rubber",
                                 "MAT_pocket_leather",
                                 "MAT_pocket_liner",
                                 "MAT_pocket_liner",
                                 "MAT_pocket_void"]))),
                             "pocket_v10c_profile": {
                                 "ball_diameter_m": BALL_DIAMETER_M,
                                 "front_z_m": POCKET_FRONT_Z_M,
                                 "center_z_m": POCKET_CENTER_Z_M,
                                 "back_z_m": POCKET_BACK_Z_M,
                                 "minimum_span_m": POCKET_MIN_SPAN_M,
                                 "side_x_margin_m": POCKET_SIDE_X_MARGIN_M,
                                 "z_margin_m": POCKET_Z_MARGIN_M,
                                 "rim_width_m": POCKET_RIM_WIDTH_M,
                                 "sidewall_drop_m": POCKET_SIDEWALL_DROP_M},
                             **({"pocket_physics_cavity_profile": {
                                 "corner_mouth_width_m":
                                     CORNER_MOUTH_WIDTH_M,
                                 "side_mouth_width_m": SIDE_MOUTH_WIDTH_M,
                                 "throat_depth_m":
                                     POCKET_PHYSICS_THROAT_DEPTH_M,
                                 "cavity_drop_m":
                                     POCKET_PHYSICS_CAVITY_DROP_M}}
                                if split_pocket_facing_candidate else {})}},
        "extensionsUsed": ["KHR_materials_clearcoat", "KHR_texture_transform"],
        "scene": 0,
        "scenes": [{"name": "SCENE_focused_practice_hall_modular_v1_LOD0",
                    "nodes": scene_nodes}],
        "nodes": builder.nodes, "meshes": builder.meshes, "materials": materials,
        "samplers": [
            {"magFilter": 9729, "minFilter": 9987, "wrapS": 33071, "wrapT": 33071},
            {"magFilter": 9729, "minFilter": 9987, "wrapS": 10497, "wrapT": 10497}],
        "images": images,
        "textures": textures,
        "accessors": builder.accessors, "bufferViews": builder.views,
        "buffers": [{"byteLength": len(builder.binary), "uri": binary_uri}],
    }
    document["asset"]["extras"]["physics_marker_binding_sha256"] = \
        marker_binding_sha256(document, marker_nodes)
    if outward_ball_winding:
        document["asset"]["extras"]["ball_triangle_winding"] = \
            "outward_ccw_v1"
    if smooth_pocket_candidate:
        document["asset"]["extras"]["pocket_surface_topology"] = {
            "leather_terminal": (
                "jaw_nose_welded_to_liner_lip"
                if recessed_funnel_candidate else
                "throat_cap_only" if split_pocket_facing_candidate else
                "shared_longitudinal_vertices_hard_cross_section_folds"),
            "leather_terminal_alignment": (
                "shared_mouth_endpoint_edge"
                if recessed_funnel_candidate else
                "cut_end_of_cushion_jaw" if split_pocket_facing_candidate else
                "inner_edge_on_cloth_boundary_outer_edge_on_rail"),
            "liner_rim": (
                "open_3d_descending_leather_lip"
                if recessed_funnel_candidate else
                "shared_u_ring_vertices_hard_stage_crease"),
            "liner_sidewall": (
                "open_3d_descending_liner_wall"
                if recessed_funnel_candidate else
                "shared_u_ring_vertices_hard_stage_crease"),
            "liner_finish": (
                "leather_lip_liner_wall_ao_void"
                if recessed_funnel_candidate else
                "existing_pure_black_cavity_material"),
            "liner_opening_seam": "matches_actual_felt_cut_boundary",
        }
        if split_pocket_facing_candidate:
            document["asset"]["extras"]["pocket_surface_topology"][
                "cushion_jaw"] = \
                "shared_inner_nose_no_terminal_caps_quadratic_width"
            document["asset"]["extras"]["pocket_surface_topology"][
                "cavity"] = (
                    "physics_outward_descending_open_funnel_after_6cm_throat"
                    if recessed_funnel_candidate else
                    "physics_outward_vertical_bowl_to_6cm_throat")
            document["asset"]["extras"]["pocket_surface_topology"][
                "apron"] = "table_body_support_without_separate_throat_frame"
    return document, lod_sets, marker_nodes, triangle_stats


def _rgba8_full_mip_bytes(width, height):
    total = 0
    while True:
        total += width * height * 4
        if width == 1 and height == 1:
            return total
        width, height = max(1, width // 2), max(1, height // 2)


def build_manifest(gltf_bytes, atlas_bytes, svg_bytes, ball_closeup_bytes,
                   pbr_outputs, pbr_parameters,
                   document, lod_sets, marker_nodes, triangle_stats):
    mesh_triangles = {mesh["name"]: mesh["extras"]["triangle_count"] for mesh in document["meshes"]}
    lod_triangles = {}
    for set_name, levels in lod_sets.items():
        lod_triangles[set_name] = {}
        for lod, root_index in levels.items():
            pending = [root_index]
            total = 0
            while pending:
                node = document["nodes"][pending.pop()]
                pending.extend(node.get("children", []))
                if "mesh" in node:
                    total += document["meshes"][node["mesh"]]["extras"]["triangle_count"]
            lod_triangles[set_name][lod] = total
    marker_names = [document["nodes"][index]["name"] for index in marker_nodes]
    pbr_texture_files = {
        texture_set: {
            channel: {"path": "assets/modern/phase2/" + texture_filename(texture_set, channel),
                      "sha256": _sha256(pbr_outputs[texture_filename(texture_set, channel)])}
            for channel in ("basecolor", "normal", "orm")}
        for texture_set, *_ in TEXTURE_SPECS
    }
    pbr_disk_bytes = sum(
        len(pbr_outputs[texture_filename(texture_set, channel)])
        for texture_set, *_ in TEXTURE_SPECS
        for channel in ("basecolor", "normal", "orm"))
    pbr_gpu_bytes = sum(
        3 * (4 * ((4 * size * size - 1) // 3))
        for _, size, *_ in TEXTURE_SPECS)
    return {
        "schema_version": 1,
        "asset_id": "focused_practice_hall_modular_v1",
        "status": "phase2_production_candidate_not_final_high_mesh",
        "authorship": "original_procedural_billiardgl",
        "pbr_texture_sampling_contract": {
            "felt_noise_wrap": "toroidal_lattice_v1",
            "mip_wrap_audit": "relative_edge_step_plus_rgba8_quantization_v1",
        },
        "files": {
            "gltf": {"path": "assets/modern/phase2/" + ASSET_NAME, "sha256": _sha256(gltf_bytes)},
            "ball_atlas_png": {"path": "assets/modern/phase2/" + ATLAS_NAME,
                               "sha256": _sha256(atlas_bytes), "width_px": ATLAS_WIDTH,
                               "height_px": ATLAS_HEIGHT, "encoding": "srgb_iec61966_2_1_rgba8"},
            "ball_atlas_vector_source": {"path": "assets/modern/phase2/" + ATLAS_SOURCE_NAME,
                                         "sha256": _sha256(svg_bytes), "format": "svg_original_procedural"},
            "ball_identity_closeup": {"path": "assets/modern/phase2/" + BALL_CLOSEUP_NAME,
                                      "sha256": _sha256(ball_closeup_bytes),
                                      "identities": [1, 8, 11, 15], "runtime": False},
            "pbr_parameters": {"path": "assets/modern/phase2/" + PARAMETERS_NAME,
                               "sha256": _sha256(pbr_outputs[PARAMETERS_NAME])},
            "lighting_rig": {"path": "assets/modern/phase2/" + LIGHTING_RIG_NAME,
                             "sha256": _sha256(pbr_outputs[LIGHTING_RIG_NAME])},
            "material_review_preview": {"path": "assets/modern/phase2/" + PREVIEW_NAME,
                                        "sha256": _sha256(pbr_outputs[PREVIEW_NAME]),
                                        "runtime": False},
            "pbr_textures": pbr_texture_files,
        },
        "alignment_binding": {
            "alignment_id": "focused_practice_hall_m1_alignment_v1",
            "runtime_profile_id": "chinese_pool_interactive_120hz_v5",
            "geometry_id": "legacy_opening_band", "surface_y_cm": 86.483976,
            "nose_height_ratio": 1.4, "pocket_marker_scope": "mouth_only",
            "marker_tolerance_cm": 0.05, "no_visual_solid_clearance_cm": 0.10,
            "capture_presentation": "simulation_driven_no_detach",
            "physics_marker_binding_sha256":
                document["asset"]["extras"]["physics_marker_binding_sha256"],
            "visual_to_marker_frame": {
                "x_visual": "x_marker",
                "y_visual": "y_marker",
                "z_visual": "-z_marker",
                "visual_handedness": "right"}
        },
        "pocket_v10b_contract": {
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
            "manual_visual_status": "PENDING_TA"},
        "lod_sets": {name: {"nodes": levels, "triangles": lod_triangles[name],
                             "alignment_markers": marker_names}
                     for name, levels in lod_sets.items()},
        "ball_material_variants": {
            str(identity): {"material": "MAT_ball_identity_%02d" % identity,
                            "atlas_tile": [identity % 8, identity // 8],
                            "safe_inset_px": BALL_ATLAS_INSET_PX}
            for identity in range(16)},
        "ball_atlas_contract": {
            "tile_size_px": [ATLAS_WIDTH // ATLAS_COLUMNS, ATLAS_HEIGHT // ATLAS_ROWS],
            "safe_inset_px": BALL_ATLAS_INSET_PX,
            "padding": "edge_dilated_identity_base",
            "review_identities": [1, 8, 11, 15],
            "sphere_uv_u_orientation": "decreases_with_positive_longitude",
            "glyph_storage_orientation": "upright_source",
        },
        **({"ball_geometry_contract": {
            "triangle_winding": "outward_ccw_v1",
            "normal_orientation": "outward_radial_v1",
        }} if document["asset"]["extras"].get("ball_triangle_winding") ==
              "outward_ccw_v1" else {}),
        "felt_topology": {
            "surface": "single_manifold_with_boundary",
            "boundary_loops": 1, "pocket_openings": 6,
            "winding": "counter_clockwise_when_viewed_from_positive_y",
            "max_edge_incidence": 2, "nondegenerate_triangles": True,
        },
        "pbr_material_bindings": {
            material_name: {
                "texture_set": texture_set,
                "uv_repeat": pbr_parameters["texture_sets"][texture_set]["uv_repeat"],
                "channels": {"basecolor": "srgb", "normal": "linear_opengl_positive_y",
                             "orm": "linear_r_ao_g_roughness_b_metallic"},
            }
            for material_name, texture_set in MATERIAL_TEXTURE_SETS.items()
        },
        "budgets": {
            "unique_triangles": sum(mesh_triangles.values()),
            "materials_total": len(document["materials"]),
            "functional_material_slots": {"table": 7, "cue": 2, "lamp": 2,
                                           "background": 1, "ball_variants": 16},
            "textures": 1 + len(PBR_TEXTURE_KEYS),
            "atlas_gpu_bytes_rgba8_with_full_mips": _rgba8_full_mip_bytes(ATLAS_WIDTH, ATLAS_HEIGHT),
            "atlas_disk_bytes": len(atlas_bytes),
            "pbr_texture_gpu_bytes_rgba8_with_full_mips": pbr_gpu_bytes,
            "pbr_texture_disk_bytes": pbr_disk_bytes,
            "runtime_texture_gpu_bytes_rgba8_with_full_mips":
                _rgba8_full_mip_bytes(ATLAS_WIDTH, ATLAS_HEIGHT) + pbr_gpu_bytes,
            "runtime_texture_disk_bytes": len(atlas_bytes) + pbr_disk_bytes,
            "review_preview_disk_bytes_excluded_from_runtime": len(pbr_outputs[PREVIEW_NAME]),
            "embedded_geometry_buffer_bytes": document["buffers"][0]["byteLength"]
        },
        "constraints": {
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
        },
        "generator": "scripts/generate_phase2_modular_gltf.py"
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, required=True)
    topology = parser.add_mutually_exclusive_group()
    topology.add_argument("--smooth-pocket-candidate",
                          dest="smooth_pocket_candidate", action="store_true",
                          help="generate the promoted cloth-seam pocket topology")
    topology.add_argument("--legacy-pocket-topology",
                          dest="smooth_pocket_candidate", action="store_false",
                          help="reproduce the pre-promotion centred pocket topology")
    parser.set_defaults(smooth_pocket_candidate=True)
    facing = parser.add_mutually_exclusive_group()
    facing.add_argument("--split-pocket-facing-candidate",
                        dest="split_pocket_facing_candidate",
                        action="store_true",
                        help="generate the promoted physics-aligned split jaw/facing pocket")
    facing.add_argument("--legacy-unsplit-pocket-facing",
                        dest="split_pocket_facing_candidate",
                        action="store_false",
                        help="reproduce the pre-promotion unsplit pocket facing")
    parser.set_defaults(split_pocket_facing_candidate=True)
    parser.add_argument(
        "--legacy-inward-ball-winding", action="store_true",
        help="reproduce the pre-correction inward ball index order")
    parser.add_argument(
        "--recessed-funnel-candidate", action="store_true",
        help="generate the reviewed sloped pocket-cavity closure candidate")
    parser.add_argument(
        "--recessed-wall-material", choices=("void", "liner", "leather"),
        default="void",
        help="select an existing material for the isolated recessed wall A/B")
    arguments = parser.parse_args()
    arguments.output_dir.mkdir(parents=True, exist_ok=True)
    atlas_bytes, svg_bytes = build_ball_atlas()
    ball_closeup_bytes = build_ball_identity_closeup(atlas_bytes)
    pbr_outputs, pbr_parameters, _ = build_package()
    document, lod_sets, marker_nodes, triangle_stats = build_gltf(
        smooth_pocket_candidate=arguments.smooth_pocket_candidate,
        split_pocket_facing_candidate=arguments.split_pocket_facing_candidate,
        outward_ball_winding=not arguments.legacy_inward_ball_winding,
        recessed_funnel_candidate=arguments.recessed_funnel_candidate,
        recessed_wall_material=arguments.recessed_wall_material)
    gltf_bytes = (json.dumps(document, indent=2, sort_keys=True) + "\n").encode("utf-8")
    manifest = build_manifest(gltf_bytes, atlas_bytes, svg_bytes, ball_closeup_bytes, pbr_outputs,
                              pbr_parameters, document, lod_sets, marker_nodes,
                              triangle_stats)
    outputs = {
        ASSET_NAME: gltf_bytes,
        ATLAS_NAME: atlas_bytes,
        ATLAS_SOURCE_NAME: svg_bytes,
        BALL_CLOSEUP_NAME: ball_closeup_bytes,
        MANIFEST_NAME: (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode("utf-8")}
    outputs.update(pbr_outputs)
    for name, payload in outputs.items():
        (arguments.output_dir / name).write_bytes(payload)
    print("generated phase2 modular candidate: triangles=%d materials=%d atlas_disk=%d" %
          (manifest["budgets"]["unique_triangles"], manifest["budgets"]["materials_total"],
           manifest["budgets"]["atlas_disk_bytes"]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
