#!/usr/bin/env python3
"""Fail-closed validator for the standalone ProductShell vector-token package."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from generate_phase3_shell_token_package import (  # noqa: E402
    FIXED_STATE_USAGE,
    MANIFEST_FILE,
    MANIFEST_ID,
    PALETTES,
    PROOF_FILE,
    SCREEN_USAGE,
    TOKEN_FILE,
    TOKEN_SET_ID,
    build_token_set,
)


ALLOWED_PRIMITIVES = {"rect", "circle", "polyline", "polygon"}
ALLOWED_ROLES = set(next(iter(PALETTES.values()))) | {"none"}


def _sha256(data):
    return "sha256:" + hashlib.sha256(data).hexdigest()


def _rgb(value):
    if not isinstance(value, str) or not re.fullmatch(r"#[0-9A-Fa-f]{6}", value):
        return None
    return tuple(int(value[index:index + 2], 16) / 255.0 for index in (1, 3, 5))


def _luma(color):
    def channel(value):
        return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4
    return sum(weight * channel(value) for weight, value in zip((0.2126, 0.7152, 0.0722), color))


def _contrast(first, second):
    a, b = _luma(first), _luma(second)
    return (max(a, b) + 0.05) / (min(a, b) + 0.05)


def _quoted_literals(value):
    return [bytes(match, "utf-8").decode("unicode_escape") for match in
            re.findall(r'"((?:\\.|[^"\\])*)"', value)]


def _matching_parenthesis(text, opening):
    depth = 0
    quote = None
    escape = False
    index = opening
    while index < len(text):
        char = text[index]
        next_char = text[index + 1] if index + 1 < len(text) else ""
        if quote:
            if escape:
                escape = False
            elif char == "\\":
                escape = True
            elif char == quote:
                quote = None
            index += 1
            continue
        if char in {'"', "'"}:
            quote = char
        elif char == "/" and next_char == "/":
            newline = text.find("\n", index + 2)
            index = len(text) if newline < 0 else newline
            continue
        elif char == "/" and next_char == "*":
            ending = text.find("*/", index + 2)
            index = len(text) if ending < 0 else ending + 2
            continue
        elif char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
            if depth == 0:
                return index
        index += 1
    raise ValueError("unbalanced call parenthesis")


def _split_arguments(body):
    values = []
    start = 0
    depths = {"(": 0, "[": 0, "{": 0}
    closing = {")": "(", "]": "[", "}": "{"}
    quote = None
    escape = False
    index = 0
    while index < len(body):
        char = body[index]
        next_char = body[index + 1] if index + 1 < len(body) else ""
        if quote:
            if escape:
                escape = False
            elif char == "\\":
                escape = True
            elif char == quote:
                quote = None
            index += 1
            continue
        if char in {'"', "'"}:
            quote = char
        elif char == "/" and next_char == "/":
            newline = body.find("\n", index + 2)
            index = len(body) if newline < 0 else newline
            continue
        elif char == "/" and next_char == "*":
            ending = body.find("*/", index + 2)
            index = len(body) if ending < 0 else ending + 2
            continue
        elif char in depths:
            depths[char] += 1
        elif char in closing:
            depths[closing[char]] -= 1
        elif char == "," and not any(depths.values()):
            values.append(body[start:index].strip())
            start = index + 1
        index += 1
    values.append(body[start:].strip())
    return values


def _calls(text, name):
    pattern = re.compile(r"\b" + re.escape(name) + r"\s*\(")
    for match in pattern.finditer(text):
        opening = text.find("(", match.start())
        try:
            ending = _matching_parenthesis(text, opening)
        except ValueError:
            continue
        yield _split_arguments(text[opening + 1:ending])


def audit_cpp_icon_tokens(controller_text, draw_list_text):
    """Extract every literal that can enter ProductShell's Icon token field."""
    tokens = set()
    for name in ("item", "setting"):
        for arguments in _calls(controller_text, name):
            if len(arguments) >= 5:
                tokens.update(_quoted_literals(arguments[4]))
    for arguments in _calls(draw_list_text, "addPrimitive"):
        if len(arguments) >= 6 and "UiPrimitiveKind::Icon" in arguments[1]:
            tokens.update(_quoted_literals(arguments[5]))
    return tokens


