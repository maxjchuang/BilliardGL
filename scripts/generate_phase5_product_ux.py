#!/usr/bin/env python3
"""Generate the Phase 5/M4 product UX and content acceptance package."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / "assets" / "modern" / "product_ux"
RULESET_ID = "wpa_heyball_2025_08_16_v1"


def pretty(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n").encode()


def digest(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def write(path: Path, payload: bytes) -> str:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(payload)
    return digest(payload)


def reason(code: str, key: str, zh_title: str, en_title: str, zh_body: str,
           en_body: str, priority: int, duration: object, icon: str,
           silhouette: str, classification: str = "foul") -> dict:
    if code == "none":
        binding = {"status": "sentinel_no_display", "required_capability": None}
    else:
        binding = {
            "evidence": "rules_engine_unit_tested_not_connected_to_game_runtime",
            "required_capability": "rules_reason_hud_bridge_v1",
            "status": "capability_gated",
        }
    return {
        "classification": classification,
        "code": code,
        "color_role": "destructive" if classification in {"foul", "loss"} else "warning",
        "icon": {
            "accessible_label_en": en_title,
            "accessible_label_zh_cn": zh_title,
            "non_color_prefix": "[LOSS]" if classification == "loss" else "[FOUL]" if classification == "foul" else "[DECISION]",
            "silhouette": silhouette,
            "token": icon,
        },
        "locale": {
            "body_en": en_body,
            "body_zh_cn": zh_body,
            "title_en": en_title,
            "title_zh_cn": zh_title,
        },
        "presentation": {
            "duration": duration,
            "icon_and_text_required": code != "none",
            "priority": priority,
            "screen_reader_announcement": "assertive" if priority >= 80 else "polite",
        },
        "runtime_binding": binding,
        "text_key": key,
    }


def reasons() -> list[dict]:
    return [
        reason("none", "rules.chinese8.none", "无判罚", "No ruling", "不显示判罚提示。", "No ruling banner is displayed.", 0, {"mode": "hidden", "milliseconds": 0}, "none", "none", "sentinel"),
        reason("illegal_break", "rules.chinese8.foul.illegal_break", "开球未达要求", "Illegal break", "未进球且少于四颗目标球碰库；请选择接受台面或重新摆球。", "No ball was pocketed and fewer than four object balls reached a cushion; choose table acceptance or a re-rack.", 60, {"mode": "until_decision", "fallback_milliseconds": 12000}, "decision_diamond", "diamond_plus_three_bars", "decision"),
        reason("no_object_ball_contact", "rules.chinese8.foul.no_object_contact", "未碰目标球", "No object-ball contact", "母球未先碰到任何目标球，对手获得自由球。", "The cue ball did not contact an object ball first; the opponent receives ball in hand.", 75, {"mode": "timed", "milliseconds": 4500}, "empty_contact_ring", "broken_ring", "foul"),
        reason("wrong_first_contact", "rules.chinese8.foul.wrong_first_contact", "首碰球错误", "Wrong first contact", "母球首先碰到了当前不合法的目标球。", "The cue ball first contacted a ball that was not a legal target.", 70, {"mode": "timed", "milliseconds": 4500}, "numbered_ball_cross", "circle_with_diagonal_bar", "foul"),
        reason("no_rail_after_contact", "rules.chinese8.foul.no_rail_after_contact", "碰球后未吃库", "No cushion after contact", "未进球，且合法接触后没有球碰库。", "No ball was pocketed and no ball reached a cushion after legal contact.", 65, {"mode": "timed", "milliseconds": 4500}, "rail_gap", "three_bars_with_gap", "foul"),
        reason("cue_ball_scratch", "rules.chinese8.foul.cue_ball_scratch", "母球落袋", "Cue-ball scratch", "母球落袋，对手获得自由球。", "The cue ball was pocketed; the opponent receives ball in hand.", 90, {"mode": "timed", "milliseconds": 5000}, "cue_ball_down", "white_circle_down_arrow", "foul"),
        reason("cue_ball_off_table", "rules.chinese8.foul.cue_ball_off_table", "母球离台", "Cue ball off table", "母球离开台面，对手获得自由球。", "The cue ball left the table; the opponent receives ball in hand.", 92, {"mode": "timed", "milliseconds": 5000}, "cue_ball_exit", "white_circle_outside_frame", "foul"),
        reason("object_ball_off_table", "rules.chinese8.foul.object_ball_off_table", "目标球离台", "Object ball off table", "一颗或多颗目标球离开台面，本杆犯规。", "One or more object balls left the table; the shot is a foul.", 85, {"mode": "timed", "milliseconds": 5000}, "object_ball_exit", "numbered_circle_outside_frame", "foul"),
        reason("illegal_eight_ball", "rules.chinese8.loss.illegal_eight", "黑八违规落袋", "Illegal 8-ball", "黑八在不合法条件下落袋，本局判负。", "The 8-ball was pocketed illegally; the rack is lost.", 100, {"mode": "persistent_until_acknowledged", "minimum_milliseconds": 6000}, "eight_ball_loss", "octagon_with_eight_and_cross", "loss"),
    ]


def ai_tiers() -> list[dict]:
    shared = {
        "evaluation": {
            "attempts": 600,
            "confidence_reporting": "wilson_95_percent_interval",
            "fixed_seed_set": "ai_acceptance_seeds_v1_0_to_199_repeated_per_scenario_class",
            "metric": "rack_win_rate_against_reference_policy",
            "ruleset_id": RULESET_ID,
            "scenario_strata": {"break": 200, "open_table": 200, "runout_pressure": 200},
            "time_control": "fixed_step_no_wall_clock",
        },
        "runtime_binding": {
            "required_capability": "single_player_ai_v1",
            "status": "capability_gated",
        },
    }
    return [
        {**copy.deepcopy(shared), "id": "rookie", "order": 1,
         "locale": {"name_zh_cn": "新手", "name_en": "Rookie", "description_zh_cn": "会完成明显进球，但会留下可利用的机会，适合熟悉规则。", "description_en": "Takes obvious pots but leaves exploitable chances; intended for learning the rules."},
         "target_success_rate": {"minimum": 0.35, "maximum": 0.50}},
        {**copy.deepcopy(shared), "id": "club", "order": 2,
         "locale": {"name_zh_cn": "球房", "name_en": "Club", "description_zh_cn": "能规划两到三颗球的路线，仍会出现可读的走位误差。", "description_en": "Plans two-to-three-ball routes while retaining readable positional errors."},
         "target_success_rate": {"minimum": 0.58, "maximum": 0.72}},
        {**copy.deepcopy(shared), "id": "expert", "order": 3,
         "locale": {"name_zh_cn": "高手", "name_en": "Expert", "description_zh_cn": "优先处理局面风险并稳定连续进球，但不读取隐藏状态或作弊。", "description_en": "Prioritizes table risk and runs balls consistently without hidden-state access or cheats."},
         "target_success_rate": {"minimum": 0.78, "maximum": 0.88}},
    ]


def binding(device: str, control: str, trigger: str, status: str = "runtime_existing") -> dict:
    return {"control": control, "device": device, "status": status, "trigger": trigger}


def actions() -> list[dict]:
    return [
        {"id": "toggle_help", "context": "global", "label": {"zh_cn": "帮助", "en": "Help"}, "defaults": [binding("keyboard", "H", "press")]},
        {"id": "toggle_aim", "context": "play", "label": {"zh_cn": "切换瞄准", "en": "Toggle aim"}, "defaults": [binding("keyboard", "Tab", "press")]},
        {"id": "shoot", "context": "aim", "label": {"zh_cn": "击球", "en": "Shoot"}, "defaults": [binding("mouse", "LeftButton", "release")]},
        {"id": "adjust_aim", "context": "aim", "label": {"zh_cn": "调整方向", "en": "Adjust aim"}, "defaults": [binding("mouse", "MoveX", "continuous")]},
        {"id": "power_increase", "context": "aim", "label": {"zh_cn": "增加力度", "en": "Increase power"}, "defaults": [binding("keyboard", "+", "press"), binding("mouse", "WheelUp", "step")]},
        {"id": "power_decrease", "context": "aim", "label": {"zh_cn": "降低力度", "en": "Decrease power"}, "defaults": [binding("keyboard", "-", "press"), binding("mouse", "WheelDown", "step")]},
        {"id": "camera_orbit", "context": "observe", "label": {"zh_cn": "环绕镜头", "en": "Orbit camera"}, "defaults": [binding("keyboard", "ArrowKeys", "continuous"), binding("mouse", "LeftDrag", "continuous")]},
        {"id": "camera_pan", "context": "observe", "label": {"zh_cn": "平移镜头", "en": "Pan camera"}, "defaults": [binding("keyboard", "A/D", "continuous"), binding("mouse", "Shift+LeftDrag", "continuous")]},
        {"id": "zoom_in", "context": "observe", "label": {"zh_cn": "镜头拉近", "en": "Zoom in"}, "defaults": [binding("keyboard", "W", "continuous"), binding("mouse", "WheelUp", "step")]},
        {"id": "zoom_out", "context": "observe", "label": {"zh_cn": "镜头拉远", "en": "Zoom out"}, "defaults": [binding("keyboard", "S", "continuous"), binding("mouse", "WheelDown", "step")]},
        {"id": "toggle_camera_anchor", "context": "play", "label": {"zh_cn": "切换镜头跟随", "en": "Toggle camera anchor"}, "defaults": [binding("keyboard", "C", "press")]},
        {"id": "return_to_cue_ball", "context": "play", "label": {"zh_cn": "返回母球", "en": "Return to cue ball"}, "defaults": [binding("keyboard", "Space", "press")]},
        {"id": "quit", "context": "global", "label": {"zh_cn": "退出", "en": "Quit"}, "defaults": [binding("keyboard", "Escape", "press")]},
        {"id": "open_settings", "context": "global", "label": {"zh_cn": "设置", "en": "Settings"}, "defaults": [binding("keyboard", "F10", "press", "capability_gated")]},
        {"id": "reset_training_attempt", "context": "training", "label": {"zh_cn": "重置练习", "en": "Reset drill"}, "defaults": [binding("keyboard", "R", "press", "capability_gated")]},
    ]


def profiles() -> dict:
    return {
        "minimum_1280x720_96dpi": {
            "logical_viewport_px": [1280, 720], "physical_viewport_px": [1280, 720], "os_dpi": 96, "resolved_scale": 1.0,
            "safe_area_px": {"left": 43, "top": 24, "right": 43, "bottom": 36},
            "regions_px": {"navigation": [43, 48, 245, 600], "content": [312, 48, 925, 540], "footer": [312, 604, 925, 80]},
            "font_px": {"caption": 14, "body": 16, "heading": 24},
        },
        "reference_1920x1080_96dpi": {
            "logical_viewport_px": [1920, 1080], "physical_viewport_px": [1920, 1080], "os_dpi": 96, "resolved_scale": 1.0,
            "safe_area_px": {"left": 64, "top": 36, "right": 64, "bottom": 54},
            "regions_px": {"navigation": [64, 72, 360, 900], "content": [460, 72, 1396, 810], "footer": [460, 906, 1396, 120]},
            "font_px": {"caption": 14, "body": 18, "heading": 32},
        },
        "high_dpi_1920x1080_144dpi": {
            "logical_viewport_px": [1280, 720], "physical_viewport_px": [1920, 1080], "os_dpi": 144, "resolved_scale": 1.5,
            "safe_area_px": {"left": 64, "top": 36, "right": 64, "bottom": 54},
            "regions_px": {"navigation": [64, 72, 360, 900], "content": [460, 72, 1396, 810], "footer": [460, 906, 1396, 120]},
            "font_px": {"caption": 21, "body": 24, "heading": 36},
        },
    }


def svg_for(profile_id: str, profile: dict) -> bytes:
    width, height = profile["physical_viewport_px"]
    nav = profile["regions_px"]["navigation"]
    content = profile["regions_px"]["content"]
    footer = profile["regions_px"]["footer"]
    body = profile["font_px"]["body"]
    heading = profile["font_px"]["heading"]
    caption = profile["font_px"]["caption"]
    rows = [
        ("切换瞄准 / Toggle aim", "Tab"), ("击球 / Shoot", "Mouse Left Release"),
        ("调整力度 / Power", "+ / −  ·  Wheel"), ("镜头环绕 / Orbit", "Arrows  ·  Left Drag"),
        ("镜头平移 / Pan", "A / D  ·  Shift Drag"),
    ]
    row_h = max(54, int(54 * profile["resolved_scale"]))
    start_y = content[1] + int(120 * profile["resolved_scale"])
    lines = []
    for index, (label, key) in enumerate(rows):
        y = start_y + index * row_h
        lines.append(f'<text x="{content[0]+36}" y="{y}" class="body">{label}</text>')
        lines.append(f'<rect x="{content[0]+content[2]-330}" y="{y-body-12}" width="285" height="{body+28}" rx="8" class="key"/>')
        lines.append(f'<text x="{content[0]+content[2]-310}" y="{y}" class="body">{key}</text>')
    payload = f'''<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}" role="img" aria-labelledby="title desc">
<title id="title">BilliardGL Phase 5 settings mockup {profile_id}</title><desc id="desc">Input rebinding and accessibility settings with a non-color foul preview.</desc>
<style>.bg{{fill:#0D1216}}.panel{{fill:#10181E;stroke:#64737D;stroke-width:2}}.active{{fill:#17353B;stroke:#31B7C8;stroke-width:3}}.key{{fill:#0B1116;stroke:#C1CBD0;stroke-width:2}}.primary{{fill:#F4F7F8;font-family:system-ui,sans-serif;font-size:{heading}px;font-weight:600}}.body{{fill:#F4F7F8;font-family:system-ui,sans-serif;font-size:{body}px}}.caption{{fill:#C1CBD0;font-family:system-ui,sans-serif;font-size:{caption}px}}.warning{{fill:#F2BC55}}.danger{{fill:#F06B5D}}</style>
<rect width="{width}" height="{height}" class="bg"/><rect x="{nav[0]}" y="{nav[1]}" width="{nav[2]}" height="{nav[3]}" rx="16" class="panel"/><rect x="{content[0]}" y="{content[1]}" width="{content[2]}" height="{content[3]}" rx="16" class="panel"/><rect x="{footer[0]}" y="{footer[1]}" width="{footer[2]}" height="{footer[3]}" rx="16" class="panel"/>
<text x="{nav[0]+28}" y="{nav[1]+52}" class="primary">设置</text><rect x="{nav[0]+18}" y="{nav[1]+92}" width="{nav[2]-36}" height="56" rx="10" class="active"/><text x="{nav[0]+34}" y="{nav[1]+128}" class="body">输入绑定</text><text x="{nav[0]+34}" y="{nav[1]+190}" class="body">可访问性</text><text x="{nav[0]+34}" y="{nav[1]+242}" class="body">辅助等级</text>
<text x="{content[0]+36}" y="{content[1]+62}" class="primary">键鼠输入 / INPUT</text><text x="{content[0]+36}" y="{content[1]+96}" class="caption">选择一个动作后按新按键；同场景冲突会阻止保存。</text>{''.join(lines)}
<g transform="translate({content[0]+36},{content[1]+content[3]-92})"><path d="M24 0 L48 12 L48 38 L24 52 L0 38 L0 12 Z" class="danger"/><text x="17" y="34" class="body">!</text><text x="68" y="23" class="body">[犯规] 母球落袋</text><text x="68" y="50" class="caption">图标 + 轮廓 + 文本，绝不只依赖颜色</text></g>
<text x="{footer[0]+28}" y="{footer[1]+47}" class="body">恢复默认</text><text x="{footer[0]+footer[2]-330}" y="{footer[1]+47}" class="caption">Esc 取消</text><rect x="{footer[0]+footer[2]-190}" y="{footer[1]+18}" width="150" height="48" rx="10" class="active"/><text x="{footer[0]+footer[2]-157}" y="{footer[1]+50}" class="body">应用设置</text>
</svg>'''
    return payload.encode()


def manifest(mockup_entries: list[dict]) -> dict:
    return {
        "accessibility": {
            "assist_levels": [
                {"id": "off", "features": []},
                {"id": "basic", "features": ["aim_line", "first_contact_marker"]},
                {"id": "guided", "features": ["aim_line", "first_contact_marker", "target_zone", "rule_explanations"]},
            ],
            "assist_invariant": "never_auto_aim_or_auto_shot",
            "color_vision_modes": ["default", "deuteranopia", "protanopia", "tritanopia", "high_contrast_patterns"],
            "contrast": {"normal_text_min": 4.5, "large_text_min": 3.0, "focus_indicator_min": 3.0},
            "font_scale": {"default": 1.0, "maximum": 1.35, "minimum": 0.85, "reflow_required_above": 1.15},
            "subtitles": {"default": True, "options": ["off", "essential", "all"], "speaker_labels": True, "non_speech_captions": True},
        },
        "ai_difficulty": ai_tiers(),
        "authorship": "original_billiardgl_product_ux_no_third_party_assets",
        "input_rebinding": {
            "actions": actions(),
            "conflict_detection": {
                "hard_conflict": "same normalized control and overlapping context",
                "on_hard_conflict": "block_apply_and_identify_both_actions",
                "reserved_controls": ["OS_reserved_shortcuts"],
                "same_control_disjoint_context": "allowed_with_explicit_context_labels",
            },
            "excluded_debug_bindings": ["legacy_light_toggle_0_to_9", "legacy_light_toggle_TYUIOP", "debug_toggle_all_lights_F"],
            "persistence": "atomic_write_after_validation",
            "restore_defaults": {"confirmation_required": True, "scope": ["current_category", "all_inputs"]},
            "runtime_binding": {"required_capability": "input_rebinding_v1", "status": "capability_gated"},
        },
        "mockups": mockup_entries,
        "reason_messages": reasons(),
        "reason_ordering": {"maximum_simultaneous_visible": 3, "sort": ["priority_desc", "code_ascending"], "overflow": "plus_n_more_with_details_action"},
        "references": {
            "hud_manifest": "assets/modern/hud/focused_practice_hud_v1.hud.json",
            "rules_profile": "docs/rules/wpa-heyball-2025-08-16-profile.md",
            "ruleset_id": RULESET_ID,
        },
        "responsive_profiles": profiles(),
        "schema": "product_ux_acceptance_v1",
        "schema_version": 1,
        "settings": {
            "apply_semantics": "preview_then_atomic_commit",
            "categories": ["gameplay", "input", "accessibility", "audio", "display"],
            "cancel_semantics": "discard_unapplied_changes",
            "focus_navigation": {"keyboard_only_complete": True, "visible_focus_required": True, "wrap": False},
        },
    }


def bad_inputs(base: dict) -> dict[str, tuple[str, dict]]:
    result = {}
    value = copy.deepcopy(base); del value["reason_messages"][1]["locale"]["title_en"]
    result["missing_reason_locale.invalid.json"] = ("E_REASON_LOCALE", value)
    value = copy.deepcopy(base); value["reason_messages"][2]["presentation"]["icon_and_text_required"] = False
    result["color_only_reason.invalid.json"] = ("E_COLOR_ONLY_REASON", value)
    value = copy.deepcopy(base); value["ai_difficulty"][1]["target_success_rate"] = {"minimum": 0.45, "maximum": 0.65}
    result["overlapping_ai_ranges.invalid.json"] = ("E_AI_TARGET_RANGES", value)
    value = copy.deepcopy(base); value["input_rebinding"]["actions"][1]["defaults"] = [binding("keyboard", "H", "press")]; value["input_rebinding"]["actions"][1]["context"] = "global"
    result["binding_conflict.invalid.json"] = ("E_DEFAULT_BINDING_CONFLICT", value)
    value = copy.deepcopy(base); value["reason_messages"][1]["runtime_binding"]["status"] = "connected"
    result["false_runtime_binding.invalid.json"] = ("E_REASON_RUNTIME_GATE", value)
    value = copy.deepcopy(base); value["responsive_profiles"]["minimum_1280x720_96dpi"]["regions_px"]["content"] = [1200, 48, 925, 540]
    result["unsafe_responsive_region.invalid.json"] = ("E_RESPONSIVE_SAFE_AREA", value)
    return result


def generate(output: Path) -> None:
    output.mkdir(parents=True, exist_ok=True)
    mockups = []
    for profile_id, profile in profiles().items():
        filename = f"phase5_settings_{profile_id}.svg"
        payload = svg_for(profile_id, profile)
        mockups.append({"file": filename, "profile": profile_id, "sha256": write(output / filename, payload), "state": "input_and_accessibility_settings"})
    base = manifest(mockups)
    bad_entries = []
    for filename, (code, value) in bad_inputs(base).items():
        fixture = {"expected_error_code": code, "manifest": value}
        bad_entries.append({"expected_error_code": code, "file": f"bad_inputs/{filename}", "sha256": write(output / "bad_inputs" / filename, pretty(fixture))})
    base["bad_input_fixtures"] = bad_entries
    write(output / "product_ux_v1.json", pretty(base))
    print(f"generated Phase 5 product UX package: {output}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    generate(args.output.resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
