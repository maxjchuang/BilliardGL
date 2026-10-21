#!/usr/bin/env python3
"""Validate Phase-2 procedural PBR textures, lighting rig, and review preview."""

import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import sys
import zlib

from generate_phase2_pbr_package import (
    COLOR_REVISION, LIGHTING_RIG_NAME, PACKAGE_ID, PARAMETERS_NAME, PREVIEW_NAME, TEXTURE_SPECS,
    texture_filename,
)
from generate_phase2_modular_gltf import MANIFEST_NAME


def _sha256(payload):
    return "sha256:" + hashlib.sha256(payload).hexdigest()


def _read_png(path, errors, expected_size=None):
    try:
        payload = path.read_bytes()
    except OSError as error:
        errors.append("cannot read %s: %s" % (path.name, error))
        return None
    if payload[:8] != b"\x89PNG\r\n\x1a\n":
        errors.append(path.name + " is not a PNG")
        return None
    offset = 8
    width = height = depth = color_type = None
    compressed = bytearray()
    while offset + 12 <= len(payload):
        length = struct.unpack_from(">I", payload, offset)[0]
        kind = payload[offset + 4:offset + 8]
        data = payload[offset + 8:offset + 8 + length]
        crc = struct.unpack_from(">I", payload, offset + 8 + length)[0]
        if zlib.crc32(kind + data) & 0xFFFFFFFF != crc:
            errors.append(path.name + " has an invalid PNG CRC")
            return None
        if kind == b"IHDR":
            width, height, depth, color_type, compression, filtering, interlace = struct.unpack(
                ">IIBBBBB", data)
            if (depth, color_type, compression, filtering, interlace) != (8, 6, 0, 0, 0):
                errors.append(path.name + " must be RGBA8 non-interlaced PNG")
        elif kind == b"IDAT":
            compressed.extend(data)
        elif kind == b"IEND":
            break
        offset += 12 + length
    if expected_size is not None and (width, height) != expected_size:
        errors.append("%s must be %dx%d" % (path.name, expected_size[0], expected_size[1]))
    try:
        raw = zlib.decompress(compressed)
    except zlib.error:
        errors.append(path.name + " has invalid compressed pixels")
        return None
    stride = width * 4
    if len(raw) != height * (stride + 1):
        errors.append(path.name + " decoded byte count is invalid")
        return None
    pixels = bytearray(width * height * 4)
    for y in range(height):
        source = y * (stride + 1)
        if raw[source] != 0:
            errors.append(path.name + " must use deterministic PNG filter 0")
            return None
        pixels[y * stride:(y + 1) * stride] = raw[source + 1:source + 1 + stride]
    return width, height, pixels, payload


def _channel_stats(pixels, channel):
    values = pixels[channel::4]
    mean = sum(values) / len(values)
    variance = sum((value - mean) ** 2 for value in values) / len(values)
    return min(values), max(values), mean, math.sqrt(variance)


def _edge_difference(pixels, width, height):
    horizontal = 0.0
    vertical = 0.0
    count_h = height * 3
    count_v = width * 3
    for y in range(height):
        left, right = (y * width) * 4, (y * width + width - 1) * 4
        horizontal += sum(abs(pixels[left + c] - pixels[right + c]) for c in range(3))
    for x in range(width):
        top, bottom = x * 4, ((height - 1) * width + x) * 4
        vertical += sum(abs(pixels[top + c] - pixels[bottom + c]) for c in range(3))
    return horizontal / count_h, vertical / count_v


def _srgb_to_linear(value):
    encoded = value / 255.0
    return encoded / 12.92 if encoded <= 0.04045 else \
        ((encoded + 0.055) / 1.055) ** 2.4


def _linear_to_srgb8(value):
    encoded = 12.92 * value if value <= 0.0031308 else \
        1.055 * value ** (1.0 / 2.4) - 0.055
    return max(0, min(255, int(round(encoded * 255.0))))


