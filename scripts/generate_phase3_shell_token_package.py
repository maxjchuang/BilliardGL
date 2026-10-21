#!/usr/bin/env python3
"""Generate the original, standalone ProductShell vector-token package."""

from __future__ import annotations

import argparse
import hashlib
import html
import json
from pathlib import Path


TOKEN_FILE = "focused_practice_shell_shipping_tokens_v1.json"
PROOF_FILE = "focused_practice_shell_shipping_tokens_flat_proof_v1.svg"
MANIFEST_FILE = "focused_practice_shell_shipping_tokens_v1.manifest.json"
TOKEN_SET_ID = "focused_practice_shell_shipping_tokens_v1"
MANIFEST_ID = "focused_practice_shell_shipping_assets_v1"

PALETTES = {
    "standard": {
        "surface": "#10181E", "text_primary": "#F4F7F8",
        "text_secondary": "#C1CBD0", "icon_primary": "#F4F7F8",
        "accent": "#31B7C8", "focus": "#FFD166",
        "error": "#FF8C69", "disabled": "#9AA5AA",
    },
    "color_vision_safe": {
        "surface": "#10181E", "text_primary": "#F4F7F8",
        "text_secondary": "#C1CBD0", "icon_primary": "#F4F7F8",
        "accent": "#5DD9C1", "focus": "#FFD166",
        "error": "#FF9F80", "disabled": "#AAB2B6",
    },
}

SCREEN_USAGE = {
    "main_menu": ["single_player_badge", "training_target", "replay_triangle",
                  "settings_sliders", "help_circle", "exit_door"],
    "single_player_setup": ["rules_shield", "ai_diamond", "play_triangle"],
    "match": ["rules_shield", "settings_sliders", "help_circle"],
    "training_catalog": ["training_target", "empty_box"],
    "training_session": ["training_target", "reset_arrow", "catalog_grid"],
    "replay_browser": ["replay_file", "empty_box"],
    "replay_playback": ["play_triangle", "pause_bars", "speed_down", "speed_up",
                        "camera_frame", "seek_previous", "seek_next", "replay_file"],
    "settings": ["assist_rings", "mouse_axis", "vertical_arrows", "keyboard_outline",
                 "text_scale", "pattern_circle", "caption_box", "music_note",
                 "venue_waves", "impact_burst", "ui_click", "camera_frame",
                 "distance_arrow", "glow_circle", "focus_brackets", "camera_motion",
                 "shake_lines"],
    "help": ["aim_crosshair", "power_bars", "cue_ball", "camera_frame",
             "rules_shield", "back_arrow"],
}

FIXED_STATE_USAGE = {
    "error": ["error_cross_octagon"],
    "fallback_item": ["empty_box", "item_square"],
    "selected": ["selected_check_diamond"],
    "focus": ["focus_chevron"],
}


def rect(x, y, width, height, role="none", stroke="icon_primary", stroke_width=2):
    return {"primitive": "rect", "x": x, "y": y, "width": width,
            "height": height, "fill_role": role, "stroke_role": stroke,
            "stroke_width": stroke_width}


def circle(cx, cy, radius, role="none", stroke="icon_primary", stroke_width=2):
    return {"primitive": "circle", "cx": cx, "cy": cy, "radius": radius,
            "fill_role": role, "stroke_role": stroke,
            "stroke_width": stroke_width}


def line(points, stroke="icon_primary", width=2, closed=False, fill="none"):
    return {"primitive": "polygon" if closed else "polyline", "points": points,
            "fill_role": fill, "stroke_role": stroke, "stroke_width": width,
            "line_cap": "round", "line_join": "round"}


def filled_rect(x, y, width, height, role="icon_primary"):
    return {"primitive": "rect", "x": x, "y": y, "width": width,
            "height": height, "fill_role": role}


