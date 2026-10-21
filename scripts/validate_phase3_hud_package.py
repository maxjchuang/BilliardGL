#!/usr/bin/env python3
"""Offline validation for the original Phase-3 product HUD package."""

import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

from generate_phase3_hud_package import (
    COLORS, ICON_TOKENS, MANIFEST_NAME, MOCKUP_STATES, PACKAGE_ID,
    PALETTE_MODES, PROFILES, SHIPPING_TOKENS_NAME, SHIPPING_TOKEN_SET_ID,
    TOKEN_PROOF_NAME, build_shipping_tokens,
)


EXPECTED_STATES = {"default", "aiming", "post_shot", "foul", "help"}
EXPECTED_ELEMENTS = {
    "player_status", "power_meter", "tip_offset", "context_hint",
    "foul_banner", "shot_result", "help_panel",
}
MOCKUP_REQUIRED_ELEMENTS = {
    "default": {"player_status", "context_hint"},
    "aiming": {"player_status", "context_hint", "power_meter",
               "estimated_cue_speed", "tip_offset"},
    "post_shot_foul": {"player_status", "context_hint", "foul_banner", "shot_result"},
    "help": {"help_panel"},
}


def _sha256(payload):
    return "sha256:" + hashlib.sha256(payload).hexdigest()


def _hex_rgb(value):
    if not isinstance(value, str) or not re.fullmatch(r"#[0-9A-Fa-f]{6}([0-9A-Fa-f]{2})?", value):
        return None
    return tuple(int(value[index:index + 2], 16) / 255.0 for index in (1, 3, 5))


def _relative_luminance(rgb):
    def linear(value):
        return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4
    values = [linear(value) for value in rgb]
    return 0.2126 * values[0] + 0.7152 * values[1] + 0.0722 * values[2]


def _contrast(first, second):
    first_luminance, second_luminance = _relative_luminance(first), _relative_luminance(second)
    lighter, darker = max(first_luminance, second_luminance), min(first_luminance, second_luminance)
    return (lighter + 0.05) / (darker + 0.05)


def _rect_inside_safe(rect, profile):
    width, height = profile["viewport_px"]
    safe = profile["safe_area_px"]
    x, y, rect_width, rect_height = rect
    return (x >= safe["left"] and y >= safe["top"] and
            x + rect_width <= width - safe["right"] and
            y + rect_height <= height - safe["bottom"])


def _rect_overlap(first, second):
    return not (first[0] + first[2] <= second[0] or second[0] + second[2] <= first[0] or
                first[1] + first[3] <= second[1] or second[1] + second[3] <= first[1])


def _parse_svg(path, errors):
    try:
        payload = path.read_bytes()
        root = ET.fromstring(payload)
    except (OSError, ET.ParseError) as error:
        errors.append("cannot parse %s: %s" % (path.name, error))
        return None, b""
    for element in root.iter():
        for key, value in element.attrib.items():
            if key.endswith("href") or "http://" in value or "https://" in value:
                errors.append(path.name + " contains an external reference")
    return root, payload


