#!/usr/bin/env python3
"""Validate real-human Phase-3 user-study evidence without inventing results."""

from __future__ import annotations

import argparse
from datetime import datetime
import json
import math
from pathlib import Path
import re
import statistics
import sys


sys.path.insert(0, str(Path(__file__).resolve().parent))
from generate_phase3_user_study_templates import (  # noqa: E402
    FROZEN_BINDINGS, GROUP_SLOTS, TASK_IDS, TASK_STEP_IDS, THRESHOLDS,
    build_pending_study,
)


SHA_PATTERN = re.compile(r"sha256:[0-9a-f]{64}")
PARTICIPANT_ID_PATTERN = re.compile(r"P3-[A-Z0-9]{10}")
OBSERVER_ID_PATTERN = re.compile(r"OBS-[A-Z0-9]{8}")
EMAIL_PATTERN = re.compile(r"\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}\b")
PHONE_PATTERN = re.compile(r"(?<![A-Za-z0-9])[0-9]{8,}(?![A-Za-z0-9])")
NAME_PATTERN = re.compile(r"\bmy name is\b|\b我叫\b|\b姓名[:：]", re.IGNORECASE)


def validate_pending_template(value):
    return [] if value == build_pending_study() else [
        "PENDING user-study template differs or contains premature evidence/aggregate"]


def _parse_utc(value):
    if not isinstance(value, str) or value == "PENDING":
        return None
    try:
        return datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError:
        return None


def _contains_pii(value):
    if not isinstance(value, str):
        return False
    return bool(EMAIL_PATTERN.search(value) or PHONE_PATTERN.search(value) or
                NAME_PATTERN.search(value))


def _validate_task(task, session_duration, errors, context):
    task_id = task.get("task_id")
    if task_id not in TASK_IDS or task.get("status") != "COMPLETE":
        errors.append(context + " task identity/status is incomplete")
        return None
    start, end = task.get("start_ms"), task.get("end_ms")
    if not isinstance(start, int) or not isinstance(end, int) or \
            start < 0 or end < start or end > session_duration:
        errors.append(context + " task timing is invalid")
        return None
    steps = task.get("steps")
    if not isinstance(steps, list) or [step.get("step_id") for step in steps
                                      if isinstance(step, dict)] != list(TASK_STEP_IDS[task_id]):
        errors.append(context + " required step/time coverage differs")
        return None
    times = []
    error_total = 0
    help_total = 0
    for step in steps:
        at_ms = step.get("at_ms")
        step_errors = step.get("errors")
        if not isinstance(at_ms, int) or at_ms < start or at_ms > end or \
                step.get("outcome") not in {"COMPLETED", "ERROR_RECOVERED"} or \
                not isinstance(step.get("help_opened"), bool) or \
                not isinstance(step_errors, list) or any(
                    not isinstance(item, str) or not item.strip() or _contains_pii(item)
                    for item in step_errors):
            errors.append(context + " step evidence is invalid: " + str(step.get("step_id")))
        times.append(at_ms if isinstance(at_ms, int) else -1)
        error_total += len(step_errors) if isinstance(step_errors, list) else 0
        help_total += int(step.get("help_opened") is True)
    if times != sorted(times):
        errors.append(context + " step timestamps are not monotonic")
    if task.get("error_count") != error_total or task.get("help_open_count") != help_total:
        errors.append(context + " task error/help aggregate differs from steps")
    response = task.get("open_response")
    if not isinstance(response, str) or not response.strip() or response == "PENDING" or _contains_pii(response):
        errors.append(context + " open response is missing or contains PII")
    return {step["step_id"]: step for step in steps}, start, end