def icon_commands():
    """Return deterministic original 24x24 geometry for every emitted shell token."""
    icons = {
        "aim_crosshair": [circle(12, 12, 7), line([[2, 12], [7, 12]]),
                          line([[17, 12], [22, 12]]), line([[12, 2], [12, 7]]),
                          line([[12, 17], [12, 22]]), circle(12, 12, 1, "accent", "accent", 1)],
        "ai_diamond": [line([[12, 2], [22, 12], [12, 22], [2, 12]], "accent", 2, True),
                       circle(9, 11, 1, "icon_primary", "icon_primary", 1),
                       circle(15, 11, 1, "icon_primary", "icon_primary", 1),
                       line([[9, 16], [12, 18], [15, 16]])],
        "assist_rings": [circle(12, 12, 9), circle(12, 12, 6, "none", "accent"),
                         circle(12, 12, 2, "focus", "focus", 1)],
        "back_arrow": [line([[11, 5], [4, 12], [11, 19]], "accent", 2.5),
                       line([[5, 12], [21, 12]], "accent", 2.5)],
        "camera_frame": [line([[8, 4], [4, 4], [4, 8]]),
                         line([[16, 4], [20, 4], [20, 8]]),
                         line([[4, 16], [4, 20], [8, 20]]),
                         line([[20, 16], [20, 20], [16, 20]]),
                         circle(12, 12, 4, "none", "accent")],
        "camera_motion": [rect(5, 7, 12, 10), circle(11, 12, 3, "none", "accent"),
                          line([[3, 4], [7, 2], [11, 4]], "focus"),
                          line([[15, 20], [19, 22], [22, 19]], "focus")],
        "caption_box": [rect(3, 5, 18, 14), line([[6, 10], [11, 10]], "accent"),
                        line([[13, 10], [18, 10]], "accent"),
                        line([[6, 14], [10, 14]], "accent"),
                        line([[12, 14], [18, 14]], "accent")],
        "catalog_grid": [rect(3, 3, 7, 7, "accent", "accent", 1),
                         rect(14, 3, 7, 7), rect(3, 14, 7, 7), rect(14, 14, 7, 7)],
        "cue_ball": [circle(12, 12, 9, "none", "icon_primary"),
                     circle(9, 8, 2, "accent", "accent", 1)],
        "distance_arrow": [line([[4, 12], [20, 12]], "accent", 2.5),
                           line([[8, 8], [4, 12], [8, 16]], "accent", 2.5),
                           line([[16, 8], [20, 12], [16, 16]], "accent", 2.5)],
        "empty_box": [rect(3, 4, 18, 16, "none", "disabled"),
                      line([[7, 12], [17, 12]], "disabled", 2)],
        "error_cross_octagon": [line([[8, 2], [16, 2], [22, 8], [22, 16], [16, 22],
                                       [8, 22], [2, 16], [2, 8]], "error", 2, True),
                                line([[8, 8], [16, 16]], "error", 2.5),
                                line([[16, 8], [8, 16]], "error", 2.5)],
        "exit_door": [rect(4, 3, 11, 18), circle(12, 12, 1, "accent", "accent", 1),
                      line([[13, 12], [22, 12]], "focus", 2),
                      line([[18, 8], [22, 12], [18, 16]], "focus", 2)],
        "focus_brackets": [line([[8, 3], [3, 3], [3, 8]], "focus"),
                           line([[16, 3], [21, 3], [21, 8]], "focus"),
                           line([[3, 16], [3, 21], [8, 21]], "focus"),
                           line([[21, 16], [21, 21], [16, 21]], "focus")],
        "focus_chevron": [line([[7, 4], [15, 12], [7, 20]], "focus", 3)],
        "glow_circle": [circle(12, 12, 9, "none", "accent", 1),
                        circle(12, 12, 6, "none", "focus", 1),
                        circle(12, 12, 2, "focus", "focus", 1)],
        "help_circle": [circle(12, 12, 9, "none", "focus"),
                        line([[8, 9], [10, 7], [14, 7], [16, 9], [16, 11],
                              [12, 14], [12, 16]], "focus"),
                        circle(12, 19, 1, "focus", "focus", 1)],
        "impact_burst": [line([[12, 2], [14, 8], [21, 5], [17, 11], [22, 14],
                                [15, 15], [16, 22], [11, 17], [6, 21], [8, 14],
                                [2, 12], [8, 9]], "accent", 2, True, "none")],
        "item_square": [rect(4, 4, 16, 16, "none", "accent")],
        "keyboard_outline": [rect(2, 5, 20, 14),
                             filled_rect(5, 8, 2, 2, "accent"),
                             filled_rect(9, 8, 2, 2, "accent"),
                             filled_rect(13, 8, 2, 2, "accent"),
                             filled_rect(17, 8, 2, 2, "accent"),
                             rect(6, 13, 12, 3, "none", "accent", 1)],
        "mouse_axis": [rect(7, 3, 10, 18), line([[12, 3], [12, 10]], "accent"),
                       circle(12, 7, 1, "focus", "focus", 1),
                       line([[3, 12], [6, 9], [3, 12], [6, 15]], "accent"),
                       line([[21, 12], [18, 9], [21, 12], [18, 15]], "accent")],
        "music_note": [line([[14, 4], [14, 16], [19, 14], [19, 3], [14, 4]], "accent", 2),
                       circle(10, 18, 3, "accent", "accent", 1),
                       circle(16, 16, 3, "accent", "accent", 1)],
        "pattern_circle": [circle(12, 12, 9), line([[5, 8], [19, 8]], "accent"),
                           line([[3, 12], [21, 12]], "accent"),
                           line([[5, 16], [19, 16]], "accent")],
        "pause_bars": [filled_rect(6, 4, 4, 16, "accent"),
                       filled_rect(14, 4, 4, 16, "accent")],
        "play_triangle": [line([[6, 3], [20, 12], [6, 21]], "accent", 2, True, "accent")],
        "power_bars": [filled_rect(3, 15, 3, 6, "accent"),
                       filled_rect(8, 11, 3, 10, "accent"),
                       filled_rect(13, 7, 3, 14, "focus"),
                       filled_rect(18, 3, 3, 18, "focus")],
        "replay_file": [rect(4, 2, 14, 20), line([[14, 2], [20, 8], [14, 8], [14, 2]]),
                        line([[9, 10], [16, 14], [9, 18]], "accent", 2, True, "accent")],
        "replay_triangle": [line([[8, 6], [18, 12], [8, 18]], "accent", 2, True, "accent"),
                            line([[5, 5], [3, 8], [3, 16], [5, 19]], "focus")],
        "reset_arrow": [line([[18, 7], [18, 3], [22, 3]], "focus"),
                        line([[18, 4], [14, 2], [8, 3], [4, 7], [3, 13], [6, 18],
                              [11, 21], [17, 19]], "accent", 2)],
        "rules_shield": [line([[12, 2], [20, 5], [19, 14], [12, 22], [5, 14], [4, 5]],
                               "accent", 2, True),
                         line([[8, 12], [11, 15], [16, 9]], "focus", 2)],
        "seek_next": [line([[5, 5], [15, 12], [5, 19]], "accent", 2, True, "accent"),
                      filled_rect(18, 5, 2, 14, "focus")],
        "seek_previous": [line([[19, 5], [9, 12], [19, 19]], "accent", 2, True, "accent"),
                          filled_rect(4, 5, 2, 14, "focus")],
        "selected_check_diamond": [line([[12, 2], [22, 12], [12, 22], [2, 12]],
                                        "focus", 2, True),
                                   line([[7, 12], [10, 15], [17, 8]], "focus", 2.5)],
        "settings_sliders": [line([[4, 6], [20, 6]], "accent"),
                             line([[4, 12], [20, 12]], "accent"),
                             line([[4, 18], [20, 18]], "accent"),
                             circle(9, 6, 2, "surface", "focus"),
                             circle(15, 12, 2, "surface", "focus"),
                             circle(7, 18, 2, "surface", "focus")],
        "shake_lines": [line([[3, 8], [6, 5], [9, 9], [12, 5], [15, 9], [18, 5], [21, 8]],
                             "accent", 2),
                        line([[3, 16], [6, 13], [9, 17], [12, 13], [15, 17], [18, 13], [21, 16]],
                             "focus", 2)],
        "single_player_badge": [circle(12, 12, 9, "none", "accent"),
                                circle(12, 9, 3, "focus", "focus", 1),
                                line([[6, 19], [8, 15], [12, 14], [16, 15], [18, 19]], "focus")],
        "speed_down": [line([[4, 8], [12, 16], [20, 8]], "accent", 2.5),
                       line([[6, 20], [18, 20]], "focus", 2)],
        "speed_up": [line([[4, 16], [12, 8], [20, 16]], "accent", 2.5),
                     line([[12, 3], [12, 7]], "focus", 2),
                     line([[9, 5], [15, 5]], "focus", 2)],
        "text_scale": [rect(3, 4, 18, 16), line([[6, 8], [18, 8]], "accent"),
                       line([[9, 8], [9, 17]], "accent", 2.5),
                       line([[15, 8], [15, 14]], "focus", 2)],
        "training_target": [circle(12, 12, 9), circle(12, 12, 5, "none", "accent"),
                            circle(12, 12, 2, "focus", "focus", 1)],
        "ui_click": [line([[5, 3], [18, 12], [13, 14], [16, 21], [12, 22],
                            [9, 15], [5, 18]], "accent", 2, True),
                     line([[18, 3], [18, 7]], "focus"),
                     line([[20, 9], [23, 9]], "focus")],
        "venue_waves": [line([[3, 7], [7, 5], [11, 7], [15, 5], [21, 7]], "accent"),
                        line([[3, 12], [7, 10], [11, 12], [15, 10], [21, 12]], "accent"),
                        line([[3, 17], [7, 15], [11, 17], [15, 15], [21, 17]], "focus")],
        "vertical_arrows": [line([[12, 3], [7, 8], [12, 3], [17, 8]], "accent", 2.5),
                            line([[12, 3], [12, 21]], "accent", 2.5),
                            line([[7, 16], [12, 21], [17, 16]], "accent", 2.5)],
    }
    return icons


