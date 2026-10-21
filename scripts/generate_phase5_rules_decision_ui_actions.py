#!/usr/bin/env python3
"""Generate the pending Phase-5 rules decision UI/action implementation contract."""

from __future__ import annotations

import argparse
import hashlib
import html
import json
from pathlib import Path
import textwrap


ROOT = Path(__file__).resolve().parents[1]
MANIFEST_NAME = "phase5-rules-decision-ui-actions-v1.pending.json"
SCHEMA_NAME = "phase5-rules-decision-ui-actions-v1.schema.json"
PROOF_NAME = "phase5-rules-decision-ui-actions-flat-proof-v1.svg"

FROZEN_COPY = ROOT / "docs/art/phase5-rules-shipping-copy-v1.json"
FROZEN_SCHEMA = ROOT / "docs/art/phase5-rules-shipping-copy-v1.schema.json"
FROZEN_PROOF = ROOT / "docs/art/phase5-rules-shipping-copy-flat-proof-v1.svg"
OFFICIAL_AUDIT = ROOT / "docs/rules/phase5-wpa-heyball-completeness-audit.json"
HUD_TOKENS = ROOT / "assets/modern/hud/focused_practice_hud_shipping_tokens_v1.json"
SHELL_TOKENS = ROOT / "assets/modern/hud/focused_practice_shell_shipping_tokens_v1.json"

CAPABILITY_IDS = (
    "break_foul_opponent_choices",
    "eight_on_break_spot_or_rerack",
    "free_ball_placement",
    "simultaneous_contact_and_group_choice",
    "ball_respot_and_rerack_executor",
    "stalemate_decision",
)

FLOW_IDS = (
    "break_foul_opponent_choice",
    "eight_on_break_spot_or_rerack",
    "ball_in_hand_full_table",
    "ball_in_hand_behind_head_string",
    "simultaneous_group_choice",
    "respot_rerack_progress_error",
    "stalemate_proposal",
)

SOURCE_ROWS = {
    "break_foul_opponent_choices": ["WPA-II-06-E"],
    "eight_on_break_spot_or_rerack": ["WPA-II-06-F"],
    "free_ball_placement": ["WPA-II-06-D", "WPA-II-08", "WPA-II-09", "WPA-II-17"],
    "simultaneous_contact_and_group_choice": ["WPA-II-10-4", "WPA-II-13"],
    "ball_respot_and_rerack_executor": ["WPA-II-04", "WPA-II-06-E", "WPA-II-06-F", "WPA-II-07", "WPA-II-08", "WPA-II-21"],
    "stalemate_decision": ["WPA-II-21"],
}


def _sha(path: Path) -> str:
    return "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest()


def _proof_lines(value: str, width: int, limit: int, *, cjk: bool = False) -> list[str]:
    if cjk:
        lines = [value[index:index + width] for index in range(0, len(value), width)]
    else:
        lines = textwrap.wrap(value, width=width, break_long_words=False,
                              break_on_hyphens=False)
    if len(lines) > limit:
        lines = lines[:limit]
        lines[-1] = lines[-1].rstrip("。.,;: ") + "…"
    return lines + [""] * (limit - len(lines))


def localized(en: str, zh: str) -> dict:
    return {"en": en, "zh_cn": zh}


def action(action_id: str, label_en: str, label_zh: str, from_state: str,
           target_state: str, command: str, *, legacy_id: str | None = None,
           authority_mutation: bool = True, requires_ack: bool = True,
           enabled_when: list[str] | None = None) -> dict:
    value = {
        "action_id": action_id,
        "label": localized(label_en, label_zh),
        "allowed_from_state": from_state,
        "target_state_before_ack": target_state,
        "authority_command": command,
        "mutates_authoritative_state": authority_mutation,
        "requires_authority_ack": requires_ack,
        "optimistic_authoritative_mutation": False,
        "enabled_when": enabled_when or ["action_id is present in the authority offer"],
    }
    if legacy_id:
        value["frozen_copy_action_id"] = legacy_id
    return value


def state(flow_id: str, state_id: str, surface: str, *, blocking: bool,
          authority_mutation_allowed: bool = False) -> dict:
    return {
        "state_id": f"{flow_id}.{state_id}",
        "surface": surface,
        "blocking": blocking,
        "authority_mutation_allowed": authority_mutation_allowed,
    }


