#!/usr/bin/env python3
"""Generate the original Phase-3 HUD manifest and eight deterministic SVG mockups."""

import argparse
import hashlib
import html
import json
from pathlib import Path


MANIFEST_NAME = "focused_practice_hud_v1.hud.json"
PACKAGE_ID = "focused_practice_hud_v1"
SHIPPING_TOKENS_NAME = "focused_practice_hud_shipping_tokens_v1.json"
SHIPPING_TOKEN_SET_ID = "focused_practice_hud_shipping_tokens_v1"
TOKEN_PROOF_NAME = "focused_practice_hud_shipping_tokens_flat_proof_v1.svg"

PROFILES = {
    "wide_1920x1080": {
        "viewport_px": [1920, 1080],
        "safe_area_px": {"left": 64, "top": 36, "right": 64, "bottom": 54},
        "breakpoint": "wide_16_9",
        "font_px": {"display": 32, "primary": 24, "body": 18, "caption": 14},
    },
    "compact_1024x768": {
        "viewport_px": [1024, 768],
        "safe_area_px": {"left": 32, "top": 24, "right": 32, "bottom": 32},
        "breakpoint": "compact_4_3",
        "font_px": {"display": 24, "primary": 20, "body": 16, "caption": 13},
    },
}

MOCKUP_STATES = ("default", "aiming", "post_shot_foul", "help")

COLORS = {
    "text_primary": "#F4F7F8",
    "text_secondary": "#C1CBD0",
    "surface": "#10181EEB",
    "surface_strong": "#0B1116F5",
    "border": "#64737D",
    "accent": "#31B7C8",
    "focus": "#C7A8FF",
    "warning": "#F2BC55",
    "destructive": "#F06B5D",
    "table_felt": "#126D7C",
    "table_wood": "#49291F",
    "room": "#0D1216",
}

PALETTE_MODES = {
    "standard": {
        "surface": "#10181EEB", "surface_strong": "#0B1116F5",
        "text_primary": "#F4F7F8", "text_secondary": "#C1CBD0",
        "icon_primary": "#F4F7F8", "accent": "#31B7C8",
        "focus": "#C7A8FF", "warning": "#F2BC55",
        "destructive": "#F06B5D",
    },
    "color_vision_safe": {
        "surface": "#10181EEB", "surface_strong": "#0B1116F5",
        "text_primary": "#F4F7F8", "text_secondary": "#C1CBD0",
        "icon_primary": "#F4F7F8", "accent": "#5DD9C1",
        "focus": "#FFD166", "warning": "#FFD166",
        "destructive": "#FF8C69",
    },
}


def _circle_command(cx, cy, radius, stroke="icon_primary", fill="none", width=2):
    return {"primitive": "circle", "cx": cx, "cy": cy, "radius": radius,
            "stroke_role": stroke, "fill_role": fill, "stroke_width": width}


def _line_command(points, stroke="icon_primary", width=2, closed=False,
                  fill="none"):
    return {"primitive": "polygon" if closed else "polyline", "points": points,
            "stroke_role": stroke, "fill_role": fill, "stroke_width": width,
            "line_cap": "round", "line_join": "round"}


