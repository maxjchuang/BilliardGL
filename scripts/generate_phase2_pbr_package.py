#!/usr/bin/env python3
"""Generate original deterministic PBR textures, lighting rig, and review preview."""

import argparse
import json
import math
from pathlib import Path
import struct
import zlib


PACKAGE_ID = "focused_practice_hall_pbr_v1"
PARAMETERS_NAME = "focused_practice_hall_pbr_v1.params.json"
LIGHTING_RIG_NAME = "focused_practice_hall_lighting_rig_v1.json"
PREVIEW_NAME = "focused_practice_hall_pbr_review_v1.png"

TEXTURE_SPECS = (
    ("felt", 512, 1403, [32, 92, 99], 0.82, 0.0, [1.0, 1.0]),
    ("walnut", 512, 2711, [62, 42, 30], 0.34, 0.0, [2.0, 1.0]),
    ("rubber", 256, 3907, [18, 27, 29], 0.64, 0.0, [4.0, 16.0]),
    ("gunmetal", 256, 4519, [55, 61, 67], 0.30, 1.0, [2.0, 2.0]),
    ("leather", 256, 5839, [45, 27, 20], 0.62, 0.0, [2.0, 2.0]),
    ("cue", 512, 6421, [184, 119, 54], 0.32, 0.0, [1.0, 12.0]),
    # The bottom-only pocket void material maps its longitudinal UV range
    # [mouth, deep rounded back] onto texture V [0, 0.5].  The mirrored half
    # keeps the texture repeat-safe while AO falls from 0.88 to 0.08.  The
    # deep value follows the roughly 0.04--0.09 visible-hemisphere fraction
    # of a 43--66 mm half-mouth viewed from the 145 mm recessed bottom.
    ("pocket_void", 256, 7297, [255, 255, 255], 0.90, 0.0, [1.0, 0.5]),
)

COLOR_REVISION = {
    "revision_id": "phase2_ta_color_frequency_correction_v2",
    "evidence_basis": "v4_sha_bound_manual_review_2026_07_27",
    "felt": {
        "before_target_basecolor_srgb8": [20, 111, 126],
        "after_target_basecolor_srgb8": [32, 92, 99],
        "before_uv_repeat": [6.0, 12.0],
        "after_uv_repeat": [1.0, 1.0],
        "before_chroma_range_srgb8": 106,
        "after_chroma_range_srgb8": 67,
        "intent": "reduce cyan saturation while preserving approved low-saturation teal-blue",
    },
    "walnut": {
        "before_target_basecolor_srgb8": [78, 34, 19],
        "after_target_basecolor_srgb8": [62, 42, 30],
        "before_uv_repeat": [3.0, 2.0],
        "after_uv_repeat": [2.0, 1.0],
        "before_red_green_ratio": 2.2941,
        "after_red_green_ratio": 1.4762,
        "before_grain_cycles": 18.0,
        "after_grain_cycles": 5.0,
        "before_pore_cells": 72,
        "after_pore_cells": 24,
        "intent": "reduce red bias, contrast, and high-frequency procedural grain",
    },
}


def texture_filename(texture_set, channel):
    suffix = {"basecolor": "basecolor_srgb", "normal": "normal_gl",
              "orm": "orm_linear"}[channel]
    return "focused_practice_hall_%s_%s_v1.png" % (texture_set, suffix)


