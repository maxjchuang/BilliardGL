#!/usr/bin/env python3
"""Validate the Phase 5/M4 product UX acceptance package."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path


REASON_CODES = {
    "none": "rules.chinese8.none",
    "illegal_break": "rules.chinese8.foul.illegal_break",
    "no_object_ball_contact": "rules.chinese8.foul.no_object_contact",
    "wrong_first_contact": "rules.chinese8.foul.wrong_first_contact",
    "no_rail_after_contact": "rules.chinese8.foul.no_rail_after_contact",
    "cue_ball_scratch": "rules.chinese8.foul.cue_ball_scratch",
    "cue_ball_off_table": "rules.chinese8.foul.cue_ball_off_table",
    "object_ball_off_table": "rules.chinese8.foul.object_ball_off_table",
    "illegal_eight_ball": "rules.chinese8.loss.illegal_eight",
}
EXPECTED_ACTIONS = {
    "toggle_help", "toggle_aim", "shoot", "adjust_aim", "power_increase",
    "power_decrease", "camera_orbit", "camera_pan", "zoom_in", "zoom_out",
    "toggle_camera_anchor", "return_to_cue_ball", "quit", "open_settings",
    "reset_training_attempt",
}
EXPECTED_PROFILES = {
    "minimum_1280x720_96dpi", "reference_1920x1080_96dpi",
    "high_dpi_1920x1080_144dpi",
}


class ValidationError(Exception):
    def __init__(self, code: str, message: str):
        super().__init__(message)
        self.code = code


def require(condition: bool, code: str, message: str) -> None:
    if not condition:
        raise ValidationError(code, message)


def sha256(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def finite_tree(value: object) -> bool:
    if isinstance(value, float):
        return math.isfinite(value)
    if isinstance(value, dict):
        return all(finite_tree(item) for item in value.values())
    if isinstance(value, list):
        return all(finite_tree(item) for item in value)
    return True


def contexts_overlap(first: str, second: str) -> bool:
    return first == "global" or second == "global" or first == second


def validate_reasons(items: object) -> None:
    require(isinstance(items, list), "E_REASONS", "reason_messages")
    by_code = {item.get("code"): item for item in items if isinstance(item, dict)}
    require(set(by_code) == set(REASON_CODES) and len(items) == len(by_code), "E_REASON_COVERAGE", "reason code coverage")
    priorities = []
    for code, expected_key in REASON_CODES.items():
        item = by_code[code]
        require(item.get("text_key") == expected_key, "E_REASON_TEXT_KEY", code)
        locale = item.get("locale", {})
        require(all(isinstance(locale.get(key), str) and locale[key].strip() for key in ("title_zh_cn", "title_en", "body_zh_cn", "body_en")), "E_REASON_LOCALE", code)
        presentation = item.get("presentation", {})
        duration = presentation.get("duration", {})
        require(isinstance(presentation.get("priority"), int) and 0 <= presentation["priority"] <= 100, "E_REASON_PRIORITY", code)
        require(duration.get("mode") in {"hidden", "timed", "until_decision", "persistent_until_acknowledged"}, "E_REASON_DURATION", code)
        priorities.append(presentation["priority"])
        if code == "none":
            require(presentation.get("icon_and_text_required") is False and duration == {"mode": "hidden", "milliseconds": 0}, "E_REASON_SENTINEL", code)
            require(item.get("runtime_binding", {}).get("status") == "sentinel_no_display", "E_REASON_RUNTIME_GATE", code)
            continue
        icon = item.get("icon", {})
        require(presentation.get("icon_and_text_required") is True, "E_COLOR_ONLY_REASON", code)
        require(all(isinstance(icon.get(key), str) and icon[key].strip() for key in ("token", "silhouette", "non_color_prefix", "accessible_label_zh_cn", "accessible_label_en")), "E_COLOR_ONLY_REASON", code)
        require(icon["silhouette"] != "color_only" and icon["non_color_prefix"].startswith("["), "E_COLOR_ONLY_REASON", code)
        binding = item.get("runtime_binding", {})
        require(binding.get("status") == "capability_gated" and binding.get("required_capability") == "rules_reason_hud_bridge_v1", "E_REASON_RUNTIME_GATE", code)
        require(binding.get("evidence") == "rules_engine_unit_tested_not_connected_to_game_runtime", "E_REASON_RUNTIME_GATE", code)
    require(len(priorities) == len(set(priorities)), "E_REASON_PRIORITY", "priorities must be deterministic and unique")


def validate_ai(items: object) -> None:
    require(isinstance(items, list) and [item.get("id") for item in items] == ["rookie", "club", "expert"], "E_AI_TIERS", "AI tiers")
    prior_max = 0.0
    for index, item in enumerate(items):
        locale = item.get("locale", {})
        require(all(isinstance(locale.get(key), str) and locale[key].strip() for key in ("name_zh_cn", "name_en", "description_zh_cn", "description_en")), "E_AI_LOCALE", item.get("id", "?"))
        target = item.get("target_success_rate", {})
        low, high = target.get("minimum"), target.get("maximum")
        require(isinstance(low, (int, float)) and isinstance(high, (int, float)) and 0 <= low < high <= 1, "E_AI_TARGET_RANGES", item["id"])
        require(index == 0 or low > prior_max, "E_AI_TARGET_RANGES", item["id"])
        prior_max = high
        evaluation = item.get("evaluation", {})
        require(evaluation.get("attempts") == 600 and sum(evaluation.get("scenario_strata", {}).values()) == 600, "E_AI_MEASUREMENT", item["id"])
        require(evaluation.get("confidence_reporting") == "wilson_95_percent_interval" and evaluation.get("time_control") == "fixed_step_no_wall_clock", "E_AI_MEASUREMENT", item["id"])
        binding = item.get("runtime_binding", {})
        require(binding == {"required_capability": "single_player_ai_v1", "status": "capability_gated"}, "E_AI_RUNTIME_GATE", item["id"])


def validate_inputs(value: object) -> None:
    require(isinstance(value, dict), "E_INPUT", "input_rebinding")
    actions = value.get("actions", [])
    require(isinstance(actions, list) and {item.get("id") for item in actions} == EXPECTED_ACTIONS and len(actions) == len(EXPECTED_ACTIONS), "E_ACTION_COVERAGE", "actions")
    occupied: list[tuple[str, str, str]] = []
    devices = set()
    for action in actions:
        require(action.get("context") in {"global", "play", "aim", "observe", "training"}, "E_ACTION_CONTEXT", action.get("id", "?"))
        label = action.get("label", {})
        require(all(isinstance(label.get(key), str) and label[key] for key in ("zh_cn", "en")), "E_ACTION_LOCALE", action.get("id", "?"))
        defaults = action.get("defaults")
        require(isinstance(defaults, list) and defaults, "E_ACTION_BINDING", action["id"])
        for item in defaults:
            require(item.get("device") in {"keyboard", "mouse"} and item.get("status") in {"runtime_existing", "capability_gated"}, "E_ACTION_BINDING", action["id"])
            require(isinstance(item.get("control"), str) and isinstance(item.get("trigger"), str), "E_ACTION_BINDING", action["id"])
            devices.add(item["device"])
            normalized = f"{item['device']}:{item['control']}:{item['trigger']}".lower()
            for other_normalized, other_context, other_id in occupied:
                require(not (normalized == other_normalized and contexts_overlap(action["context"], other_context)), "E_DEFAULT_BINDING_CONFLICT", f"{action['id']} conflicts with {other_id}")
            occupied.append((normalized, action["context"], action["id"]))
    require(devices == {"keyboard", "mouse"}, "E_DEVICE_COVERAGE", "keyboard and mouse")
    conflicts = value.get("conflict_detection", {})
    require(conflicts.get("hard_conflict") == "same normalized control and overlapping context" and conflicts.get("on_hard_conflict") == "block_apply_and_identify_both_actions", "E_CONFLICT_POLICY", "conflicts")
    require(value.get("restore_defaults", {}).get("confirmation_required") is True, "E_RESTORE_DEFAULTS", "restore defaults")
    require(value.get("persistence") == "atomic_write_after_validation", "E_INPUT_PERSISTENCE", "persistence")
    require(value.get("runtime_binding") == {"required_capability": "input_rebinding_v1", "status": "capability_gated"}, "E_INPUT_RUNTIME_GATE", "runtime binding")


def rectangles_overlap(first: list[float], second: list[float]) -> bool:
    return not (first[0] + first[2] <= second[0] or second[0] + second[2] <= first[0] or first[1] + first[3] <= second[1] or second[1] + second[3] <= first[1])


def validate_profiles(profiles: object) -> None:
    require(isinstance(profiles, dict) and set(profiles) == EXPECTED_PROFILES, "E_RESPONSIVE_PROFILES", "profiles")
    for profile_id, profile in profiles.items():
        width, height = profile.get("physical_viewport_px", [0, 0])
        safe = profile.get("safe_area_px", {})
        rects = list(profile.get("regions_px", {}).items())
        require(len(rects) == 3, "E_RESPONSIVE_REGIONS", profile_id)
        for name, rect in rects:
            require(isinstance(rect, list) and len(rect) == 4 and rect[2] > 0 and rect[3] > 0, "E_RESPONSIVE_REGIONS", f"{profile_id}:{name}")
            require(rect[0] >= safe.get("left", -1) and rect[1] >= safe.get("top", -1) and rect[0] + rect[2] <= width - safe.get("right", -1) and rect[1] + rect[3] <= height - safe.get("bottom", -1), "E_RESPONSIVE_SAFE_AREA", f"{profile_id}:{name}")
        for index, (_, first) in enumerate(rects):
            for _, second in rects[index + 1:]:
                require(not rectangles_overlap(first, second), "E_RESPONSIVE_OVERLAP", profile_id)
        fonts = profile.get("font_px", {})
        require(fonts.get("caption", 0) >= 14 and fonts.get("heading", 0) > fonts.get("body", 0) >= fonts.get("caption", 0), "E_RESPONSIVE_FONT", profile_id)
    high = profiles["high_dpi_1920x1080_144dpi"]
    require(high.get("logical_viewport_px") == [1280, 720] and high.get("physical_viewport_px") == [1920, 1080] and high.get("resolved_scale") == 1.5, "E_HIGH_DPI", "high DPI")


def validate_accessibility(value: object) -> None:
    require(isinstance(value, dict), "E_ACCESSIBILITY", "accessibility")
    contrast = value.get("contrast", {})
    require(contrast.get("normal_text_min", 0) >= 4.5 and contrast.get("large_text_min", 0) >= 3.0 and contrast.get("focus_indicator_min", 0) >= 3.0, "E_CONTRAST", "contrast")
    require(len(value.get("color_vision_modes", [])) >= 5 and "high_contrast_patterns" in value.get("color_vision_modes", []), "E_COLOR_VISION", "color vision")
    require(value.get("subtitles", {}).get("speaker_labels") is True and value.get("subtitles", {}).get("non_speech_captions") is True, "E_SUBTITLES", "subtitles")
    require(value.get("font_scale", {}).get("maximum", 0) >= 1.35 and value.get("font_scale", {}).get("reflow_required_above", 9) <= 1.15, "E_FONT_SCALE", "font scale")
    require([item.get("id") for item in value.get("assist_levels", [])] == ["off", "basic", "guided"] and value.get("assist_invariant") == "never_auto_aim_or_auto_shot", "E_ASSIST_LEVELS", "assist")


def validate_manifest(value: object) -> None:
    require(isinstance(value, dict), "E_ROOT", "manifest")
    item: dict = value
    require(item.get("schema") == "product_ux_acceptance_v1" and item.get("schema_version") == 1, "E_SCHEMA", "manifest")
    require(item.get("authorship") == "original_billiardgl_product_ux_no_third_party_assets", "E_AUTHORSHIP", "manifest")
    refs = item.get("references", {})
    require(refs.get("ruleset_id") == "wpa_heyball_2025_08_16_v1" and refs.get("hud_manifest") == "assets/modern/hud/focused_practice_hud_v1.hud.json", "E_REFERENCES", "references")
    require(finite_tree(item), "E_NONFINITE", "manifest")
    validate_reasons(item.get("reason_messages"))
    validate_ai(item.get("ai_difficulty"))
    validate_inputs(item.get("input_rebinding"))
    validate_accessibility(item.get("accessibility"))
    validate_profiles(item.get("responsive_profiles"))
    settings = item.get("settings", {})
    require(settings.get("apply_semantics") == "preview_then_atomic_commit" and settings.get("cancel_semantics") == "discard_unapplied_changes", "E_SETTINGS_SEMANTICS", "settings")
    require(settings.get("focus_navigation", {}).get("keyboard_only_complete") is True and settings.get("focus_navigation", {}).get("visible_focus_required") is True, "E_FOCUS_NAVIGATION", "focus")


def validate_package(path: Path, check_bad_inputs: bool = True) -> dict:
    manifest = json.loads(path.read_text(encoding="utf-8"))
    validate_manifest(manifest)
    root = path.parent
    mockups = manifest.get("mockups", [])
    require(len(mockups) == 3 and {item.get("profile") for item in mockups} == EXPECTED_PROFILES, "E_MOCKUP_COVERAGE", "mockups")
    for item in mockups:
        payload = (root / item["file"]).read_bytes()
        require(sha256(payload) == item.get("sha256") and payload.startswith(b"<svg"), "E_MOCKUP_HASH", item["file"])
    verified = 0
    if check_bad_inputs:
        entries = manifest.get("bad_input_fixtures", [])
        require(len(entries) == 6, "E_BAD_FIXTURE_COUNT", "bad inputs")
        for entry in entries:
            payload = (root / entry["file"]).read_bytes()
            require(sha256(payload) == entry.get("sha256"), "E_BAD_FIXTURE_HASH", entry["file"])
            fixture = json.loads(payload)
            require(fixture.get("expected_error_code") == entry.get("expected_error_code"), "E_BAD_FIXTURE", entry["file"])
            try:
                validate_manifest(fixture.get("manifest"))
            except ValidationError as error:
                require(error.code == entry["expected_error_code"], "E_BAD_FIXTURE", f"{entry['file']}: expected {entry['expected_error_code']}, got {error.code}")
                verified += 1
            else:
                raise ValidationError("E_BAD_FIXTURE", f"{entry['file']} unexpectedly passed")
    return {"ai_tiers": len(manifest["ai_difficulty"]), "bad_inputs": verified, "mockups": len(mockups), "reasons": len(manifest["reason_messages"])}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("path", nargs="?", type=Path, default=Path("assets/modern/product_ux/product_ux_v1.json"))
    parser.add_argument("--skip-bad-inputs", action="store_true")
    args = parser.parse_args()
    try:
        result = validate_package(args.path.resolve(), not args.skip_bad_inputs)
    except (ValidationError, KeyError, OSError, json.JSONDecodeError, ValueError) as error:
        code = error.code if isinstance(error, ValidationError) else "E_IO_OR_JSON"
        print(f"FAIL {code}: {error}")
        return 1
    print(f"PASS product_ux_acceptance_v1: reasons={result['reasons']} ai_tiers={result['ai_tiers']} mockups={result['mockups']} bad_inputs={result['bad_inputs']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