def _usage_for(token):
    screens = sorted(name for name, tokens in SCREEN_USAGE.items() if token in tokens)
    states = []
    if screens:
        states.extend(["normal", "disabled"])
    states.extend(name for name, tokens in FIXED_STATE_USAGE.items() if token in tokens)
    return {"screens": screens or ["all_screens"], "states": sorted(set(states))}


def build_token_set():
    icons = {name: {"commands": commands, "usage": _usage_for(name)}
             for name, commands in sorted(icon_commands().items())}
    all_item_shapes = sorted({token for tokens in SCREEN_USAGE.values() for token in tokens})
    state_usage = dict(FIXED_STATE_USAGE)
    state_usage["disabled"] = all_item_shapes
    state_usage["normal"] = all_item_shapes
    return {
        "schema_version": 1,
        "token_set_id": TOKEN_SET_ID,
        "namespace": "product_shell",
        "authorship": "original_billiardgl_vector_tokens",
        "external_assets": [],
        "gpu_contract": {
            "coordinate_space": "normalized_24x24_top_left_y_down",
            "allowed_primitives": ["rect", "circle", "polyline", "polygon"],
            "curve_policy": "straight_segments_and_analytic_circles_only",
            "stroke_units": "logical_px_scaled_then_pixel_snapped",
            "color_input": "srgb8_unpremultiplied_role_token",
            "icon_expansion": "cpu_expand_to_solid_and_line_triangles_before_gpu_submit",
            "unknown_token_policy": "hard_error_no_fallback",
            "registry_policy": "standalone_product_shell_set_no_runtime_merge_required",
            "font_backend": "existing_freetype_sdf_atlas",
            "svg_runtime_dependency": False,
        },
        "palette_modes": PALETTES,
        "accessibility_contract": {
            "never_color_only": True,
            "required_redundant_channels": ["shape", "adjacent_text_label", "state_or_value_text"],
            "minimum_text_contrast_ratio": 4.5,
            "minimum_essential_graphic_contrast_ratio": 3.0,
            "disabled_state_preserves_shape_and_text": True,
            "focus_state_uses_outline_plus_chevron_plus_text": True,
            "selected_state_uses_diamond_check_plus_text": True,
            "error_state_uses_octagon_cross_plus_error_text": True,
        },
        "typography_and_scale": {
            "font_backend": "existing_freetype_sdf_atlas",
            "minimum_text_logical_px": 14,
            "minimum_text_retina_physical_px": 21,
            "item_icon_recommended_logical_px": 24,
            "focus_chevron_minimum_logical_px": 10,
            "selected_icon_minimum_logical_px": 14,
            "error_icon_minimum_logical_px": 22,
            "supported_viewports": [
                {"logical": [1280, 720], "physical": [1280, 720], "dpi": 96},
                {"logical": [1920, 1080], "physical": [1920, 1080], "dpi": 96},
                {"logical": [1280, 720], "physical": [1920, 1080], "dpi": 144},
            ],
        },
        "screen_icon_usage": SCREEN_USAGE,
        "global_state_icon_usage": state_usage,
        "aliases": {},
        "icon_tokens": icons,
        "debug_isolation": {
            "shipping_namespace": "product_shell",
            "debug_namespace": "debug_overlay",
            "shares_runtime_icon_nodes": False,
            "debug_default_visible": False,
            "debug_allowed_in_release_capture": False,
        },
        "source_audit": {
            "screen_count": 9,
            "screen_enum": ["MainMenu", "SinglePlayerSetup", "Match", "TrainingCatalog",
                            "TrainingSession", "ReplayBrowser", "ReplayPlayback", "Settings", "Help"],
            "source_files": ["src/Billiards/render/ui/product_shell_draw_list.cpp",
                             "src/Billiards/product_shell_controller.cpp",
                             "src/Billiards/product_shell_controller.h"],
            "exact_emitted_icon_tokens": sorted(icons),
        },
    }