ICON_TOKENS = {
    "power_bars": {"commands": [
        {"primitive": "rect", "x": 3, "y": 15, "width": 3, "height": 6,
         "fill_role": "accent"},
        {"primitive": "rect", "x": 8, "y": 11, "width": 3, "height": 10,
         "fill_role": "accent"},
        {"primitive": "rect", "x": 13, "y": 7, "width": 3, "height": 14,
         "fill_role": "warning"},
        {"primitive": "rect", "x": 18, "y": 3, "width": 3, "height": 18,
         "fill_role": "warning"}]},
    "tip_target": {"commands": [
        _circle_command(12, 12, 9),
        _line_command([[3, 12], [21, 12]], "text_secondary", 1),
        _line_command([[12, 3], [12, 21]], "text_secondary", 1),
        _circle_command(14.5, 8.5, 2, "surface_strong", "focus", 1)]},
    "tip_ball_outline": {"commands": [_circle_command(12, 12, 9)]},
    "tip_position_marker": {"commands": [
        _circle_command(12, 12, 4, "surface_strong", "focus", 1)]},
    "camera_frame": {"commands": [
        _line_command([[8, 4], [4, 4], [4, 8]]),
        _line_command([[16, 4], [20, 4], [20, 8]]),
        _line_command([[4, 16], [4, 20], [8, 20]]),
        _line_command([[20, 16], [20, 20], [16, 20]]),
        _circle_command(12, 12, 4, "accent", "none", 2)]},
    "warning_triangle": {"commands": [
        _line_command([[12, 3], [22, 21], [2, 21]], "destructive", 2, True,
                      "none"),
        _line_command([[12, 8], [12, 14]], "destructive", 2),
        _circle_command(12, 18, 1, "destructive", "destructive", 1)]},
    "help_circle": {"commands": [
        _circle_command(12, 12, 9, "focus", "none", 2),
        _line_command([[9, 9], [10, 7], [14, 7], [16, 9], [16, 11],
                       [12, 14], [12, 16]], "focus", 2),
        _circle_command(12, 19, 1, "focus", "focus", 1)]},
    "group_solid_filled_circle": {"commands": [
        _circle_command(12, 12, 8, "icon_primary", "accent", 2)]},
    "group_stripe_banded_ring": {"commands": [
        _circle_command(12, 12, 8, "icon_primary", "none", 2),
        _line_command([[5, 10], [19, 10]], "accent", 3),
        _line_command([[5, 14], [19, 14]], "accent", 3)]},
    "group_open_diamond": {"commands": [
        _line_command([[12, 3], [21, 12], [12, 21], [3, 12]], "accent", 2,
                      True, "none")]},
    "question_diamond": {"commands": [
        _line_command([[12, 3], [21, 12], [12, 21], [3, 12]], "warning", 2,
                      True, "none"),
        _line_command([[9, 9], [10, 7], [14, 7], [16, 9], [12, 13], [12, 15]],
                      "warning", 1.5),
        _circle_command(12, 18, 0.8, "warning", "warning", 1)]},
    "crossed_circle": {"commands": [
        _circle_command(12, 12, 8, "destructive", "none", 2),
        _line_command([[6, 6], [18, 18]], "destructive", 2)]},
    "rail_with_slash": {"commands": [
        {"primitive": "rect", "x": 3, "y": 9, "width": 18, "height": 6,
         "stroke_role": "destructive", "fill_role": "none", "stroke_width": 2},
        _line_command([[5, 20], [19, 4]], "destructive", 2)]},
    "cue_ball_down_arrow": {"commands": [
        _circle_command(10, 9, 6, "destructive", "none", 2),
        _line_command([[17, 8], [17, 20], [13, 16], [17, 20], [21, 16]],
                      "destructive", 2)]},
    "ball_outside_rectangle": {"commands": [
        {"primitive": "rect", "x": 3, "y": 5, "width": 13, "height": 14,
         "stroke_role": "destructive", "fill_role": "none", "stroke_width": 2},
        _circle_command(19, 4, 3, "destructive", "none", 2)]},
    "eight_ball_octagon": {"commands": [
        _line_command([[8, 3], [16, 3], [21, 8], [21, 16], [16, 21],
                       [8, 21], [3, 16], [3, 8]], "destructive", 2, True, "none")],
        "label_slot": {"text": "8", "font_role": "caption_sdf", "center": [12, 12]}},
}

ICON_ALIASES = {
    "outlined_triangle": "warning_triangle",
    "group_unknown_question_diamond": "question_diamond",
    "numbered_ball_cross": "crossed_circle",
    "numbered_ball_outside_rectangle": "ball_outside_rectangle",
    "rule_unknown": "question_diamond",
    "wrong_ball": "crossed_circle",
    "no_contact": "crossed_circle",
    "rail_missing": "rail_with_slash",
    "cue_scratch": "cue_ball_down_arrow",
    "illegal_eight": "eight_ball_octagon",
}


def _sha256(payload):
    return "sha256:" + hashlib.sha256(payload).hexdigest()


def _rect(x, y, width, height, fill, radius=12, stroke=None, attrs=""):
    stroke_value = (' stroke="%s" stroke-width="2"' % stroke) if stroke else ""
    return ('<rect x="%g" y="%g" width="%g" height="%g" rx="%g" fill="%s"%s %s/>' %
            (x, y, width, height, radius, fill, stroke_value, attrs))


def _text(x, y, value, size, fill=None, weight=500, anchor="start", attrs=""):
    return ('<text x="%g" y="%g" font-family="system-ui,-apple-system,sans-serif" '
            'font-size="%g" font-weight="%d" fill="%s" text-anchor="%s" %s>%s</text>' %
            (x, y, size, weight, fill or COLORS["text_primary"], anchor, attrs,
             html.escape(value)))


def _line(x1, y1, x2, y2, stroke, width=2, attrs=""):
    return ('<line x1="%g" y1="%g" x2="%g" y2="%g" stroke="%s" '
            'stroke-width="%g" stroke-linecap="round" %s/>' %
            (x1, y1, x2, y2, stroke, width, attrs))


def _circle(cx, cy, radius, fill, stroke=None, width=2, attrs=""):
    stroke_value = (' stroke="%s" stroke-width="%g"' % (stroke, width)) if stroke else ""
    return '<circle cx="%g" cy="%g" r="%g" fill="%s"%s %s/>' % (
        cx, cy, radius, fill, stroke_value, attrs)