def _enum_screens(header_text):
    match = re.search(r"enum\s+class\s+ProductScreen\s*\{([^}]*)\}", header_text,
                      flags=re.DOTALL)
    if not match:
        return []
    return [name.strip() for name in match.group(1).split(",") if name.strip()]


def _validate_command(command, context, errors):
    primitive = command.get("primitive")
    if primitive not in ALLOWED_PRIMITIVES:
        errors.append(context + " uses an unsupported primitive")
        return
    for key in ("fill_role", "stroke_role"):
        if key in command and command[key] not in ALLOWED_ROLES:
            errors.append(context + " uses unknown palette role " + str(command[key]))
    if command.get("stroke_width", 0) < 0 or command.get("stroke_width", 0) > 4:
        errors.append(context + " stroke width is outside 0..4")
    coordinates = []
    if primitive == "rect":
        values = [command.get(key) for key in ("x", "y", "width", "height")]
        if not all(isinstance(value, (int, float)) for value in values):
            errors.append(context + " rectangle geometry is incomplete")
            return
        x, y, width, height = values
        coordinates = [x, y, x + width, y + height]
        if width <= 0 or height <= 0:
            errors.append(context + " rectangle must have positive extent")
    elif primitive == "circle":
        values = [command.get(key) for key in ("cx", "cy", "radius")]
        if not all(isinstance(value, (int, float)) for value in values):
            errors.append(context + " circle geometry is incomplete")
            return
        cx, cy, radius = values
        coordinates = [cx - radius, cy - radius, cx + radius, cy + radius]
        if radius <= 0:
            errors.append(context + " circle radius must be positive")
    else:
        points = command.get("points")
        if not isinstance(points, list) or len(points) < (3 if primitive == "polygon" else 2) or \
                any(not isinstance(point, list) or len(point) != 2 or not all(
                    isinstance(value, (int, float)) for value in point) for point in points):
            errors.append(context + " point geometry is incomplete")
            return
        coordinates = [value for point in points for value in point]
    if any(value < 0 or value > 24 for value in coordinates):
        errors.append(context + " exits normalized 24x24 bounds")