def _compute_aggregate(sessions):
    new_users = [session for session in sessions if session["group"] == "project_new_user"]
    experienced = [session for session in sessions
                   if session["group"] == "billiards_experienced_user"]
    shot_seconds = {}
    no_help = 0
    camera_unchanged = 0
    all_tasks = 0
    experienced_pass = 0
    for session in sessions:
        tasks = {task["task_id"]: task for task in session["tasks"]}
        all_tasks += int(all(task["status"] == "COMPLETE" for task in session["tasks"]))
        camera = tasks["overhead_to_player_low_roundtrip"]["measurement"]
        camera_unchanged += int(camera["overhead_selected"] is True and
                                camera["player_low_restored"] is True and
                                camera["shot_intent_before_sha256"] ==
                                camera["shot_intent_after_sha256"])
        if session["group"] == "project_new_user":
            first = tasks["first_valid_shot"]["measurement"]
            seconds = first["valid_shot_at_ms"] / 1000.0
            shot_seconds[session["participant_id"]] = seconds
            no_help += int(first["help_opened_before_power_and_tip_found"] is False)
        else:
            debrief = session["experienced_debrief"]
            experienced_pass += int(all(debrief[key] is True for key in (
                "tip_contact_correct", "shot_direction_correct", "assist_level_correct")))
    median = statistics.median(shot_seconds.values())
    without_help_rate = no_help / len(new_users)
    experienced_rate = experienced_pass / len(experienced)
    camera_rate = camera_unchanged / len(sessions)
    completion_rate = all_tasks / len(sessions)
    threshold_results = {
        "new_user_first_valid_shot_median": median <=
            THRESHOLDS["new_user_first_valid_shot_median_seconds_max"],
        "new_user_power_and_tip_without_help": without_help_rate >=
            THRESHOLDS["new_user_power_and_tip_without_help_rate_min"],
        "experienced_tip_direction_assist_explanation": experienced_rate >=
            THRESHOLDS["experienced_tip_direction_assist_explanation_rate_min"],
        "camera_roundtrip_shot_intent_unchanged": camera_rate >=
            THRESHOLDS["camera_roundtrip_shot_intent_unchanged_rate_min"],
        "all_four_tasks_complete": completion_rate >=
            THRESHOLDS["all_four_tasks_completion_rate_min"],
    }
    return {
        "status": "PASS" if all(threshold_results.values()) else "FAIL",
        "participant_count": len(sessions), "project_new_user_count": len(new_users),
        "billiards_experienced_user_count": len(experienced),
        "new_user_first_valid_shot_seconds_by_participant": shot_seconds,
        "new_user_first_valid_shot_median_seconds": median,
        "new_user_power_and_tip_without_help_count": no_help,
        "new_user_power_and_tip_without_help_rate": without_help_rate,
        "experienced_explanation_pass_count": experienced_pass,
        "experienced_explanation_pass_rate": experienced_rate,
        "camera_intent_unchanged_count": camera_unchanged,
        "camera_intent_unchanged_rate": camera_rate,
        "all_four_tasks_complete_count": all_tasks,
        "all_four_tasks_completion_rate": completion_rate,
        "threshold_results": threshold_results,
        "gate_pass": all(threshold_results.values()),
    }


