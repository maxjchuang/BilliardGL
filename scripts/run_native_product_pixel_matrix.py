#!/usr/bin/env python3
"""Run and audit the native visible product HUD/trajectory pixel matrix.

This is deliberately a consumer of the existing Cocoa/NSOpenGLView product
loop.  It does not create a hidden drawable.  Three base profiles produce the
21 required state images; a fourth profile repeats the seven states with the
shipping deuteranopia setting.  Some hard-to-reach states are explicitly
labelled controller/rules fixtures in the application receipt and are not
reported as human-interaction E2E coverage.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import pathlib
import statistics
import subprocess
import sys
from array import array
from collections import Counter
from dataclasses import dataclass


STATES = (
    "default",
    "aim",
    "post_shot",
    "foul_modal",
    "help",
    "training",
    "replay",
)
PROFILES = (
    "physical-1280x720",
    "physical-1920x1080",
    "retina-1280x720",
    "cvd-1920x1080",
)
EXPECTED_SIZE = {
    "physical-1280x720": (1280, 720),
    "physical-1920x1080": (1920, 1080),
    "retina-1280x720": (2560, 1440),
    "cvd-1920x1080": (1920, 1080),
}


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


@dataclass
class Ppm:
    width: int
    height: int
    pixels: bytearray

    def rgb(self, x: int, y: int) -> tuple[int, int, int]:
        x = min(max(x, 0), self.width - 1)
        y = min(max(y, 0), self.height - 1)
        offset = (y * self.width + x) * 3
        return tuple(self.pixels[offset : offset + 3])  # type: ignore[return-value]

    def copy(self) -> "Ppm":
        return Ppm(self.width, self.height, bytearray(self.pixels))


def read_ppm(path: pathlib.Path) -> Ppm:
    data = path.read_bytes()
    if not data.startswith(b"P6\n"):
        raise ValueError(f"{path}: not binary PPM")
    cursor = 3
    tokens: list[bytes] = []
    while len(tokens) < 3:
        while cursor < len(data) and data[cursor] in b" \t\r\n":
            cursor += 1
        if cursor < len(data) and data[cursor] == ord("#"):
            cursor = data.index(b"\n", cursor) + 1
            continue
        end = cursor
        while end < len(data) and data[end] not in b" \t\r\n":
            end += 1
        tokens.append(data[cursor:end])
        cursor = end
    # P6 has one whitespace separator after maxval. Everything after it is
    # binary raster data, including bytes that happen to represent whitespace.
    if cursor >= len(data) or data[cursor] not in b" \t\r\n":
        raise ValueError(f"{path}: missing PPM raster separator")
    cursor += 1
    width, height, maximum = map(int, tokens)
    if width <= 0 or height <= 0 or maximum != 255 or len(data) - cursor != width * height * 3:
        raise ValueError(f"{path}: invalid PPM dimensions/payload")
    return Ppm(width, height, bytearray(data[cursor:]))


def srgb_luminance(rgb: tuple[int, int, int]) -> float:
    def linear(value: int) -> float:
        channel = value / 255.0
        return channel / 12.92 if channel <= 0.04045 else ((channel + 0.055) / 1.055) ** 2.4

    return 0.2126 * linear(rgb[0]) + 0.7152 * linear(rgb[1]) + 0.0722 * linear(rgb[2])


def contrast(first: tuple[int, int, int], second: tuple[int, int, int]) -> float:
    a, b = srgb_luminance(first), srgb_luminance(second)
    return (max(a, b) + 0.05) / (min(a, b) + 0.05)


def local_best_contrast(image: Ppm, x: int, y: int, background: tuple[int, int, int]) -> float:
    return max(
        contrast(image.rgb(x + dx, y + dy), background)
        for dy in (-1, 0, 1)
        for dx in (-1, 0, 1)
    )


def edge_count(image: Ppm, y0: int, y1: int) -> int:
    y0, y1 = max(1, y0), min(image.height - 1, y1)
    step = max(1, image.width // 960)
    total = 0
    for y in range(y0, y1, step):
        for x in range(1, image.width - 1, step):
            current = image.rgb(x, y)
            right = image.rgb(x + 1, y)
            below = image.rgb(x, y + 1)
            if max(abs(current[i] - right[i]) for i in range(3)) >= 36 or max(
                abs(current[i] - below[i]) for i in range(3)
            ) >= 36:
                total += 1
    return total


def quantized_colors(image: Ppm) -> int:
    stride = max(1, (image.width * image.height) // 200_000)
    colors = set()
    for offset in range(0, len(image.pixels), stride * 3):
        if offset + 2 >= len(image.pixels):
            break
        colors.add(tuple(value // 16 for value in image.pixels[offset : offset + 3]))
    return len(colors)


def trajectory_probe_stats(image: Ppm, receipt: dict, track: str,
                           component_role: int = 0) -> dict:
    probes = [
        item
        for item in receipt["trajectory_probes"]
        if item["track"] == track
        and int(item.get("component_role", 0 if item.get("halo_valid") else 1)) == component_role
    ]
    ratios: list[float] = []
    for item in probes:
        x, y = int(item["x"]), int(item["y"])
        bx, by = int(item["background_x"]), int(item["background_y"])
        hx, hy = int(item["halo_x"]), int(item["halo_y"])
        if not (0 <= x < image.width and 0 <= y < image.height and
                0 <= hx < image.width and 0 <= hy < image.height and
                0 <= bx < image.width and 0 <= by < image.height):
            raise ValueError(f"{track}: trajectory probe lies outside the drawable")
        background = image.rgb(bx, by)
        # The production compound stroke owns core plus adaptive halo/rim.
        # Both authored coordinates may land one raster sample from their
        # resolved coverage edge, so audit an exact +/-1 pixel neighborhood
        # around each and retain the stronger owned component.
        ratios.append(max(
            local_best_contrast(image, x, y, background),
            local_best_contrast(image, hx, hy, background),
        ))
    visible = [value for value in ratios if value >= 4.0]
    return {
        "component_role": component_role,
        "source_probe_count": len(probes),
        "in_bounds_probe_count": len(ratios),
        "visible_probe_count": len(visible),
        "visible_fraction": len(visible) / len(ratios) if ratios else 0.0,
        "median_contrast": statistics.median(ratios) if ratios else 0.0,
        "minimum_visible_contrast": min(visible) if visible else 0.0,
        "contrast_contract": 4.0,
        "coordinate_tolerance_px": 1,
        "below_contract_probes": [
            {"primitive_id": probe.get("primitive_id"),
             "arc_segment_id": probe.get("arc_segment_id"),
             "probe_ordinal": probe.get("probe_ordinal"),
             "x": probe["x"], "y": probe["y"], "contrast": ratio}
            for probe, ratio in zip(probes, ratios) if ratio < 4.0
        ],
    }


def validate_tick_inventory(receipt: dict) -> None:
    count = receipt["actual_tick_count"]
    if type(count) is not int or count <= 0:
        raise ValueError("replay: invalid actual tick count")
    ticks = [probe for probe in receipt["trajectory_probes"]
             if probe["track"] == "actual" and probe.get("component_role") == 3]
    identities = [probe.get("primitive_id") for probe in ticks]
    expected = {f"actual_tick_{index}_core" for index in range(count)}
    if len(identities) != count or set(identities) != expected:
        raise ValueError("replay: actual tick identity inventory is incomplete or duplicated")
    if any(probe.get("halo_valid") is not True for probe in ticks):
        raise ValueError("replay: actual tick has no valid authored halo sample")


def text_run_stats(image: Ppm, receipt: dict) -> dict:
    runs = receipt.get("product_ui_text_runs", [])
    if not runs:
        raise ValueError("product UI retained no submitted text runs")
    truncated: list[str] = []
    pixel_heights: list[int] = []
    requested: list[float] = []
    for item in runs:
        text = str(item["text"])
        size = float(item["requested_px"])
        measured = float(item["measured_width_px"])
        origin = float(item["origin_x"])
        baseline = float(item["baseline_y"])
        clip_x = float(item["clip_x"])
        clip_y = float(item["clip_y"])
        clip_width = float(item["clip_width"])
        clip_height = float(item["clip_height"])
        geometry = (size, measured, origin, baseline, clip_x, clip_y,
                    clip_width, clip_height)
        if (not all(math.isfinite(value) for value in geometry) or
                size <= 0 or measured <= 0 or clip_width <= 0 or clip_height <= 0):
            raise ValueError("product UI text run has invalid geometry")
        tolerance = 0.05  # float layout rounding, not a pixel clipping allowance
        if (clip_x < -tolerance or clip_y < -tolerance or
                clip_x + clip_width > image.width + tolerance or
                clip_y + clip_height > image.height + tolerance):
            raise ValueError("product UI text clip lies outside the drawable")
        # Width alone misses a run whose origin is offset inside its clip.
        # Vertical ink bounds require glyph metrics, not an assumed em box;
        # until those are emitted, only the baseline is proven here.
        if (origin < clip_x - tolerance or
                origin + measured > clip_x + clip_width + tolerance or
                baseline < clip_y - tolerance or
                baseline > clip_y + clip_height + tolerance):
            truncated.append(text)
        requested.append(size)
        x0 = max(1, int(math.floor(float(item["origin_x"]))))
        x1 = min(image.width - 1, int(math.ceil(min(
            float(item["origin_x"]) + measured,
            float(item["clip_x"]) + clip_width,
        ))))
        baseline = float(item["baseline_y"])
        y0 = max(1, int(math.floor(baseline - size * 1.20)))
        y1 = min(image.height - 1, int(math.ceil(baseline + size * 0.30)))
        edge_rows: list[int] = []
        for y in range(y0, y1):
            row_edges = 0
            for x in range(x0, x1):
                here = image.rgb(x, y)
                right = image.rgb(x + 1, y)
                below = image.rgb(x, y + 1)
                if max(abs(here[c] - right[c]) for c in range(3)) >= 24 or max(
                    abs(here[c] - below[c]) for c in range(3)
                ) >= 24:
                    row_edges += 1
            if row_edges >= 2:
                edge_rows.append(y)
        pixel_heights.append(
            max(edge_rows) - min(edge_rows) + 1 if edge_rows else 0
        )
    if truncated:
        raise ValueError(f"product UI text exceeds its submitted clip: {truncated}")
    visible = [
        height
        for height, size in zip(pixel_heights, requested)
        if height >= max(5, int(math.floor(size * 0.35)))
    ]
    visible_fraction = len(visible) / len(runs)
    if min(requested) < 13.0 or visible_fraction < 1.0:
        raise ValueError(
            "product UI glyph height evidence is insufficient: "
            f"minimum_requested={min(requested):.3f} visible_fraction={visible_fraction:.3f}"
        )
    return {
        "run_count": len(runs),
        "truncated_run_count": 0,
        "minimum_requested_px": min(requested),
        "median_pixel_edge_height": statistics.median(pixel_heights),
        "pixel_visible_fraction": visible_fraction,
        "glyph_ink_pixel_contract_verified": False,
        "evidence_scope": "run geometry and edge preflight; no glyph attribution",
    }


def paired_glyph_pixel_stats(normal: Ppm, suppressed: Ppm, receipt: dict) -> dict:
    """Causal pixel coverage only; caller must authenticate frozen paired frames.

    This does not authenticate the glyph's shape against the source font atlas.
    Shared ink pixels cannot establish either glyph's visibility. SDF smoothing
    may extend beyond FreeType bitmap ink: allowed differences use the actual
    submitted, clipped quad, not an arbitrary one-pixel ink dilation.
    """
    if ((normal.width, normal.height) != (suppressed.width, suppressed.height) or
            len(normal.pixels) != normal.width * normal.height * 3 or
            len(suppressed.pixels) != len(normal.pixels)):
        raise ValueError("glyph pair dimensions/payload differ")
    glyph_geometry_stats(normal, receipt)
    glyphs = receipt["product_ui_glyph_quads"]
    count = normal.width * normal.height
    owners = array("i", [-1]) * count
    allowed = bytearray(count)
    for index, glyph in enumerate(glyphs):
        x, y, width, height = (float(glyph["ink_" + key])
                              for key in ("x", "y", "width", "height"))
        qx, qy, qw, qh = (float(glyph[key]) for key in ("x", "y", "width", "height"))
        if (not all(math.isfinite(v) for v in (qx, qy, qw, qh)) or qw <= 0 or qh <= 0 or
                qx < 0 or qy < 0 or qx + qw > normal.width + 0.05 or
                qy + qh > normal.height + 0.05):
            raise ValueError("glyph pair submitted quad is invalid")
        x0, y0 = math.floor(qx), math.floor(qy)
        x1, y1 = math.ceil(qx + qw), math.ceil(qy + qh)
        for py in range(max(0, y0), min(normal.height, y1)):
            for px in range(max(0, x0), min(normal.width, x1)):
                offset = py * normal.width + px
                allowed[offset] = 1
                if x <= px + 0.5 < x + width and y <= py + 0.5 < y + height:
                    owners[offset] = index if owners[offset] == -1 else -2
    changed = 0
    owned_changes = [0] * len(glyphs)
    strongest = [0.0] * len(glyphs)
    for pixel in range(count):
        offset = pixel * 3
        foreground = tuple(normal.pixels[offset:offset + 3])
        background = tuple(suppressed.pixels[offset:offset + 3])
        if foreground == background:
            continue
        changed += 1
        if not allowed[pixel]:
            raise ValueError("glyph pair differs outside submitted glyph quads: "
                             f"pixel=({pixel % normal.width},{pixel // normal.width}) "
                             f"normal={foreground} suppressed={background}")
        owner = owners[pixel]
        if owner >= 0:
            owned_changes[owner] += 1
            strongest[owner] = max(strongest[owner], contrast(foreground, background))
    failed_runs = {}
    for index, glyph in enumerate(glyphs):
        if owned_changes[index] == 0 or strongest[index] < 4.5:
            run_index = glyph["text_run_index"]
            failure = failed_runs.setdefault(run_index, {
                "text": receipt["product_ui_text_runs"][run_index]["text"],
                "failed_glyph_count": 0, "missing_exclusive_pixels": 0,
                "minimum_peak_contrast": strongest[index]})
            failure["failed_glyph_count"] += 1
            failure["missing_exclusive_pixels"] += int(owned_changes[index] == 0)
            failure["minimum_peak_contrast"] = min(
                failure["minimum_peak_contrast"], strongest[index])
    if failed_runs:
        raise ValueError("glyph lacks exclusive contrasting final pixels: " +
                         json.dumps(failed_runs, sort_keys=True))
    return {"glyph_count": len(glyphs), "changed_pixel_count": changed,
            "minimum_glyph_peak_contrast": min(strongest),
            "contrast_contract": 4.5,
            "difference_domain": "submitted clipped glyph quads; evidence cores use bitmap ink",
            "paired_frame_binding_verified": False,
            "font_shape_identity_verified": False}


def validate_glyph_pair_binding(directory: pathlib.Path, state: str, receipt: dict) -> dict:
    """Bind persisted files to the producer's frozen-frame/atlas evidence.

    Producer assertions are authenticated by the separately bound native binary;
    they are not an independent font re-rasterization by this Python consumer.
    """
    def digest(field):
        value = receipt[field]
        if (not isinstance(value, str) or len(value) != 64 or
                any(c not in "0123456789abcdef" for c in value)):
            raise ValueError(f"invalid glyph pair digest: {field}")
        return value

    expected_name = f"{state}-no-glyphs.ppm"
    if receipt["paired_no_glyph_image"] != expected_name:
        raise ValueError("glyph pair image identity mismatch")
    for filename, field in ((f"{state}.ppm", "paired_normal_ppm_sha256"),
                            (expected_name, "paired_no_glyph_ppm_sha256")):
        if sha256(directory / filename) != digest(field):
            raise ValueError("glyph pair persisted image digest mismatch")
    for fields in (("product_ui_canonical_sha256", "paired_no_glyph_canonical_sha256",
                    "paired_restored_canonical_sha256"),
                   ("paired_frozen_state_sha256", "paired_frozen_state_before_sha256",
                    "paired_frozen_state_after_sha256"),
                   ("paired_normal_rgba_sha256", "paired_restored_rgba_sha256")):
        if len({digest(field) for field in fields}) != 1:
            raise ValueError("glyph pair frozen/restored state drift")
    digest("paired_no_glyph_rgba_sha256")
    if (receipt["paired_restored_user_visible"] is not True or
            receipt["glyph_atlas_binding_verified"] is not True or
            not isinstance(receipt["font_atlas_cache_key"], str) or
            not receipt["font_atlas_cache_key"]):
        raise ValueError("glyph pair producer binding incomplete")
    count = receipt["paired_no_glyph_suppressed_count"]
    icons = receipt["paired_no_glyph_suppressed_icon_labels"]
    glyphs = receipt["product_ui_glyph_quads"]
    if (type(count) is not int or count != len(glyphs) or count <= 0 or
            type(icons) is not int or icons != 0):
        raise ValueError("glyph pair suppression inventory mismatch")
    for glyph in glyphs:
        value = glyph.get("atlas_record_sha256")
        if (not isinstance(value, str) or len(value) != 64 or
                any(c not in "0123456789abcdef" for c in value)):
            raise ValueError("glyph pair missing canonical atlas record")
    return {"persisted_pair_hashes_verified": True,
            "producer_frozen_state_consistent": True,
            "producer_atlas_binding_present": True,
            "independent_font_rasterization_verified": False}


def ui_band(state: str, height: int) -> tuple[int, int]:
    if state in {"aim", "post_shot", "training", "replay"}:
        return int(height * 0.62), height
    if state == "foul_modal":
        return int(height * 0.18), int(height * 0.88)
    return 0, int(height * 0.72)


def glyph_geometry_stats(image: Ppm, receipt: dict) -> dict:
    """Check submitted glyph ink against its original coverage and scissor.

    This proves geometry, not that final pixels came from the glyph atlas.
    Punctuation is legitimately shorter than the minimum *font size*.
    """
    glyphs = receipt.get("product_ui_glyph_quads", [])
    if not glyphs:
        raise ValueError("product UI retained no submitted glyph geometry")
    runs = receipt.get("product_ui_text_runs", [])
    if not runs:
        raise ValueError("product UI glyphs have no source text runs")
    expected = Counter((run_index, ordinal, ord(character))
                       for run_index, run in enumerate(runs)
                       for ordinal, character in enumerate(run["text"])
                       if not character.isspace())
    submitted_identities = []
    for glyph in glyphs:
        identity = tuple(glyph[field] for field in
                         ("text_run_index", "codepoint_ordinal", "codepoint"))
        if any(type(value) is not int or value < 0 for value in identity):
            raise ValueError("product UI glyph identity must use nonnegative integers")
        submitted_identities.append(identity)
    if expected != Counter(submitted_identities):
        raise ValueError("product UI source text and submitted glyph coverage differ")
    tolerance = 0.05
    for glyph in glyphs:
        def rect(prefix):
            values = tuple(float(glyph[prefix + name])
                           for name in ("x", "y", "width", "height"))
            if (not all(math.isfinite(value) for value in values) or
                    values[2] <= 0 or values[3] <= 0):
                raise ValueError("product UI glyph has invalid ink/clip geometry")
            return values

        original = rect("ink_source_")
        submitted = rect("ink_")
        clip = rect("clip_")
        if any(abs(a - b) > tolerance for a, b in zip(original, submitted)):
            raise ValueError("product UI glyph ink was clipped before submission")
        x, y, width, height = original
        cx, cy, cw, ch = clip
        if (x < max(0.0, cx) - tolerance or
                y < max(0.0, cy) - tolerance or
                x + width > min(float(image.width), cx + cw) + tolerance or
                y + height > min(float(image.height), cy + ch) + tolerance):
            raise ValueError("product UI glyph ink exceeds clip or drawable")
    return {
        "submitted_glyph_count": len(glyphs),
        "clipped_ink_count": 0,
        "evidence_scope": "submitted ink geometry; final-pixel attribution pending",
        "final_pixel_attribution_verified": False,
    }


def validate_image(image: Ppm, receipt: dict, state: str, expected_size: tuple[int, int]) -> dict:
    if (image.width, image.height) != expected_size:
        raise ValueError(f"{state}: image size does not match profile")
    if (int(receipt["width_px"]), int(receipt["height_px"])) != expected_size:
        raise ValueError(f"{state}: receipt drawable size does not match profile")
    colors = quantized_colors(image)
    if colors < 2:
        raise ValueError(f"{state}: uniform/blank frame ({colors} colors)")
    y0, y1 = ui_band(state, image.height)
    edges = edge_count(image, y0, y1)
    minimum_edges = max(100, image.width // 3)
    if edges < minimum_edges:
        raise ValueError(f"{state}: final UI foreground edge evidence is missing ({edges})")
    if int(receipt["product_ui_draw_calls"]) <= 0 or int(receipt["product_ui_sdf_vertices"]) <= 0:
        raise ValueError(f"{state}: UI semantic receipt is empty")
    if int(receipt["product_ui_clipped_items"]) != 0:
        raise ValueError(f"{state}: product UI reports clipped items")
    result = {
        "quantized_colors": colors,
        "ui_band_edges": edges,
        "ui_band_minimum": minimum_edges,
        "text": text_run_stats(image, receipt),
        "glyph_geometry": glyph_geometry_stats(image, receipt),
    }
    if state == "training":
        stats = trajectory_probe_stats(image, receipt, "predicted")
        if (stats["source_probe_count"] < 4 or
                stats["visible_fraction"] < 1.0 or
                stats["median_contrast"] < 4.0 or
                int(receipt["predicted_strokes"]) < 4):
            raise ValueError(f"{state}: predicted trajectory lacks final-pixel probe evidence: {stats}")
        result["predicted"] = stats
    if state == "replay":
        stats = trajectory_probe_stats(image, receipt, "actual")
        if (stats["source_probe_count"] < 4 or
                stats["visible_fraction"] < 1.0 or
                stats["median_contrast"] < 4.0):
            raise ValueError(f"{state}: actual trajectory lacks final-pixel probe evidence: {stats}")
        if int(receipt["actual_tick_count"]) <= 0:
            raise ValueError("replay: actual trajectory receipt has no non-color tick identity")
        result["actual"] = stats
        # Tick probes have their own geometry and semantic identity. A positive
        # draw count or passing segment bodies cannot stand in for their pixels.
        ticks = trajectory_probe_stats(image, receipt, "actual", component_role=3)
        if (ticks["source_probe_count"] != int(receipt["actual_tick_count"]) or
                ticks["visible_fraction"] < 1.0 or
                ticks["median_contrast"] < 4.0):
            raise ValueError(f"{state}: actual ticks lack final-pixel probe evidence: {ticks}")
        result["actual_ticks"] = ticks
        validate_tick_inventory(receipt)
    return result


def shifted(image: Ppm, dx: int) -> Ppm:
    output = Ppm(image.width, image.height, bytearray(len(image.pixels)))
    for y in range(image.height):
        for x in range(image.width):
            source_x = x - dx
            if 0 <= source_x < image.width:
                target = (y * image.width + x) * 3
                source = (y * image.width + source_x) * 3
                output.pixels[target : target + 3] = image.pixels[source : source + 3]
    return output


def blurred(image: Ppm) -> Ppm:
    output = image.copy()
    block = 12
    for y0 in range(0, image.height, block):
        for x0 in range(0, image.width, block):
            x1, y1 = min(x0 + block, image.width), min(y0 + block, image.height)
            count = (x1 - x0) * (y1 - y0)
            sums = [0, 0, 0]
            for y in range(y0, y1):
                offset = (y * image.width + x0) * 3
                for _x in range(x0, x1):
                    sums[0] += image.pixels[offset]
                    sums[1] += image.pixels[offset + 1]
                    sums[2] += image.pixels[offset + 2]
                    offset += 3
            color = bytes(value // count for value in sums)
            for y in range(y0, y1):
                offset = (y * image.width + x0) * 3
                for _x in range(x0, x1):
                    output.pixels[offset : offset + 3] = color
                    offset += 3
    return output


def masked_probes(image: Ppm, receipt: dict, track: str) -> Ppm:
    output = image.copy()
    for item in receipt["trajectory_probes"]:
        if item["track"] != track:
            continue
        x, y = int(item["x"]), int(item["y"])
        background = output.rgb(int(item["background_x"]), int(item["background_y"]))
        for dy in range(-3, 4):
            for dx in range(-3, 4):
                px, py = x + dx, y + dy
                if 0 <= px < output.width and 0 <= py < output.height:
                    offset = (py * output.width + px) * 3
                    output.pixels[offset : offset + 3] = bytes(background)
    return output


def masked_text_runs(image: Ppm, receipt: dict) -> Ppm:
    output = image.copy()
    for item in receipt.get("product_ui_text_runs", []):
        size = float(item["requested_px"])
        x0 = max(0, int(math.floor(float(item["origin_x"]))) - 2)
        x1 = min(output.width, int(math.ceil(
            float(item["origin_x"]) + float(item["measured_width_px"]))) + 2)
        y0 = max(0, int(math.floor(float(item["baseline_y"]) - size * 1.25)))
        y1 = min(output.height, int(math.ceil(
            float(item["baseline_y"]) + size * 0.35)))
        sample_x = min(output.width - 1,
            max(0, int(float(item["clip_x"]) + 1)))
        sample_y = min(output.height - 1,
            max(0, int(float(item["clip_y"]) + 1)))
        background = bytes(output.rgb(sample_x, sample_y))
        for y in range(y0, y1):
            for x in range(x0, x1):
                offset = (y * output.width + x) * 3
                output.pixels[offset : offset + 3] = background
    return output


def assert_rejected(name: str, operation) -> dict:
    try:
        operation()
    except (ValueError, KeyError) as error:
        return {"name": name, "rejected": True, "reason": str(error)}
    raise RuntimeError(f"negative mutation was accepted: {name}")


def difference_fraction(first: Ppm, second: Ppm) -> float:
    if (first.width, first.height) != (second.width, second.height):
        return 1.0
    changed = 0
    pixels = first.width * first.height
    step = max(1, pixels // 250_000)
    sampled = 0
    for index in range(0, pixels, step):
        offset = index * 3
        if max(abs(first.pixels[offset + c] - second.pixels[offset + c]) for c in range(3)) >= 12:
            changed += 1
        sampled += 1
    return changed / sampled


def validate_profile_receipts(rows: list[dict], profile: str) -> None:
    if [row["state"] for row in rows] != list(STATES):
        raise ValueError(f"{profile}: receipt state order mismatch")
    # ProductSettings::ColorVisionMode: Default=0, Deuteranopia=1.
    # Any other nonzero mode is not evidence for this specific CVD profile.
    expected_mode = 1 if profile == "cvd-1920x1080" else 0
    for row in rows:
        if (row.get("schema") != "billiardgl_native_product_pixel_matrix_v1" or
                row.get("profile") != profile):
            raise ValueError(f"{profile}: receipt schema/profile mismatch")
        if type(row.get("color_vision_mode")) is not int or row["color_vision_mode"] != expected_mode:
            raise ValueError(f"{profile}: unexpected color-vision mode")


def run_profile(binary: pathlib.Path, root: pathlib.Path, profile: str) -> dict:
    directory = root / profile
    if directory.exists() and any(directory.iterdir()):
        raise ValueError(f"{profile}: capture directory must be empty; use offline audit for existing evidence")
    directory.mkdir(parents=True, exist_ok=True)
    command = [str(binary), str(directory), "--pixel-matrix", profile]
    invocation_path = directory / "capture-invocation.json"
    invocation = {
        "schema": "billiardgl_native_pixel_capture_invocation_v1",
        "command": command,
        "profile": profile,
        "expected_size": EXPECTED_SIZE[profile],
        "binary_sha256_before": sha256(binary),
        "auditor_sha256": sha256(pathlib.Path(__file__)),
        "status": "STARTED",
        "all_pixel_verdicts_passed": False,
    }
    invocation_path.write_text(json.dumps(invocation, indent=2, sort_keys=True)
                               + "\n", encoding="utf-8")
    completed = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    (directory / "stdout.log").write_text(completed.stdout, encoding="utf-8")
    (directory / "stderr.log").write_text(completed.stderr, encoding="utf-8")
    invocation.update({
        "status": "PROCESS_EXITED",
        "returncode": completed.returncode,
        "binary_sha256_after": sha256(binary),
        "stdout_sha256": sha256(directory / "stdout.log"),
        "stderr_sha256": sha256(directory / "stderr.log"),
        "captured_image_sha256": {
            state: sha256(directory / f"{state}.ppm") for state in STATES
            if (directory / f"{state}.ppm").is_file()
        },
    })
    invocation_path.write_text(json.dumps(invocation, indent=2, sort_keys=True)
                               + "\n", encoding="utf-8")
    if invocation["binary_sha256_before"] != invocation["binary_sha256_after"]:
        raise RuntimeError(f"{profile}: native binary changed during capture")
    if completed.returncode != 0:
        raise RuntimeError(
            f"{profile}: native visible consumer failed ({completed.returncode})\n{completed.stderr}"
        )
    receipt_path = directory / "native-pixel-matrix-receipt.jsonl"
    receipts = [json.loads(line) for line in receipt_path.read_text(encoding="utf-8").splitlines() if line]
    validate_profile_receipts(receipts, profile)
    images = {}
    verdicts = {}
    errors = {}
    for index, state in enumerate(STATES):
        try:
            images[state] = read_ppm(directory / f"{state}.ppm")
            verdicts[state] = validate_image(
                images[state], receipts[index], state, EXPECTED_SIZE[profile])
        except (ValueError, KeyError, OSError) as error:
            errors[state] = str(error)
    # Keep every state diagnosis from this capture. A Training failure must
    # not prevent discovery of a separate Replay failure in the same run.
    glyph_report = audit_existing_glyph_pairs(directory, profile)
    glyph_report.update({
        "new_capture_performed": True,
        "capture_binary_binding_verified": True,
        "capture_invocation_sha256": sha256(invocation_path),
    })
    glyph_report_path = directory / "glyph-pair-audit.json"
    glyph_report_path.write_text(
        json.dumps(glyph_report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    state_report = {
        "schema": "billiardgl_native_profile_preflight_v1",
        "profile": profile,
        "capture_invocation_sha256": sha256(invocation_path),
        "receipt_sha256": sha256(receipt_path),
        "state_metrics": verdicts,
        "state_errors": errors,
        "glyph_pair_audit_sha256": sha256(glyph_report_path),
        "glyph_pairs_passed": glyph_report["glyph_pairs_passed"],
        "all_pixel_verdicts_passed": False,
        "scope": "per-state preflight; adjacent-state and final pixel gates separate",
    }
    (directory / "state-preflight.json").write_text(
        json.dumps(state_report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    if errors:
        raise RuntimeError(f"{profile}: state preflight failed: {errors}")
    if not glyph_report["glyph_pairs_passed"]:
        raise RuntimeError(f"{profile}: paired glyph audit failed: {glyph_report['states']}")
    differences = {
        f"{STATES[index]}->{STATES[index + 1]}": difference_fraction(
            images[STATES[index]], images[STATES[index + 1]]
        )
        for index in range(len(STATES) - 1)
    }
    if min(differences.values()) < 0.002:
        raise RuntimeError(f"{profile}: adjacent product states are not visibly distinct: {differences}")
    return {
        "profile": profile,
        "expected_size": EXPECTED_SIZE[profile],
        "state_count": len(STATES),
        "origins": {row["state"]: row["origin"] for row in receipts},
        "verdicts": verdicts,
        "glyph_pair_audit_sha256": sha256(glyph_report_path),
        "glyph_pairs_passed": True,
        "adjacent_difference_fraction": differences,
        "image_sha256": {state: sha256(directory / f"{state}.ppm") for state in STATES},
        "receipt_sha256": sha256(receipt_path),
    }


def audit_existing_profile(directory: pathlib.Path, profile: str) -> dict:
    """Read-only offline recheck; never imply a new capture or binary binding."""
    receipt_path = directory / "native-pixel-matrix-receipt.jsonl"
    rows = [json.loads(line) for line in receipt_path.read_text(
        encoding="utf-8").splitlines() if line]
    validate_profile_receipts(rows, profile)
    results = {}
    for state, row in zip(STATES, rows):
        path = directory / f"{state}.ppm"
        result = {"image": str(path)}
        try:
            result["image_sha256"] = sha256(path)
            result["metrics"] = validate_image(
                read_ppm(path), row, state, EXPECTED_SIZE[profile])
            result["preflight_passed"] = True
        except (ValueError, KeyError, OSError) as error:
            result["preflight_passed"] = False
            result["error"] = str(error)
        results[state] = result
    return {
        "schema": "billiardgl_offline_pixel_preflight_v1",
        "status": "PREFLIGHT_ONLY",
        "new_capture_performed": False,
        "capture_binary_binding_verified": False,
        "all_pixel_verdicts_passed": False,
        "profile": profile,
        "receipt_sha256": sha256(receipt_path),
        "auditor_sha256": sha256(pathlib.Path(__file__)),
        "states": results,
    }


def audit_existing_glyph_pairs(directory: pathlib.Path, profile: str) -> dict:
    """Read-only paired audit, independent of unrelated trajectory preflight."""
    receipt_path = directory / "native-pixel-matrix-receipt.jsonl"
    rows = [json.loads(line) for line in receipt_path.read_text(
        encoding="utf-8").splitlines() if line]
    validate_profile_receipts(rows, profile)
    results = {}
    for state, row in zip(STATES, rows):
        try:
            binding = validate_glyph_pair_binding(directory, state, row)
            normal = read_ppm(directory / f"{state}.ppm")
            suppressed = read_ppm(directory / f"{state}-no-glyphs.ppm")
            if (normal.width, normal.height) != EXPECTED_SIZE[profile]:
                raise ValueError("glyph pair drawable does not match profile")
            pixels = paired_glyph_pixel_stats(normal, suppressed, row)
            results[state] = {"passed": True, "binding": binding, "pixels": pixels}
        except (ValueError, KeyError, OSError) as error:
            results[state] = {"passed": False, "error": str(error)}
    return {"schema": "billiardgl_glyph_pair_audit_v1", "profile": profile,
            "states": results, "new_capture_performed": False,
            "receipt_sha256": sha256(receipt_path),
            "auditor_sha256": sha256(pathlib.Path(__file__)),
            "capture_binary_binding_verified": False,
            "all_pixel_verdicts_passed": False,
            "glyph_pairs_passed": all(item["passed"] for item in results.values())}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--audit-existing-profile", choices=PROFILES)
    parser.add_argument("--audit-glyph-pairs", choices=PROFILES)
    args = parser.parse_args()
    if args.audit_existing_profile and args.audit_glyph_pairs:
        parser.error("choose exactly one offline audit mode")
    if args.audit_glyph_pairs:
        report = audit_existing_glyph_pairs(args.output.resolve(), args.audit_glyph_pairs)
        print(json.dumps(report, indent=2, sort_keys=True))
        return 0 if report["glyph_pairs_passed"] else 1
    if args.audit_existing_profile:
        # Output here names an existing capture directory; results go to stdout
        # so an immutable evidence directory is never silently rewritten.
        report = audit_existing_profile(args.output.resolve(),
                                        args.audit_existing_profile)
        print(json.dumps(report, indent=2, sort_keys=True))
        return 0 if all(row["preflight_passed"]
                        for row in report["states"].values()) else 1
    if args.binary is None:
        parser.error("--binary is required when capturing new frames")
    binary = args.binary.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    before = sha256(binary)
    profiles = [run_profile(binary, output, profile) for profile in PROFILES]
    after = sha256(binary)
    if before != after:
        raise RuntimeError("native pixel matrix binary changed during capture")

    reference_dir = output / "physical-1920x1080"
    cvd_dir = output / "cvd-1920x1080"
    receipts = [json.loads(line) for line in (reference_dir / "native-pixel-matrix-receipt.jsonl").read_text(encoding="utf-8").splitlines() if line]
    receipt_by_state = {row["state"]: row for row in receipts}
    images = {state: read_ppm(reference_dir / f"{state}.ppm") for state in STATES}
    cvd_images = {state: read_ppm(cvd_dir / f"{state}.ppm") for state in STATES}
    cvd_differences = {
        state: difference_fraction(images[state], cvd_images[state])
        for state in ("training", "replay")
    }
    if min(cvd_differences.values()) < 0.00005:
        raise RuntimeError(
            "CVD setting did not produce distinct final trajectory/UI pixels: "
            f"{cvd_differences}"
        )
    expected = EXPECTED_SIZE["physical-1920x1080"]
    mutations = [
        assert_rejected(
            "blank_frame",
            lambda: validate_image(Ppm(expected[0], expected[1], bytearray(expected[0] * expected[1] * 3)), receipt_by_state["aim"], "aim", expected),
        ),
        assert_rejected(
            "wrong_backing_scale",
            lambda: validate_image(images["aim"], {**receipt_by_state["aim"], "width_px": 1280}, "aim", expected),
        ),
        assert_rejected(
            "shifted_trajectory",
            lambda: validate_image(shifted(images["training"], expected[0] // 8), receipt_by_state["training"], "training", expected),
        ),
        assert_rejected(
            "masked_predicted_trajectory",
            lambda: validate_image(masked_probes(images["training"], receipt_by_state["training"], "predicted"), receipt_by_state["training"], "training", expected),
        ),
        assert_rejected(
            "masked_actual_trajectory",
            lambda: validate_image(masked_probes(images["replay"], receipt_by_state["replay"], "actual"), receipt_by_state["replay"], "replay", expected),
        ),
        assert_rejected(
            "blurred_replay_foreground",
            lambda: validate_image(blurred(images["replay"]), receipt_by_state["replay"], "replay", expected),
        ),
        assert_rejected(
            "masked_aim_text_glyphs",
            lambda: validate_image(masked_text_runs(images["aim"], receipt_by_state["aim"]), receipt_by_state["aim"], "aim", expected),
        ),
    ]
    report = {
        "schema": "billiardgl_native_product_pixel_matrix_report_v1",
        "scope": {
            "native_visible_window": True,
            "reads_gl_back": True,
            "full_human_interaction_e2e": False,
            "fixture_states_explicitly_labelled": True,
            "base_profile_state_count": 21,
            "cvd_additional_state_count": 7,
            "training_user_acceptance_repeated": False,
        },
        "binary": str(binary),
        "binary_sha256_before": before,
        "binary_sha256_after": after,
        "profiles": profiles,
        "cvd_final_pixel_difference_fraction": cvd_differences,
        "mutation_gates": mutations,
    }
    report_path = output / "native-product-pixel-matrix-report.json"
    report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    receipt = {
        "schema": "billiardgl_native_product_pixel_matrix_receipt_v1",
        "report": str(report_path),
        "report_sha256": sha256(report_path),
        "binary_sha256": before,
        "profiles": list(PROFILES),
        "base_state_count": 21,
        "cvd_state_count": 7,
        "mutation_gate_count": len(mutations),
        "capture_preflight_passed": True,
        "all_pixel_verdicts_passed": False,
        "status": "PREFLIGHT_ONLY",
        "remaining_pixel_gates": [
            "glyph-attributed ink height and vertical clipping",
            "physical trajectory width and endpoint coverage",
            "trajectory dash/tick identity and HUD exclusion",
        ],
    }
    (output / "receipt.json").write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(receipt, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:  # Stable one-line CTest diagnostic plus traceback.
        print(f"native-product-pixel-matrix: {error}", file=sys.stderr)
        raise