def _layout(profile_name):
    profile = PROFILES[profile_name]
    width, height = profile["viewport_px"]
    safe = profile["safe_area_px"]
    compact = profile_name.startswith("compact")
    status = [safe["left"], safe["top"], 304 if compact else 380, 142 if compact else 158]
    hint_width, hint_height = (284, 92) if compact else (354, 100)
    context = [width - safe["right"] - hint_width, safe["top"], hint_width, hint_height]
    power_width, power_height = (520, 126) if compact else (650, 138)
    power = [safe["left"], height - safe["bottom"] - power_height,
             power_width, power_height]
    tip_size = 172 if compact else 210
    tip = [width - safe["right"] - tip_size,
           height - safe["bottom"] - tip_size, tip_size, tip_size]
    banner_width, banner_height = (520, 78) if compact else (660, 88)
    banner_y = safe["top"] + status[3] + 16 if compact else safe["top"]
    banner = [(width - banner_width) / 2, banner_y, banner_width, banner_height]
    result_width, result_height = (430, 126) if compact else (510, 138)
    result = [safe["left"], height - safe["bottom"] - result_height,
              result_width, result_height]
    help_width, help_height = ((760, 650) if compact else (900, 700))
    help_panel = [(width - help_width) / 2, (height - help_height) / 2,
                  help_width, help_height]
    return {"player_status": status, "context_hint": context, "power_meter": power,
            "tip_offset": tip, "foul_banner": banner, "shot_result": result,
            "help_panel": help_panel}


def _background(width, height):
    values = [_rect(0, 0, width, height, COLORS["room"], radius=0,
                    attrs='data-element="scene_background"')]
    table_width = width * (0.43 if width / height > 1.5 else 0.56)
    table_height = height * 0.70
    table_x, table_y = (width - table_width) / 2, height * 0.12
    values.append(_rect(table_x - 22, table_y - 22, table_width + 44, table_height + 44,
                        COLORS["table_wood"], radius=18))
    values.append(_rect(table_x, table_y, table_width, table_height,
                        COLORS["table_felt"], radius=10))
    for px, py in ((table_x, table_y), (table_x + table_width, table_y),
                   (table_x, table_y + table_height),
                   (table_x + table_width, table_y + table_height),
                   (table_x, table_y + table_height / 2),
                   (table_x + table_width, table_y + table_height / 2)):
        values.append(_circle(px, py, 16, "#020405"))
    ball_positions = ((0.50, 0.72, "#ECE9DC"), (0.44, 0.35, "#E7B833"),
                      (0.55, 0.31, "#3F72D8"), (0.49, 0.28, "#D94B42"),
                      (0.51, 0.24, "#181B1D"))
    for x, y, color in ball_positions:
        values.append(_circle(table_x + table_width * x, table_y + table_height * y,
                              12, color, stroke="#F4F7F8", width=1))
    return values


def _status_panel(rect, fonts):
    x, y, width, height = rect
    values = [_rect(x, y, width, height, COLORS["surface"], stroke=COLORS["border"],
                    attrs='data-element="player_status"')]
    values.append(_text(x + 20, y + 32, "PLAYER 1  ·  AT TABLE", fonts["caption"],
                        COLORS["text_secondary"]))
    values.append(_text(x + 20, y + 68, "SOLIDS", fonts["primary"]))
    values.append(_circle(x + width - 42, y + 58, 15, COLORS["accent"],
                          stroke=COLORS["text_primary"], attrs='data-encoding="filled-circle"'))
    values.append(_text(x + 20, y + 108, "TARGET", fonts["caption"],
                        COLORS["text_secondary"]))
    values.append(_circle(x + 112, y + 102, 17, "#E7B833", stroke=COLORS["text_primary"]))
    values.append(_text(x + 112, y + 109, "3", fonts["caption"], "#10181E", anchor="middle"))
    values.append(_text(x + 140, y + 109, "RIGHT CORNER", fonts["body"]))
    return values


def _context_hint(rect, fonts, aiming=False):
    x, y, width, height = rect
    values = [_rect(x, y, width, height, COLORS["surface"], stroke=COLORS["border"],
                    attrs='data-element="context_hint"')]
    values.append(_text(x + 18, y + 30, "CAMERA  ·  PLAYER", fonts["caption"],
                        COLORS["text_secondary"]))
    values.append(_text(x + 18, y + 62, "ASSIST  LOW", fonts["body"]))
    prompt = "SPACE  HOLD / RELEASE" if aiming else "TAB  AIM   ·   F1  HELP"
    values.append(_text(x + 18, y + height - 14, prompt, fonts["caption"], COLORS["focus"]))
    return values


def _power_panel(rect, fonts):
    x, y, width, height = rect
    values = [_rect(x, y, width, height, COLORS["surface_strong"], stroke=COLORS["border"],
                    attrs='data-element="power_meter"')]
    values.append(_text(x + 20, y + 32, "SHOT POWER", fonts["caption"],
                        COLORS["text_secondary"]))
    values.append(_text(x + width - 20, y + 36, "62%", fonts["primary"], anchor="end"))
    bar_x, bar_y, bar_width = x + 20, y + 54, width - 40
    values.append(_rect(bar_x, bar_y, bar_width, 24, "#26323A", radius=6))
    values.append(_rect(bar_x, bar_y, bar_width * 0.62, 24, COLORS["warning"], radius=6))
    for tick in range(11):
        tick_x = bar_x + bar_width * tick / 10
        values.append(_line(tick_x, bar_y + 28, tick_x, bar_y + 38,
                            COLORS["text_secondary"], 1))
    values.append(_text(x + 20, y + height - 14, "EST. CUE SPEED  2.8 m/s",
                        fonts["body"], COLORS["text_primary"],
                        attrs='data-element="estimated_cue_speed"'))
    return values


