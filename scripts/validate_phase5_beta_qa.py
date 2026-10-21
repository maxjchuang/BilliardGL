#!/usr/bin/env python3
"""Validate Phase-5/M4 real-human Beta QA evidence, fail closed."""

from __future__ import annotations

import argparse
from datetime import datetime
import json
from pathlib import Path
import re
import sys


sys.path.insert(0, str(Path(__file__).resolve().parent))
from generate_phase5_beta_qa_templates import (  # noqa: E402
    AI_LEVELS, AUDIO_DECISIONS, CHECKS, FROZEN_TOKENS, RULE_EVENTS,
    build_pending_matrix,
)


SHA = re.compile(r"sha256:[0-9a-f]{64}")
EXECUTOR = re.compile(r"P5QA-[A-Z0-9]{10}")
OBSERVER = re.compile(r"OBS-[A-Z0-9]{8}")
VIEWPORT = re.compile(r"[1-9][0-9]{2,4}x[1-9][0-9]{2,4}")
EMAIL = re.compile(r"\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}\b")
PHONE = re.compile(r"(?<![A-Za-z0-9])[0-9]{8,}(?![A-Za-z0-9])")
NAME = re.compile(r"\bmy name is\b|\b我叫\b|\b姓名[:：]", re.IGNORECASE)


def _match(pattern, value):
    return isinstance(value, str) and pattern.fullmatch(value) is not None


def _pii(value):
    return isinstance(value, str) and bool(EMAIL.search(value) or PHONE.search(value) or NAME.search(value))


def _utc(value):
    if not isinstance(value, str) or value == "PENDING":
        return None
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
        return parsed if parsed.tzinfo is not None else None
    except ValueError:
        return None


def validate_pending_template(value):
    return [] if value == build_pending_matrix() else [
        "PENDING Beta matrix differs from current source contracts or contains premature evidence"]


def _validate_executor(record, errors, context):
    executor = record.get("executor", {})
    if not _match(EXECUTOR, executor.get("executor_id")) or \
            executor.get("real_external_human") is not True or \
            executor.get("project_member") is not False or \
            executor.get("agent_or_robot") is not False:
        errors.append(context + " executor must be an anonymous external real human")


def _validate_environment(record, source_contract, errors, context):
    env = record.get("environment")
    if not isinstance(env, dict):
        errors.append(context + " environment missing")
        return {}
    for key in ("build_sha256", "content_sha256", "physics_profile_sha256",
                "gpu_capture_manifest_sha256", "gpu_manual_review_sha256"):
        if not _match(SHA, env.get(key)):
            errors.append(context + " environment hash invalid: " + key)
    for key, expected in {**source_contract["source_hashes"], **FROZEN_TOKENS}.items():
        if env.get(key) != expected:
            errors.append(context + " frozen source/token/capture-matrix binding differs: " + key)
    for key in ("physics_profile_id", "device_code", "operating_system", "gpu_device"):
        value = env.get(key)
        if not isinstance(value, str) or not value.strip() or value == "PENDING" or _pii(value):
            errors.append(context + " anonymous environment field invalid: " + key)
    inputs = env.get("input_devices")
    if not isinstance(inputs, list) or not {"keyboard", "mouse"}.issubset(set(inputs)):
        errors.append(context + " input_devices must include keyboard and mouse")
    for key in ("logical_viewport_px", "physical_viewport_px"):
        if not _match(VIEWPORT, env.get(key)):
            errors.append(context + " viewport invalid: " + key)
    dpi = env.get("dpi")
    if not isinstance(dpi, (int, float)) or isinstance(dpi, bool) or dpi <= 0:
        errors.append(context + " dpi must be positive")
    return env


def _validate_session(record, errors, context):
    session = record.get("session", {})
    start, end = _utc(session.get("started_utc")), _utc(session.get("ended_utc"))
    duration = session.get("duration_ms")
    if start is None or end is None or end <= start or not isinstance(duration, int) or duration <= 0:
        errors.append(context + " session timing invalid")
    elif abs((end - start).total_seconds() * 1000 - duration) > 2000:
        errors.append(context + " duration differs from UTC timestamps")