def decision_states(flow_id: str, open_surface: str = "decision_modal") -> list[dict]:
    return [
        state(flow_id, "awaiting_authority_offer", "hidden", blocking=True),
        state(flow_id, "decision_open", open_surface, blocking=True),
        state(flow_id, "submitting_authority_action", "blocking_progress", blocking=True),
        state(flow_id, "authority_error", "blocking_error", blocking=True),
        state(flow_id, "completed_authoritative", "hidden", blocking=False,
              authority_mutation_allowed=True),
    ]


def flow(flow_id: str, capability_id: str, machine_code: str, text_key: str,
         shape_tokens: list[str], title: dict, body: dict, actions: list[dict],
         preconditions: list[str], postconditions: list[str], *,
         open_surface: str = "decision_modal", policy_dependencies: list[str] | None = None,
         frozen_machine_code: str | None = None, frozen_text_key: str | None = None) -> dict:
    return {
        "flow_id": flow_id,
        "capability_id": capability_id,
        "capability_state": "pending",
        "product_exposed": False,
        "machine_code": machine_code,
        "text_key": text_key,
        "frozen_copy_machine_code": frozen_machine_code,
        "frozen_copy_text_key": frozen_text_key,
        "source_rows": SOURCE_ROWS[capability_id],
        "shape_tokens": shape_tokens,
        "copy": {"title": title, "body": body},
        "preconditions": preconditions,
        "postconditions": postconditions,
        "states": decision_states(flow_id, open_surface),
        "initial_state": f"{flow_id}.awaiting_authority_offer",
        "completed_state": f"{flow_id}.completed_authoritative",
        "actions": actions,
        "policy_dependencies": policy_dependencies or [],
        "failure_policy": {
            "default_action_on_timeout": None,
            "dismiss_on_escape": False,
            "advance_turn_without_ack": False,
            "clear_entitlement_without_ack": False,
            "retry_reuses_idempotency_key": True,
            "resume_by": ["decision_id", "authority_state_revision"],
        },
    }