def _sha256(data):
    return "sha256:" + hashlib.sha256(data).hexdigest()


def _svg_command(command, palette):
    def color(role):
        return "none" if role == "none" else palette[role]
    primitive = command["primitive"]
    fill = color(command.get("fill_role", "none"))
    stroke = color(command.get("stroke_role", "none"))
    stroke_attr = "" if stroke == "none" else ' stroke="%s" stroke-width="%g"' % (
        stroke, command.get("stroke_width", 1))
    if primitive == "rect":
        return '<rect x="%g" y="%g" width="%g" height="%g" fill="%s"%s/>' % (
            command["x"], command["y"], command["width"], command["height"], fill, stroke_attr)
    if primitive == "circle":
        return '<circle cx="%g" cy="%g" r="%g" fill="%s"%s/>' % (
            command["cx"], command["cy"], command["radius"], fill, stroke_attr)
    points = " ".join("%g,%g" % tuple(point) for point in command["points"])
    cap = ' stroke-linecap="round" stroke-linejoin="round"'
    return '<%s points="%s" fill="%s"%s%s/>' % (
        primitive, points, fill, stroke_attr, cap)


def build_flat_proof(token_set):
    width, height = 1920, 1080
    values = [
        '<svg xmlns="http://www.w3.org/2000/svg" width="1920" height="1080" viewBox="0 0 1920 1080" '
        'data-proof-kind="flat-token-proof" data-runtime-capture="false" data-user-research="false">',
        '<title>ProductShell shipping tokens — FLAT PROOF, not a GPU screenshot</title>',
        '<desc>Original vector token inventory for two palettes. This is neither runtime capture nor user study evidence.</desc>',
        '<rect width="1920" height="1080" fill="#0A1014"/>',
        '<text x="40" y="48" fill="#F4F7F8" font-family="system-ui,sans-serif" font-size="28" font-weight="700">PRODUCT SHELL SHIPPING TOKENS · FLAT PROOF</text>',
        '<text x="40" y="78" fill="#C1CBD0" font-family="system-ui,sans-serif" font-size="16">43 exact tokens · 9 screens · standard + color-vision-safe · NOT A GPU SCREENSHOT / USER STUDY</text>',
    ]
    names = sorted(token_set["icon_tokens"])
    for palette_index, (palette_name, palette) in enumerate(token_set["palette_modes"].items()):
        panel_x = 30 + palette_index * 950
        values.append('<rect x="%d" y="110" width="920" height="920" rx="14" fill="%s" stroke="#56656E" stroke-width="2"/>' %
                      (panel_x, palette["surface"]))
        values.append('<text x="%d" y="148" fill="%s" font-family="system-ui,sans-serif" font-size="22" font-weight="700">%s</text>' %
                      (panel_x + 22, palette["text_primary"], html.escape(palette_name)))
        for index, name in enumerate(names):
            column, row = index % 6, index // 6
            card_x = panel_x + 22 + column * 148
            card_y = 170 + row * 104
            values.append('<g data-icon-token="%s" data-palette-mode="%s">' %
                          (html.escape(name), html.escape(palette_name)))
            values.append('<rect x="%d" y="%d" width="136" height="92" rx="8" fill="#18232A" stroke="#43515A"/>' %
                          (card_x, card_y))
            values.append('<g transform="translate(%g %g) scale(1.75)">' %
                          (card_x + 47, card_y + 8))
            values.extend(_svg_command(command, palette)
                          for command in token_set["icon_tokens"][name]["commands"])
            values.append('</g>')
            values.append('<text x="%g" y="%g" fill="%s" font-family="ui-monospace,monospace" font-size="9" text-anchor="middle">%s</text>' %
                          (card_x + 68, card_y + 82, palette["text_secondary"], html.escape(name)))
            values.append('</g>')
    values.append('</svg>')
    return ("\n".join(values) + "\n").encode("utf-8")


