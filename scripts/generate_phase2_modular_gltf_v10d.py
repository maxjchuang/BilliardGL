#!/usr/bin/env python3
"""Generate the versioned Phase-2 v10d modular glTF package."""

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
from generate_phase2_venue_gltf import (
    ASSET_NAME as VENUE_ASSET_NAME,
    build_gltf as build_venue_gltf,
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

# TA-approved Phase-2 v10d pocket profile.  These are authored visual values;
# signed physics pocket markers remain untouched.  The visual frame maps
# +X=tangent, +Y=world up, +Z=cavity inward, so Z_visual=-Z_marker.
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


MATERIAL_DEFINITIONS = [
    ("MAT_table_felt_teal", [0.0144, 0.1070, 0.1248, 1.0], 0.82, 0.0, None),
    ("MAT_table_cushion_rubber", [0.006, 0.02, 0.022, 1.0], 0.66, 0.0, None),
    ("MAT_table_walnut", [0.0482, 0.0232, 0.0130, 1.0], 0.33, 0.0, (0.42, 0.18)),
    ("MAT_table_gunmetal", [0.0272, 0.0331, 0.0395, 1.0], 0.28, 1.0, None),
    ("MAT_pocket_leather", [0.026, 0.016, 0.011, 1.0], 0.62, 0.0, None),
    ("MAT_pocket_liner", [0.021, 0.025, 0.027, 1.0], 0.76, 0.0, None),
    ("MAT_pocket_void", [0.0005, 0.0005, 0.0006, 1.0], 0.90, 0.0, None),
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
    length = math.sqrt(sum(value * value for value in vector))
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


def transform_geometry(geometry, center, axis_x=(1, 0, 0), axis_z=(0, 0, 1)):
    positions, normals, tangents, uvs, indices = geometry
    axis_y = (0, 1, 0)

    def transform3(value, translate):
        result = tuple(axis_x[i] * value[0] + axis_y[i] * value[1] + axis_z[i] * value[2]
                       for i in range(3))
        return tuple(result[i] + (center[i] if translate else 0.0) for i in range(3))
    transformed_tangents = []
    for tangent in tangents:
        xyz = transform3(tangent[:3], False)
        transformed_tangents.append(xyz + (tangent[3],))
    return ([transform3(value, True) for value in positions],
            [transform3(value, False) for value in normals], transformed_tangents,
            list(uvs), list(indices))


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


def felt_grid(columns, rows):
    """Build a clean six-pocket cloth contour instead of omitting coarse grid cells."""
    del rows
    corner_radius = CORNER_MOUTH_WIDTH_M * 0.56
    side_radius = SIDE_MOUTH_WIDTH_M * 0.56
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


def sphere(radius, columns, rows):
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


def _strip_between_boundaries(first, second, first_y, second_y):
    if len(first) != len(second):
        raise ValueError("pocket boundary cardinality mismatch")
    quads = []
    for index in range(len(first) - 1):
        quads.append(_quad_geometry((
            (first[index][0], first_y, first[index][1]),
            (first[index + 1][0], first_y, first[index + 1][1]),
            (second[index + 1][0], second_y, second[index + 1][1]),
            (second[index][0], second_y, second[index][1]),
        )))
    return merge_geometries(quads)


def profiled_pocket_v10d(width, segments):
    """Connected mouth -> rim -> sidewall -> rounded tapered drop geometry."""
    inner_half = width * 0.5 - POCKET_SIDE_X_MARGIN_M
    lower_half = inner_half - POCKET_RIM_WIDTH_M - 0.001
    if lower_half <= 0.0:
        raise ValueError("pocket rim consumes the mouth")
    rim_inner = _pocket_u_boundary(inner_half, 0.0,
                                   POCKET_BACK_Z_M + POCKET_RIM_WIDTH_M,
                                   segments)
    rim_outer = _pocket_u_boundary(inner_half + POCKET_RIM_WIDTH_M,
                                   0.0,
                                   POCKET_BACK_Z_M + 2.0 * POCKET_RIM_WIDTH_M,
                                   segments)
    drop_boundary = _pocket_u_boundary(lower_half, POCKET_FRONT_Z_M,
                                       POCKET_BACK_Z_M, segments)
    rim_y = -0.004
    drop_y = rim_y - POCKET_SIDEWALL_DROP_M
    rim = _strip_between_boundaries(rim_inner, rim_outer, rim_y, 0.001)
    sidewall = _strip_between_boundaries(drop_boundary, rim_inner,
                                         drop_y, rim_y)
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


def mouth_apron_wedge(width):
    """Walnut apron wedge terminating exactly at local mouth plane Z=0."""
    inner_x, outer_x = width * 0.52, width * 0.74
    inner_z, outer_z = 0.0, -width * 0.70
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
    return merge_geometries((_quad_geometry(top), _quad_geometry(bottom),
                             *(_quad_geometry(side) for side in sides)))


def table_body_with_pocket_apertures():
    """Walnut frame pieces whose gaps are the six physical apertures."""
    height = 0.78
    y = height * 0.5
    side_width = 0.14
    side_gap = 0.24
    corner_gap = 0.22
    side_length = HALF_LENGTH_M - side_gap * 0.5 - corner_gap
    side_center = side_gap * 0.5 + side_length * 0.5
    end_length = 2.0 * (HALF_WIDTH_M - corner_gap * 0.5)
    pieces = []
    for x in (-HALF_WIDTH_M - side_width * 0.5,
              HALF_WIDTH_M + side_width * 0.5):
        for z in (-side_center, side_center):
            pieces.append(box((x, y, z),
                              (side_width, height, side_length)))
    for z in (-HALF_LENGTH_M - side_width * 0.5,
              HALF_LENGTH_M + side_width * 0.5):
        pieces.append(box((0.0, y, z),
                          (end_length, height, side_width)))
    return merge_geometries(pieces)


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


def curved_leather_terminal(length, rail_half_width, throat_half_width,
                            lateral_curve, sections=7):
    """Curved cushion terminal ribbon descending continuously into the throat."""
    rings = []
    for section in range(sections + 1):
        t = section / sections
        center_x = lateral_curve * math.sin(math.pi * t)
        half_width = throat_half_width + (rail_half_width - throat_half_width) * t
        # Keep the jaw nose at rail height, then fold its rear half below the
        # cloth plane so it reads as a continuous facing entering the pocket,
        # rather than a flat leather wing spread across the apron.
        lower = -0.035 + 0.025 * t * t
        upper = -0.006 + (NOSE_HEIGHT_M + 0.006) * t * t
        rings.append((
            (center_x - half_width, lower, length * t),
            (center_x + half_width, lower, length * t),
            (center_x + half_width, upper, length * t),
            (center_x - half_width, upper, length * t),
        ))
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
            material["pbrMetallicRoughness"]["baseColorFactor"] = [1.0, 1.0, 1.0, 1.0]
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


def build_gltf():
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
        apron_parts, apron_ranges = [], []
        body_geometry = table_body_with_pocket_apertures()
        children.append(add("table_body_LOD%d" % lod, body_geometry, "MAT_table_walnut", "table_body", lod))
        children.append(add("table_felt_LOD%d" % lod, felt_grid(*grid), "MAT_table_felt_teal", "table_felt", lod))
        apron_vertex_start = 0
        for pocket_id, _, _, center, inward, tangent, width in POCKETS:
            local_apron = mouth_apron_wedge(width)
            if max(position[2] for position in local_apron[0]) > 1e-6:
                raise ValueError("pocket apron crosses inward of mouth plane")
            apron_parts.append(transform_geometry(
                local_apron, (center[0], PLAY_SURFACE_Y_M, center[1]),
                (tangent[0], 0, tangent[1]), (inward[0], 0, inward[1])))
            apron_ranges.append({"pocket_id": pocket_id,
                                  "vertex_start": apron_vertex_start,
                                  "vertex_count": len(local_apron[0])})
            apron_vertex_start += len(local_apron[0])
        children.append(add("pocket_apron_wedge_assembly_LOD%d" % lod,
                            merge_geometries(apron_parts), "MAT_table_walnut",
                            "pocket_apron_integrated_wedge", lod,
                            extras={"mouth_plane_profile": "connected_cavity_v10d",
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
                    length = 0.140 if pocket_type == "corner" else 0.070
                    rail_half_width = 0.040
                    hidden_center = center
                    guard_center = (hidden_center[0] + tangent[0] * sign * (width * 0.56),
                                    PLAY_SURFACE_Y_M,
                                     hidden_center[1] + tangent[1] * sign * (width * 0.56))
                    lateral_curve = 0.020 * (inward[0] * cross_axis[0] +
                                             inward[1] * cross_axis[2])
                    throat_half_width = 0.002
                    local = curved_leather_terminal(length, rail_half_width,
                                                    throat_half_width,
                                                    lateral_curve)
                    geometry = transform_geometry(local, guard_center, cross_axis,
                                                  railward)
                    facing_parts.append(geometry)
        if facing_parts:
            children.append(add("pocket_leather_facing_assembly_LOD%d" % lod,
                                merge_geometries(facing_parts), "MAT_pocket_leather",
                                "pocket_facing_visual_only", lod,
                                extras={"no_visual_solid_checked": True,
                                        "rail_top_to_throat_section": True,
                                        "material_stage": "facing"}))
        segment_count = 24 if lod == 0 else (16 if lod == 1 else 10)
        void_parts, rim_parts, sidewall_parts = [], [], []
        void_ranges, rim_ranges, sidewall_ranges = [], [], []
        void_vertex_start = rim_vertex_start = sidewall_vertex_start = 0
        for pocket_id, _, _, center, inward, tangent, width in POCKETS:
            basis_x = (tangent[0], 0, tangent[1])
            basis_z = (inward[0], 0, inward[1])
            local_rim, local_sidewall, local_void, rim_inner, drop_boundary = \
                profiled_pocket_v10d(width, segment_count)
            if min(position[2] for position in local_void[0]) < -1e-6:
                raise ValueError("pocket bottom crosses outward of mouth plane")
            if min(position[2] for position in local_rim[0]) < -1e-6 or \
                    min(position[2] for position in local_sidewall[0]) < -1e-6:
                raise ValueError("pocket liner crosses outward of mouth plane")
            void_parts.append(transform_geometry(
                local_void, (center[0], PLAY_SURFACE_Y_M, center[1]),
                basis_x, basis_z))
            rim_parts.append(transform_geometry(
                local_rim, (center[0], PLAY_SURFACE_Y_M, center[1]),
                basis_x, basis_z))
            sidewall_parts.append(transform_geometry(
                local_sidewall, (center[0], PLAY_SURFACE_Y_M, center[1]),
                basis_x, basis_z))
            void_ranges.append({"pocket_id": pocket_id,
                                "vertex_start": void_vertex_start,
                                "vertex_count": len(local_void[0]),
                                "boundary_vertex_count": len(drop_boundary),
                                "boundary": drop_boundary})
            rim_ranges.append({"pocket_id": pocket_id,
                               "vertex_start": rim_vertex_start,
                               "vertex_count": len(local_rim[0]),
                               "inner_boundary": rim_inner})
            sidewall_ranges.append({"pocket_id": pocket_id,
                                    "vertex_start": sidewall_vertex_start,
                                    "vertex_count": len(local_sidewall[0]),
                                    "upper_boundary": rim_inner,
                                    "lower_boundary": drop_boundary})
            void_vertex_start += len(local_void[0])
            rim_vertex_start += len(local_rim[0])
            sidewall_vertex_start += len(local_sidewall[0])
        children.append(add("pocket_void_assembly_LOD%d" % lod, merge_geometries(void_parts),
                            "MAT_pocket_void", "pocket_void_visual_only", lod,
                            extras={"visual_only_non_solid": True,
                                    "mouth_only_assembly": True,
                                    "mouth_plane_profile": "connected_cavity_v10d",
                                    "halfspace": "inward_or_plane",
                                    "local_ranges": void_ranges,
                                    "profile_stations_d": [1.0, 1.6, 2.2],
                                    "depth_span_d": 1.20,
                                    "rounded_tapered": True,
                                    "pure_black": True}))
        children.append(add("pocket_liner_rim_assembly_LOD%d" % lod,
                            merge_geometries(rim_parts), "MAT_pocket_liner",
                            "pocket_liner_rim_visual_only", lod,
                            extras={"visual_only_non_solid": True,
                                    "mouth_only_assembly": True,
                                    "shape": "rounded_u_rim",
                                    "mouth_plane_profile": "connected_cavity_v10d",
                                    "halfspace": "inward_or_plane",
                                    "local_ranges": rim_ranges,
                                    "arc_segments_per_half": segment_count,
                                    "authored_rim_width_m": POCKET_RIM_WIDTH_M}))
        children.append(add("pocket_liner_sidewall_assembly_LOD%d" % lod,
                            merge_geometries(sidewall_parts), "MAT_pocket_liner",
                            "pocket_liner_sidewall_visual_only", lod,
                            extras={"visual_only_non_solid": True,
                                    "mouth_only_assembly": True,
                                    "shape": "rounded_tapered_u_sidewall",
                                    "mouth_plane_profile": "connected_cavity_v10d",
                                    "halfspace": "inward_or_plane",
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
        node = add("ball_shared_LOD%d" % lod, sphere(BALL_RADIUS_M, columns, rows),
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
        "asset": {"version": "2.0", "generator": "BilliardGL generate_phase2_modular_gltf_v10d.py v1",
                  "extras": {"asset_id": "focused_practice_hall_modular_v1",
                             "authorship": "original_procedural_billiardgl",
                             "external_source_assets": False,
                             "production_candidate": True,
                             "final_high_mesh": False,
                             "unit": "metre", "up_axis": "+Y", "forward_axis": "+Z",
                             "handedness": "right", "physics_boundary": "legacy_opening_band",
                             "pocket_visual_profile": "connected_cavity_v10d",
                             "pocket_marker_scope": "mouth_only",
                             "pocket_visual_frame": {
                                 "x": "physics_tangent",
                                 "y": "world_up",
                                 "z": "cavity_inward",
                                 "marker_z_mapping": "z_visual=-z_marker",
                                 "handedness": "right"},
                             "pocket_material_sequence": [
                                 "MAT_table_cushion_rubber",
                                 "MAT_pocket_leather",
                                 "MAT_pocket_liner",
                                 "MAT_pocket_liner",
                                 "MAT_pocket_void"],
                             "pocket_v10d_profile": {
                                 "ball_diameter_m": BALL_DIAMETER_M,
                                 "front_z_m": POCKET_FRONT_Z_M,
                                 "center_z_m": POCKET_CENTER_Z_M,
                                 "back_z_m": POCKET_BACK_Z_M,
                                 "minimum_span_m": POCKET_MIN_SPAN_M,
                                 "side_x_margin_m": POCKET_SIDE_X_MARGIN_M,
                                 "z_margin_m": POCKET_Z_MARGIN_M,
                                 "rim_width_m": POCKET_RIM_WIDTH_M,
                                 "sidewall_drop_m": POCKET_SIDEWALL_DROP_M}}},
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
        "generator": "scripts/generate_phase2_modular_gltf_v10d.py"
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, required=True)
    arguments = parser.parse_args()
    arguments.output_dir.mkdir(parents=True, exist_ok=True)
    atlas_bytes, svg_bytes = build_ball_atlas()
    ball_closeup_bytes = build_ball_identity_closeup(atlas_bytes)
    pbr_outputs, pbr_parameters, _ = build_package()
    document, lod_sets, marker_nodes, triangle_stats = build_gltf()
    venue_document, _, _, _, _ = build_venue_gltf()
    gltf_bytes = (json.dumps(document, indent=2, sort_keys=True) + "\n").encode("utf-8")
    venue_gltf_bytes = (json.dumps(venue_document, indent=2, sort_keys=True) +
                        "\n").encode("utf-8")
    manifest = build_manifest(gltf_bytes, atlas_bytes, svg_bytes, ball_closeup_bytes, pbr_outputs,
                              pbr_parameters, document, lod_sets, marker_nodes,
                              triangle_stats)
    outputs = {
        ASSET_NAME: gltf_bytes,
        ATLAS_NAME: atlas_bytes,
        ATLAS_SOURCE_NAME: svg_bytes,
        BALL_CLOSEUP_NAME: ball_closeup_bytes,
        VENUE_ASSET_NAME: venue_gltf_bytes,
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