def build_flows() -> list[dict]:
    common_pre = [
        "balls are stationary",
        "authority offer contains decision_id and authority_state_revision",
        "local player identity matches authority actor",
        "capability has been promoted by a future implementation gate",
    ]
    break_id = "break_foul_opponent_choice"
    break_open = f"{break_id}.decision_open"
    break_submit = f"{break_id}.submitting_authority_action"
    break_actions = [
        action("rules.break_foul.accept_table", "Accept table", "接受台面", break_open,
               break_submit, "rules.submit_break_foul_choice", legacy_id="accept_table"),
        action("rules.break_foul.rerack_opponent_breaks", "Rerack - I break", "重摆 - 由我开球",
               break_open, break_submit, "rules.submit_break_foul_choice",
               legacy_id="rerack_opponent_breaks"),
        action("rules.break_foul.rerack_original_breaker", "Rerack - original breaker", "重摆 - 原开球方开球",
               break_open, break_submit, "rules.submit_break_foul_choice",
               legacy_id="rerack_original_breaker"),
    ]
    break_flow = flow(
        break_id, "break_foul_opponent_choices", "ui.break_foul_decision.v1",
        "rules.chinese8.pending.break_foul_choices", ["rules_shield"],
        localized("Break decision", "开球犯规选择"),
        localized("Choose the authority-offered result. The rack and turn do not change until confirmed.",
                  "请选择裁判系统提供的处理方式；确认前球局与回合均不改变。"),
        break_actions, common_pre + ["the authority classified the break as a foul and offers this opponent a choice"],
        ["accepted choice is recorded with decision_id", "authority returns next player, rack state, ball-in-hand scope, and new revision"],
        frozen_machine_code="ui.break_foul_decision",
        frozen_text_key="rules.chinese8.pending.break_foul_choices")

    eight_id = "eight_on_break_spot_or_rerack"
    eight_open = f"{eight_id}.decision_open"
    eight_submit = f"{eight_id}.submitting_authority_action"
    eight_actions = [
        action("rules.eight_on_break.spot_continue", "Spot 8-ball and continue", "重置8号球并继续",
               eight_open, eight_submit, "rules.submit_eight_on_break_choice",
               legacy_id="spot_eight_continue"),
        action("rules.eight_on_break.spot_ball_in_hand_head_string",
               "Spot 8-ball; ball in hand behind head string", "重置8号球；开球线后自由球",
               eight_open, eight_submit, "rules.submit_eight_on_break_choice",
               legacy_id="spot_eight_ball_in_hand_head_string"),
        action("rules.eight_on_break.rerack_original_breaker", "Rerack - original breaker", "重摆 - 原开球方开球",
               eight_open, eight_submit, "rules.submit_eight_on_break_choice", legacy_id="rerack"),
        action("rules.eight_on_break.rerack_opponent_breaks", "Rerack - opponent breaks", "重摆 - 对手开球",
               eight_open, eight_submit, "rules.submit_eight_on_break_choice", legacy_id="rerack"),
    ]
    eight_flow = flow(
        eight_id, "eight_on_break_spot_or_rerack", "ui.eight_on_break_decision.v1",
        "rules.chinese8.pending.eight_on_break", ["reset_arrow", "rules_shield"],
        localized("8-ball on the break", "开球时8号球入袋"),
        localized("This is not a win. Only actions offered for this exact foul/no-foul branch are enabled.",
                  "这不算获胜；只启用裁判系统为当前有犯规或无犯规分支提供的操作。"),
        eight_actions, common_pre + ["authority confirms the 8-ball was pocketed on the break", "offer identifies foul or no-foul branch"],
        ["8-ball is spotted or rack is rebuilt by one atomic authority transaction", "next player and ball-in-hand scope come only from ACK"],
        frozen_machine_code="ui.eight_on_break_decision",
        frozen_text_key="rules.chinese8.pending.eight_on_break")

    full_id = "ball_in_hand_full_table"
    full_open = f"{full_id}.decision_open"
    full_submit = f"{full_id}.submitting_authority_action"
    full_actions = [
        action("rules.ball_in_hand.preview_position", "Move cue-ball preview", "移动母球预览",
               full_open, full_open, "ui.local_preview_only", legacy_id="place_cue_ball",
               authority_mutation=False, requires_ack=False,
               enabled_when=["pointer or keyboard position is finite", "preview remains inside full playing surface"]),
        action("rules.ball_in_hand.confirm_full_table_placement", "Confirm placement", "确认放置",
               full_open, full_submit, "rules.confirm_ball_in_hand_placement",
               legacy_id="confirm_placement",
               enabled_when=["authority validation preview says legal", "cue ball does not overlap any ball or forbidden geometry"]),
        action("rules.ball_in_hand.reset_preview", "Reset preview", "重置预览",
               full_open, full_open, "ui.reset_local_preview", authority_mutation=False,
               requires_ack=False),
    ]
    full_flow = flow(
        full_id, "free_ball_placement", "ui.ball_in_hand_placement.full_table.v1",
        "rules.chinese8.pending.ball_in_hand_placement.full_table", ["cue_ball", "focus_brackets"],
        localized("Ball in hand - full table", "自由球 - 全台"),
        localized("Move the preview anywhere inside the legal playing surface, then confirm.",
                  "在合法台面范围内移动母球预览，然后确认。"),
        full_actions, common_pre + ["authority grant scope is full_table", "current player owns ball-in-hand entitlement"],
        ["ACK returns exact cue-ball position and clears entitlement", "shot input remains blocked until ACK"],
        open_surface="ball_in_hand_placement_overlay",
        frozen_machine_code="ui.ball_in_hand_placement",
        frozen_text_key="rules.chinese8.pending.ball_in_hand_placement")

    head_id = "ball_in_hand_behind_head_string"
    head_open = f"{head_id}.decision_open"
    head_submit = f"{head_id}.submitting_authority_action"
    head_actions = [
        action("rules.ball_in_hand.preview_position", "Move cue-ball preview", "移动母球预览",
               head_open, head_open, "ui.local_preview_only", legacy_id="place_cue_ball",
               authority_mutation=False, requires_ack=False,
               enabled_when=["cue-ball center remains behind the authority head-string plane"]),
        action("rules.ball_in_hand.confirm_behind_head_string", "Confirm behind head string", "确认放置在开球线后",
               head_open, head_submit, "rules.confirm_ball_in_hand_placement",
               legacy_id="confirm_placement",
               enabled_when=["authority validation preview says legal", "cue-ball center is behind head string", "no overlap"]),
        action("rules.ball_in_hand.request_nearest_legal_respot", "Request legal ball re-spot", "请求重置最近合法目标球",
               head_open, head_submit, "rules.request_head_string_object_respot",
               enabled_when=["authority says every legal object ball is behind the head string"]),
        action("rules.ball_in_hand.choose_equidistant_respot_ball", "Choose equidistant ball", "选择等距目标球",
               head_open, head_submit, "rules.submit_equidistant_respot_choice",
               enabled_when=["authority offer contains two or more equidistant legal object-ball IDs"]),
        action("rules.ball_in_hand.reset_preview", "Reset preview", "重置预览",
               head_open, head_open, "ui.reset_local_preview", authority_mutation=False,
               requires_ack=False),
    ]
    head_flow = flow(
        head_id, "free_ball_placement", "ui.ball_in_hand_placement.behind_head_string.v1",
        "rules.chinese8.pending.ball_in_hand_placement.behind_head_string", ["cue_ball", "distance_arrow", "focus_brackets"],
        localized("Ball in hand - behind head string", "自由球 - 开球线后"),
        localized("Keep the cue-ball center behind the line. The authority validates placement and any legal-ball re-spot.",
                  "母球球心必须位于开球线后；放置及合法目标球重置均由裁判系统验证。"),
        head_actions, common_pre + ["authority grant scope is behind_head_string", "authority provides head-string plane and legal object-ball set"],
        ["ACK returns exact cue-ball and optional re-spotted object-ball positions", "first-shot restrictions remain authority-owned"],
        open_surface="ball_in_hand_placement_overlay",
        frozen_machine_code="ui.ball_in_hand_placement",
        frozen_text_key="rules.chinese8.pending.ball_in_hand_placement")

    group_id = "simultaneous_group_choice"
    group_open = f"{group_id}.decision_open"
    group_submit = f"{group_id}.submitting_authority_action"
    group_actions = [
        action("rules.group_choice.choose_solids", "Choose solids", "选择全色球",
               group_open, group_submit, "rules.submit_simultaneous_group_choice"),
        action("rules.group_choice.choose_stripes", "Choose stripes", "选择花色球",
               group_open, group_submit, "rules.submit_simultaneous_group_choice"),
    ]
    group_flow = flow(
        group_id, "simultaneous_contact_and_group_choice", "ui.simultaneous_group_choice.v1",
        "rules.chinese8.pending.simultaneous_group_choice", ["group_solid_filled_circle", "group_stripe_banded_ring", "group_open_diamond"],
        localized("Choose your group", "选择球组"),
        localized("Both groups qualified on the same legal shot. Choose one; the other group is assigned by the authority.",
                  "同一合法击球使两组均满足条件；请选择一组，另一组由裁判系统分配。"),
        group_actions, common_pre + ["table is open", "authority confirms simultaneous legal contact and qualifying pockets for both groups"],
        ["ACK atomically assigns both players' groups", "turn continuation and next target come from ACK"])

    progress_id = "respot_rerack_progress_error"
    progress_states = [
        state(progress_id, "awaiting_authority_transaction", "blocking_progress", blocking=True),
        state(progress_id, "respot_in_progress", "blocking_progress", blocking=True),
        state(progress_id, "rerack_in_progress", "blocking_progress", blocking=True),
        state(progress_id, "verifying_authoritative_state", "blocking_progress", blocking=True),
        state(progress_id, "execution_error", "blocking_error", blocking=True),
        state(progress_id, "completed_authoritative", "hidden", blocking=False,
              authority_mutation_allowed=True),
    ]
    progress_actions = [
        action("rules.execution.retry_status", "Retry status check", "重试状态检查",
               f"{progress_id}.execution_error", f"{progress_id}.awaiting_authority_transaction",
               "rules.query_idempotent_transaction", authority_mutation=False, requires_ack=True,
               enabled_when=["error is retryable", "transaction_id is unchanged"]),
        action("rules.execution.return_to_decision", "Return to decision", "返回规则选择",
               f"{progress_id}.execution_error", f"{progress_id}.awaiting_authority_transaction",
               "rules.request_decision_recovery", authority_mutation=False, requires_ack=True,
               enabled_when=["authority reports transaction not applied", "original decision remains open"]),
    ]
    progress_flow = {
        "flow_id": progress_id,
        "capability_id": "ball_respot_and_rerack_executor",
        "capability_state": "pending", "product_exposed": False,
        "machine_code": "ui.rules_execution_progress.v1",
        "text_key": "rules.chinese8.pending.execution_progress",
        "frozen_copy_machine_code": None, "frozen_copy_text_key": None,
        "source_rows": SOURCE_ROWS["ball_respot_and_rerack_executor"],
        "shape_tokens": ["reset_arrow", "error_cross_octagon"],
        "copy": {
            "title": localized("Updating the table", "正在更新球台"),
            "body": localized("Wait for the authoritative re-spot or rerack transaction. Do not shoot or leave the screen.",
                              "请等待权威重置或重摆事务完成；期间不能击球或离开此界面。"),
        },
        "preconditions": ["a choice ACK contains transaction_id", "balls are stationary", "all shot input is blocked"],
        "postconditions": ["position hash, rack ownership, turn, and authority revision are verified", "failure never applies a local fallback layout"],
        "states": progress_states,
        "initial_state": f"{progress_id}.awaiting_authority_transaction",
        "completed_state": f"{progress_id}.completed_authoritative",
        "actions": progress_actions,
        "policy_dependencies": [],
        "failure_policy": {
            "default_action_on_timeout": None, "dismiss_on_escape": False,
            "advance_turn_without_ack": False, "clear_entitlement_without_ack": False,
            "retry_reuses_idempotency_key": True,
            "resume_by": ["transaction_id", "authority_state_revision"],
        },
    }

    stale_id = "stalemate_proposal"
    stale_open = f"{stale_id}.decision_open"
    stale_submit = f"{stale_id}.submitting_authority_action"
    stale_actions = [
        action("rules.stalemate.accept_rerack", "Accept rerack", "接受重摆",
               stale_open, stale_submit, "rules.submit_stalemate_response",
               legacy_id="confirm_stalemate"),
        action("rules.stalemate.refuse_continue", "Continue play", "继续比赛",
               stale_open, stale_submit, "rules.submit_stalemate_response",
               enabled_when=["authority policy version permits player refusal", "continuation window is authority-defined"]),
    ]
    stale_flow = flow(
        stale_id, "stalemate_decision", "ui.stalemate_decision.v1",
        "rules.chinese8.pending.stalemate", ["warning_triangle", "reset_arrow"],
        localized("Stalemate proposal", "僵局处理建议"),
        localized("Respond to the authority proposal. No shot count, automatic rerack, or final declaration is assumed locally.",
                  "请回应裁判系统的处理建议；本地不预设追加击球次数、自动重摆或最终宣告。"),
        stale_actions, common_pre + ["authority/referee has issued a stalemate proposal", "offer includes original breaker and shooting-order snapshot", "signed policy version is present"],
        ["accept routes to an order-preserving rerack transaction", "refusal continuation window and any final declaration remain authority-owned"],
        policy_dependencies=["stalemate_declaration_authority_policy", "stalemate_continuation_window_policy"],
        frozen_machine_code="ui.stalemate_decision",
        frozen_text_key="rules.chinese8.pending.stalemate")

    return [break_flow, eight_flow, full_flow, head_flow, group_flow,
            progress_flow, stale_flow]