def _validate_checks(record, errors, context):
    expected = list(CHECKS[record["record_id"]])
    checks = record.get("checks")
    if not isinstance(checks, list) or [item.get("check_id") for item in checks
                                       if isinstance(item, dict)] != expected:
        errors.append(context + " required ordered checks differ")
        return False
    passed = True
    for item in checks:
        result, notes = item.get("result"), item.get("notes")
        if result not in {"PASS", "FAIL"}:
            errors.append(context + " check result invalid: " + str(item.get("check_id")))
        if not isinstance(notes, str) or not notes.strip() or notes == "PENDING" or _pii(notes):
            errors.append(context + " check notes missing or contain PII: " + str(item.get("check_id")))
        passed = passed and result == "PASS"
    return passed


def _validate_full_match(evidence, errors, context):
    for key in ("match_trace_sha256", "match_replay_sha256"):
        if not _match(SHA, evidence.get(key)):
            errors.append(context + " full-match hash invalid: " + key)
    if evidence.get("started_phase") != "break" or evidence.get("ended_phase") != "complete":
        errors.append(context + " full match must run from break to complete")
    if evidence.get("winner") not in {0, 1}:
        errors.append(context + " full match winner invalid")
    shots = evidence.get("physical_shot_count")
    if not isinstance(shots, int) or shots <= 0:
        errors.append(context + " physical shot count invalid")
    for key in ("used_scenario_shortcut", "used_state_injection"):
        if not isinstance(evidence.get(key), bool):
            errors.append(context + " shortcut disclosure invalid: " + key)
    return evidence.get("started_phase") == "break" and evidence.get("ended_phase") == "complete" and \
        evidence.get("winner") in {0, 1} and isinstance(shots, int) and shots > 0 and \
        evidence.get("used_scenario_shortcut") is False and evidence.get("used_state_injection") is False


def _validate_rules(evidence, errors, context):
    events = evidence.get("events")
    if not isinstance(events, list) or [item.get("event_id") for item in events
                                       if isinstance(item, dict)] != [item["event_id"] for item in RULE_EVENTS]:
        errors.append(context + " rules evidence must exactly cover ordered rule events")
        return False
    passed = True
    for actual, expected in zip(events, RULE_EVENTS):
        if actual.get("reason_codes") != expected["reason_codes"]:
            errors.append(context + " reason code differs: " + expected["event_id"])
        expected_keys = expected["text_keys"]
        actual_keys = actual.get("text_keys")
        if expected["event_id"] == "group_assignment":
            allowed = {"rules.chinese8.group.solids", "rules.chinese8.group.stripes"}
            if not isinstance(actual_keys, list) or len(actual_keys) != 1 or actual_keys[0] not in allowed:
                errors.append(context + " group assignment text key differs")
        elif actual_keys != expected_keys:
            errors.append(context + " text keys differ: " + expected["event_id"])
        if not _match(SHA, actual.get("screenshot_sha256")):
            errors.append(context + " screenshot SHA invalid: " + expected["event_id"])
        for key in ("observed", "hud_player_readable", "ball_in_hand_visible", "turn_handoff_visible"):
            if not isinstance(actual.get(key), bool):
                errors.append(context + " rule event boolean invalid: " + expected["event_id"] + "/" + key)
        event_pass = actual.get("observed") is True and actual.get("hud_player_readable") is True
        if expected["event_id"] == "ball_in_hand":
            event_pass = event_pass and actual.get("ball_in_hand_visible") is True
        if expected["event_id"] == "turn_handoff":
            event_pass = event_pass and actual.get("turn_handoff_visible") is True
        passed = passed and event_pass
    return passed