def validate_actual_study(value):
    errors = []
    pending = build_pending_study()
    if value.get("schema") != "phase3_user_study_v1" or \
            value.get("study_id") != "phase3_first_shot_and_controls_v1" or \
            value.get("status") not in {"PASS", "FAIL"}:
        errors.append("actual study identity/status is invalid")
    for key in ("protocol_path", "research_ethics", "group_contract", "task_contract",
                "thresholds", "evidence_boundary"):
        if value.get(key) != pending[key]:
            errors.append("study contract differs: " + key)
    sessions = value.get("sessions")
    if not isinstance(sessions, list) or len(sessions) != 8:
        errors.append("actual study requires exactly eight complete sessions")
        return errors, None
    expected_slots = dict(GROUP_SLOTS)
    by_slot = {session.get("slot_id"): session for session in sessions if isinstance(session, dict)}
    if len(by_slot) != 8 or set(by_slot) != set(expected_slots):
        errors.append("session slots must exactly cover five new and three experienced users")
    participant_ids = [session.get("participant_id") for session in sessions]
    if any(not isinstance(value_id, str) or PARTICIPANT_ID_PATTERN.fullmatch(value_id) is None
           for value_id in participant_ids) or len(set(participant_ids)) != 8:
        errors.append("participant IDs must be eight unique anonymous P3 codes")
    shared_bindings = None
    for slot_id, group in GROUP_SLOTS:
        session = by_slot.get(slot_id)
        if session is None:
            continue
        context = slot_id
        if session.get("status") != "COMPLETE" or session.get("group") != group:
            errors.append(context + " status/group differs from recruited slot")
        eligibility = session.get("eligibility", {})
        if eligibility.get("real_human") is not True or \
                eligibility.get("project_member") is not False or \
                eligibility.get("agent_or_robot") is not False or \
                eligibility.get("group_qualification_attested") is not True:
            errors.append(context + " real-human/non-project eligibility is invalid")
        years = eligibility.get("in_person_billiards_experience_years")
        if not isinstance(years, (int, float)) or years < 0:
            errors.append(context + " billiards experience declaration is invalid")
        if group == "project_new_user" and eligibility.get("first_time_billiardgl_user") is not True:
            errors.append(context + " must be a first-time BilliardGL user")
        if group == "billiards_experienced_user" and (not isinstance(years, (int, float)) or years < 1):
            errors.append(context + " lacks the required real billiards experience")
        bindings = session.get("evidence_bindings", {})
        if any(bindings.get(key) != value_sha for key, value_sha in FROZEN_BINDINGS.items()) or \
                any(SHA_PATTERN.fullmatch(str(bindings.get(key))) is None for key in (
                    "build_sha256", "content_sha256", "gpu_capture_manifest_sha256")):
            errors.append(context + " build/content/token/capture SHA binding is invalid")
        if shared_bindings is None:
            shared_bindings = bindings
        elif bindings != shared_bindings:
            errors.append(context + " evidence bindings differ across participants")
        device = session.get("device", {})
        if set(device) != {"device_code", "operating_system", "gpu_device", "input_devices",
                           "logical_viewport_px", "physical_viewport_px", "dpi"} or \
                not isinstance(device.get("device_code"), str) or not device["device_code"].startswith("DEV-") or \
                not isinstance(device.get("input_devices"), list) or not device["input_devices"] or \
                any(not isinstance(value_dim, list) or len(value_dim) != 2 or
                    any(not isinstance(number, int) or number <= 0 for number in value_dim)
                    for value_dim in (device.get("logical_viewport_px"), device.get("physical_viewport_px"))) or \
                not isinstance(device.get("dpi"), (int, float)) or device["dpi"] <= 0:
            errors.append(context + " anonymous device/viewport evidence is invalid")
        timing = session.get("session", {})
        start_utc, end_utc = _parse_utc(timing.get("started_utc")), _parse_utc(timing.get("ended_utc"))
        duration = timing.get("duration_ms")
        if start_utc is None or end_utc is None or end_utc <= start_utc or \
                not isinstance(duration, int) or duration <= 0 or \
                abs((end_utc - start_utc).total_seconds() * 1000 - duration) > 1000:
            errors.append(context + " session UTC/duration evidence is invalid")
            duration = 0 if not isinstance(duration, int) else duration
        tasks = session.get("tasks")
        if not isinstance(tasks, list) or [task.get("task_id") for task in tasks
                                          if isinstance(task, dict)] != list(TASK_IDS):
            errors.append(context + " must contain the four tasks in protocol order")
            continue
        validated = {}
        intervals = []
        for task in tasks:
            result = _validate_task(task, duration, errors, context + "/" + task.get("task_id", "?"))
            if result:
                validated[task["task_id"]] = result[0]
                intervals.append((result[1], result[2]))
        if intervals != sorted(intervals) or any(intervals[index][1] > intervals[index + 1][0]
                                                 for index in range(len(intervals) - 1)):
            errors.append(context + " task intervals overlap or are out of order")
        task_by_id = {task["task_id"]: task for task in tasks}
        first = task_by_id["first_valid_shot"]
        first_measure = first.get("measurement", {})
        first_steps = validated.get("first_valid_shot", {})
        power_time = first_measure.get("power_control_found_at_ms")
        tip_time = first_measure.get("tip_control_found_at_ms")
        valid_time = first_measure.get("valid_shot_at_ms")
        derived_help = any(step["help_opened"] and step["at_ms"] <= max(power_time, tip_time)
                           for step in first_steps.values()) if first_steps and all(
                               isinstance(item, int) for item in (power_time, tip_time)) else None
        if first.get("start_ms") != 0 or first_measure.get("valid_shot") is not True or \
                not all(isinstance(item, int) for item in (power_time, tip_time, valid_time)) or \
                valid_time != first_steps.get("valid_shot_confirmed", {}).get("at_ms") or \
                power_time != first_steps.get("power_control_found", {}).get("at_ms") or \
                tip_time != first_steps.get("tip_control_found", {}).get("at_ms") or \
                first_measure.get("help_opened_before_power_and_tip_found") is not derived_help:
            errors.append(context + " first-valid-shot timing/help derivation differs")
        spin = task_by_id["low_draw_and_right_english"].get("measurement", {})
        low, right = spin.get("low_draw_tip_offset_xy_R"), spin.get("right_english_tip_offset_xy_R")
        if spin.get("low_draw_submitted") is not True or spin.get("right_english_submitted") is not True or \
                not isinstance(low, list) or len(low) != 2 or not all(isinstance(x, (int, float)) for x in low) or \
                not isinstance(right, list) or len(right) != 2 or not all(isinstance(x, (int, float)) for x in right) or \
                low[1] >= 0 or right[0] <= 0 or any(abs(x) > 1 for x in low + right):
            errors.append(context + " low-draw/right-English submission evidence is invalid")
        camera = task_by_id["overhead_to_player_low_roundtrip"].get("measurement", {})
        if camera.get("overhead_selected") is not True or camera.get("player_low_restored") is not True or \
                SHA_PATTERN.fullmatch(str(camera.get("shot_intent_before_sha256"))) is None or \
                camera.get("shot_intent_before_sha256") != camera.get("shot_intent_after_sha256"):
            errors.append(context + " camera roundtrip changed or lacks shot-intent SHA")
        rules = task_by_id["foul_and_handoff_explanation"].get("measurement", {})
        if rules.get("foul_reason_identified") is not True or rules.get("handoff_identified") is not True or \
                any(not isinstance(rules.get(key), str) or not rules[key].strip() or _contains_pii(rules[key])
                    for key in ("foul_explanation", "handoff_explanation")):
            errors.append(context + " foul/handoff open explanation is invalid")
        debrief = session.get("experienced_debrief", {})
        if group == "billiards_experienced_user":
            for answer in ("tip_contact_answer", "shot_direction_answer", "assist_level_answer"):
                if not isinstance(debrief.get(answer), str) or not debrief[answer].strip() or \
                        debrief[answer] == "PENDING" or _contains_pii(debrief[answer]):
                    errors.append(context + " experienced debrief answer is missing/PII: " + answer)
            if any(not isinstance(debrief.get(key), bool) for key in (
                    "tip_contact_correct", "shot_direction_correct", "assist_level_correct")):
                errors.append(context + " experienced explanation rubric is incomplete")
        elif any(debrief.get(key) != "N/A" for key in debrief):
            errors.append(context + " new-user experienced debrief must be N/A")
        attestations = session.get("attestations", {})
        participant_attestation = attestations.get("participant", {})
        observer_attestation = attestations.get("observer", {})
        observer_id = observer_attestation.get("observer_id")
        if participant_attestation.get("accepted") is not True or \
                participant_attestation.get("signer_code") != session.get("participant_id") or \
                participant_attestation.get("statement_version") != "phase3_user_study_attestation_v1" or \
                _parse_utc(participant_attestation.get("signed_utc")) is None or \
                observer_attestation.get("accepted") is not True or \
                not isinstance(observer_id, str) or OBSERVER_ID_PATTERN.fullmatch(observer_id) is None or \
                observer_id == session.get("participant_id") or \
                observer_attestation.get("statement_version") != "phase3_user_study_attestation_v1" or \
                _parse_utc(observer_attestation.get("signed_utc")) is None:
            errors.append(context + " participant/observer dual attestation is invalid")
        pii = session.get("pii_review", {})
        if pii != {"performed": True, "status": "CLEAR", "reviewer_observer_id": observer_id}:
            errors.append(context + " PII review is missing or not observer-bound")
    if errors:
        return errors, None
    aggregate = _compute_aggregate(sessions)
    if value.get("aggregate") != aggregate:
        errors.append("aggregate must be validator-computed only after all eight complete sessions")
    if value.get("status") != aggregate["status"]:
        errors.append("study status differs from computed acceptance gate")
    return errors, aggregate


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("study", type=Path)
    parser.add_argument("--pending-template", action="store_true")
    args = parser.parse_args()
    try:
        value = json.loads(args.study.read_text())
    except (OSError, json.JSONDecodeError) as error:
        print("Phase3 user-study validation failed: " + str(error), file=sys.stderr)
        return 1
    if args.pending_template:
        errors = validate_pending_template(value)
        if errors:
            for error in errors:
                print("Phase3 user-study validation failed: " + error, file=sys.stderr)
            return 1
        print("Phase3 real-user-study PENDING template valid: participants=0/8 result=NOT_RUN")
        return 0
    errors, aggregate = validate_actual_study(value)
    if errors:
        for error in errors:
            print("Phase3 user-study validation failed: " + error, file=sys.stderr)
        return 1
    print("Phase3 real-user-study evidence valid: participants=8/8 result=%s" %
          aggregate["status"])
    return 0 if aggregate["gate_pass"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