def build_manifest() -> dict:
    flows = build_flows()
    capabilities = []
    for capability_id in CAPABILITY_IDS:
        capabilities.append({
            "capability_id": capability_id,
            "state": "pending",
            "product_exposed": False,
            "source_rows": SOURCE_ROWS[capability_id],
            "flow_ids": [item["flow_id"] for item in flows
                         if item["capability_id"] == capability_id],
        })
    return {
        "schema": "phase5_rules_decision_ui_actions_v1",
        "schema_version": 1,
        "package_id": "phase5_wpa_heyball_rules_decision_ui_actions_v1",
        "ruleset_id": "wpa_heyball_2025_08_16_v1",
        "status": "PENDING",
        "profile_complete": False,
        "product_exposed": False,
        "source_bindings": {
            "frozen_rules_copy": {"path": "docs/art/phase5-rules-shipping-copy-v1.json", "sha256": _sha(FROZEN_COPY), "read_only": True},
            "frozen_rules_copy_schema": {"path": "docs/art/phase5-rules-shipping-copy-v1.schema.json", "sha256": _sha(FROZEN_SCHEMA), "read_only": True},
            "frozen_rules_copy_proof": {"path": "docs/art/phase5-rules-shipping-copy-flat-proof-v1.svg", "sha256": _sha(FROZEN_PROOF), "read_only": True},
            "official_completeness_audit": {"path": "docs/rules/phase5-wpa-heyball-completeness-audit.json", "sha256": _sha(OFFICIAL_AUDIT), "read_only": True},
            "hud_tokens": {"path": "assets/modern/hud/focused_practice_hud_shipping_tokens_v1.json", "sha256": _sha(HUD_TOKENS), "read_only": True},
            "shell_tokens": {"path": "assets/modern/hud/focused_practice_shell_shipping_tokens_v1.json", "sha256": _sha(SHELL_TOKENS), "read_only": True},
        },
        "evidence_boundary": {
            "changes_cpp": False,
            "changes_rule_semantics": False,
            "changes_frozen_rules_copy": False,
            "flat_proof_is_gpu_capture": False,
            "flat_proof_is_runtime_evidence": False,
            "capability_promotion_allowed": False,
            "official_off_table_conflict_resolved": False,
            "policy_placeholders_are_decisions": False,
        },
        "interaction_policy": {
            "hierarchy_high_to_low": [
                "match_result_modal", "blocking_error", "blocking_progress",
                "rules_decision_modal", "ball_in_hand_placement_overlay",
                "foul_banner", "ball_in_hand_banner", "turn_handoff_toast",
                "informational_toast",
            ],
            "hud_to_modal": {
                "reason_banner_may_announce_before_modal": True,
                "modal_keeps_banner_summary_visible_but_unfocused": True,
                "modal_never_uses_hud_banner_as_authority": True,
                "shot_input_blocked_while_any_flow_blocking": True,
            },
            "focus": {
                "modal_traps_focus": True,
                "initial_focus": "first authority-enabled action",
                "restore_focus": "gameplay_root_or_previous_rules_control",
                "visible_focus_shape_token": "focus_chevron",
                "selected_shape_token": "selected_check_diamond",
                "error_shape_token": "error_cross_octagon",
            },
            "input": {
                "keyboard": ["tab", "shift_tab", "left", "right", "up", "down", "enter", "space", "escape"],
                "pointer": ["hover", "primary_click", "drag_for_placement"],
                "gamepad_required_for_this_slice": False,
                "escape_on_required_decision": "announce_decision_required_and_keep_open",
                "escape_on_placement": "clear_local_preview_only_and_keep_entitlement",
                "escape_during_transaction": "ignored_with_progress_announcement",
            },
            "timeout_and_resume": {
                "auto_select_action": False,
                "auto_confirm_placement": False,
                "authority_ack_timeout_ms": 10000,
                "on_ack_timeout": "enter_authority_error_without_local_mutation",
                "resume_requires_exact_decision_or_transaction_id": True,
                "stale_revision_reopens_from_latest_authority_offer": True,
            },
        },
        "authority_contract": {
            "request_required": ["schema_version", "ruleset_id", "capability_id", "flow_id", "decision_id", "action_id", "authority_state_revision", "idempotency_key", "actor_player_id", "payload"],
            "ack_required": ["accepted", "decision_id", "action_id", "idempotency_key", "previous_state_revision", "authoritative_state_revision", "effects", "authoritative_state_hash"],
            "error_codes": [
                "rules_decision_capability_pending", "rules_decision_invalid_state",
                "rules_decision_action_not_offered", "rules_decision_stale_revision",
                "rules_decision_authority_unavailable", "rules_decision_timeout",
                "rules_placement_out_of_bounds", "rules_placement_overlap",
                "rules_placement_wrong_scope", "rules_respot_no_legal_position",
                "rules_rerack_transaction_failed", "rules_policy_unresolved",
            ],
            "effects_must_include_when_changed": ["current_player", "turn_continues", "ball_in_hand_scope", "groups", "ball_positions", "rack_generation", "original_breaker", "shooting_order"],
            "client_rules": {
                "local_preview_never_changes_authority": True,
                "no_state_commit_before_ack": True,
                "duplicate_idempotency_key_returns_same_result": True,
                "unknown_effect_field_fails_closed": True,
                "partial_transaction_effects_rejected": True,
            },
        },
        "accessibility": {
            "never_color_only": True,
            "shape_plus_title_plus_body_plus_action": True,
            "palettes": ["standard", "color_vision_safe"],
            "essential_rules_text_ignores_subtitle_visibility": True,
            "screen_reader": {
                "decision_and_error_live_region": "assertive",
                "progress_live_region": "polite",
                "announce_order": ["title", "body", "authority_status", "focused_action", "position_or_progress_detail"],
                "placement_coordinates_have_human_description": True,
            },
            "motion": {
                "reduced_motion_disables_nonessential_animation": True,
                "progress_remains_text_and_shape_redundant": True,
            },
        },
        "policy_decision_placeholders": [
            {
                "policy_id": "stalemate_declaration_authority_policy",
                "status": "UNRESOLVED",
                "product_exposed": False,
                "question": "Which signed authority may issue the initial and final stalemate declarations in each supported product mode?",
                "assumed_answer": None,
                "blocks_capabilities": ["stalemate_decision"],
            },
            {
                "policy_id": "stalemate_continuation_window_policy",
                "status": "UNRESOLVED",
                "product_exposed": False,
                "question": "How is the official referee-discretion continuation window represented and versioned for the supported product mode?",
                "assumed_answer": None,
                "blocks_capabilities": ["stalemate_decision"],
            },
            {
                "policy_id": "off_table_12_3_vs_17_e_interpretation",
                "status": "UNRESOLVED",
                "product_exposed": False,
                "question": "What signed interpretation resolves the official PDF conflict between Chapter II 12.3 page 9 and 17(e) page 11?",
                "assumed_answer": None,
                "blocks_capabilities": [],
            },
        ],
        "capabilities": capabilities,
        "flows": flows,
        "flat_proof": {
            "path": "docs/art/" + PROOF_NAME,
            "evidence_class": "flat_text_interaction_layout_proof_not_gpu_not_runtime",
        },
    }