def _tip_panel(rect, fonts):
    x, y, width, height = rect
    values = [_rect(x, y, width, height, COLORS["surface_strong"], stroke=COLORS["border"],
                    attrs='data-element="tip_offset"')]
    values.append(_text(x + 14, y + 28, "TIP OFFSET", fonts["caption"],
                        COLORS["text_secondary"]))
    radius = min(width, height) * 0.28
    cx, cy = x + width / 2, y + height * 0.52
    values.append(_circle(cx, cy, radius, "#DDE5E7", stroke=COLORS["text_primary"], width=2))
    values.append(_line(cx - radius, cy, cx + radius, cy, "#5B6971", 1))
    values.append(_line(cx, cy - radius, cx, cy + radius, "#5B6971", 1))
    marker_x, marker_y = cx + radius * 0.15, cy - radius * 0.35
    values.append(_circle(marker_x, marker_y, 7, COLORS["focus"], stroke="#10181E", width=2))
    values.append(_text(cx, y + height - 14, "X +0.15R   Y +0.35R", fonts["caption"],
                        anchor="middle"))
    return values


def _foul_banner(rect, fonts):
    x, y, width, height = rect
    values = [_rect(x, y, width, height, COLORS["surface_strong"],
                    stroke=COLORS["destructive"], attrs='data-element="foul_banner"')]
    values.append('<path d="M %g %g L %g %g L %g %g Z" fill="%s"/>' %
                  (x + 26, y + height - 20, x + 46, y + 20, x + 66,
                   y + height - 20, COLORS["destructive"]))
    values.append(_text(x + 86, y + 36, "FOUL  ·  NO RAIL AFTER CONTACT", fonts["body"]))
    values.append(_text(x + 86, y + 64, "BALL IN HAND  ·  PLAYER 2", fonts["caption"],
                        COLORS["text_secondary"]))
    return values


def _shot_result(rect, fonts):
    x, y, width, height = rect
    values = [_rect(x, y, width, height, COLORS["surface"], stroke=COLORS["border"],
                    attrs='data-element="shot_result"')]
    values.append(_text(x + 18, y + 30, "SHOT COMPLETE", fonts["caption"],
                        COLORS["text_secondary"]))
    values.append(_text(x + 18, y + 62, "CUE BALL  1.84 m", fonts["body"]))
    values.append(_text(x + 18, y + 92, "CONTACT  3  →  CUSHION", fonts["body"]))
    values.append(_text(x + 18, y + height - 12, "Q / E  REVIEW CAMERA", fonts["caption"],
                        COLORS["focus"]))
    return values


def _help_panel(rect, fonts):
    x, y, width, height = rect
    values = [_rect(x, y, width, height, COLORS["surface_strong"],
                    stroke=COLORS["focus"], attrs='data-element="help_panel"')]
    values.append(_text(x + 28, y + 46, "QUICK CONTROLS", fonts["display"]))
    entries = (
        ("TAB", "ENTER / EXIT AIM"), ("MOUSE", "ORBIT CAMERA"),
        ("WHEEL", "ZOOM"), ("ARROWS", "MOVE TIP OFFSET"),
        ("SPACE", "HOLD POWER · RELEASE SHOT"), ("Q / E", "CAMERA PRESET"),
        ("F1 / ESC", "CLOSE HELP"),
    )
    row_height = (height - 130) / len(entries)
    for index, (key, label) in enumerate(entries):
        row_y = y + 88 + index * row_height
        values.append(_rect(x + 28, row_y - 24, 118, 38, "#28343C", radius=7))
        values.append(_text(x + 87, row_y + 2, key, fonts["caption"], anchor="middle"))
        values.append(_text(x + 172, row_y + 2, label, fonts["body"]))
    values.append(_text(x + 28, y + height - 28,
                        "FIRST SHOT: TAB  →  AIM  →  HOLD SPACE  →  RELEASE",
                        fonts["caption"], COLORS["warning"]))
    return values


