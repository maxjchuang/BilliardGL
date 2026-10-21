#!/usr/bin/env python3
"""Generate a transition-only atlas that diagnoses runtime U handedness.

The output is a complete Phase-2 asset directory for use through
BILLIARDGL_MODERN_PHASE2_ROOT. It never edits the production asset directory.
"""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import zlib

from generate_phase2_modular_gltf import (
    ATLAS_COLUMNS, ATLAS_HEIGHT, ATLAS_NAME, ATLAS_ROWS, ATLAS_WIDTH,
    _pack_png_rgba, _paint_circle, _paint_rect, build_ball_atlas,
)


def decode_rgba(payload):
    compressed = bytearray()
    offset = 8
    while offset + 12 <= len(payload):
        length = struct.unpack_from(">I", payload, offset)[0]
        kind = payload[offset + 4:offset + 8]
        if kind == b"IDAT":
            compressed.extend(payload[offset + 8:offset + 8 + length])
        offset += 12 + length
    raw = zlib.decompress(compressed)
    stride = ATLAS_WIDTH * 4
    pixels = bytearray()
    for y in range(ATLAS_HEIGHT):
        row = y * (stride + 1)
        if raw[row] != 0:
            raise ValueError("probe source requires deterministic PNG filter 0")
        pixels.extend(raw[row + 1:row + 1 + stride])
    return pixels


def paint_arrow(pixels, cx, cy, direction, color):
    """Paint an asymmetric filled arrow with a rectangular tail."""
    if direction == "left":
        _paint_rect(pixels, cx - 2, cy - 4, cx + 13, cy + 5, color)
        for dx in range(13):
            half_height = max(1, dx)
            _paint_rect(pixels, cx - 14 + dx, cy - half_height,
                        cx - 13 + dx, cy + half_height + 1, color)
    else:
        _paint_rect(pixels, cx - 13, cy - 4, cx + 2, cy + 5, color)
        for dx in range(13):
            half_height = max(1, 12 - dx)
            _paint_rect(pixels, cx + 2 + dx, cy - half_height,
                        cx + 3 + dx, cy + half_height + 1, color)


def paint_vertical_arrow(pixels, cx, cy, direction, color):
    """Paint a vertical arrow so V inversion cannot hide behind U evidence."""
    if direction == "up":
        _paint_rect(pixels, cx - 4, cy - 2, cx + 5, cy + 13, color)
        for dy in range(13):
            half_width = max(1, dy)
            _paint_rect(pixels, cx - half_width, cy - 14 + dy,
                        cx + half_width + 1, cy - 13 + dy, color)
    else:
        _paint_rect(pixels, cx - 4, cy - 13, cx + 5, cy + 2, color)
        for dy in range(13):
            half_width = max(1, 12 - dy)
            _paint_rect(pixels, cx - half_width, cy + 2 + dy,
                        cx + half_width + 1, cy + 3 + dy, color)


def paint_digit(pixels, digit, x, y, scale, color):
    glyphs = {
        "1": ("00100", "01100", "00100", "00100", "00100", "00100", "01110"),
        "5": ("11111", "10000", "10000", "11110", "00001", "00001", "11110"),
    }
    for row, line in enumerate(glyphs[digit]):
        for column, filled in enumerate(line):
            if filled == "1":
                _paint_rect(pixels, x + column * scale, y + row * scale,
                            x + (column + 1) * scale, y + (row + 1) * scale, color)


def build_probe():
    source, _ = build_ball_atlas()
    pixels = decode_rgba(source)
    tile_width = ATLAS_WIDTH // ATLAS_COLUMNS
    tile_height = ATLAS_HEIGHT // ATLAS_ROWS
    ivory = (239, 234, 211, 255)
    ink = (17, 18, 20, 255)
    red = (230, 35, 45, 255)
    green = (35, 190, 85, 255)
    blue = (35, 95, 230, 255)
    orange = (245, 125, 25, 255)
    for identity in (1, 8, 11, 15):
        x0 = identity % ATLAS_COLUMNS * tile_width
        y0 = identity // ATLAS_COLUMNS * tile_height
        for patch_x in (x0 + tile_width // 4, x0 + 3 * tile_width // 4):
            patch_y = y0 + tile_height // 2
            _paint_circle(pixels, patch_x, patch_y, 54, ivory)
            paint_arrow(pixels, patch_x - 39, patch_y, "left", red)
            paint_digit(pixels, "1", patch_x - 22, patch_y - 18, 5, ink)
            paint_digit(pixels, "5", patch_x + 3, patch_y - 18, 5, ink)
            paint_arrow(pixels, patch_x + 39, patch_y, "right", green)
            paint_vertical_arrow(pixels, patch_x, patch_y - 40, "up", blue)
            paint_vertical_arrow(pixels, patch_x, patch_y + 40, "down", orange)
    return _pack_png_rgba(ATLAS_WIDTH, ATLAS_HEIGHT, pixels)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    source = args.source_dir.resolve()
    output = args.output_dir.resolve()
    if source == output:
        raise SystemExit("orientation probe output must not be the production source")
    output.mkdir(parents=True, exist_ok=True)
    for path in source.iterdir():
        if path.is_file():
            shutil.copy2(path, output / path.name)
    probe = build_probe()
    (output / ATLAS_NAME).write_bytes(probe)
    record = {
        "status": "TRANSITION_ONLY_NOT_FOR_FORMAL_REVIEW",
        "diagnostic": "red left / green right plus blue up / orange down around 15",
        "duplicated_patch_longitudes": [0.25, 0.75],
        "expected_if_upright": "blue up on top, orange down on bottom, red left, 15, green right",
        "probe_atlas_sha256": hashlib.sha256(probe).hexdigest(),
        "source_dir": str(source),
    }
    (output / "TRANSITION_ONLY_orientation_probe.json").write_text(
        json.dumps(record, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(record, sort_keys=True))


if __name__ == "__main__":
    main()