def validate(manifest, asset_dir, repository_root=ROOT, verify_hashes=True,
             audit_sources=True):
    errors = []
    asset_dir = Path(asset_dir)
    repository_root = Path(repository_root)
    if manifest.get("schema_version") != 1 or manifest.get("manifest_id") != MANIFEST_ID:
        errors.append("shell token manifest identity/schema is invalid")
    if manifest.get("authorship") != "original_billiardgl_product_shell_ui" or \
            manifest.get("external_assets") != []:
        errors.append("shell package must be original and contain no external assets")
    token_entry = manifest.get("token_set", {})
    proof_entry = manifest.get("flat_proof", {})
    if token_entry.get("file") != TOKEN_FILE or token_entry.get("token_set_id") != TOKEN_SET_ID or \
            token_entry.get("icon_count") != 43 or token_entry.get("screen_count") != 9 or \
            token_entry.get("palette_modes") != ["standard", "color_vision_safe"]:
        errors.append("shell token manifest binding is incomplete")
    if proof_entry.get("file") != PROOF_FILE or proof_entry.get("runtime_capture") is not False or \
            proof_entry.get("user_research") is not False:
        errors.append("flat proof cannot claim GPU capture or user research")
    integration = manifest.get("integration_contract", {})
    if integration.get("standalone") is not True or \
            integration.get("unknown_token_policy") != "hard_error_no_fallback" or \
            integration.get("existing_hud_token_set_modified") is not False or \
            integration.get("gpu_capture_required_for_runtime_acceptance") is not True:
        errors.append("standalone integration/evidence policy is invalid")

    token_path = asset_dir / TOKEN_FILE
    proof_path = asset_dir / PROOF_FILE
    try:
        token_bytes = token_path.read_bytes()
        token_set = json.loads(token_bytes)
    except (OSError, json.JSONDecodeError) as error:
        errors.append("cannot load shell token set: " + str(error))
        token_bytes, token_set = b"", {}
    expected = build_token_set()
    if token_set != expected:
        errors.append("shell token geometry/usage/contract differs from approved generated set")
    if token_set.get("schema_version") != 1 or token_set.get("token_set_id") != TOKEN_SET_ID or \
            token_set.get("namespace") != "product_shell" or \
            token_set.get("authorship") != "original_billiardgl_vector_tokens" or \
            token_set.get("external_assets") != []:
        errors.append("shell token identity/authorship/external-assets contract is invalid")
    gpu = token_set.get("gpu_contract", {})
    if gpu.get("allowed_primitives") != ["rect", "circle", "polyline", "polygon"] or \
            gpu.get("unknown_token_policy") != "hard_error_no_fallback" or \
            gpu.get("svg_runtime_dependency") is not False or \
            gpu.get("font_backend") != "existing_freetype_sdf_atlas":
        errors.append("shell GPU expansion contract is invalid")
    if token_set.get("aliases") != {}:
        errors.append("shell aliases must remain empty; approximations and cycles are forbidden")
    icons = token_set.get("icon_tokens", {})
    if set(icons) != set(expected["icon_tokens"]) or len(icons) != 43:
        errors.append("shell icon token inventory must exactly match all 43 emitted tokens")
    for name, icon in icons.items():
        commands = icon.get("commands")
        if not isinstance(commands, list) or not commands:
            errors.append(name + " has no geometry commands")
            continue
        for index, command in enumerate(commands):
            if not isinstance(command, dict):
                errors.append("%s command %d is not an object" % (name, index))
            else:
                _validate_command(command, "%s command %d" % (name, index), errors)
        if icon.get("usage") != expected["icon_tokens"][name]["usage"]:
            errors.append(name + " screen/state usage differs")

    palettes = token_set.get("palette_modes", {})
    if palettes != PALETTES:
        errors.append("standard/CVD palette modes differ")
    for name, palette in palettes.items():
        surface = _rgb(palette.get("surface"))
        for role in ("text_primary", "text_secondary"):
            color = _rgb(palette.get(role))
            if surface is None or color is None or _contrast(surface, color) < 4.5:
                errors.append("%s %s contrast is below 4.5:1" % (name, role))
        for role in ("icon_primary", "accent", "focus", "error", "disabled"):
            color = _rgb(palette.get(role))
            if surface is None or color is None or _contrast(surface, color) < 3.0:
                errors.append("%s %s graphic contrast is below 3:1" % (name, role))
    accessibility = token_set.get("accessibility_contract", {})
    if accessibility.get("never_color_only") is not True or \
            accessibility.get("required_redundant_channels") != \
            ["shape", "adjacent_text_label", "state_or_value_text"] or \
            not all(accessibility.get(key) is True for key in (
                "disabled_state_preserves_shape_and_text", "focus_state_uses_outline_plus_chevron_plus_text",
                "selected_state_uses_diamond_check_plus_text", "error_state_uses_octagon_cross_plus_error_text")):
        errors.append("shape/text/state accessibility redundancy is incomplete")
    scale = token_set.get("typography_and_scale", {})
    if scale.get("font_backend") != "existing_freetype_sdf_atlas" or \
            scale.get("minimum_text_logical_px", 0) < 14 or \
            scale.get("minimum_text_retina_physical_px", 0) < 21 or \
            scale.get("focus_chevron_minimum_logical_px", 0) < 10 or \
            len(scale.get("supported_viewports", [])) != 3:
        errors.append("font/minimum-size/viewport contract is invalid")
    if token_set.get("screen_icon_usage") != SCREEN_USAGE or \
            set(token_set.get("screen_icon_usage", {})) != set(SCREEN_USAGE):
        errors.append("all nine ProductShell screen usages are required")
    states = token_set.get("global_state_icon_usage", {})
    if any(states.get(name) != tokens for name, tokens in FIXED_STATE_USAGE.items()) or \
            set(states) != {"error", "fallback_item", "selected", "focus", "disabled", "normal"}:
        errors.append("error/item/disabled/selected/focus state coverage is incomplete")
    debug = token_set.get("debug_isolation", {})
    if debug.get("shipping_namespace") != "product_shell" or \
            debug.get("debug_namespace") != "debug_overlay" or \
            debug.get("shares_runtime_icon_nodes") is not False or \
            debug.get("debug_default_visible") is not False or \
            debug.get("debug_allowed_in_release_capture") is not False:
        errors.append("shipping/debug token isolation is invalid")

    if verify_hashes and token_bytes and token_entry.get("sha256") != _sha256(token_bytes):
        errors.append("shell token SHA-256 differs")
    try:
        proof_bytes = proof_path.read_bytes()
        root = ET.fromstring(proof_bytes)
    except (OSError, ET.ParseError) as error:
        errors.append("cannot load flat proof: " + str(error))
        proof_bytes, root = b"", None
    if root is not None:
        if root.get("width") != "1920" or root.get("height") != "1080" or \
                root.get("data-proof-kind") != "flat-token-proof" or \
                root.get("data-runtime-capture") != "false" or \
                root.get("data-user-research") != "false":
            errors.append("flat proof metadata/evidence boundary is invalid")
        nodes = [node for node in root.iter() if node.get("data-icon-token")]
        if len(nodes) != 86 or {node.get("data-icon-token") for node in nodes} != set(icons) or \
                {node.get("data-palette-mode") for node in nodes} != set(PALETTES):
            errors.append("flat proof must show all 43 tokens in both palettes")
    if verify_hashes and proof_bytes and proof_entry.get("sha256") != _sha256(proof_bytes):
        errors.append("shell flat proof SHA-256 differs")

    if audit_sources:
        try:
            controller = (repository_root / "src/Billiards/product_shell_controller.cpp").read_text()
            draw_list = (repository_root / "src/Billiards/render/ui/product_shell_draw_list.cpp").read_text()
            header = (repository_root / "src/Billiards/product_shell_controller.h").read_text()
            actual_tokens = audit_cpp_icon_tokens(controller, draw_list)
            if actual_tokens != set(expected["icon_tokens"]):
                errors.append("C++ ProductShell emitted token audit differs: missing=%s extra=%s" %
                              (sorted(set(expected["icon_tokens"]) - actual_tokens),
                               sorted(actual_tokens - set(expected["icon_tokens"]))))
            if _enum_screens(header) != expected["source_audit"]["screen_enum"]:
                errors.append("ProductScreen enum no longer matches the audited nine screens")
        except OSError as error:
            errors.append("cannot audit ProductShell C++ sources: " + str(error))
    return errors, {"icons": len(icons), "palettes": len(palettes),
                    "screens": len(token_set.get("screen_icon_usage", {})),
                    "proof_icons": 86 if root is not None else 0}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", nargs="?",
                        default="assets/modern/hud/" + MANIFEST_FILE)
    args = parser.parse_args()
    path = Path(args.manifest)
    try:
        manifest = json.loads(path.read_text())
    except (OSError, json.JSONDecodeError) as error:
        print("ProductShell token validation failed: " + str(error), file=sys.stderr)
        return 1
    errors, report = validate(manifest, path.parent)
    if errors:
        for error in errors:
            print("ProductShell token validation failed: " + error, file=sys.stderr)
        return 1
    print("ProductShell shipping tokens valid: icons=%d palettes=%d screens=%d proof_icons=%d" %
          (report["icons"], report["palettes"], report["screens"], report["proof_icons"]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