def build_mockup(profile_name, state):
    profile = PROFILES[profile_name]
    width, height = profile["viewport_px"]
    safe = profile["safe_area_px"]
    fonts = profile["font_px"]
    layout = _layout(profile_name)
    values = [
        '<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
        'viewBox="0 0 %d %d" role="img" data-profile="%s" data-state="%s" '
        'data-safe-area="%d,%d,%d,%d">' %
        (width, height, width, height, profile_name, state, safe["left"], safe["top"],
         safe["right"], safe["bottom"]),
        '<title>Focused Practice HUD %s %s</title>' % (profile_name, state),
        '<desc>Original product HUD layout for the selected review state.</desc>',
    ]
    values.extend(_background(width, height))
    if state != "help":
        values.extend(_status_panel(layout["player_status"], fonts))
        values.extend(_context_hint(layout["context_hint"], fonts, aiming=state == "aiming"))
    if state == "aiming":
        values.extend(_power_panel(layout["power_meter"], fonts))
        values.extend(_tip_panel(layout["tip_offset"], fonts))
    elif state == "post_shot_foul":
        values.extend(_foul_banner(layout["foul_banner"], fonts))
        values.extend(_shot_result(layout["shot_result"], fonts))
    elif state == "help":
        values.append(_rect(0, 0, width, height, "#020609B8", radius=0))
        values.extend(_help_panel(layout["help_panel"], fonts))
    values.append("</svg>")
    return ("\n".join(values) + "\n").encode("utf-8")


def build_shipping_tokens():
    return {
        "schema_version": 1,
        "token_set_id": SHIPPING_TOKEN_SET_ID,
        "authorship": "original_billiardgl_vector_tokens",
        "external_assets": False,
        "gpu_contract": {
            "coordinate_space": "normalized_24x24_top_left_y_down",
            "allowed_primitives": ["rect", "circle", "polyline", "polygon"],
            "curve_policy": "straight_segments_only_no_runtime_tessellation",
            "stroke_units": "logical_px_scaled_then_pixel_snapped",
            "color_input": "srgb8_unpremultiplied_role_token",
            "output_primitives": ["Panel", "SolidRect", "Line", "Icon", "GlyphQuad"],
            "icon_expansion": "cpu_expand_to_solid_and_line_triangles_before_gpu_submit",
            "font_backend": "existing_freetype_sdf_atlas",
            "svg_runtime_dependency": False,
        },
        "palette_modes": PALETTE_MODES,
        "palette_accessibility": {
            "supported_modes": ["standard", "color_vision_safe"],
            "never_color_only": True,
            "required_redundant_channels": ["shape", "text", "value_or_pattern"],
            "foul_encoding": ["warning_shape", "FOUL_text", "reason_text"],
            "power_encoding": ["bar_length", "segment_count", "numeric_percent"],
            "group_encoding": ["solid_or_band_pattern", "shape", "text_label", "ball_number"],
        },
        "typography": {
            "backend": "existing_freetype_sdf_atlas",
            "hierarchy": ["display", "primary", "body", "caption"],
            "wide_logical_px": {"display": 32, "primary": 24, "body": 18, "caption": 14},
            "compact_logical_px": {"display": 24, "primary": 20, "body": 16, "caption": 13},
            "minimum_logical_px": 13,
            "retina_minimum_physical_px": 21,
            "tabular_numeric_fields": ["power_percent", "cue_speed", "tip_offset", "ball_number"],
        },
        "layout_contract": {
            "required_viewports": [[1280, 720], [1920, 1080]],
            "retina_case": {"logical_px": [1280, 720], "physical_px": [1920, 1080],
                            "scale": 1.5},
            "wide_safe_margin_px": {"left": 64, "top": 36, "right": 64, "bottom": 54},
            "minimum_720p_safe_margin_px": {"left": 43, "top": 24,
                                             "right": 43, "bottom": 36},
        },
        "state_icon_usage": {
            "default": ["group_solid_filled_circle", "group_stripe_banded_ring",
                        "camera_frame"],
            "aiming": ["power_bars", "tip_target", "camera_frame"],
            "post_shot": ["camera_frame"],
            "foul": ["warning_triangle"],
            "help": ["help_circle", "power_bars", "tip_target", "camera_frame"],
        },
        "icon_tokens": ICON_TOKENS,
        "aliases": ICON_ALIASES,
        "debug_isolation": {
            "product_token_namespace": "shipping_hud",
            "debug_token_namespace": "debug_telemetry",
            "shared_layout_nodes": False,
            "debug_default_visible": False,
            "debug_forbidden_in_release_capture": True,
        },
    }


def _proof_color(palette, role):
    return "none" if role == "none" else palette[role]


def _icon_proof_svg(token, x, y, scale, palette, palette_name, state):
    values = ['<g data-icon-token="%s" data-palette-mode="%s" data-state="%s" '
              'transform="translate(%g %g) scale(%g)">' %
              (token, palette_name, state, x, y, scale)]
    for command in ICON_TOKENS[token]["commands"]:
        primitive = command["primitive"]
        stroke = _proof_color(palette, command.get("stroke_role", "none"))
        fill = _proof_color(palette, command.get("fill_role", "none"))
        width = command.get("stroke_width", 0)
        common = 'stroke="%s" fill="%s" stroke-width="%g" stroke-linecap="round" stroke-linejoin="round"' % (
            stroke, fill, width)
        if primitive == "circle":
            values.append('<circle cx="%g" cy="%g" r="%g" %s/>' %
                          (command["cx"], command["cy"], command["radius"], common))
        elif primitive == "rect":
            values.append('<rect x="%g" y="%g" width="%g" height="%g" rx="1" %s/>' %
                          (command["x"], command["y"], command["width"],
                           command["height"], common))
        else:
            points = " ".join("%g,%g" % tuple(point) for point in command["points"])
            values.append('<%s points="%s" %s/>' % (primitive, points, common))
    values.append("</g>")
    return values


