#!/usr/bin/env python3
"""Generate the Phase-5 bilingual rules-feedback copy contract and flat proof."""

from __future__ import annotations

import argparse
import hashlib
import html
import json
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
MANIFEST_NAME = "phase5-rules-shipping-copy-v1.json"
SCHEMA_NAME = "phase5-rules-shipping-copy-v1.schema.json"
PROOF_NAME = "phase5-rules-shipping-copy-flat-proof-v1.svg"

RULES_CPP = ROOT / "src/Billiards/rules/chinese_eight_ball_rules.cpp"
HUD_CPP = ROOT / "src/Billiards/product_hud.cpp"
PROFILE_DOC = ROOT / "docs/rules/wpa-heyball-2025-08-16-profile.md"
HUD_TOKENS = ROOT / "assets/modern/hud/focused_practice_hud_shipping_tokens_v1.json"
SHELL_TOKENS = ROOT / "assets/modern/hud/focused_practice_shell_shipping_tokens_v1.json"

NOTICE_DATA = (
    ("illegal_break", "rules.chinese8.foul.illegal_break", "Break requirements not met",
     "开球条件未满足", "The break did not pocket an object ball or drive four object balls to rails.",
     "本次开球既未进目标球，也未让四颗目标球触库。",
     "Wait for the opponent's accept-table or rerack decision.", "等待对手选择接受台面或重新摆球。",
     "warning_triangle", "outlined_triangle", "conditional_on_other_break_foul", "decision_required",
     "conditional", "none", "none"),
    ("no_object_ball_contact", "rules.chinese8.foul.no_object_contact", "No object-ball contact",
     "未碰到目标球", "The cue ball did not contact any object ball.", "母球没有碰到任何目标球。",
     "Turn passes; the opponent receives ball in hand.", "回合交给对手；对手获得自由球。",
     "crossed_circle", "crossed_circle", "opponent", "passes", "opponent", "none", "none"),
    ("wrong_first_contact", "rules.chinese8.foul.wrong_first_contact", "Wrong ball contacted first",
     "首碰球错误", "The first contacted ball was not legal for the shooter's group.",
     "首先碰到的球不属于击球方当前可击球组。",
     "Turn passes; the opponent receives ball in hand.", "回合交给对手；对手获得自由球。",
     "crossed_circle", "numbered_ball_cross", "opponent", "passes", "opponent", "none", "none"),
    ("no_rail_after_contact", "rules.chinese8.foul.no_rail_after_contact", "No rail after contact",
     "碰球后无球触库", "After first contact, no ball was pocketed or reached a rail.",
     "首次碰球后，没有球入袋或触库。", "Turn passes; the opponent receives ball in hand.",
     "回合交给对手；对手获得自由球。", "rail_with_slash", "rail_with_slash", "opponent",
     "passes", "opponent", "none", "none"),
    ("cue_ball_scratch", "rules.chinese8.foul.cue_ball_scratch", "Cue-ball scratch", "母球落袋",
     "The cue ball was pocketed.", "母球已落袋。",
     "Turn passes; place the cue ball when ball-in-hand placement is available.",
     "回合交给对手；自由球放置功能可用后放置母球。", "cue_ball_down_arrow", "cue_ball_down_arrow",
     "opponent", "passes", "opponent", "none", "none"),
    ("cue_ball_off_table", "rules.chinese8.foul.cue_ball_off_table", "Cue ball off the table",
     "母球离台", "The cue ball left the playing surface.", "母球离开了台面。",
     "Turn passes; the opponent receives ball in hand.", "回合交给对手；对手获得自由球。",
     "ball_outside_rectangle", "ball_outside_rectangle", "opponent", "passes", "opponent", "none", "none"),
    ("object_ball_off_table", "rules.chinese8.foul.object_ball_off_table", "Object ball off the table",
     "目标球离台", "An object ball left the playing surface.", "一颗目标球离开了台面。",
     "Turn passes; the opponent receives ball in hand.", "回合交给对手；对手获得自由球。",
     "ball_outside_rectangle", "numbered_ball_outside_rectangle", "opponent", "passes", "opponent",
     "none", "none"),
    ("illegal_eight_ball", "rules.chinese8.loss.illegal_eight", "Illegal 8-ball",
     "8号球非法入袋", "The 8-ball was pocketed before the legal winning shot.",
     "8号球在合法制胜球之前入袋。", "The match ends; the opponent wins.", "本局结束；对手获胜。",
     "eight_ball_octagon", "eight_ball_octagon", "none", "match_complete", "none", "opponent", "shooter"),
)