def build_schema() -> dict:
    return {
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "$id": "billiardgl://schemas/phase5-rules-decision-ui-actions-v1",
        "title": "Phase 5 pending rules decision UI/actions",
        "type": "object", "additionalProperties": False,
        "required": ["schema", "schema_version", "package_id", "ruleset_id", "status",
                     "profile_complete", "product_exposed", "source_bindings",
                     "evidence_boundary", "interaction_policy", "authority_contract",
                     "accessibility", "policy_decision_placeholders", "capabilities",
                     "flows", "flat_proof"],
        "properties": {
            "schema": {"const": "phase5_rules_decision_ui_actions_v1"},
            "schema_version": {"const": 1},
            "package_id": {"const": "phase5_wpa_heyball_rules_decision_ui_actions_v1"},
            "ruleset_id": {"const": "wpa_heyball_2025_08_16_v1"},
            "status": {"const": "PENDING"},
            "profile_complete": {"const": False},
            "product_exposed": {"const": False},
            "source_bindings": {"type": "object"},
            "evidence_boundary": {"type": "object"},
            "interaction_policy": {"type": "object"},
            "authority_contract": {"type": "object"},
            "accessibility": {"type": "object"},
            "policy_decision_placeholders": {"type": "array", "minItems": 3, "maxItems": 3},
            "capabilities": {
                "type": "array", "minItems": 6, "maxItems": 6,
                "items": {"type": "object", "required": ["capability_id", "state", "product_exposed", "source_rows", "flow_ids"],
                          "properties": {"state": {"const": "pending"}, "product_exposed": {"const": False}}},
            },
            "flows": {
                "type": "array", "minItems": 7, "maxItems": 7,
                "items": {"type": "object", "required": ["flow_id", "capability_id", "capability_state", "product_exposed", "machine_code", "text_key", "source_rows", "shape_tokens", "copy", "preconditions", "postconditions", "states", "initial_state", "completed_state", "actions", "policy_dependencies", "failure_policy"],
                          "properties": {"capability_state": {"const": "pending"}, "product_exposed": {"const": False}}},
            },
            "flat_proof": {"type": "object"},
        },
    }