def build_token_proof():
    width, height = 1280, 720
    state_icons = (("default", "group_solid_filled_circle"),
                   ("aiming", "power_bars"), ("post-shot", "camera_frame"),
                   ("foul", "warning_triangle"), ("help", "help_circle"))
    values = [
        '<svg xmlns="http://www.w3.org/2000/svg" width="1280" height="720" '
        'viewBox="0 0 1280 720" role="img" data-proof-kind="flat-token-proof" '
        'data-runtime-capture="false" data-user-research="false">',
        '<title>Focused Practice shipping HUD token flat proof</title>',
        '<desc>Flat engineering proof for original GPU vector tokens; not a rendered gameplay capture.</desc>',
        _rect(0, 0, width, height, "#081015", radius=0),
        _text(48, 52, "SHIPPING HUD TOKENS · FLAT PROOF", 26, "#F4F7F8", 500),
        _text(48, 80, "24×24 normalized geometry · existing FreeType/SDF text · not a GPU screenshot", 14,
              "#C1CBD0", 400),
    ]
    for row, palette_name in enumerate(("standard", "color_vision_safe")):
        palette = PALETTE_MODES[palette_name]
        top = 112 + row * 278
        values.append(_text(48, top + 26, palette_name.replace("_", " ").upper(), 18,
                            palette["focus"], 500))
        for index, (state, token) in enumerate(state_icons):
            x = 48 + index * 238
            y = top + 44
            values.append(_rect(x, y, 214, 190, palette["surface_strong"], 12,
                                palette["text_secondary"],
                                'data-proof-card="%s:%s"' % (palette_name, state)))
            if state == "aiming":
                values.extend(_icon_proof_svg("power_bars", x + 24, y + 36, 2.7,
                                              palette, palette_name, state))
                values.extend(_icon_proof_svg("tip_target", x + 126, y + 36, 2.7,
                                              palette, palette_name, state))
            else:
                values.extend(_icon_proof_svg(token, x + 59, y + 22, 4.0, palette,
                                              palette_name, state))
            values.append(_text(x + 107, y + 144, state.upper(), 16,
                                palette["text_primary"], 500, "middle"))
            token_label = "power_bars + tip_target" if state == "aiming" else token
            values.append(_text(x + 107, y + 169, token_label, 12,
                                palette["text_secondary"], 400, "middle"))
    values.append(_text(48, 690,
                        "Foul and group meaning always retain shape + text; color is redundant.",
                        14, "#C1CBD0", 400))
    values.append("</svg>")
    return ("\n".join(values) + "\n").encode("utf-8")