def build_manifest(token_bytes, proof_bytes):
    return {
        "schema_version": 1,
        "manifest_id": MANIFEST_ID,
        "authorship": "original_billiardgl_product_shell_ui",
        "external_assets": [],
        "token_set": {"token_set_id": TOKEN_SET_ID, "file": TOKEN_FILE,
                      "sha256": _sha256(token_bytes), "icon_count": 43,
                      "palette_modes": ["standard", "color_vision_safe"],
                      "screen_count": 9},
        "flat_proof": {"file": PROOF_FILE, "sha256": _sha256(proof_bytes),
                       "runtime_capture": False, "user_research": False,
                       "purpose": "inventory_and_palette_review_only"},
        "integration_contract": {
            "consumer": "ProductShellDrawList UiPrimitiveKind::Icon",
            "standalone": True,
            "unknown_token_policy": "hard_error_no_fallback",
            "existing_hud_token_set_modified": False,
            "gpu_capture_required_for_runtime_acceptance": True,
        },
        "source_audit": {
            "source_files": ["src/Billiards/render/ui/product_shell_draw_list.cpp",
                             "src/Billiards/product_shell_controller.cpp",
                             "src/Billiards/product_shell_controller.h"],
            "expected_exact_token_count": 43,
            "expected_screen_count": 9,
        },
    }


def write_package(output_dir):
    output_dir.mkdir(parents=True, exist_ok=True)
    token_set = build_token_set()
    token_bytes = (json.dumps(token_set, indent=2, sort_keys=True) + "\n").encode("utf-8")
    proof_bytes = build_flat_proof(token_set)
    manifest_bytes = (json.dumps(build_manifest(token_bytes, proof_bytes), indent=2,
                                 sort_keys=True) + "\n").encode("utf-8")
    (output_dir / TOKEN_FILE).write_bytes(token_bytes)
    (output_dir / PROOF_FILE).write_bytes(proof_bytes)
    (output_dir / MANIFEST_FILE).write_bytes(manifest_bytes)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", type=Path,
                        default=Path("assets/modern/hud"))
    args = parser.parse_args()
    write_package(args.output_dir)
    print("Generated ProductShell shipping token package: icons=43 palettes=2 screens=9")


if __name__ == "__main__":
    main()