PENDING_UI = (
    {"capability_id": "break_foul_opponent_choices", "machine_code": "ui.break_foul_decision",
     "text_key": "rules.chinese8.pending.break_foul_choices", "shape_token": "rules_shield",
     "title": {"en": "Break decision", "zh_cn": "开球判定选择"},
     "explanation": {"en": "Choose whether to accept the table or rerack.",
                     "zh_cn": "选择接受当前台面或重新摆球。"},
     "actions": [{"id": "accept_table", "en": "Accept table", "zh_cn": "接受台面"},
                 {"id": "rerack_opponent_breaks", "en": "Rerack — opponent breaks", "zh_cn": "重摆—对手开球"},
                 {"id": "rerack_original_breaker", "en": "Rerack — original breaker", "zh_cn": "重摆—原开球方开球"}]},
    {"capability_id": "eight_on_break_spot_or_rerack", "machine_code": "ui.eight_on_break_decision",
     "text_key": "rules.chinese8.pending.eight_on_break", "shape_token": "reset_arrow",
     "title": {"en": "8-ball on the break", "zh_cn": "开球时8号球入袋"},
     "explanation": {"en": "The 8-ball must be spotted or the rack reset; this is not a win.",
                     "zh_cn": "8号球必须重置点位或重新摆球；这不算获胜。"},
     "actions": [{"id": "spot_eight_continue", "en": "Spot 8-ball and continue", "zh_cn": "重置8号球并继续"},
                 {"id": "spot_eight_ball_in_hand_head_string", "en": "Spot 8-ball; ball in hand behind head string", "zh_cn": "重置8号球；开球线后自由球"},
                 {"id": "rerack", "en": "Rerack", "zh_cn": "重新摆球"}]},
    {"capability_id": "stalemate_decision", "machine_code": "ui.stalemate_decision",
     "text_key": "rules.chinese8.pending.stalemate", "shape_token": "warning_triangle",
     "title": {"en": "Stalemate", "zh_cn": "僵局"},
     "explanation": {"en": "Play cannot progress under the approved procedure.",
                     "zh_cn": "按既定程序已无法继续推进比赛。"},
     "actions": [{"id": "confirm_stalemate", "en": "Confirm and follow rerack procedure", "zh_cn": "确认并执行重摆程序"}]},
    {"capability_id": "free_ball_placement", "machine_code": "ui.ball_in_hand_placement",
     "text_key": "rules.chinese8.pending.ball_in_hand_placement", "shape_token": "cue_ball",
     "title": {"en": "Ball in hand", "zh_cn": "自由球"},
     "explanation": {"en": "Place the cue ball only inside the currently legal area.",
                     "zh_cn": "只能在当前规则允许的区域内放置母球。"},
     "actions": [{"id": "place_cue_ball", "en": "Place cue ball", "zh_cn": "放置母球"},
                 {"id": "confirm_placement", "en": "Confirm placement", "zh_cn": "确认位置"}]},
)


def _sha(path: Path) -> str:
    return "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest()


def _source_audit():
    rules = RULES_CPP.read_text(encoding="utf-8")
    actual_codes = tuple(re.findall(r'case FoulReason::(?!None)[A-Za-z]+: return "([a-z_]+)";', rules))
    expected_codes = tuple(item[0] for item in NOTICE_DATA)
    if actual_codes != expected_codes:
        raise ValueError("actual ChineseEightBallRules reason codes changed: " + str(actual_codes))
    hud = HUD_CPP.read_text(encoding="utf-8")
    hud_tokens = json.loads(HUD_TOKENS.read_text(encoding="utf-8"))
    shell_tokens = json.loads(SHELL_TOKENS.read_text(encoding="utf-8"))
    for row in NOTICE_DATA:
        code, text_key, canonical, alias = row[0], row[1], row[8], row[9]
        if f'case FoulReason::' not in rules or text_key not in rules or code not in hud or alias not in hud:
            raise ValueError("rules/HUD mapping changed: " + code)
        if canonical not in hud_tokens["icon_tokens"]:
            raise ValueError("HUD canonical shape token missing: " + canonical)
        if alias != canonical and hud_tokens["aliases"].get(alias) != canonical:
            raise ValueError("HUD runtime alias is not an existing reviewed alias: " + alias)
    for item in PENDING_UI:
        token = item["shape_token"]
        if token not in hud_tokens["icon_tokens"] and token not in shell_tokens["icon_tokens"]:
            raise ValueError("pending UI token not present in approved HUD/Shell sets: " + token)
    return {
        "rules_cpp_sha256": _sha(RULES_CPP), "product_hud_cpp_sha256": _sha(HUD_CPP),
        "rules_profile_doc_sha256": _sha(PROFILE_DOC), "hud_token_sha256": _sha(HUD_TOKENS),
        "shell_token_sha256": _sha(SHELL_TOKENS),
    }