def build_manifest(mockups, shipping_tokens_bytes, token_proof_bytes):
    profile_layouts = {}
    for profile_name, profile in PROFILES.items():
        rectangles = _layout(profile_name)
        profile_layouts[profile_name] = {**profile, "element_rects_px": rectangles,
                                         "minimum_text_px": profile["font_px"]["caption"]}
    medium_rects = {
        "player_status": [43, 24, 380, 158],
        "context_hint": [883, 24, 354, 100],
        "power_meter": [43, 546, 650, 138],
        "tip_offset": [1027, 474, 210, 210],
        "foul_banner": [310, 198, 660, 88],
        "shot_result": [43, 546, 510, 138],
        "help_panel": [260, 50, 760, 620],
    }

    def scaled(values, factor):
        return [int(round(value * factor)) for value in values]

    active_sets = {
        "default": ["player_status", "context_hint"],
        "aiming": ["player_status", "context_hint", "power_meter", "tip_offset"],
        "post_shot_foul": ["player_status", "context_hint", "foul_banner", "shot_result"],
        "help": ["help_panel"],
    }
    responsive_cases = {
        "minimum_wide_1280x720_96dpi": {
            "physical_viewport_px": [1280, 720], "logical_viewport_px": [1280, 720],
            "os_dpi": 96, "user_scale": 1.0, "resolved_scale": 1.0,
            "selected_profile": "wide_1920x1080", "layout_variant": "wide_minimum_reflow",
            "safe_area_physical_px": {"left": 43, "top": 24, "right": 43, "bottom": 36},
            "element_rects_physical_px": medium_rects,
            "minimum_text_physical_px": 14, "active_state_sets": active_sets,
        },
        "reference_1920x1080_96dpi": {
            "physical_viewport_px": [1920, 1080], "logical_viewport_px": [1920, 1080],
            "os_dpi": 96, "user_scale": 1.0, "resolved_scale": 1.0,
            "selected_profile": "wide_1920x1080", "layout_variant": "wide_reference",
            "safe_area_physical_px": PROFILES["wide_1920x1080"]["safe_area_px"],
            "element_rects_physical_px": profile_layouts["wide_1920x1080"]["element_rects_px"],
            "minimum_text_physical_px": 14, "active_state_sets": active_sets,
        },
        "high_dpi_1920x1080_144dpi": {
            "physical_viewport_px": [1920, 1080], "logical_viewport_px": [1280, 720],
            "os_dpi": 144, "user_scale": 1.0, "resolved_scale": 1.5,
            "selected_profile": "wide_1920x1080", "layout_variant": "wide_minimum_reflow",
            "safe_area_physical_px": {"left": 64, "top": 36, "right": 64, "bottom": 54},
            "element_rects_physical_px": {
                name: scaled(rect, 1.5) for name, rect in medium_rects.items()},
            "minimum_text_physical_px": 21, "active_state_sets": active_sets,
        },
    }
    return {
        "schema_version": 1,
        "manifest_id": PACKAGE_ID,
        "status": "phase3_hud_product_spec_candidate",
        "authorship": "original_billiardgl_product_ui",
        "external_assets": False,
        "profiles": profile_layouts,
        "dpi_scaling": {
            "logical_reference_dpi": 96,
            "os_density_scale_formula": "clamp(os_dpi / 96.0, 0.85, 1.50)",
            "user_scale_range": [0.85, 1.35],
            "resolved_scale_formula": "os_density_scale * user_scale",
            "pixel_snapping": "rect_edges_and_one_pixel_strokes_after_scale",
            "profile_selection": "compact_1024x768 when aspect_ratio < 1.55 or width < 1280",
        },
        "responsive_validation_cases": responsive_cases,
        "typography": {
            "font_stack": ["system-ui", "-apple-system", "Segoe UI", "sans-serif"],
            "weights": [400, 500],
            "hierarchy": ["display", "primary", "body", "caption"],
            "minimum_px": 13,
            "contrast_ratio_text_min": 4.5,
            "contrast_ratio_large_text_min": 3.0,
            "numeric_style": "tabular_figures",
        },
        "colors": COLORS,
        "contrast_pairs": [
            {"foreground": "text_primary", "background": "surface_strong", "minimum": 4.5},
            {"foreground": "text_secondary", "background": "surface_strong", "minimum": 4.5},
            {"foreground": "warning", "background": "surface_strong", "minimum": 4.5},
            {"foreground": "destructive", "background": "surface_strong", "minimum": 4.5},
        ],
        "states": {
            "default": {"visible_elements": ["player_status", "context_hint"]},
            "aiming": {"visible_elements": ["player_status", "context_hint", "power_meter",
                                                "estimated_cue_speed", "tip_offset"]},
            "post_shot": {"visible_elements": ["player_status", "context_hint", "shot_result"]},
            "foul": {"visible_elements": ["player_status", "context_hint", "foul_banner"]},
            "help": {"visible_elements": ["help_panel"], "pauses_shot_input": True},
        },
        "product_elements": {
            "player_status": {
                "fields": ["current_player", "ball_group", "target_ball", "target_pocket"],
                "group_encoding": ["text_label", "filled_or_striped_symbol", "ball_number", "color"],
            },
            "power_meter": {
                "fields": ["power_percent", "estimated_cue_speed_mps"],
                "speed_label_prefix": "EST.", "speed_is_authoritative_physics": False,
                "encodings": ["segmented_bar", "tick_marks", "numeric_percent", "numeric_speed_with_unit"],
            },
            "tip_offset": {
                "coordinate_unit": "ball_radius_R", "range": {"x": [-1.0, 1.0], "y": [-1.0, 1.0]},
                "encodings": ["ball_outline", "crosshair", "position_marker", "numeric_xy"],
            },
            "context_hint": {
                "fields": ["camera_preset", "assist_level", "contextual_input_prompt"],
                "assist_encodings": ["text", "icon", "color"], "maximum_visible_seconds": 3.0,
            },
            "foul_banner": {
                "fields": ["foul_label", "reason", "resolution"],
                "encodings": ["FOUL_text", "warning_triangle", "destructive_color"],
            },
            "shot_result": {
                "fields": ["shot_complete", "cue_ball_travel_m", "contact_summary", "review_prompt"]},
            "help_panel": {"input_modes": ["keyboard", "mouse", "trackpad"]},
        },
        "input_prompts": {
            "keyboard_mouse": {
                "aim_toggle": "TAB", "power_charge_release": "SPACE",
                "tip_offset": "ARROW KEYS", "camera_orbit": "MOUSE DRAG",
                "camera_zoom": "WHEEL", "camera_preset": "Q / E",
                "help": "F1", "dismiss": "ESC",
            },
            "prompt_priority": ["blocking_error", "current_action", "camera", "help"],
            "maximum_simultaneous_prompts": 3,
        },
        "first_shot_path_30s": [
            {"seconds": [0, 3], "step": "read_player_group_and_target", "required_input": None},
            {"seconds": [3, 7], "step": "orbit_or_accept_default_camera", "required_input": "optional_mouse"},
            {"seconds": [7, 11], "step": "enter_aim", "required_input": "TAB"},
            {"seconds": [11, 18], "step": "confirm_aim_and_optional_tip_offset", "required_input": "optional_arrows"},
            {"seconds": [18, 27], "step": "hold_power_and_read_percent_speed", "required_input": "SPACE_hold"},
            {"seconds": [27, 30], "step": "release_first_shot", "required_input": "SPACE_release"},
        ],
        "first_shot_automation_proxies": {
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
        },
        "color_vision_accessibility": {
            "never_color_only": True,
            "solid_group": {"shape": "filled_circle", "pattern": "solid", "label": "SOLIDS"},
            "stripe_group": {"shape": "ring", "pattern": "equatorial_band", "label": "STRIPES"},
            "target_always_shows_ball_number": True,
            "foul_always_shows_icon_and_text": True,
        },
        "shipping_visual_audit": {
            "layout_state_contract": "PASS_EXISTING",
            "responsive_safe_area_contract": "PASS_EXISTING",
            "freetype_sdf_contract": "PASS_EXISTING",
            "opaque_icon_token_geometry_before_increment": "BLOCKING_GAP_CLOSED",
            "standard_and_color_vision_palette_runtime_tokens": "BLOCKING_GAP_CLOSED",
            "flat_proof_is_gpu_capture": False,
            "flat_proof_is_user_research": False,
        },
        "shipping_token_set": {
            "token_set_id": SHIPPING_TOKEN_SET_ID,
            "gpu_consumption": "expand_normalized_commands_to_existing_UiPrimitive_draw_list",
            "required_state_coverage": ["default", "aiming", "post_shot", "foul", "help"],
            "required_palette_modes": ["standard", "color_vision_safe"],
        },
        "debug_overlay": {
            "separate_overlay_id": "billiardgl_debug_telemetry_v1",
            "default_visible": False, "toggle": "F3",
            "shares_product_layout_nodes": False,
            "forbidden_in_release_capture": True,
            "fields": ["fps", "frame_ms", "draw_calls", "triangles", "snapshot_tick"],
        },
        "capability_gated_placeholders": {
            "cue_elevation": {
                "default_visible": False,
                "visible_if": ["capability.cue_elevation_ui", "physics.cue_elevation_promoted"],
                "absence_behavior": "omit_node_no_disabled_control",
            },
            "chalk": {
                "default_visible": False,
                "visible_if": ["capability.chalk_ui", "physics.chalk_model_promoted"],
                "absence_behavior": "omit_node_no_disabled_control",
            },
        },
        "files": {
            "shipping_tokens": {
                "path": "assets/modern/hud/" + SHIPPING_TOKENS_NAME,
                "sha256": _sha256(shipping_tokens_bytes),
            },
            "flat_token_proof": {
                "path": "assets/modern/hud/" + TOKEN_PROOF_NAME,
                "sha256": _sha256(token_proof_bytes),
                "runtime_capture": False,
                "user_research": False,
            },
            "mockups": {
                profile_name: {
                    state: {"path": "assets/modern/hud/" + filename,
                            "sha256": _sha256(payload)}
                    for (candidate_profile, state), (filename, payload) in mockups.items()
                    if candidate_profile == profile_name}
                for profile_name in PROFILES}
        },
        "generator": "scripts/generate_phase3_hud_package.py",
    }