def _validate_ai(record_id, evidence, errors, context):
    level = record_id.split("_")[1]
    if evidence.get("difficulty_id") != level:
        errors.append(context + " AI difficulty differs from product contract")
    block = evidence.get("main_thread_max_block_ms")
    if not isinstance(block, (int, float)) or isinstance(block, bool) or block < 0:
        errors.append(context + " main-thread block measurement invalid")
    if not _match(SHA, evidence.get("automation_trace_sha256")):
        errors.append(context + " AI automation trace SHA invalid")
    for key in ("product_entry", "challenge_started", "challenge_completed", "hidden_physics_interface_used"):
        if not isinstance(evidence.get(key), bool):
            errors.append(context + " AI evidence boolean invalid: " + key)
    return evidence.get("difficulty_id") == level and evidence.get("product_entry") is True and \
        evidence.get("challenge_started") is True and evidence.get("challenge_completed") is True and \
        evidence.get("hidden_physics_interface_used") is False and isinstance(block, (int, float)) and \
        not isinstance(block, bool) and 0 <= block <= 16.7


def _validate_settings(evidence, contract, errors, context):
    bool_keys = ("startup_loaded", "bad_file_fell_back_to_defaults", "preview_visible_before_apply",
                 "apply_committed", "cancel_restored_live_values", "atomic_save_succeeded",
                 "context_rebind_succeeded", "conflict_rejected", "conflict_rollback_preserved_binding")
    for key in bool_keys:
        if not isinstance(evidence.get(key), bool):
            errors.append(context + " settings evidence boolean invalid: " + key)
    if evidence.get("bad_file_warning_code") != contract["bad_file_warning_code"] or \
            evidence.get("conflict_error_code") != contract["rebind_conflict_code"]:
        errors.append(context + " settings stable error code differs")
    for key in ("saved_file_sha256", "restart_loaded_same_sha256"):
        if not _match(SHA, evidence.get(key)):
            errors.append(context + " settings save/restart SHA invalid: " + key)
    hashes_equal = evidence.get("saved_file_sha256") == evidence.get("restart_loaded_same_sha256")
    return all(evidence.get(key) is True for key in bool_keys) and hashes_equal


def _validate_accessibility(evidence, contract, errors, context):
    exact = {
        "minimum_ui_scale": contract["ui_scale_range"][0],
        "maximum_ui_scale": contract["ui_scale_range"][1],
        "reachable_cvd_modes": contract["color_vision_modes"],
        "reachable_subtitle_modes": contract["subtitle_modes"],
        "reachable_assist_modes": contract["assist_modes"],
        "reachable_audio_buses": contract["audio_buses"],
        "reachable_display_effects": contract["display_effects"],
    }
    if any(evidence.get(key) != value for key, value in exact.items()):
        errors.append(context + " accessibility/control reachable set differs")
    for key in ("assist_auto_aimed", "assist_auto_shot"):
        if not isinstance(evidence.get(key), bool):
            errors.append(context + " assist authority disclosure invalid: " + key)
    return all(evidence.get(key) == value for key, value in exact.items()) and \
        evidence.get("assist_auto_aimed") is False and evidence.get("assist_auto_shot") is False


def _validate_audio(record_id, evidence, contract, errors, context):
    immutable = contract["audio_decisions"][record_id]
    if any(evidence.get(key) != value for key, value in immutable.items()):
        errors.append(context + " audio file/alias/semantic-cue disclosure differs")
    if evidence.get("reported_status") != "degraded_asset_alias":
        errors.append(context + " audio status must remain degraded_asset_alias")
    disposition = evidence.get("content_disposition")
    if disposition not in {"ACCEPTED_DEGRADED", "BLOCKED"}:
        errors.append(context + " audio content disposition must be ACCEPTED_DEGRADED or BLOCKED")
    notes = evidence.get("review_notes")
    if not isinstance(notes, str) or not notes.strip() or notes == "PENDING" or _pii(notes):
        errors.append(context + " audio content review notes missing or contain PII")
    for key in ("disclosed_to_content_review", "claimed_distinct_asset", "claimed_pitch_supported"):
        if not isinstance(evidence.get(key), bool):
            errors.append(context + " audio disclosure boolean invalid: " + key)
    return evidence.get("reported_status") == "degraded_asset_alias" and \
        evidence.get("disclosed_to_content_review") is True and \
        evidence.get("claimed_distinct_asset") is False and \
        evidence.get("claimed_pitch_supported") is False and disposition == "ACCEPTED_DEGRADED"