def _box_mip_rgba8(pixels, width, height, srgb=False):
    """CPU equivalent of the package's full-chain 2x2 box mip policy."""
    next_width, next_height = max(1, width // 2), max(1, height // 2)
    result = bytearray(next_width * next_height * 4)
    for y in range(next_height):
        ys = (min(height - 1, y * 2), min(height - 1, y * 2 + 1))
        for x in range(next_width):
            xs = (min(width - 1, x * 2), min(width - 1, x * 2 + 1))
            source = [[pixels[(source_y * width + source_x) * 4 + channel]
                       for source_y in ys for source_x in xs]
                      for channel in range(4)]
            target = (y * next_width + x) * 4
            for channel in range(4):
                if srgb and channel < 3:
                    result[target + channel] = _linear_to_srgb8(
                        sum(_srgb_to_linear(value) for value in source[channel]) / 4.0)
                else:
                    result[target + channel] = (sum(source[channel]) + 2) // 4
    return result, next_width, next_height


def _mip_wrap_discontinuities(pixels, width, height, srgb=False,
                              quantization_slop=2):
    """Find wrap steps larger than both adjacent interior steps at every mip."""
    failures = []
    level = 0
    while True:
        for axis in ("s", "t"):
            scan_count = height if axis == "s" else width
            for scan in range(scan_count):
                for channel in range(3):
                    def at(x, y):
                        return pixels[(y * width + x) * 4 + channel]

                    if axis == "s":
                        first, last = at(0, scan), at(width - 1, scan)
                        first_inside = at(min(1, width - 1), scan)
                        last_inside = at(max(0, width - 2), scan)
                    else:
                        first, last = at(scan, 0), at(scan, height - 1)
                        first_inside = at(scan, min(1, height - 1))
                        last_inside = at(scan, max(0, height - 2))
                    wrap_step = abs(first - last)
                    local_step = max(abs(first - first_inside),
                                     abs(last - last_inside))
                    if wrap_step > local_step + quantization_slop:
                        failures.append({
                            "level": level, "width": width, "height": height,
                            "axis": axis, "scan": scan, "channel": channel,
                            "wrap_step": wrap_step, "local_step": local_step,
                        })
        if width == 1 and height == 1:
            return failures
        pixels, width, height = _box_mip_rgba8(
            pixels, width, height, srgb)
        level += 1


def validate(parameters, rig, manifest, asset_dir, verify_hashes=True):
    errors = []
    asset_dir = Path(asset_dir)
    periodic_felt_contract = {
        "felt_noise_wrap": "toroidal_lattice_v1",
        "mip_wrap_audit": "relative_edge_step_plus_rgba8_quantization_v1",
    }
    declared_felt_contract = manifest.get("pbr_texture_sampling_contract")
    if declared_felt_contract is not None and \
            declared_felt_contract != periodic_felt_contract:
        errors.append("felt periodic texture-sampling contract is invalid")
    require_periodic_felt = declared_felt_contract == periodic_felt_contract
    if parameters.get("schema_version") != 1 or parameters.get("package_id") != PACKAGE_ID:
        errors.append("PBR parameter identity/schema is invalid")
    if parameters.get("authorship") != "original_procedural_billiardgl" or \
            parameters.get("external_source_assets") is not False:
        errors.append("PBR package must declare original procedural authorship")
    expected_management = {
        "basecolor": "srgb_iec61966_2_1",
        "normal": "linear_opengl_positive_y_rgba8",
        "orm": "linear_rgba8_r_ao_g_roughness_b_metallic",
    }
    if parameters.get("color_management") != expected_management:
        errors.append("baseColor/normal/ORM color management is invalid")
    if parameters.get("color_frequency_revision") != COLOR_REVISION:
        errors.append("felt/walnut before-after color and frequency evidence is invalid")
    else:
        felt_revision = COLOR_REVISION["felt"]
        walnut_revision = COLOR_REVISION["walnut"]
        if felt_revision["after_chroma_range_srgb8"] >= \
                felt_revision["before_chroma_range_srgb8"] or \
                walnut_revision["after_red_green_ratio"] >= \
                walnut_revision["before_red_green_ratio"] or \
                walnut_revision["after_grain_cycles"] >= \
                walnut_revision["before_grain_cycles"] or \
                walnut_revision["after_pore_cells"] >= walnut_revision["before_pore_cells"]:
            errors.append("felt/walnut correction must reduce saturation/red bias/spatial frequency")
    expected_moire_control = {
        "regular_sine_fibers": False,
        "normal_strength": 0.30,
        "runtime_uv_repeat": [1.0, 1.0],
        "goal": "subpixel stochastic micro-surface without screen-space periodic bands",
    }
    if parameters.get("felt_moire_control") != expected_moire_control:
        errors.append("felt moire-control contract is invalid")
    mip = parameters.get("mip_policy", {})
    if mip.get("runtime") != "generate_full_chain_box_or_higher_quality_offline" or \
            mip.get("normal") != "linear_filter_then_renormalize" or \
            mip.get("basecolor") != "srgb_filtered" or mip.get("orm") != \
            "linear_filtered_preserve_channels":
        errors.append("mip generation policy is incomplete")

    specs = {spec[0]: spec for spec in TEXTURE_SPECS}
    texture_sets = parameters.get("texture_sets", {})
    if set(texture_sets) != set(specs):
        errors.append("exactly the seven approved PBR texture sets are required")
    if len({spec[2] for spec in TEXTURE_SPECS}) != len(TEXTURE_SPECS):
        errors.append("texture seeds must be unique")
    runtime_disk_bytes = 0
    expected_gpu_bytes = 0
    texture_count = 0
    files_manifest = manifest.get("files", {}).get("pbr_textures", {})
    for name, spec in specs.items():
        _, size, seed, target, roughness, metallic, repeat = spec
        entry = texture_sets.get(name, {})
        if entry.get("resolution_px") != [size, size] or entry.get("seed") != seed or \
                entry.get("target_basecolor_srgb8") != target or \
                entry.get("uv_repeat") != repeat:
            errors.append(name + " deterministic texture parameters differ")
        decoded = {}
        for channel in ("basecolor", "normal", "orm"):
            filename = texture_filename(name, channel)
            if entry.get("files", {}).get(channel) != filename:
                errors.append(name + " " + channel + " filename differs")
            result = _read_png(asset_dir / filename, errors, (size, size))
            if result is None:
                continue
            width, height, pixels, payload = result
            decoded[channel] = pixels
            runtime_disk_bytes += len(payload)
            texture_count += 1
            expected_gpu_bytes += 4 * ((4 * size * size - 1) // 3)
            if verify_hashes and files_manifest.get(name, {}).get(channel, {}).get("sha256") != \
                    _sha256(payload):
                errors.append(name + " " + channel + " SHA-256 differs")
            if any(value != 255 for value in pixels[3::4]):
                errors.append(name + " " + channel + " must be opaque")
            edge_x, edge_y = _edge_difference(pixels, width, height)
            if edge_x > 32.0 or edge_y > 32.0:
                errors.append("%s %s has excessive wrap-edge discontinuity" % (name, channel))
            if name == "felt" and require_periodic_felt:
                wrap_failures = _mip_wrap_discontinuities(
                    pixels, width, height, srgb=channel == "basecolor")
                if wrap_failures:
                    first = wrap_failures[0]
                    errors.append(
                        "felt %s violates periodic full-mip wrap contract "
                        "at level %d axis %s scan %d channel %d" % (
                            channel, first["level"], first["axis"],
                            first["scan"], first["channel"]))
        if "basecolor" in decoded:
            standard_deviation = sum(_channel_stats(decoded["basecolor"], c)[3]
                                     for c in range(3)) / 3.0
            if name == "pocket_void":
                if any(decoded["basecolor"][offset:offset + 4] !=
                       bytes((255, 255, 255, 255))
                       for offset in range(0, len(decoded["basecolor"]), 4)):
                    errors.append("pocket_void baseColor must remain neutral white")
            elif standard_deviation < 0.45:
                errors.append(name + " baseColor is unexpectedly flat")
        if "normal" in decoded:
            pixels = decoded["normal"]
            samples = len(pixels) // 4
            length_error = 0.0
            for index in range(0, len(pixels), 4):
                vector = tuple(pixels[index + axis] / 127.5 - 1.0 for axis in range(3))
                length_error += abs(math.sqrt(sum(value * value for value in vector)) - 1.0)
            if length_error / samples > 0.012 or _channel_stats(pixels, 2)[2] < 205:
                errors.append(name + " normal is not normalized tangent-space +Z")
            if name == "pocket_void" and any(
                    pixels[offset:offset + 4] != bytes((128, 128, 255, 255))
                    for offset in range(0, len(pixels), 4)):
                errors.append("pocket_void normal must remain flat OpenGL +Z")
        if "orm" in decoded:
            pixels = decoded["orm"]
            ao_stats = _channel_stats(pixels, 0)
            ao = ao_stats[2] / 255.0
            rough_mean = _channel_stats(pixels, 1)[2] / 255.0
            metal_mean = _channel_stats(pixels, 2)[2] / 255.0
            if name == "pocket_void":
                opening_ao = pixels[0] / 255.0
                deepest_ao = pixels[(size // 2 * size) * 4] / 255.0
                if not 0.86 <= opening_ao <= 0.90 or \
                        not 0.06 <= deepest_ao <= 0.10 or \
                        not 0.46 <= ao <= 0.50 or \
                        deepest_ao >= opening_ao:
                    errors.append("pocket_void AO depth profile is invalid")
                if any(pixels[offset + 1] != 230 or
                       pixels[offset + 2] != 0 or
                       pixels[offset + 3] != 255
                       for offset in range(0, len(pixels), 4)):
                    errors.append("pocket_void ORM must retain roughness 0.90 and metallic 0")
            elif ao < 0.82:
                errors.append(name + " ORM ambient occlusion differs from target")
            if abs(rough_mean - roughness) > 0.13 or \
                    abs(metal_mean - metallic) > 0.01:
                errors.append(name + " ORM channel statistics differ from targets")

    budgets = manifest.get("budgets", {})
    if texture_count != 21 or budgets.get("pbr_texture_disk_bytes") != runtime_disk_bytes or \
            budgets.get("pbr_texture_gpu_bytes_rgba8_with_full_mips") != expected_gpu_bytes:
        errors.append("PBR texture count/disk/GPU budget differs")
    if expected_gpu_bytes > 16 * 1024 * 1024:
        errors.append("PBR textures exceed the 16 MiB full-mip GPU budget")

    if rig.get("schema_version") != 1 or rig.get("rig_id") != \
            "focused_practice_hall_lighting_rig_v1":
        errors.append("lighting rig identity/schema is invalid")
    if rig.get("authorship") != "original_procedural_billiardgl" or \
            rig.get("asset_binding") != "focused_practice_hall_modular_v1":
        errors.append("lighting rig authorship/binding is invalid")
    fixtures = rig.get("fixtures", [])
    if len(fixtures) != 1:
        errors.append("lighting rig must have one dominant rectangular fixture")
    else:
        fixture = fixtures[0]
        if fixture.get("type") != "rect_area_or_renderer_equivalent" or \
                fixture.get("emitter_size_m") != [0.61, 1.81] or \
                fixture.get("color_temperature_k") != 3900 or \
                fixture.get("casts_shadow") is not True:
            errors.append("rectangular key fixture parameters are invalid")
    environment = rig.get("environment", {})
    if environment.get("type") != "analytic_constant_plus_ground_bounce_no_external_hdr" or \
            any(token in json.dumps(environment).lower() for token in (".hdr", ".exr", "http://", "https://")):
        errors.append("environment must remain analytic with no external HDR")
    exposure = rig.get("exposure", {})
    if exposure.get("mode") != "manual" or exposure.get("adaptation_enabled") is not False or \
            not 5.0 <= exposure.get("ev100", -1) <= 10.0:
        errors.append("manual deterministic exposure is invalid")
    tiers = rig.get("shadow_quality_tiers", {})
    try:
        resolutions = [tiers[name]["map_resolution"] for name in ("low", "medium", "high")]
        taps = [tiers[name]["filter_taps"] for name in ("low", "medium", "high")]
        softness = [tiers[name]["softness_world_radius_m"] for name in ("low", "medium", "high")]
        if not resolutions[0] < resolutions[1] < resolutions[2] or \
                not taps[0] < taps[1] < taps[2] or not softness[0] < softness[1] < softness[2]:
            errors.append("shadow quality/softness tiers must strictly increase")
    except (KeyError, TypeError):
        errors.append("low/medium/high shadow tiers are required")
    if rig.get("validation", {}).get("deterministic_capture") is not True:
        errors.append("lighting validation must require deterministic capture")

    preview = _read_png(asset_dir / PREVIEW_NAME, errors, (1536, 768))
    if preview is not None and manifest.get("files", {}).get(
            "material_review_preview", {}).get("runtime") is not False:
        errors.append("material review preview must be excluded from runtime")
    if verify_hashes:
        support = {
            "pbr_parameters": asset_dir / PARAMETERS_NAME,
            "lighting_rig": asset_dir / LIGHTING_RIG_NAME,
            "material_review_preview": asset_dir / PREVIEW_NAME,
        }
        for key, path in support.items():
            if path.is_file() and manifest.get("files", {}).get(key, {}).get("sha256") != \
                    _sha256(path.read_bytes()):
                errors.append(key + " SHA-256 differs")

    report = {
        "texture_sets": len(texture_sets), "runtime_textures": texture_count,
        "runtime_disk_bytes": runtime_disk_bytes,
        "runtime_gpu_bytes_full_mips": expected_gpu_bytes,
        "shadow_tiers": len(tiers),
    }
    return errors, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("asset_dir", nargs="?", type=Path,
                        default=Path("assets/modern/phase2"))
    arguments = parser.parse_args()
    try:
        parameters = json.loads((arguments.asset_dir / PARAMETERS_NAME).read_text(encoding="utf-8"))
        rig = json.loads((arguments.asset_dir / LIGHTING_RIG_NAME).read_text(encoding="utf-8"))
        manifest = json.loads((arguments.asset_dir / MANIFEST_NAME).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        print("phase2 PBR validation failed: " + str(error), file=sys.stderr)
        return 2
    errors, report = validate(parameters, rig, manifest, arguments.asset_dir)
    if errors:
        for error in errors:
            print("error: " + error, file=sys.stderr)
        return 1
    print("phase2 PBR package valid: texture_sets=%d runtime_textures=%d "
          "runtime_disk_bytes=%d runtime_gpu_bytes_full_mips=%d shadow_tiers=%d" %
          (report["texture_sets"], report["runtime_textures"],
           report["runtime_disk_bytes"], report["runtime_gpu_bytes_full_mips"],
           report["shadow_tiers"]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