def build_package():
    mockups = {}
    for profile_name in PROFILES:
        for state in MOCKUP_STATES:
            filename = "focused_practice_hud_%s_%s_v1.svg" % (profile_name, state)
            mockups[(profile_name, state)] = (filename, build_mockup(profile_name, state))
    shipping_tokens_bytes = (json.dumps(build_shipping_tokens(), indent=2,
                                        sort_keys=True) + "\n").encode("utf-8")
    token_proof_bytes = build_token_proof()
    manifest = build_manifest(mockups, shipping_tokens_bytes, token_proof_bytes)
    outputs = {filename: payload for filename, payload in mockups.values()}
    outputs[SHIPPING_TOKENS_NAME] = shipping_tokens_bytes
    outputs[TOKEN_PROOF_NAME] = token_proof_bytes
    outputs[MANIFEST_NAME] = (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode("utf-8")
    return outputs, manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", required=True, type=Path)
    arguments = parser.parse_args()
    arguments.output_dir.mkdir(parents=True, exist_ok=True)
    outputs, manifest = build_package()
    for name, payload in outputs.items():
        (arguments.output_dir / name).write_bytes(payload)
    print("generated Phase3 HUD package: profiles=%d states=%d mockups=%d" %
          (len(manifest["profiles"]), len(manifest["states"]),
           len(PROFILES) * len(MOCKUP_STATES)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