def _validate_gpu(evidence, errors, context):
    for key in ("actual_gpu_capture", "hud_cases_present", "shell_cases_present"):
        if not isinstance(evidence.get(key), bool):
            errors.append(context + " GPU evidence boolean invalid: " + key)
    if evidence.get("capture_manifest_status") not in {"PASS", "FAIL"} or \
            evidence.get("manual_review_status") not in {"PASS", "FAIL"}:
        errors.append(context + " GPU capture/manual status invalid")
    for key in ("opaque_icon_count", "unexpanded_icon_count", "debug_node_count"):
        if not isinstance(evidence.get(key), int) or evidence.get(key) < 0:
            errors.append(context + " GPU metric invalid: " + key)
    if not _match(SHA, evidence.get("capture_artifact_sha256")):
        errors.append(context + " GPU capture artifact SHA invalid")
    return evidence.get("actual_gpu_capture") is True and \
        evidence.get("capture_manifest_status") == "PASS" and \
        evidence.get("manual_review_status") == "PASS" and \
        evidence.get("hud_cases_present") is True and evidence.get("shell_cases_present") is True and \
        all(evidence.get(key) == 0 for key in
            ("opaque_icon_count", "unexpanded_icon_count", "debug_node_count"))


def _validate_automation(record, errors, context):
    binding = record.get("automation_binding", {})
    for key in ("attached", "passed", "substitutes_for_human_qa"):
        if not isinstance(binding.get(key), bool):
            errors.append(context + " automation binding boolean invalid: " + key)
    if binding.get("attached") is True and not _match(SHA, binding.get("artifact_sha256")):
        errors.append(context + " automation artifact SHA invalid")
    if binding.get("substitutes_for_human_qa") is not False:
        errors.append(context + " automation/model test cannot substitute for human QA")
    return binding.get("attached") is True and binding.get("passed") is True and \
        binding.get("substitutes_for_human_qa") is False


def _validate_attestations(record, errors, context):
    executor_id = record.get("executor", {}).get("executor_id")
    values = record.get("attestations", {})
    executor, observer = values.get("executor", {}), values.get("observer", {})
    observer_id = observer.get("observer_id")
    if executor.get("accepted") is not True or executor.get("signer_code") != executor_id or \
            executor.get("statement_version") != "phase5_beta_qa_attestation_v1" or \
            _utc(executor.get("signed_utc")) is None:
        errors.append(context + " executor attestation invalid")
    if observer.get("accepted") is not True or not _match(OBSERVER, observer_id) or \
            observer.get("statement_version") != "phase5_beta_qa_attestation_v1" or \
            _utc(observer.get("signed_utc")) is None:
        errors.append(context + " observer attestation invalid")
    if observer_id == executor_id:
        errors.append(context + " executor cannot self-attest as observer")
    pii = record.get("pii_review", {})
    if pii.get("performed") is not True or pii.get("status") != "CLEAR" or \
            pii.get("reviewer_observer_id") != observer_id:
        errors.append(context + " PII review missing or not observer-bound")


def _aggregate(records, passes):
    count = sum(all(values.values()) for values in passes.values())
    audio_accepted = sum(all(passes[item].values()) for item in AUDIO_DECISIONS)
    all_complete = sum(record.get("status") == "COMPLETE" for record in records)
    gate = count == all_complete == 12 and audio_accepted == 4
    return {"status": "PASS" if gate else "FAIL", "record_count": len(records),
            "complete_record_count": all_complete, "passing_record_count": count,
            "audio_accepted_degraded_count": audio_accepted,
            "record_results": {key: "PASS" if all(value.values()) else "FAIL"
                               for key, value in passes.items()},
            "all_gates_pass": gate, "product_mode_default_switch_authorized": gate}