def _validate_shipping_increment(manifest, asset_dir, verify_hashes, errors):
    files = manifest.get("files", {})
    token_entry = files.get("shipping_tokens", {})
    proof_entry = files.get("flat_token_proof", {})
    token_path = asset_dir / Path(token_entry.get("path", "")).name
    proof_path = asset_dir / Path(proof_entry.get("path", "")).name
    if token_path.name != SHIPPING_TOKENS_NAME or proof_path.name != TOKEN_PROOF_NAME:
        errors.append("shipping token/proof file identity is invalid")
        return 0, 0
    try:
        token_bytes = token_path.read_bytes()
        tokens = json.loads(token_bytes.decode("utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        errors.append("cannot read shipping token set: %s" % error)
        return 0, 0
    if verify_hashes and token_entry.get("sha256") != _sha256(token_bytes):
        errors.append("shipping token set SHA-256 differs")
    if tokens != build_shipping_tokens():
        errors.append("shipping token geometry/palette contract differs")
    if tokens.get("token_set_id") != SHIPPING_TOKEN_SET_ID or \
            tokens.get("external_assets") is not False:
        errors.append("shipping token identity/authorship is invalid")
    gpu = tokens.get("gpu_contract", {})
    if gpu.get("coordinate_space") != "normalized_24x24_top_left_y_down" or \
            gpu.get("allowed_primitives") != ["rect", "circle", "polyline", "polygon"] or \
            gpu.get("font_backend") != "existing_freetype_sdf_atlas" or \
            gpu.get("svg_runtime_dependency") is not False:
        errors.append("shipping GPU primitive contract is invalid")
    palettes = tokens.get("palette_modes", {})
    if palettes != PALETTE_MODES or set(palettes) != {"standard", "color_vision_safe"}:
        errors.append("standard/color-vision-safe palette modes are required")
    for palette_name, palette in palettes.items():
        background = _hex_rgb(palette.get("surface_strong"))
        for role in ("text_primary", "text_secondary", "warning", "destructive"):
            foreground = _hex_rgb(palette.get(role))
            if foreground is None or background is None or _contrast(
                    foreground, background) < 4.5:
                errors.append("%s %s contrast is below 4.5:1" % (palette_name, role))
    accessibility = tokens.get("palette_accessibility", {})
    if accessibility.get("never_color_only") is not True or \
            set(accessibility.get("required_redundant_channels", [])) != \
            {"shape", "text", "value_or_pattern"}:
        errors.append("shipping palette accessibility is color-only")
    typography = tokens.get("typography", {})
    if typography.get("backend") != "existing_freetype_sdf_atlas" or \
            typography.get("minimum_logical_px", 0) < 13 or \
            typography.get("retina_minimum_physical_px", 0) < 21:
        errors.append("shipping typography/SDF minimum sizes are invalid")
    layout = tokens.get("layout_contract", {})
    if layout.get("required_viewports") != [[1280, 720], [1920, 1080]] or \
            layout.get("retina_case") != {"logical_px": [1280, 720],
                                           "physical_px": [1920, 1080], "scale": 1.5}:
        errors.append("shipping 720p/reference/Retina layout contract is invalid")
    states = tokens.get("state_icon_usage", {})
    if set(states) != {"default", "aiming", "post_shot", "foul", "help"}:
        errors.append("shipping icon state coverage is incomplete")
    icons = tokens.get("icon_tokens", {})
    required_icons = {"power_bars", "tip_target", "camera_frame", "warning_triangle",
                      "help_circle", "group_solid_filled_circle",
                      "group_stripe_banded_ring", "group_open_diamond",
                      "tip_ball_outline", "tip_position_marker"}
    if not required_icons.issubset(icons):
        errors.append("power/tip/camera/foul/help/group icon geometry is incomplete")
    allowed_primitives = {"rect", "circle", "polyline", "polygon"}
    roles = set(next(iter(PALETTE_MODES.values()))) | {"none"}
    for token, icon in icons.items():
        commands = icon.get("commands", [])
        if not commands:
            errors.append("icon %s has no GPU commands" % token)
        for command in commands:
            if command.get("primitive") not in allowed_primitives or \
                    command.get("stroke_role", "none") not in roles or \
                    command.get("fill_role", "none") not in roles:
                errors.append("icon %s has an invalid primitive/color role" % token)
            numbers = []
            for key in ("x", "y", "width", "height", "cx", "cy", "radius"):
                if key in command: numbers.append(command[key])
            numbers.extend(value for point in command.get("points", []) for value in point)
            if any(not isinstance(value, (int, float)) or value < 0 or value > 24
                   for value in numbers):
                errors.append("icon %s exits normalized 24x24 bounds" % token)
    aliases = tokens.get("aliases", {})
    if any(target not in icons for target in aliases.values()):
        errors.append("shipping icon alias references missing geometry")
    debug = tokens.get("debug_isolation", {})
    if debug.get("product_token_namespace") == debug.get("debug_token_namespace") or \
            debug.get("shared_layout_nodes") is not False or \
            debug.get("debug_default_visible") is not False or \
            debug.get("debug_forbidden_in_release_capture") is not True:
        errors.append("shipping/debug token namespaces are not isolated")

    root, proof_bytes = _parse_svg(proof_path, errors)
    if root is not None:
        if root.get("width") != "1280" or root.get("height") != "720" or \
                root.get("data-proof-kind") != "flat-token-proof" or \
                root.get("data-runtime-capture") != "false" or \
                root.get("data-user-research") != "false":
            errors.append("flat token proof metadata must deny runtime/research evidence")
        proof_nodes = [node for node in root.iter() if node.get("data-icon-token")]
        if len(proof_nodes) != 12 or \
                {node.get("data-palette-mode") for node in proof_nodes} != \
                {"standard", "color_vision_safe"} or \
                {node.get("data-state") for node in proof_nodes} != \
                {"default", "aiming", "post-shot", "foul", "help"}:
            errors.append("flat token proof palette/state coverage is incomplete")
        if verify_hashes and proof_entry.get("sha256") != _sha256(proof_bytes):
            errors.append("flat token proof SHA-256 differs")
    if proof_entry.get("runtime_capture") is not False or \
            proof_entry.get("user_research") is not False:
        errors.append("flat token proof cannot claim GPU capture or user research")
    return len(icons), len(palettes)


def validate(manifest, asset_dir, verify_hashes=True):
    errors = []
    asset_dir = Path(asset_dir)
    if manifest.get("schema_version") != 1 or manifest.get("manifest_id") != PACKAGE_ID:
        errors.append("HUD manifest identity/schema is invalid")
    if manifest.get("authorship") != "original_billiardgl_product_ui" or \
            manifest.get("external_assets") is not False:
        errors.append("HUD package must be original and self-contained")

    audit = manifest.get("shipping_visual_audit", {})
    if audit.get("layout_state_contract") != "PASS_EXISTING" or \
            audit.get("responsive_safe_area_contract") != "PASS_EXISTING" or \
            audit.get("freetype_sdf_contract") != "PASS_EXISTING" or \
            audit.get("opaque_icon_token_geometry_before_increment") != \
            "BLOCKING_GAP_CLOSED" or \
            audit.get("standard_and_color_vision_palette_runtime_tokens") != \
            "BLOCKING_GAP_CLOSED" or \
            audit.get("flat_proof_is_gpu_capture") is not False or \
            audit.get("flat_proof_is_user_research") is not False:
        errors.append("shipping visual audit status/evidence boundary is invalid")
    shipping = manifest.get("shipping_token_set", {})
    if shipping.get("token_set_id") != SHIPPING_TOKEN_SET_ID or \
            shipping.get("required_state_coverage") != \
            ["default", "aiming", "post_shot", "foul", "help"] or \
            shipping.get("required_palette_modes") != \
            ["standard", "color_vision_safe"]:
        errors.append("shipping token set binding is invalid")

    profiles = manifest.get("profiles", {})
    if set(profiles) != set(PROFILES):
        errors.append("HUD must define the 1920x1080 and 1024x768 profiles")
    for name, expected in PROFILES.items():
        profile = profiles.get(name, {})
        for key in ("viewport_px", "safe_area_px", "breakpoint", "font_px"):
            if profile.get(key) != expected[key]:
                errors.append("%s %s differs from the approved profile" % (name, key))
        if profile.get("minimum_text_px") != expected["font_px"]["caption"] or \
                profile.get("minimum_text_px", 0) < 13:
            errors.append(name + " minimum text size is invalid")
        rectangles = profile.get("element_rects_px", {})
        if set(rectangles) != {"player_status", "context_hint", "power_meter", "tip_offset",
                              "foul_banner", "shot_result", "help_panel"}:
            errors.append(name + " element rectangle set is incomplete")
        for element_name, rect in rectangles.items():
            if not isinstance(rect, list) or len(rect) != 4 or not all(
                    isinstance(value, (int, float)) and value >= 0 for value in rect):
                errors.append("%s %s rectangle is invalid" % (name, element_name))
            elif not _rect_inside_safe(rect, profile):
                errors.append("%s %s exits the safe area" % (name, element_name))
        for state, elements in {
                "default": ("player_status", "context_hint"),
                "aiming": ("player_status", "context_hint", "power_meter", "tip_offset"),
                "post_shot_foul": ("player_status", "context_hint", "foul_banner", "shot_result"),
                "help": ("help_panel",)}.items():
            for index, first_name in enumerate(elements):
                for second_name in elements[index + 1:]:
                    if first_name in rectangles and second_name in rectangles and \
                            _rect_overlap(rectangles[first_name], rectangles[second_name]):
                        errors.append("%s %s elements %s/%s overlap" %
                                      (name, state, first_name, second_name))

    scaling = manifest.get("dpi_scaling", {})
    expected_scaling = {
        "logical_reference_dpi": 96,
        "os_density_scale_formula": "clamp(os_dpi / 96.0, 0.85, 1.50)",
        "user_scale_range": [0.85, 1.35],
        "resolved_scale_formula": "os_density_scale * user_scale",
        "pixel_snapping": "rect_edges_and_one_pixel_strokes_after_scale",
        "profile_selection": "compact_1024x768 when aspect_ratio < 1.55 or width < 1280",
    }
    for key, expected in expected_scaling.items():
        if scaling.get(key) != expected:
            errors.append("DPI scaling %s differs" % key)

    responsive_cases = manifest.get("responsive_validation_cases", {})
    expected_cases = {"minimum_wide_1280x720_96dpi", "reference_1920x1080_96dpi",
                      "high_dpi_1920x1080_144dpi"}
    if set(responsive_cases) != expected_cases:
        errors.append("1280x720, 1920x1080, and high-DPI validation cases are required")
    for case_name, case in responsive_cases.items():
        physical = case.get("physical_viewport_px")
        logical = case.get("logical_viewport_px")
        dpi = case.get("os_dpi")
        user_scale = case.get("user_scale")
        resolved = case.get("resolved_scale")
        expected_scale = min(1.50, max(0.85, dpi / 96.0)) * user_scale \
            if isinstance(dpi, (int, float)) and isinstance(user_scale, (int, float)) else None
        if not isinstance(physical, list) or len(physical) != 2 or \
                not isinstance(logical, list) or len(logical) != 2 or \
                expected_scale is None or abs(resolved - expected_scale) > 1e-9 or \
                any(abs(logical[index] * resolved - physical[index]) > 1.0 for index in range(2)):
            errors.append(case_name + " physical/logical/DPI scale proof is invalid")
            continue
        if case.get("selected_profile") != "wide_1920x1080" or \
                case.get("minimum_text_physical_px", 0) < 13:
            errors.append(case_name + " profile or minimum text proof is invalid")
        case_profile = {"viewport_px": physical,
                        "safe_area_px": case.get("safe_area_physical_px", {})}
        rectangles = case.get("element_rects_physical_px", {})
        if set(rectangles) != {"player_status", "context_hint", "power_meter", "tip_offset",
                              "foul_banner", "shot_result", "help_panel"}:
            errors.append(case_name + " responsive element rectangle set is incomplete")
            continue
        for element_name, rect in rectangles.items():
            if not _rect_inside_safe(rect, case_profile):
                errors.append("%s %s exits physical safe area" % (case_name, element_name))
        active_sets = case.get("active_state_sets", {})
        if set(active_sets) != {"default", "aiming", "post_shot_foul", "help"}:
            errors.append(case_name + " active-state overlap proof is incomplete")
            continue
        for state_name, active in active_sets.items():
            for index, first_name in enumerate(active):
                for second_name in active[index + 1:]:
                    if _rect_overlap(rectangles[first_name], rectangles[second_name]):
                        errors.append("%s %s overlaps %s/%s" %
                                      (case_name, state_name, first_name, second_name))

    typography = manifest.get("typography", {})
    if typography.get("minimum_px", 0) < 13 or typography.get("weights") != [400, 500] or \
            typography.get("contrast_ratio_text_min") != 4.5 or \
            typography.get("numeric_style") != "tabular_figures":
        errors.append("typography hierarchy/contrast/numeric policy is invalid")
    colors = manifest.get("colors", {})
    if colors != COLORS:
        errors.append("HUD color token set differs from approved palette")
    for pair in manifest.get("contrast_pairs", []):
        foreground = _hex_rgb(colors.get(pair.get("foreground")))
        background = _hex_rgb(colors.get(pair.get("background")))
        if foreground is None or background is None or \
                _contrast(foreground, background) + 1e-6 < pair.get("minimum", 100):
            errors.append("contrast pair %s/%s does not meet %.1f:1" %
                          (pair.get("foreground"), pair.get("background"),
                           pair.get("minimum", 0)))

    states = manifest.get("states", {})
    if set(states) != EXPECTED_STATES:
        errors.append("default/aiming/post_shot/foul/help states are required")
    for state_name, state in states.items():
        visible = state.get("visible_elements", [])
        if len(visible) != len(set(visible)) or any(
                element not in EXPECTED_ELEMENTS and element != "estimated_cue_speed"
                for element in visible):
            errors.append(state_name + " visible element list is invalid")
    if states.get("help", {}).get("pauses_shot_input") is not True:
        errors.append("help state must pause shot input")

    elements = manifest.get("product_elements", {})
    if set(elements) != EXPECTED_ELEMENTS:
        errors.append("product element specification is incomplete")
    player = elements.get("player_status", {})
    if player.get("fields") != ["current_player", "ball_group", "target_ball", "target_pocket"] or \
            not {"text_label", "filled_or_striped_symbol", "ball_number", "color"}.issubset(
                set(player.get("group_encoding", []))):
        errors.append("player/group/target encoding is incomplete")
    power = elements.get("power_meter", {})
    if power.get("speed_label_prefix") != "EST." or \
            power.get("speed_is_authoritative_physics") is not False or \
            not {"tick_marks", "numeric_percent", "numeric_speed_with_unit"}.issubset(
                set(power.get("encodings", []))):
        errors.append("power and estimated cue-speed encoding is invalid")
    tip = elements.get("tip_offset", {})
    if tip.get("coordinate_unit") != "ball_radius_R" or \
            tip.get("range") != {"x": [-1.0, 1.0], "y": [-1.0, 1.0]} or \
            "numeric_xy" not in tip.get("encodings", []):
        errors.append("tip-offset control specification is invalid")
    if elements.get("context_hint", {}).get("maximum_visible_seconds", 100) > 3.0:
        errors.append("camera/assist short hint may not persist over three seconds")

    accessibility = manifest.get("color_vision_accessibility", {})
    if accessibility.get("never_color_only") is not True or \
            accessibility.get("solid_group", {}).get("shape") == \
            accessibility.get("stripe_group", {}).get("shape") or \
            accessibility.get("target_always_shows_ball_number") is not True or \
            accessibility.get("foul_always_shows_icon_and_text") is not True:
        errors.append("color-vision accessibility cannot rely on color alone")

    prompts = manifest.get("input_prompts", {})
    required_prompts = {"aim_toggle", "power_charge_release", "tip_offset", "camera_orbit",
                        "camera_zoom", "camera_preset", "help", "dismiss"}
    if set(prompts.get("keyboard_mouse", {})) != required_prompts or \
            prompts.get("maximum_simultaneous_prompts") != 3:
        errors.append("keyboard/mouse prompt specification is incomplete")

    onboarding = manifest.get("first_shot_path_30s", [])
    expected_start = 0
    for step in onboarding:
        seconds = step.get("seconds")
        if not isinstance(seconds, list) or len(seconds) != 2 or seconds[0] != expected_start or \
                seconds[1] <= seconds[0]:
            errors.append("30-second first-shot path contains a gap or invalid range")
            break
        expected_start = seconds[1]
    if expected_start != 30 or len(onboarding) != 6:
        errors.append("first-shot path must cover exactly 0-30 seconds in six steps")
    proxies = manifest.get("first_shot_automation_proxies", {})
    expected_proxy_values = {
        "scenario_id": "hud_first_shot_keyboard_mouse_v1",
        "deadline_ms": 30000,
        "status_card_visible_by_ms": 500,
        "aim_state_entered_by_ms": 11000,
        "aim_controls_visible_after_tab_max_ms": 100,
        "power_samples_while_holding_min": 3,
        "power_percent_monotonic_during_single_hold": True,
        "estimated_speed_label_and_mps_unit_required": True,
        "tip_offset_step_optional": True,
        "shot_release_command_count": 1,
        "shot_released_by_ms": 30000,
        "post_shot_state_after_release_max_ms": 250,
        "maximum_simultaneous_prompts": 3,
        "automatic_aim_or_shot_forbidden": True,
        "required_event_order": ["hud_default_visible", "aim_toggled", "power_hold_started",
                                 "power_sampled", "shot_released", "hud_post_shot_visible"],
    }
    for key, expected in expected_proxy_values.items():
        if proxies.get(key) != expected:
            errors.append("first-shot automation proxy %s differs" % key)

    debug = manifest.get("debug_overlay", {})
    if debug.get("separate_overlay_id") != "billiardgl_debug_telemetry_v1" or \
            debug.get("default_visible") is not False or \
            debug.get("shares_product_layout_nodes") is not False or \
            debug.get("forbidden_in_release_capture") is not True:
        errors.append("debug telemetry must remain an independent hidden overlay")
    placeholders = manifest.get("capability_gated_placeholders", {})
    if set(placeholders) != {"cue_elevation", "chalk"}:
        errors.append("only cue elevation and chalk gated placeholders are approved")
    for name, placeholder in placeholders.items():
        if placeholder.get("default_visible") is not False or len(placeholder.get("visible_if", [])) != 2 or \
                not any("physics." in capability for capability in placeholder.get("visible_if", [])) or \
                placeholder.get("absence_behavior") != "omit_node_no_disabled_control":
            errors.append(name + " must stay hidden until capability and physics promotion")

    files = manifest.get("files", {}).get("mockups", {})
    if set(files) != set(PROFILES):
        errors.append("mockup profile file map is incomplete")
    mockup_count = 0
    for profile_name, profile in PROFILES.items():
        entries = files.get(profile_name, {})
        if set(entries) != set(MOCKUP_STATES):
            errors.append(profile_name + " must provide four review-state mockups")
            continue
        width, height = profile["viewport_px"]
        safe = profile["safe_area_px"]
        for state, entry in entries.items():
            path = asset_dir / Path(entry.get("path", "")).name
            root, payload = _parse_svg(path, errors)
            if root is None:
                continue
            mockup_count += 1
            expected_safe = "%d,%d,%d,%d" % (
                safe["left"], safe["top"], safe["right"], safe["bottom"])
            if root.get("width") != str(width) or root.get("height") != str(height) or \
                    root.get("viewBox") != "0 0 %d %d" % (width, height) or \
                    root.get("data-profile") != profile_name or root.get("data-state") != state or \
                    root.get("data-safe-area") != expected_safe:
                errors.append(path.name + " viewport/profile/safe metadata differs")
            actual_elements = {element.get("data-element") for element in root.iter()
                               if element.get("data-element")}
            missing = MOCKUP_REQUIRED_ELEMENTS[state] - actual_elements
            if missing:
                errors.append(path.name + " lacks elements: " + ", ".join(sorted(missing)))
            lowered = payload.lower()
            if b"debug" in lowered or b"cue elevation" in lowered or b"chalk" in lowered:
                errors.append(path.name + " exposes debug or unpromoted controls")
            if state == "help" and not all(token in payload for token in
                                            (b"TAB", b"MOUSE", b"WHEEL", b"ARROWS",
                                             b"SPACE", b"Q / E", b"F1 / ESC")):
                errors.append(path.name + " help input coverage is incomplete")
            if verify_hashes and entry.get("sha256") != _sha256(payload):
                errors.append(path.name + " SHA-256 differs")

    shipping_icon_count, shipping_palette_count = _validate_shipping_increment(
        manifest, asset_dir, verify_hashes, errors)
    report = {"profiles": len(profiles), "states": len(states), "mockups": mockup_count,
              "product_elements": len(elements), "first_shot_seconds": expected_start,
              "responsive_cases": len(responsive_cases),
              "shipping_icons": shipping_icon_count,
              "shipping_palettes": shipping_palette_count,
              "minimum_text_px": min(profile.get("minimum_text_px", 0)
                                      for profile in profiles.values()) if profiles else 0}
    return errors, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", nargs="?", type=Path,
                        default=Path("assets/modern/hud") / MANIFEST_NAME)
    arguments = parser.parse_args()
    try:
        manifest = json.loads(arguments.manifest.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        print("HUD validation failed: " + str(error), file=sys.stderr)
        return 2
    errors, report = validate(manifest, arguments.manifest.parent)
    if errors:
        for error in errors:
            print("error: " + error, file=sys.stderr)
        return 1
    print("Phase3 HUD valid: profiles=%d states=%d mockups=%d product_elements=%d "
          "first_shot_seconds=%d responsive_cases=%d shipping_icons=%d "
          "shipping_palettes=%d minimum_text_px=%d" %
          (report["profiles"], report["states"], report["mockups"],
           report["product_elements"], report["first_shot_seconds"],
           report["responsive_cases"],
           report["shipping_icons"], report["shipping_palettes"],
           report["minimum_text_px"]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
