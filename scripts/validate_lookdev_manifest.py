#!/usr/bin/env python3
"""Validate the M1 lookdev manifest without OpenGL or third-party packages."""

import argparse
import json
import math
from pathlib import Path
import sys


REQUIRED_MATERIALS = {
    "ball_resin", "table_felt", "table_wood", "table_metal",
    "pocket_leather", "cue_shaft", "cue_butt", "cue_tip",
    "lamp_housing", "lamp_diffuser", "background_wall", "background_floor",
}
REQUIRED_CAMERAS = {
    "player_shot", "low_aim", "tactical_overhead", "broadcast_oblique",
}
REQUIRED_SCENES = {
    "break", "low_aim", "ball_closeup", "pocket_closeup",
    "tactical_overhead", "post_shot_observe",
}
REQUIRED_CHECKS = {
    "mean_luma_min", "mean_luma_max", "non_black_fraction_min",
    "highlight_clip_fraction_max", "shadow_clip_fraction_max",
    "minimum_contrast_to_surround", "minimum_edge_density",
}


def _finite_number(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def _range(errors, path, value, lower=0.0, upper=1.0):
    if not isinstance(value, dict) or set(value) != {"target", "minimum", "maximum"}:
        errors.append(path + " must contain exactly target/minimum/maximum")
        return
    numbers = [value["minimum"], value["target"], value["maximum"]]
    if not all(_finite_number(item) for item in numbers):
        errors.append(path + " values must be finite numbers")
    elif not (lower <= numbers[0] <= numbers[1] <= numbers[2] <= upper):
        errors.append(path + " must satisfy lower <= minimum <= target <= maximum <= upper")


def _vector(errors, path, value, length, lower=None, upper=None):
    if not isinstance(value, list) or len(value) != length:
        errors.append(path + " must be an array of length " + str(length))
        return
    if not all(_finite_number(item) for item in value):
        errors.append(path + " must contain finite numbers")
        return
    if lower is not None and any(item < lower for item in value):
        errors.append(path + " contains a value below " + str(lower))
    if upper is not None and any(item > upper for item in value):
        errors.append(path + " contains a value above " + str(upper))


def validate(document):
    errors = []
    if document.get("schema_version") != 1:
        errors.append("schema_version must be 1")
    if document.get("manifest_id") != "focused_practice_hall_v1":
        errors.append("manifest_id must be focused_practice_hall_v1")
    authorship = document.get("authorship", {})
    if authorship.get("content_policy") != "original_billiardgl_design_no_competitor_assets":
        errors.append("authorship.content_policy must require original BilliardGL content")

    color = document.get("color_management", {})
    expected_color = {
        "pbr_workflow": "gltf2_metallic_roughness",
        "working_space": "linear_srgb_d65",
        "base_color_factor_space": "linear_srgb_d65",
        "base_color_texture_encoding": "srgb_iec61966_2_1",
        "normal_orm_mask_encoding": "linear",
        "normal_convention": "gltf_opengl_tangent_y_positive",
        "orm_channel_layout": "R=occlusion,G=roughness,B=metallic",
        "ldr_output_encoding": "srgb_iec61966_2_1_rgb8",
    }
    for key, expected in expected_color.items():
        if color.get(key) != expected:
            errors.append("color_management.%s must be %s" % (key, expected))
    if color.get("hdr_intermediate") is not True:
        errors.append("color_management.hdr_intermediate must be true")

    clearcoat = document.get("clearcoat_policy", {})
    if clearcoat.get("preferred") != "KHR_materials_clearcoat":
        errors.append("clearcoat_policy.preferred must be KHR_materials_clearcoat")
    if not clearcoat.get("fallback"):
        errors.append("clearcoat_policy.fallback is required")

    materials = document.get("materials")
    if not isinstance(materials, dict):
        errors.append("materials must be an object")
        materials = {}
    missing_materials = REQUIRED_MATERIALS - set(materials)
    if missing_materials:
        errors.append("missing materials: " + ", ".join(sorted(missing_materials)))
    for material_id, material in materials.items():
        path = "materials." + material_id
        if not isinstance(material, dict):
            errors.append(path + " must be an object")
            continue
        _vector(errors, path + ".base_color_factor_linear_srgb",
                material.get("base_color_factor_linear_srgb"), 4, 0.0, 1.0)
        if material.get("base_color_texture_encoding") != "srgb_iec61966_2_1":
            errors.append(path + ".base_color_texture_encoding must be sRGB")
        _range(errors, path + ".roughness", material.get("roughness"))
        _range(errors, path + ".metallic", material.get("metallic"))
        _range(errors, path + ".normal_scale", material.get("normal_scale"), 0.0, 2.0)
        coat = material.get("clearcoat")
        if coat is not None:
            if not isinstance(coat, dict):
                errors.append(path + ".clearcoat must be an object")
            else:
                _range(errors, path + ".clearcoat.factor", coat.get("factor"))
                _range(errors, path + ".clearcoat.roughness", coat.get("roughness"))
        if "emissive_color_linear_srgb" in material:
            _vector(errors, path + ".emissive_color_linear_srgb",
                    material["emissive_color_linear_srgb"], 3, 0.0, None)
            _range(errors, path + ".emissive_strength",
                   material.get("emissive_strength"), 0.0, 100.0)

    palette = document.get("ball_identity_palette", {})
    if palette.get("color_space") != "linear_srgb_d65" or \
            palette.get("texture_encoding") != "srgb_iec61966_2_1":
        errors.append("ball_identity_palette must label linear factors and sRGB textures")
    for key in ("number_patch", "number_ink", "yellow", "blue", "red", "purple",
                "orange", "green", "burgundy", "black"):
        _vector(errors, "ball_identity_palette." + key, palette.get(key), 3, 0.0, 1.0)

    lighting = document.get("lighting", {})
    required_units = {
        "length_unit": "metre", "temperature_unit": "kelvin",
        "intensity_unit": "lumen", "illuminance_unit": "lux",
    }
    for key, expected in required_units.items():
        if lighting.get(key) != expected:
            errors.append("lighting.%s must be %s" % (key, expected))
    key_light = lighting.get("key_light", {})
    _vector(errors, "lighting.key_light.size_m", key_light.get("size_m"), 2, 0.01, 10.0)
    _vector(errors, "lighting.key_light.offset_from_play_surface_origin_m",
            key_light.get("offset_from_play_surface_origin_m"), 3, -10.0, 10.0)
    direction = key_light.get("direction_world_unit")
    _vector(errors, "lighting.key_light.direction_world_unit", direction, 3, -1.0, 1.0)
    if isinstance(direction, list) and len(direction) == 3 and all(_finite_number(v) for v in direction):
        if abs(math.sqrt(sum(v * v for v in direction)) - 1.0) > 1e-6:
            errors.append("lighting.key_light.direction_world_unit must be normalized")
    for field, lower, upper in (
            ("correlated_color_temperature_k", 1000, 12000),
            ("luminous_flux_lm", 1, 1000000)):
        value = key_light.get(field)
        if not _finite_number(value) or not lower <= value <= upper:
            errors.append("lighting.key_light.%s is outside the declared unit range" % field)
    illuminance = key_light.get("target_table_illuminance_lux", {})
    if not all(_finite_number(illuminance.get(key)) for key in ("minimum", "center", "maximum")):
        errors.append("lighting.key_light.target_table_illuminance_lux must be numeric")
    elif not illuminance["minimum"] <= illuminance["center"] <= illuminance["maximum"]:
        errors.append("target table illuminance must be ordered")
    _range(errors, "lighting.background_control.table_to_background_ev",
           lighting.get("background_control", {}).get("table_to_background_ev"), 0.0, 10.0)

    exposure = document.get("exposure", {})
    if exposure.get("exposure_unit") != "EV100":
        errors.append("exposure.exposure_unit must be EV100")
    for field in ("reference_ev100", "white_balance_k", "sdr_reference_white_nits",
                  "frame_highlight_clip_fraction_max", "frame_shadow_clip_fraction_max"):
        if not _finite_number(exposure.get(field)):
            errors.append("exposure.%s must be a finite number" % field)

    cameras = document.get("cameras")
    if not isinstance(cameras, dict):
        errors.append("cameras must be an object")
        cameras = {}
    missing_cameras = REQUIRED_CAMERAS - set(cameras)
    if missing_cameras:
        errors.append("missing cameras: " + ", ".join(sorted(missing_cameras)))
    for camera_id, camera in cameras.items():
        path = "cameras." + camera_id
        if camera.get("projection") not in {"perspective", "orthographic"}:
            errors.append(path + ".projection is invalid")
        if camera.get("projection") == "perspective":
            _range(errors, path + ".vertical_fov_degrees",
                   camera.get("vertical_fov_degrees"), 1.0, 179.0)
        else:
            _range(errors, path + ".orthographic_vertical_span_m",
                   camera.get("orthographic_vertical_span_m"), 0.1, 100.0)
        _range(errors, path + ".height_above_play_surface_m",
               camera.get("height_above_play_surface_m"), 0.0, 100.0)
        if not isinstance(camera.get("composition"), dict) or not camera["composition"]:
            errors.append(path + ".composition must be non-empty")
        if not isinstance(camera.get("readability"), dict) or not camera["readability"]:
            errors.append(path + ".readability must be non-empty")

    acceptance = document.get("ldr_interest_region_acceptance", {})
    if acceptance.get("resolution_px") != [1920, 1080]:
        errors.append("LDR acceptance resolution must be 1920x1080")
    if acceptance.get("output_color_space") != "srgb_iec61966_2_1_rgb8":
        errors.append("LDR acceptance output color space must be sRGB RGB8")
    scenes = acceptance.get("scenes")
    if not isinstance(scenes, dict):
        errors.append("ldr_interest_region_acceptance.scenes must be an object")
        scenes = {}
    missing_scenes = REQUIRED_SCENES - set(scenes)
    if missing_scenes:
        errors.append("missing LDR scenes: " + ", ".join(sorted(missing_scenes)))
    for scene_id, scene in scenes.items():
        path = "ldr_interest_region_acceptance.scenes." + scene_id
        if scene.get("camera_id") not in cameras:
            errors.append(path + ".camera_id references an unknown camera")
        regions = scene.get("regions")
        if not isinstance(regions, list) or len(regions) < 2:
            errors.append(path + ".regions must contain at least two regions")
            continue
        ids = set()
        for index, region in enumerate(regions):
            region_path = "%s.regions[%d]" % (path, index)
            region_id = region.get("id")
            if not isinstance(region_id, str) or not region_id:
                errors.append(region_path + ".id is required")
            elif region_id in ids:
                errors.append(region_path + ".id is duplicated")
            ids.add(region_id)
            rectangle = region.get("rect_normalized")
            _vector(errors, region_path + ".rect_normalized", rectangle, 4, 0.0, 1.0)
            if isinstance(rectangle, list) and len(rectangle) == 4 and \
                    all(_finite_number(item) for item in rectangle):
                if rectangle[2] <= 0 or rectangle[3] <= 0 or \
                        rectangle[0] + rectangle[2] > 1.0 + 1e-12 or \
                        rectangle[1] + rectangle[3] > 1.0 + 1e-12:
                    errors.append(region_path + ".rect_normalized exceeds the frame")
            checks = region.get("checks")
            if not isinstance(checks, dict) or set(checks) != REQUIRED_CHECKS:
                errors.append(region_path + ".checks has missing or unknown metrics")
                continue
            bounded = REQUIRED_CHECKS - {"minimum_contrast_to_surround"}
            for check in bounded:
                value = checks[check]
                if not _finite_number(value) or not 0.0 <= value <= 1.0:
                    errors.append(region_path + ".checks." + check + " must be in [0,1]")
            if checks["mean_luma_min"] > checks["mean_luma_max"]:
                errors.append(region_path + " mean luma bounds are reversed")
            contrast = checks["minimum_contrast_to_surround"]
            if not _finite_number(contrast) or contrast < 1.0:
                errors.append(region_path + " contrast must be at least 1")

    scope = document.get("m1_scope", {})
    if scope.get("graybox_allowed") is not True:
        errors.append("m1_scope.graybox_allowed must be true")
    if scope.get("physics_profile_change_allowed") is not False or \
            scope.get("legacy_asset_modification_allowed") is not False:
        errors.append("M1 lookdev may not change physics profiles or legacy assets")
    blocker = scope.get("production_high_mesh_blocked_by", "")
    if "table-visual-physics-alignment-contract.md" not in blocker:
        errors.append("production high mesh must remain blocked by the alignment contract")
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--schema", type=Path)
    arguments = parser.parse_args()
    schema_path = arguments.schema or arguments.manifest.with_name("lookdev_manifest.schema.json")
    try:
        document = json.loads(arguments.manifest.read_text(encoding="utf-8"))
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        print("lookdev validation error: " + str(error), file=sys.stderr)
        return 2
    if schema.get("$schema") != "https://json-schema.org/draft/2020-12/schema" or \
            schema.get("title") != "BilliardGL Lookdev Manifest v1":
        print("lookdev validation error: unexpected schema document", file=sys.stderr)
        return 2
    errors = validate(document)
    if errors:
        for error in errors:
            print("lookdev validation error: " + error, file=sys.stderr)
        return 1
    print("lookdev manifest valid: " + document["manifest_id"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