def _notice(row):
    (code, text_key, en_title, zh_title, en_explanation, zh_explanation,
     en_action, zh_action, shape, alias, ball_in_hand, turn, handoff, win, loss) = row
    return {
        "reason_code": code, "text_key": text_key, "capability": "implemented",
        "product_exposed": True, "surface": "hud_foul_banner",
        "shape_token": shape, "runtime_shape_alias": alias,
        "redundant_channels": ["shape", "title_text", "explanation_text", "next_action_text"],
        "copy": {"en": {"title": en_title, "explanation": en_explanation, "next_action": en_action},
                 "zh_cn": {"title": zh_title, "explanation": zh_explanation, "next_action": zh_action}},
        "semantic_effect": {"ball_in_hand": ball_in_hand, "turn": turn, "handoff": handoff,
                            "win": win, "loss": loss},
    }


def build_manifest():
    pending = []
    for item in PENDING_UI:
        value = dict(item)
        value.update({"capability": "pending", "product_exposed": False,
                      "surface": "modal_when_implemented",
                      "redundant_channels": ["shape", "title_text", "explanation_text", "action_text"]})
        pending.append(value)
    return {
        "schema": "phase5_rules_shipping_copy_v1", "schema_version": 1,
        "package_id": "wpa_heyball_rules_feedback_zh_en_v1",
        "ruleset_id": "wpa_heyball_2025_08_16_v1", "profile_complete": False,
        "source_audit": _source_audit(),
        "evidence_boundary": {"flat_proof_is_gpu_capture": False, "changes_rule_semantics": False,
                              "changes_runtime": False, "pending_capabilities_may_be_product_exposed": False,
                              "unreviewed_shape_aliases_allowed": False},
        "presentation_policy": {
            "priority_order": ["match_result_modal", "decision_modal", "foul_banner",
                               "ball_in_hand_banner", "turn_handoff_toast", "informational_toast"],
            "duration_ms": {"match_result_modal": "until_acknowledged",
                            "decision_modal": "until_valid_choice", "foul_banner": 6000,
                            "ball_in_hand_banner": 5000, "turn_handoff_toast": 3000,
                            "informational_toast": 2500},
            "focus": {"banner_steals_focus": False, "modal_traps_focus": True,
                      "initial_modal_focus": "first_enabled_action", "visible_focus_shape_required": True,
                      "keyboard": ["tab", "shift_tab", "arrow_keys", "enter", "space"]},
            "screen_reader": {"foul_and_result_live_region": "assertive",
                              "turn_and_information_live_region": "polite",
                              "announce_order": ["title", "explanation", "next_action", "focused_action"]},
            "accessibility": {"never_color_only": True, "shape_plus_text_required": True,
                              "cvd_palettes": ["standard", "color_vision_safe"],
                              "essential_rules_text_always_visible": True,
                              "subtitle_setting_may_hide_rules_text": False},
        },
        "outcome_copy": {
            "ball_in_hand": {"shape_token": "cue_ball_down_arrow",
                             "en": "Opponent has ball in hand.", "zh_cn": "对手获得自由球。"},
            "turn_continues": {"shape_token": "selected_check_diamond",
                               "en": "Turn continues.", "zh_cn": "继续当前回合。"},
            "turn_handoff": {"shape_token": "focus_chevron",
                             "en": "Turn passes to the opponent.", "zh_cn": "回合交给对手。"},
            "win": {"shape_token": "selected_check_diamond", "en": "You win.", "zh_cn": "你获胜。"},
            "loss": {"shape_token": "error_cross_octagon", "en": "You lose.", "zh_cn": "你失利。"},
        },
        "reason_notices": [_notice(row) for row in NOTICE_DATA],
        "pending_ui_capabilities": pending,
        "flat_proof": {"path": "docs/art/" + PROOF_NAME,
                       "evidence_class": "flat_text_layout_proof_not_gpu"},
    }


