#!/usr/bin/env python3
"""Generate PENDING-only Phase-5/M4 product Beta human-QA artifacts."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
PENDING_NAME = "phase5-beta-qa-v1.pending.json"
MATRIX_SCHEMA_NAME = "phase5-beta-qa-v1.schema.json"
RECORD_SCHEMA_NAME = "phase5-beta-qa-record-v1.schema.json"

SOURCE_PATHS = {
    "rules": "src/Billiards/rules/chinese_eight_ball_rules.cpp",
    "ai": "src/Billiards/ai/shot_planner.cpp",
    "shell": "src/Billiards/product_shell_controller.cpp",
    "settings": "src/Billiards/product_settings.cpp",
    "settings_runtime": "src/Billiards/product_settings_runtime.cpp",
    "input": "src/Billiards/product_input_controller.cpp",
    "audio": "src/Billiards/audio/audio_asset_policy.cpp",
}

FROZEN_TOKENS = {
    "hud_token_sha256": "sha256:abf73aa283279c28677b5c97a9b02b3130b0d0125048c3124c85a9c0e72df860",
    "shell_token_sha256": "sha256:c144f1a25d3e7ced14e385bd6f76ca7e0f05b06597dcc8a4bacb4c570fc6bad3",
    "gpu_capture_matrix_sha256": "sha256:835210fb2a458edda80e6ba54d0dcda14805e15b5ca54c5a3c3747d154192766",
}

EXPECTED_REASON_CODES = (
    "illegal_break", "no_object_ball_contact", "wrong_first_contact",
    "no_rail_after_contact", "cue_ball_scratch", "cue_ball_off_table",
    "object_ball_off_table", "illegal_eight_ball",
)

RULE_EVENTS = (
    {"event_id": "legal_break", "reason_codes": [], "text_keys": []},
    {"event_id": "illegal_break", "reason_codes": ["illegal_break"],
     "text_keys": ["rules.chinese8.foul.illegal_break"]},
    {"event_id": "group_assignment", "reason_codes": [],
     "text_keys": ["rules.chinese8.group.solids|rules.chinese8.group.stripes"]},
    {"event_id": "no_object_ball_contact", "reason_codes": ["no_object_ball_contact"],
     "text_keys": ["rules.chinese8.foul.no_object_contact"]},
    {"event_id": "wrong_first_contact", "reason_codes": ["wrong_first_contact"],
     "text_keys": ["rules.chinese8.foul.wrong_first_contact"]},
    {"event_id": "no_rail_after_contact", "reason_codes": ["no_rail_after_contact"],
     "text_keys": ["rules.chinese8.foul.no_rail_after_contact"]},
    {"event_id": "cue_ball_scratch", "reason_codes": ["cue_ball_scratch"],
     "text_keys": ["rules.chinese8.foul.cue_ball_scratch"]},
    {"event_id": "ball_in_hand", "reason_codes": [], "text_keys": []},
    {"event_id": "turn_handoff", "reason_codes": [], "text_keys": []},
    {"event_id": "legal_eight_win", "reason_codes": [],
     "text_keys": ["rules.chinese8.win.legal_eight"]},
    {"event_id": "illegal_eight_loss", "reason_codes": ["illegal_eight_ball"],
     "text_keys": ["rules.chinese8.loss.illegal_eight"]},
)

AI_LEVELS = {
    "rookie": {"maximum_candidates": 8, "maximum_wall_time_ms": 4,
               "maximum_aim_error_radians": 0.0465,
               "maximum_power_error_fraction": 0.085, "maximum_tip_error_radius": 0.055},
    "club": {"maximum_candidates": 24, "maximum_wall_time_ms": 8,
             "maximum_aim_error_radians": 0.031,
             "maximum_power_error_fraction": 0.04, "maximum_tip_error_radius": 0.025},
    "expert": {"maximum_candidates": 64, "maximum_wall_time_ms": 12,
               "maximum_aim_error_radians": 0.024,
               "maximum_power_error_fraction": 0.012, "maximum_tip_error_radius": 0.008},
}

AUDIO_DECISIONS = {
    "audio_alias_impact": {
        "file": "hit.wav", "alias_group": "impact_hit_alias",
        "semantic_cues": ["cue_impact", "ball_ball", "rail", "pocket_jaw"],
    },
    "audio_alias_notifications": {
        "file": "GameOver.wav", "alias_group": "notification_gameover_alias",
        "semantic_cues": ["ui_feedback", "foul", "match_finish"],
    },
    "audio_alias_music": {
        "file": "background.wav", "alias_group": "background_loop_alias",
        "semantic_cues": ["music_loop"],
    },
    "audio_alias_ambience": {
        "file": "background.wav", "alias_group": "background_loop_alias",
        "semantic_cues": ["venue_ambience"],
    },
}

CHECKS = {
    "full_match_product": ("product_entry_reached", "new_match_started_at_break",
                           "all_shots_used_product_controls", "legal_win_reached",
                           "winner_and_result_readable", "no_scenario_or_state_shortcut"),
    "rules_hud_reason_coverage": ("all_rule_events_observed", "hud_player_readable",
                                  "reason_codes_exact", "ball_in_hand_readable",
                                  "turn_handoff_readable", "win_loss_readable"),
    "settings_lifecycle": ("startup_load", "bad_file_defaults_warning", "preview",
                           "apply", "cancel", "atomic_save", "restart_persistence",
                           "context_rebind", "conflict_rollback"),
    "accessibility_and_controls": ("ui_scale_85", "ui_scale_135", "all_cvd_modes",
                                   "all_subtitle_modes", "all_assist_modes", "four_audio_buses",
                                   "dynamic_effect_toggles", "assist_never_auto_aims_or_shoots"),
    "actual_gpu_shell_hud": ("actual_gpu_capture_bound", "shell_capture_pass",
                             "hud_capture_pass", "opaque_icon_zero", "debug_node_zero"),
}
for level in AI_LEVELS:
    CHECKS["ai_" + level + "_product_challenge"] = (
        "product_entry_reached", "difficulty_selected", "challenge_started",
        "challenge_completed", "no_hidden_physics_interface", "main_thread_responsive")
for record_id in AUDIO_DECISIONS:
    CHECKS[record_id] = ("alias_disclosed", "shared_asset_not_claimed_distinct",
                         "pitch_not_claimed_supported", "content_disposition_recorded")


def _sha(path: Path) -> str:
    return "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest()


def extract_source_contract():
    source_hashes = {name + "_source_sha256": _sha(ROOT / path)
                     for name, path in SOURCE_PATHS.items()}
    rules_text = (ROOT / SOURCE_PATHS["rules"]).read_text(encoding="utf-8")
    reason_codes = tuple(re.findall(r'case FoulReason::(?!None)[A-Za-z]+: return "([a-z_]+)";', rules_text))
    if reason_codes != EXPECTED_REASON_CODES:
        raise ValueError("WPA Heyball reason-code contract changed: " + str(reason_codes))
    if 'id = "wpa_heyball_2025_08_16_v1"' not in (
            ROOT / "src/Billiards/rules/chinese_eight_ball_rules.h").read_text(encoding="utf-8"):
        raise ValueError("approved WPA Heyball ruleset ID changed")
    ai_text = (ROOT / SOURCE_PATHS["ai"]).read_text(encoding="utf-8")
    for level, values in AI_LEVELS.items():
        enum = level.capitalize()
        required = (f'case Difficulty::{enum}: return "{level}";',
                    f'case Difficulty::{enum}: return {{{values["maximum_candidates"]}, '
                    f'std::chrono::milliseconds({values["maximum_wall_time_ms"]})}};')
        if any(item not in ai_text for item in required):
            raise ValueError("AI level contract changed: " + level)
    audio_text = (ROOT / SOURCE_PATHS["audio"]).read_text(encoding="utf-8")
    for contract in AUDIO_DECISIONS.values():
        if contract["file"] not in audio_text or contract["alias_group"] not in audio_text:
            raise ValueError("audio alias contract changed: " + contract["alias_group"])
    return {
        "ruleset_id": "wpa_heyball_2025_08_16_v1",
        "reason_codes": list(reason_codes),
        "rule_events": list(RULE_EVENTS),
        "ai_levels": AI_LEVELS,
        "settings": {
            "ui_scale_range": [0.85, 1.35],
            "color_vision_modes": ["default", "deuteranopia", "protanopia", "tritanopia",
                                   "high_contrast_patterns"],
            "subtitle_modes": ["off", "essential", "all"],
            "assist_modes": ["off", "basic", "guided"],
            "audio_buses": ["music", "ambience", "effects", "ui"],
            "display_effects": ["bloom", "depth_of_field", "dynamic_camera", "camera_shake"],
            "bad_file_warning_code": "E_SETTINGS_FORMAT",
            "rebind_conflict_code": "E_BINDING_CONFLICT",
        },
        "audio_decisions": AUDIO_DECISIONS,
        "source_hashes": source_hashes,
        **FROZEN_TOKENS,
    }


def _pending_environment(contract):
    return {
        "build_sha256": "PENDING", "content_sha256": "PENDING",
        "physics_profile_id": "PENDING", "physics_profile_sha256": "PENDING",
        "gpu_capture_manifest_sha256": "PENDING", "gpu_manual_review_sha256": "PENDING",
        "device_code": "PENDING", "operating_system": "PENDING", "gpu_device": "PENDING",
        "input_devices": "PENDING", "logical_viewport_px": "PENDING",
        "physical_viewport_px": "PENDING", "dpi": "PENDING",
        **contract["source_hashes"], **FROZEN_TOKENS,
    }


def _pending_evidence(record_id, contract):
    if record_id == "full_match_product":
        return {"started_phase": "PENDING", "ended_phase": "PENDING", "winner": "PENDING",
                "physical_shot_count": "PENDING", "match_trace_sha256": "PENDING",
                "match_replay_sha256": "PENDING", "used_scenario_shortcut": "PENDING",
                "used_state_injection": "PENDING"}
    if record_id == "rules_hud_reason_coverage":
        return {"events": [{"event_id": event["event_id"], "observed": "PENDING",
                            "hud_player_readable": "PENDING", "reason_codes": "PENDING",
                            "text_keys": "PENDING", "ball_in_hand_visible": "PENDING",
                            "turn_handoff_visible": "PENDING", "screenshot_sha256": "PENDING"}
                           for event in RULE_EVENTS]}
    if record_id.startswith("ai_"):
        level = record_id.split("_")[1]
        return {"difficulty_id": level, "product_entry": "PENDING", "challenge_started": "PENDING",
                "challenge_completed": "PENDING", "hidden_physics_interface_used": "PENDING",
                "main_thread_max_block_ms": "PENDING", "automation_trace_sha256": "PENDING"}
    if record_id == "settings_lifecycle":
        return {key: "PENDING" for key in (
            "startup_loaded", "bad_file_fell_back_to_defaults", "bad_file_warning_code",
            "preview_visible_before_apply", "apply_committed", "cancel_restored_live_values",
            "atomic_save_succeeded", "saved_file_sha256", "restart_loaded_same_sha256",
            "context_rebind_succeeded", "conflict_rejected", "conflict_error_code",
            "conflict_rollback_preserved_binding")}
    if record_id == "accessibility_and_controls":
        return {"minimum_ui_scale": "PENDING", "maximum_ui_scale": "PENDING",
                "reachable_cvd_modes": "PENDING", "reachable_subtitle_modes": "PENDING",
                "reachable_assist_modes": "PENDING", "reachable_audio_buses": "PENDING",
                "reachable_display_effects": "PENDING", "assist_auto_aimed": "PENDING",
                "assist_auto_shot": "PENDING"}
    if record_id in AUDIO_DECISIONS:
        return {**contract["audio_decisions"][record_id], "reported_status": "PENDING",
                "disclosed_to_content_review": "PENDING", "claimed_distinct_asset": "PENDING",
                "claimed_pitch_supported": "PENDING", "content_disposition": "PENDING",
                "review_notes": "PENDING"}
    return {"actual_gpu_capture": "PENDING", "capture_manifest_status": "PENDING",
            "manual_review_status": "PENDING", "hud_cases_present": "PENDING",
            "shell_cases_present": "PENDING", "opaque_icon_count": "PENDING",
            "unexpanded_icon_count": "PENDING", "debug_node_count": "PENDING",
            "capture_artifact_sha256": "PENDING"}


def _pending_record(record_id, record_type, contract):
    return {
        "record_id": record_id, "record_type": record_type, "status": "PENDING",
        "contract": {"check_ids": list(CHECKS[record_id])},
        "executor": {"executor_id": "PENDING", "real_external_human": "PENDING",
                     "project_member": "PENDING", "agent_or_robot": "PENDING"},
        "environment": _pending_environment(contract),
        "session": {"started_utc": "PENDING", "ended_utc": "PENDING", "duration_ms": "PENDING"},
        "checks": [{"check_id": item, "result": "PENDING", "notes": "PENDING"}
                   for item in CHECKS[record_id]],
        "evidence": _pending_evidence(record_id, contract),
        "automation_binding": {"attached": "PENDING", "artifact_sha256": "PENDING",
                               "passed": "PENDING", "substitutes_for_human_qa": False},
        "attestations": {
            "executor": {"accepted": "PENDING", "signer_code": "PENDING",
                         "signed_utc": "PENDING", "statement_version": "phase5_beta_qa_attestation_v1"},
            "observer": {"accepted": "PENDING", "observer_id": "PENDING",
                         "signed_utc": "PENDING", "statement_version": "phase5_beta_qa_attestation_v1"},
        },
        "pii_review": {"performed": "PENDING", "status": "PENDING",
                       "reviewer_observer_id": "PENDING"},
    }


def build_pending_matrix():
    contract = extract_source_contract()
    ids = [("full_match_product", "full_physical_product_match"),
           ("rules_hud_reason_coverage", "rules_and_hud"),
           ("ai_rookie_product_challenge", "ai_product_challenge"),
           ("ai_club_product_challenge", "ai_product_challenge"),
           ("ai_expert_product_challenge", "ai_product_challenge"),
           ("settings_lifecycle", "settings_transaction"),
           ("accessibility_and_controls", "accessibility_settings"),
           ("audio_alias_impact", "audio_content_disposition"),
           ("audio_alias_notifications", "audio_content_disposition"),
           ("audio_alias_music", "audio_content_disposition"),
           ("audio_alias_ambience", "audio_content_disposition"),
           ("actual_gpu_shell_hud", "actual_gpu_capture")]
    return {
        "schema": "phase5_beta_qa_v1", "status": "NOT_RUN",
        "matrix_id": "phase5_m4_product_beta_human_qa_v1",
        "protocol_path": "docs/art/phase5-beta-qa-protocol.md",
        "source_contract": contract,
        "evidence_boundary": {"real_external_human_required": True,
                              "agents_or_robots_allowed": False, "project_members_allowed": False,
                              "pii_collection_allowed": False,
                              "automation_or_model_tests_may_substitute": False,
                              "product_default_switch_before_all_gates_pass": False,
                              "aggregate_before_all_records_complete": False},
        "records": [_pending_record(record_id, record_type, contract)
                    for record_id, record_type in ids],
        "aggregate": {"status": "NOT_RUN", "product_mode_default_switch_authorized": False},
    }


def build_record_schema():
    return {"$schema": "https://json-schema.org/draft/2020-12/schema",
            "$id": "billiardgl://schemas/phase5-beta-qa-record-v1",
            "title": "Phase 5 Beta anonymous human QA record", "type": "object",
            "additionalProperties": False,
            "required": ["record_id", "record_type", "status", "contract", "executor",
                         "environment", "session", "checks", "evidence", "automation_binding",
                         "attestations", "pii_review"],
            "properties": {"record_id": {"type": "string"}, "record_type": {"type": "string"},
                           "status": {"enum": ["PENDING", "COMPLETE"]},
                           "contract": {"type": "object"}, "executor": {"type": "object"},
                           "environment": {"type": "object"}, "session": {"type": "object"},
                           "checks": {"type": "array"}, "evidence": {"type": "object"},
                           "automation_binding": {"type": "object"},
                           "attestations": {"type": "object"}, "pii_review": {"type": "object"}}}


def build_matrix_schema():
    return {"$schema": "https://json-schema.org/draft/2020-12/schema",
            "$id": "billiardgl://schemas/phase5-beta-qa-v1",
            "title": "Phase 5 M4 product Beta human QA matrix", "type": "object",
            "additionalProperties": False,
            "required": ["schema", "status", "matrix_id", "protocol_path", "source_contract",
                         "evidence_boundary", "records", "aggregate"],
            "properties": {"schema": {"const": "phase5_beta_qa_v1"},
                           "status": {"enum": ["NOT_RUN", "PASS", "FAIL"]},
                           "matrix_id": {"const": "phase5_m4_product_beta_human_qa_v1"},
                           "protocol_path": {"type": "string"},
                           "source_contract": {"type": "object"},
                           "evidence_boundary": {"type": "object"},
                           "records": {"type": "array", "minItems": 12, "maxItems": 12},
                           "aggregate": {"type": "object"}}}


def write_templates(output_dir: Path):
    output_dir.mkdir(parents=True, exist_ok=True)
    for name, value in ((PENDING_NAME, build_pending_matrix()),
                        (MATRIX_SCHEMA_NAME, build_matrix_schema()),
                        (RECORD_SCHEMA_NAME, build_record_schema())):
        (output_dir / name).write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", type=Path, default=ROOT / "docs/art")
    args = parser.parse_args()
    write_templates(args.output_dir)
    print("Generated Phase5/M4 Beta human-QA matrix: records=12 status=NOT_RUN")


if __name__ == "__main__":
    main()