def validate_actual_matrix(value):
    errors = []
    pending = build_pending_matrix()
    if value.get("schema") != "phase5_beta_qa_v1" or \
            value.get("matrix_id") != "phase5_m4_product_beta_human_qa_v1" or \
            value.get("status") not in {"PASS", "FAIL"}:
        errors.append("actual Phase5 Beta matrix identity/status invalid")
    for key in ("protocol_path", "source_contract", "evidence_boundary"):
        if value.get(key) != pending[key]:
            errors.append("Phase5 Beta frozen contract differs: " + key)
    records = value.get("records")
    if not isinstance(records, list) or len(records) != 12:
        return errors + ["actual Phase5 Beta matrix requires exactly 12 records"], None
    expected = {record["record_id"]: record for record in pending["records"]}
    ids = [record.get("record_id") for record in records if isinstance(record, dict)]
    if len(ids) != 12 or len(set(ids)) != 12 or set(ids) != set(expected):
        return errors + ["Beta records must exactly cover the 12 required record IDs"], None
    frozen = {key: set() for key in ("build_sha256", "content_sha256", "physics_profile_id",
                                     "physics_profile_sha256", "gpu_capture_manifest_sha256",
                                     "gpu_manual_review_sha256")}
    passes = {}
    contract = pending["source_contract"]
    for index, record in enumerate(records):
        record_id = record["record_id"]
        context = "record[{}] {}".format(index, record_id)
        template = expected[record_id]
        if record.get("record_type") != template["record_type"] or record.get("contract") != template["contract"]:
            errors.append(context + " immutable record contract differs")
        if record.get("status") != "COMPLETE":
            errors.append(context + " is not COMPLETE")
        _validate_executor(record, errors, context)
        env = _validate_environment(record, contract, errors, context)
        for key in frozen:
            frozen[key].add(env.get(key))
        _validate_session(record, errors, context)
        manual = _validate_checks(record, errors, context)
        evidence = record.get("evidence", {})
        if record_id == "full_match_product":
            domain = _validate_full_match(evidence, errors, context)
        elif record_id == "rules_hud_reason_coverage":
            domain = _validate_rules(evidence, errors, context)
        elif record_id.startswith("ai_"):
            domain = _validate_ai(record_id, evidence, errors, context)
        elif record_id == "settings_lifecycle":
            domain = _validate_settings(evidence, contract["settings"], errors, context)
        elif record_id == "accessibility_and_controls":
            domain = _validate_accessibility(evidence, contract["settings"], errors, context)
        elif record_id in AUDIO_DECISIONS:
            domain = _validate_audio(record_id, evidence, contract, errors, context)
        else:
            domain = _validate_gpu(evidence, errors, context)
        automation = _validate_automation(record, errors, context)
        _validate_attestations(record, errors, context)
        passes[record_id] = {"manual": manual, "domain": domain, "automation": automation}
    if any(len(values) != 1 for values in frozen.values()):
        errors.append("all 12 Beta records must bind one frozen build/content/rules/profile/capture/audio set")
    aggregate = _aggregate(records, passes)
    if value.get("aggregate") != aggregate:
        errors.append("Beta aggregate/default-switch authorization differs from computed result")
    if value.get("status") != aggregate["status"]:
        errors.append("Beta matrix status differs from computed result")
    return errors, aggregate


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("evidence", type=Path)
    parser.add_argument("--pending-template", action="store_true")
    args = parser.parse_args()
    try:
        value = json.loads(args.evidence.read_text(encoding="utf-8"))
        if args.pending_template:
            errors, aggregate = validate_pending_template(value), None
        else:
            errors, aggregate = validate_actual_matrix(value)
    except (OSError, json.JSONDecodeError, ValueError) as error:
        print("Phase5 Beta QA validation failed: " + str(error), file=sys.stderr)
        return 1
    if errors:
        for error in errors:
            print("ERROR: " + error, file=sys.stderr)
        return 1
    if args.pending_template:
        print("Phase5/M4 Beta human-QA PENDING matrix valid: records=0/12 result=NOT_RUN default_switch=UNAUTHORIZED")
        return 0
    print("Phase5/M4 Beta human-QA actual matrix valid: records=12/12 result=" + aggregate["status"])
    return 0 if aggregate["status"] == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