def _png_rgba(width, height, pixels):
    def chunk(kind, payload):
        return (struct.pack(">I", len(payload)) + kind + payload +
                struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF))
    raw = b"".join(b"\x00" + bytes(pixels[y * width * 4:(y + 1) * width * 4])
                   for y in range(height))
    return (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def _hash01(x, y, seed):
    value = (x * 374761393 + y * 668265263 + seed * 1442695041) & 0xFFFFFFFF
    value = ((value ^ (value >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((value ^ (value >> 16)) & 0xFFFFFF) / 16777215.0


def _smooth(value):
    return value * value * (3.0 - 2.0 * value)


def _noise(x, y, seed, cells, periodic=False):
    px, py = x * cells, y * cells
    ix, iy = math.floor(px), math.floor(py)
    fx, fy = _smooth(px - ix), _smooth(py - iy)
    if periodic:
        x0, x1 = ix % cells, (ix + 1) % cells
        y0, y1 = iy % cells, (iy + 1) % cells
    else:
        x0, x1 = ix, ix + 1
        y0, y1 = iy, iy + 1
    a = _hash01(x0, y0, seed)
    b = _hash01(x1, y0, seed)
    c = _hash01(x0, y1, seed)
    d = _hash01(x1, y1, seed)
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


def _fbm(x, y, seed, periodic=False):
    return (0.55 * _noise(x, y, seed, 4, periodic) +
            0.28 * _noise(x, y, seed + 17, 11, periodic) +
            0.17 * _noise(x, y, seed + 31, 29, periodic))


def _clamp8(value):
    return max(0, min(255, int(round(value))))


def _surface_sample(name, x, y, seed, target, roughness, metallic,
                    periodic_felt=True):
    periodic = name == "felt" and periodic_felt
    broad = _fbm(x, y, seed, periodic)
    fine = _noise(x, y, seed + 101, 48, periodic)
    if name == "felt":
        height = 0.50 + 0.025 * (fine - 0.5) + 0.015 * (broad - 0.5)
        modulation = 0.94 + 0.08 * broad
        rough = roughness + 0.03 * (fine - 0.5)
        ao = 0.96 + 0.04 * broad
    elif name == "walnut":
        phase = 0.65 * (broad - 0.5) + 0.20 * math.sin(2 * math.pi * y)
        grain = 0.5 + 0.5 * math.sin(2 * math.pi * (x * 5.0 + phase))
        pores = _noise(x, y, seed + 203, 24)
        height = 0.45 + 0.10 * grain + 0.025 * pores
        modulation = 0.86 + 0.18 * grain + 0.03 * (broad - 0.5)
        rough = roughness + 0.045 * (pores - 0.5) - 0.025 * grain
        ao = 0.91 + 0.09 * grain
    elif name == "rubber":
        stipple = _noise(x, y, seed + 307, 96)
        height = 0.46 + 0.16 * (stipple - 0.5) + 0.04 * (broad - 0.5)
        modulation = 0.86 + 0.20 * broad
        rough = roughness + 0.10 * (stipple - 0.5)
        ao = 0.94 + 0.06 * broad
    elif name == "gunmetal":
        brushed = 0.5 + 0.5 * math.sin(2 * math.pi * (x * 112.0 + broad * 0.3))
        height = 0.48 + 0.045 * brushed + 0.025 * (fine - 0.5)
        modulation = 0.84 + 0.20 * broad + 0.035 * brushed
        rough = roughness + 0.075 * (brushed - 0.5)
        ao = 0.98 + 0.02 * broad
    elif name == "leather":
        cell = abs(_noise(x, y, seed + 401, 26) - 0.5) * 2.0
        crease = max(0.0, 0.26 - cell) / 0.26
        height = 0.58 - 0.20 * crease + 0.07 * (fine - 0.5)
        modulation = 0.78 + 0.28 * broad - 0.08 * crease
        rough = roughness + 0.10 * crease + 0.05 * (fine - 0.5)
        ao = 0.88 + 0.12 * (1.0 - crease)
    elif name == "pocket_void":
        # A neutral texture set lets the existing near-black material factor
        # remain authoritative.  Only the ORM occlusion channel changes along
        # the depth UV; height stays constant so the generated normal is flat.
        depth = 0.5 - 0.5 * math.cos(2.0 * math.pi * y)
        height = 0.5
        modulation = 1.0
        rough = roughness
        ao = 0.88 - 0.80 * depth
    else:  # cue shaft: longitudinal, lacquered maple-like original procedural grain
        phase = 5.0 * broad + 0.9 * math.sin(2 * math.pi * y * 2.0)
        grain = 0.5 + 0.5 * math.sin(2 * math.pi * (x * 9.0 + phase))
        height = 0.48 + 0.12 * grain + 0.035 * (fine - 0.5)
        modulation = 0.78 + 0.38 * grain + 0.08 * broad
        rough = roughness + 0.055 * (fine - 0.5) - 0.035 * grain
        ao = 0.96 + 0.04 * grain
    color = tuple(_clamp8(channel * modulation) for channel in target)
    return color, height, max(0.04, min(1.0, rough)), max(0.0, min(1.0, ao)), metallic


def build_texture_set(spec, periodic_felt=True):
    name, size, seed, target, roughness, metallic, _ = spec
    colors = [None] * (size * size)
    heights = [0.0] * (size * size)
    roughnesses = [0.0] * (size * size)
    occlusions = [0.0] * (size * size)
    metallics = [0.0] * (size * size)
    for y in range(size):
        v = y / size
        for x in range(size):
            u = x / size
            sample = _surface_sample(name, u, v, seed, target, roughness,
                                     metallic, periodic_felt)
            index = y * size + x
            colors[index], heights[index], roughnesses[index], occlusions[index], metallics[index] = sample
    base_pixels = bytearray(size * size * 4)
    normal_pixels = bytearray(size * size * 4)
    orm_pixels = bytearray(size * size * 4)
    normal_strength = {"felt": 0.30, "walnut": 0.50, "rubber": 0.65,
                       "gunmetal": 0.30, "leather": 0.90, "cue": 0.42,
                       "pocket_void": 0.0}[name]
    for y in range(size):
        for x in range(size):
            index = y * size + x
            left = heights[y * size + (x - 1) % size]
            right = heights[y * size + (x + 1) % size]
            up = heights[((y - 1) % size) * size + x]
            down = heights[((y + 1) % size) * size + x]
            nx = -(right - left) * normal_strength
            ny = -(down - up) * normal_strength
            nz = 1.0
            length = math.sqrt(nx * nx + ny * ny + nz * nz)
            normal = (_clamp8((nx / length * 0.5 + 0.5) * 255),
                      _clamp8((ny / length * 0.5 + 0.5) * 255),
                      _clamp8((nz / length * 0.5 + 0.5) * 255), 255)
            color = colors[index] + (255,)
            orm = (_clamp8(occlusions[index] * 255),
                   _clamp8(roughnesses[index] * 255),
                   _clamp8(metallics[index] * 255), 255)
            offset = index * 4
            base_pixels[offset:offset + 4] = bytes(color)
            normal_pixels[offset:offset + 4] = bytes(normal)
            orm_pixels[offset:offset + 4] = bytes(orm)
    return {
        texture_filename(name, "basecolor"): _png_rgba(size, size, base_pixels),
        texture_filename(name, "normal"): _png_rgba(size, size, normal_pixels),
        texture_filename(name, "orm"): _png_rgba(size, size, orm_pixels),
    }, (base_pixels, normal_pixels, orm_pixels)


def build_parameters():
    return {
        "schema_version": 1,
        "package_id": PACKAGE_ID,
        "authorship": "original_procedural_billiardgl",
        "external_source_assets": False,
        "generator": "scripts/generate_phase2_pbr_package.py",
        "color_management": {
            "basecolor": "srgb_iec61966_2_1",
            "normal": "linear_opengl_positive_y_rgba8",
            "orm": "linear_rgba8_r_ao_g_roughness_b_metallic",
        },
        "seam_policy": "all procedural functions and finite differences wrap toroidally",
        "mip_policy": {
            "runtime": "generate_full_chain_box_or_higher_quality_offline",
            "basecolor": "srgb_filtered",
            "normal": "linear_filter_then_renormalize",
            "orm": "linear_filtered_preserve_channels",
            "alpha": "opaque",
        },
        "color_frequency_revision": COLOR_REVISION,
        "felt_moire_control": {
            "regular_sine_fibers": False,
            "normal_strength": 0.30,
            "runtime_uv_repeat": [1.0, 1.0],
            "goal": "subpixel stochastic micro-surface without screen-space periodic bands",
        },
        "texture_sets": {
            name: {
                "resolution_px": [size, size], "seed": seed,
                "target_basecolor_srgb8": target,
                "roughness_center": roughness, "metallic": metallic,
                "uv_repeat": repeat,
                "files": {channel: texture_filename(name, channel)
                          for channel in ("basecolor", "normal", "orm")},
            }
            for name, size, seed, target, roughness, metallic, repeat in TEXTURE_SPECS
        },
        "review_preview": {
            "file": PREVIEW_NAME,
            "layout": {"top_row": ["felt", "walnut", "rubber"],
                       "bottom_row": ["gunmetal", "leather", "cue"]},
            "purpose": "material sphere plus repeated UV inset; review-only, not runtime",
        },
    }


def build_lighting_rig():
    return {
        "schema_version": 1,
        "rig_id": "focused_practice_hall_lighting_rig_v1",
        "authorship": "original_procedural_billiardgl",
        "unit": "metre", "up_axis": "+Y", "handedness": "right",
        "lookdev_binding": "focused_practice_hall_v1",
        "asset_binding": "focused_practice_hall_modular_v1",
        "fixtures": [{
            "id": "key_rect_table_lamp",
            "type": "rect_area_or_renderer_equivalent",
            "position_m": [0.0, 2.21483976, 0.0],
            "orientation": {"local_emission_axis": "-Y"},
            "emitter_size_m": [0.61, 1.81],
            "color_temperature_k": 3900,
            "luminous_flux_lm": 6200,
            "normalized_intensity_fallback": 1.0,
            "casts_shadow": True,
        }],
        "environment": {
            "type": "analytic_constant_plus_ground_bounce_no_external_hdr",
            "upper_linear_rgb": [0.035, 0.043, 0.050],
            "lower_linear_rgb": [0.018, 0.015, 0.013],
            "intensity": 0.18,
            "background_relative_ev": -2.5,
        },
        "exposure": {
            "mode": "manual", "ev100": 7.2,
            "middle_gray": 0.18, "white_balance_k": 4100,
            "adaptation_enabled": False,
            "tone_map_target": "filmic_aces_like_or_documented_equivalent",
            "highlight_headroom_stops": 1.5,
        },
        "shadow_quality_tiers": {
            "low": {"map_resolution": 1024, "filter_taps": 4,
                    "softness_world_radius_m": 0.035, "contact_hardening": False},
            "medium": {"map_resolution": 2048, "filter_taps": 12,
                       "softness_world_radius_m": 0.080, "contact_hardening": True},
            "high": {"map_resolution": 4096, "filter_taps": 24,
                     "softness_world_radius_m": 0.140, "contact_hardening": True},
        },
        "validation": {
            "default_tier": "medium",
            "ball_highlight_shape": "single_stable_rectangular_key",
            "felt_average_linear_luminance_range": [0.055, 0.16],
            "background_must_remain_below_table_ev": 2.0,
            "lamp_diffuser_clipping_fraction_max": 0.02,
            "deterministic_capture": True,
        },
    }


def _sample_pixel(pixels, size, u, v):
    x = int((u % 1.0) * size) % size
    y = int((v % 1.0) * size) % size
    offset = (y * size + x) * 4
    return tuple(pixels[offset:offset + 4])


def build_preview(texture_pixels):
    width, height = 1536, 768
    result = bytearray([18, 20, 22, 255] * (width * height))
    light = (-0.38, 0.62, 0.69)
    light_length = math.sqrt(sum(value * value for value in light))
    light = tuple(value / light_length for value in light)
    # Pocket-void is a depth-only utility set, not a seventh look-dev sphere.
    # Keeping it out of this six-panel review preserves the approved preview
    # dimensions and makes the candidate observable through pocket captures.
    preview_specs = tuple(spec for spec in TEXTURE_SPECS
                          if spec[0] != "pocket_void")
    for panel_index, spec in enumerate(preview_specs):
        name, size, _, _, _, metallic, repeat = spec
        column, row = panel_index % 3, panel_index // 3
        panel_x, panel_y = column * 512, row * 384
        base, normal_map, orm = texture_pixels[name]
        cx, cy, radius = panel_x + 205, panel_y + 190, 145
        for py in range(panel_y + 28, panel_y + 354):
            for px in range(panel_x + 24, panel_x + 488):
                dx, dy = (px - cx) / radius, (cy - py) / radius
                radius_squared = dx * dx + dy * dy
                if radius_squared <= 1.0:
                    nz = math.sqrt(max(0.0, 1.0 - radius_squared))
                    u = 0.5 + math.atan2(nz, dx) / (2 * math.pi)
                    v = 0.5 - math.asin(max(-1.0, min(1.0, dy))) / math.pi
                    color = _sample_pixel(base, size, u * repeat[0], v * repeat[1])
                    orm_value = _sample_pixel(orm, size, u * repeat[0], v * repeat[1])
                    ndotl = max(0.0, dx * light[0] + dy * light[1] + nz * light[2])
                    rough = orm_value[1] / 255.0
                    half_dot = max(0.0, nz * 0.82 + dx * -0.24 + dy * 0.52)
                    specular = half_dot ** max(2.0, 24.0 * (1.0 - rough))
                    ambient = 0.22
                    diffuse = ambient + 0.78 * ndotl
                    for channel in range(3):
                        value = color[channel] * diffuse
                        value += 255 * specular * (0.15 + 0.55 * metallic)
                        result[(py * width + px) * 4 + channel] = _clamp8(value)
                    result[(py * width + px) * 4 + 3] = 255
                elif panel_x + 382 <= px < panel_x + 478 and panel_y + 236 <= py < panel_y + 332:
                    # A four-repeat UV inset makes seams and mip-prone frequencies visible.
                    u = (px - panel_x - 382) / 24.0
                    v = (py - panel_y - 236) / 24.0
                    color = _sample_pixel(base, size, u, v)
                    offset = (py * width + px) * 4
                    result[offset:offset + 4] = bytes(color)
        # Deterministic six-color panel key; order is documented in parameters/README.
        key = ((32, 92, 99), (62, 42, 30), (38, 48, 51),
               (92, 99, 106), (75, 47, 35), (202, 137, 67))[panel_index]
        for py in range(panel_y + 20, panel_y + 32):
            for px in range(panel_x + 20, panel_x + 492):
                offset = (py * width + px) * 4
                result[offset:offset + 4] = bytes(key + (255,))
    return _png_rgba(width, height, result)


def build_package():
    outputs = {}
    texture_pixels = {}
    for spec in TEXTURE_SPECS:
        generated, pixels = build_texture_set(spec)
        outputs.update(generated)
        texture_pixels[spec[0]] = pixels
    parameters = build_parameters()
    rig = build_lighting_rig()
    outputs[PARAMETERS_NAME] = (json.dumps(parameters, indent=2, sort_keys=True) + "\n").encode("utf-8")
    outputs[LIGHTING_RIG_NAME] = (json.dumps(rig, indent=2, sort_keys=True) + "\n").encode("utf-8")
    outputs[PREVIEW_NAME] = build_preview(texture_pixels)
    return outputs, parameters, rig


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", required=True, type=Path)
    arguments = parser.parse_args()
    arguments.output_dir.mkdir(parents=True, exist_ok=True)
    outputs, _, _ = build_package()
    for name, payload in outputs.items():
        (arguments.output_dir / name).write_bytes(payload)
    runtime_bytes = sum(len(payload) for name, payload in outputs.items()
                        if name.endswith(".png") and name != PREVIEW_NAME)
    print("generated phase2 PBR package: textures=%d runtime_disk_bytes=%d" %
          (len(TEXTURE_SPECS) * 3, runtime_bytes))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