def build_flat_proof(manifest: dict) -> str:
    cards = []
    positions = [(40, 120), (510, 120), (980, 120), (1450, 120),
                 (40, 560), (510, 560), (980, 560)]
    for flow_value, (x, y) in zip(manifest["flows"], positions):
        title = flow_value["copy"]["title"]
        body = flow_value["copy"]["body"]
        labels = [item["label"] for item in flow_value["actions"][:3]]
        en_body = _proof_lines(body["en"], 54, 2)
        zh_body = _proof_lines(body["zh_cn"], 24, 2, cjk=True)
        lines = [
            f'<rect x="{x}" y="{y}" width="430" height="390" rx="18" class="panel"/>',
            f'<text x="{x + 24}" y="{y + 42}" class="eyebrow">PENDING · {html.escape(flow_value["capability_id"])}</text>',
            f'<text x="{x + 24}" y="{y + 82}" class="title">{html.escape(title["en"])}</text>',
            f'<text x="{x + 24}" y="{y + 114}" class="zh">{html.escape(title["zh_cn"])}</text>',
            f'<text x="{x + 24}" y="{y + 150}" class="body">{html.escape(en_body[0])}</text>',
            f'<text x="{x + 24}" y="{y + 176}" class="body">{html.escape(en_body[1])}</text>',
            f'<text x="{x + 24}" y="{y + 208}" class="zhbody">{html.escape(zh_body[0])}</text>',
            f'<text x="{x + 24}" y="{y + 234}" class="zhbody">{html.escape(zh_body[1])}</text>',
        ]
        by = y + 260
        for index, label in enumerate(labels):
            cls = "button focus" if index == 0 else "button"
            text_cls = "buttonsmall" if len(label["en"] + label["zh_cn"]) > 42 else "buttontext"
            lines.append(f'<rect x="{x + 24}" y="{by + index * 36}" width="382" height="30" rx="7" class="{cls}"/>')
            lines.append(f'<text x="{x + 36}" y="{by + 21 + index * 36}" class="{text_cls}">{html.escape(label["en"])} / {html.escape(label["zh_cn"])}</text>')
        lines.append(f'<text x="{x + 24}" y="{y + 374}" class="foot">AUTHORITY ACK REQUIRED · NO OPTIMISTIC COMMIT</text>')
        cards.append("\n".join(lines))
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="1920" height="1080" viewBox="0 0 1920 1080" role="img" aria-labelledby="title desc">
<title id="title">Phase 5 pending rules decision UI and action flat proof</title>
<desc id="desc">Seven bilingual flow cards. This is not GPU or runtime evidence and every capability is pending and not product exposed.</desc>
<style>
.bg{{fill:#0b1014}}.panel{{fill:#111b21;stroke:#61717a;stroke-width:2}}.eyebrow{{fill:#f2bc55;font:600 13px system-ui,sans-serif}}.title{{fill:#f4f7f8;font:650 23px system-ui,sans-serif}}.zh{{fill:#f4f7f8;font:600 20px system-ui,sans-serif}}.body{{fill:#c1cbd0;font:15px system-ui,sans-serif}}.zhbody{{fill:#c1cbd0;font:15px system-ui,sans-serif}}.button{{fill:#16252c;stroke:#7b8b94;stroke-width:1.5}}.focus{{stroke:#31b7c8;stroke-width:3}}.buttontext{{fill:#f4f7f8;font:13px system-ui,sans-serif}}.buttonsmall{{fill:#f4f7f8;font:10.5px system-ui,sans-serif}}.foot{{fill:#f06b5d;font:600 11px system-ui,sans-serif}}.header{{fill:#f4f7f8;font:700 30px system-ui,sans-serif}}.sub{{fill:#f2bc55;font:600 17px system-ui,sans-serif}}
</style>
<rect width="1920" height="1080" class="bg"/>
<text id="titleText" x="40" y="52" class="header">PHASE 5 RULE DECISION UI/ACTION · FLAT TEXT/INTERACTION PROOF</text>
<text x="40" y="84" class="sub">NOT GPU · NOT RUNTIME · PENDING · PRODUCT_EXPOSED=FALSE · PROFILE INCOMPLETE</text>
{''.join(cards)}
<rect x="1450" y="560" width="430" height="390" rx="18" class="panel"/>
<text x="1474" y="604" class="title">Fail-closed authority contract</text>
<text x="1474" y="638" class="zh">权威确认与失败关闭</text>
<text x="1474" y="684" class="body">No turn, group, ball position, or rack mutation</text>
<text x="1474" y="710" class="body">before an exact ACK and state revision.</text>
<text x="1474" y="748" class="zhbody">收到精确 ACK 与状态版本前，不改变回合、球组、</text>
<text x="1474" y="774" class="zhbody">球位或球局。</text>
<rect x="1474" y="814" width="382" height="38" rx="7" class="button focus"/>
<text x="1490" y="839" class="buttontext">Retry status / 重试状态检查</text>
<rect x="1474" y="864" width="382" height="38" rx="7" class="button"/>
<text x="1490" y="889" class="buttontext">Restore exact offer / 恢复权威选择</text>
<text x="40" y="1036" class="sub">ESSENTIAL RULES TEXT IGNORES SUBTITLE VISIBILITY · SHAPE + TEXT · STANDARD + CVD PALETTES</text>
</svg>'''


def write_package(output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    manifest = build_manifest()
    (output_dir / MANIFEST_NAME).write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    (output_dir / SCHEMA_NAME).write_text(
        json.dumps(build_schema(), indent=2, sort_keys=True) + "\n", encoding="utf-8")
    (output_dir / PROOF_NAME).write_text(build_flat_proof(manifest), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", type=Path, default=ROOT / "docs/art")
    args = parser.parse_args()
    write_package(args.output_dir)
    print("Generated Phase5 rules decision UI/actions: capabilities=6 flows=7 status=PENDING product_exposed=false")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