def build_schema():
    return {"$schema": "https://json-schema.org/draft/2020-12/schema",
            "$id": "billiardgl://schemas/phase5-rules-shipping-copy-v1",
            "title": "Phase 5 bilingual rules-feedback shipping copy", "type": "object",
            "additionalProperties": False,
            "required": ["schema", "schema_version", "package_id", "ruleset_id", "profile_complete",
                         "source_audit", "evidence_boundary", "presentation_policy", "outcome_copy",
                         "reason_notices", "pending_ui_capabilities", "flat_proof"],
            "properties": {"schema": {"const": "phase5_rules_shipping_copy_v1"},
                           "schema_version": {"const": 1}, "package_id": {"type": "string"},
                           "ruleset_id": {"const": "wpa_heyball_2025_08_16_v1"},
                           "profile_complete": {"const": False}, "source_audit": {"type": "object"},
                           "evidence_boundary": {"type": "object"}, "presentation_policy": {"type": "object"},
                           "outcome_copy": {"type": "object"},
                           "reason_notices": {"type": "array", "minItems": 8, "maxItems": 8},
                           "pending_ui_capabilities": {"type": "array", "minItems": 4},
                           "flat_proof": {"type": "object"}}}


def build_flat_proof(manifest):
    panels = []
    y = 108
    for notice in manifest["reason_notices"]:
        copy = notice["copy"]
        panels.append(f'<rect x="48" y="{y}" width="912" height="106" rx="12" fill="#142229" stroke="#d7b35a"/>')
        panels.append(f'<text x="76" y="{y+30}" class="code">{html.escape(notice["reason_code"])} · {html.escape(notice["shape_token"])}</text>')
        panels.append(f'<text x="76" y="{y+58}" class="title">{html.escape(copy["zh_cn"]["title"])} / {html.escape(copy["en"]["title"])}</text>')
        panels.append(f'<text x="76" y="{y+84}" class="body">{html.escape(copy["zh_cn"]["next_action"])} / {html.escape(copy["en"]["next_action"])}</text>')
        y += 118
    y = 108
    for item in manifest["pending_ui_capabilities"]:
        panels.append(f'<rect x="992" y="{y}" width="880" height="188" rx="12" fill="#251d1d" stroke="#de7468"/>')
        panels.append(f'<text x="1020" y="{y+32}" class="pending">PENDING · NOT FOR PRODUCT · {html.escape(item["shape_token"])}</text>')
        panels.append(f'<text x="1020" y="{y+64}" class="title">{html.escape(item["title"]["zh_cn"])} / {html.escape(item["title"]["en"])}</text>')
        panels.append(f'<text x="1020" y="{y+94}" class="body">{html.escape(item["explanation"]["zh_cn"])}</text>')
        for action_index, action in enumerate(item["actions"][:3]):
            panels.append(f'<text x="1040" y="{y+124+action_index*22}" class="action">[{action_index+1}] {html.escape(action["zh_cn"])} / {html.escape(action["en"])}</text>')
        y += 204
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="1920" height="1080" viewBox="0 0 1920 1080">
<rect width="1920" height="1080" fill="#0b1115"/>
<style>.heading{{font:700 28px sans-serif;fill:#f3f6f7}}.code{{font:600 15px monospace;fill:#d7b35a}}.title{{font:700 20px sans-serif;fill:#fff}}.body{{font:15px sans-serif;fill:#d9e2e5}}.action{{font:14px sans-serif;fill:#f0d8d4}}.pending{{font:700 14px monospace;fill:#ff8f82}}</style>
<text x="48" y="48" class="heading">WPA Heyball rules feedback · 中英 shipping copy</text>
<text x="48" y="80" class="code">FLAT TEXT/LAYOUT PROOF · NOT GPU · PROFILE INCOMPLETE</text>
{''.join(panels)}
</svg>\n'''


def write_package(output_dir: Path):
    output_dir.mkdir(parents=True, exist_ok=True)
    manifest = build_manifest()
    (output_dir / MANIFEST_NAME).write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    (output_dir / SCHEMA_NAME).write_text(json.dumps(build_schema(), indent=2, sort_keys=True) + "\n", encoding="utf-8")
    (output_dir / PROOF_NAME).write_text(build_flat_proof(manifest), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", type=Path, default=ROOT / "docs/art")
    args = parser.parse_args()
    write_package(args.output_dir)
    print("Generated Phase5 rules copy: implemented_reason_codes=8 pending_ui=4 proof=FLAT_NOT_GPU")


if __name__ == "__main__":
    main()
