#!/usr/bin/env python3
"""Validate Phase-4 real-human manual-play evidence without fabricating it."""

from __future__ import annotations

import argparse
from datetime import datetime
import json
from pathlib import Path
import re
import sys


sys.path.insert(0, str(Path(__file__).resolve().parent))
from generate_phase4_manual_play_templates import (  # noqa: E402
    MANUAL_CHECK_IDS,
    PACK_PATH,
    REPLAY_REJECTION_CONTRACT,
    build_pending_matrix,
)


SHA_PATTERN = re.compile(r"sha256:[0-9a-f]{64}")
EXECUTOR_PATTERN = re.compile(r"P4QA-[A-Z0-9]{10}")
OBSERVER_PATTERN = re.compile(r"OBS-[A-Z0-9]{8}")
VIEWPORT_PATTERN = re.compile(r"[1-9][0-9]{2,4}x[1-9][0-9]{2,4}")
EMAIL_PATTERN = re.compile(r"\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}\b")
PHONE_PATTERN = re.compile(r"(?<![A-Za-z0-9])[0-9]{8,}(?![A-Za-z0-9])")
NAME_PATTERN = re.compile(r"\bmy name is\b|\b我叫\b|\b姓名[:：]", re.IGNORECASE)


def _fullmatch(pattern, value):
    return isinstance(value, str) and pattern.fullmatch(value) is not None


def _parse_utc(value):
    if not isinstance(value, str) or value == "PENDING":
        return None
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
        return parsed if parsed.tzinfo is not None else None
    except ValueError:
        return None


def _contains_pii(value):
    return isinstance(value, str) and bool(
        EMAIL_PATTERN.search(value) or PHONE_PATTERN.search(value) or NAME_PATTERN.search(value))


def validate_pending_template(value, pack_path: Path = PACK_PATH):
    return [] if value == build_pending_matrix(pack_path) else [
        "PENDING manual-play matrix differs from current manifest or contains premature evidence/aggregate"]


def _validate_environment(environment, errors, context):
    if not isinstance(environment, dict):
        errors.append(context + " environment is missing")
        return None
    for key in ("build_sha256", "asset_bundle_sha256", "physics_profile_sha256"):
        if not _fullmatch(SHA_PATTERN, environment.get(key)):
            errors.append(context + " environment hash invalid: " + key)
    for key in ("executable_version", "physics_profile_id", "device_code", "operating_system",
                "gpu_device"):
        value = environment.get(key)
        if not isinstance(value, str) or not value.strip() or value == "PENDING" or _contains_pii(value):
            errors.append(context + " anonymous environment field invalid: " + key)
    inputs = environment.get("input_devices")
    if not isinstance(inputs, list) or not {"keyboard", "mouse"}.issubset(set(inputs)):
        errors.append(context + " input_devices must include keyboard and mouse")
    for key in ("logical_viewport_px", "physical_viewport_px"):
        if not _fullmatch(VIEWPORT_PATTERN, environment.get(key)):
            errors.append(context + " viewport invalid: " + key)
    dpi = environment.get("dpi")
    if not isinstance(dpi, (int, float)) or isinstance(dpi, bool) or dpi <= 0:
        errors.append(context + " dpi must be positive")
    return environment


def _validate_session(session, errors, context):
    if not isinstance(session, dict):
        errors.append(context + " session is missing")
        return
    started = _parse_utc(session.get("started_utc"))
    ended = _parse_utc(session.get("ended_utc"))
    duration = session.get("duration_ms")
    if started is None or ended is None or ended <= started or not isinstance(duration, int) or duration <= 0:
        errors.append(context + " UTC session timing/duration is invalid")
    elif abs((ended - started).total_seconds() * 1000 - duration) > 2000:
        errors.append(context + " duration differs from UTC timestamps by more than two seconds")


def _validate_manual_checks(record, errors, context):
    checks = record.get("manual_checks")
    if not isinstance(checks, list) or [item.get("check_id") for item in checks
                                       if isinstance(item, dict)] != list(MANUAL_CHECK_IDS):
        errors.append(context + " manual checks do not exactly cover the required ordered list")
        return False
    passed = True
    for item in checks:
        result = item.get("result")
        notes = item.get("notes")
        if result not in {"PASS", "FAIL"}:
            errors.append(context + " manual check result invalid: " + str(item.get("check_id")))
        if not isinstance(notes, str) or not notes.strip() or notes == "PENDING" or _contains_pii(notes):
            errors.append(context + " manual check notes missing or contain PII: " + str(item.get("check_id")))
        passed = passed and result == "PASS"
    response = record.get("manual_open_response")
    if not isinstance(response, str) or not response.strip() or response == "PENDING" or _contains_pii(response):
        errors.append(context + " manual open response is missing or contains PII")
    return passed


def _validate_replay(record, environment, errors, context):
    evidence = record.get("replay_evidence")
    if not isinstance(evidence, dict):
        errors.append(context + " replay evidence is missing")
        return False
    for key in ("file_sha256", "content_sha256", "physics_profile_sha256",
                "declared_final_state_sha256", "first_play_final_state_sha256",
                "second_play_final_state_sha256"):
        if not _fullmatch(SHA_PATTERN, evidence.get(key)):
            errors.append(context + " replay SHA invalid: " + key)
    if evidence.get("format") != "billiardgl_replay_v1" or evidence.get("format_version") != 1:
        errors.append(context + " replay format/version is not Replay v1")
    if evidence.get("executable_version") != environment.get("executable_version"):
        errors.append(context + " replay executable binding differs from environment")
    if evidence.get("content_sha256") != record.get("scenario_file_sha256"):
        errors.append(context + " replay content hash differs from manifest scenario file SHA")
    if evidence.get("physics_profile_id") != environment.get("physics_profile_id") or \
            evidence.get("physics_profile_sha256") != environment.get("physics_profile_sha256"):
        errors.append(context + " replay physics profile binding differs from environment")
    if evidence.get("seed") != record.get("attempt_seed"):
        errors.append(context + " replay seed differs from scenario reset contract")
    final_hash = evidence.get("declared_final_state_sha256")
    first = evidence.get("first_play_final_state_sha256")
    second = evidence.get("second_play_final_state_sha256")
    first_matches = first == final_hash
    second_matches = second == final_hash
    if evidence.get("first_play_final_hash_matches") is not first_matches or \
            evidence.get("second_play_final_hash_matches") is not second_matches:
        errors.append(context + " replay match flags differ from final hashes")
    if evidence.get("same_file_replayed_twice") is not True:
        errors.append(context + " replay must be the same saved file played twice")
    rejected = True
    for field, expected_code in REPLAY_REJECTION_CONTRACT.items():
        rejection = evidence.get(field + "_rejection")
        if not isinstance(rejection, dict) or rejection.get("rejected") is not True or \
                rejection.get("error_code") != expected_code:
            errors.append(context + " replay fail-closed result differs: " + field)
            rejected = False
    return first_matches and second_matches and first == second and rejected


def _validate_trajectory(record, errors, context):
    evidence = record.get("trajectory_evidence")
    if not isinstance(evidence, dict):
        errors.append(context + " trajectory evidence is missing")
        return False
    visual_keys = ("predicted_legend_readable", "actual_legend_readable",
                   "tracks_distinguishable_without_color", "predicted_endpoint_readable",
                   "actual_endpoint_readable")
    for key in visual_keys:
        if not isinstance(evidence.get(key), bool):
            errors.append(context + " trajectory human result invalid: " + key)
    for key in ("authority_before_sha256", "authority_after_preview_sha256"):
        if not _fullmatch(SHA_PATTERN, evidence.get(key)):
            errors.append(context + " trajectory authority SHA invalid: " + key)
    unchanged = evidence.get("authority_before_sha256") == evidence.get("authority_after_preview_sha256")
    if evidence.get("preview_authority_unchanged") is not unchanged:
        errors.append(context + " preview authority flag differs from hashes")
    return all(evidence.get(key) is True for key in visual_keys) and unchanged


def _validate_automation(record, errors, context):
    evidence = record.get("automation_binding")
    if not isinstance(evidence, dict) or evidence.get("attached") is not True or \
            evidence.get("success_fixture_passed") is not True or \
            evidence.get("failure_fixture_passed") is not True or \
            evidence.get("substitutes_for_manual_play") is not False or \
            not _fullmatch(SHA_PATTERN, evidence.get("artifact_sha256")):
        errors.append(context + " automation binding is missing, failed, or claims to replace manual play")
        return False
    return True


def _validate_attestations(record, errors, context):
    attestations = record.get("attestations")
    executor_id = record.get("executor", {}).get("executor_id")
    if not isinstance(attestations, dict):
        errors.append(context + " attestations are missing")
        return
    executor = attestations.get("executor", {})
    observer = attestations.get("observer", {})
    observer_id = observer.get("observer_id")
    if executor.get("accepted") is not True or executor.get("signer_code") != executor_id or \
            executor.get("statement_version") != "phase4_manual_play_attestation_v1" or \
            _parse_utc(executor.get("signed_utc")) is None:
        errors.append(context + " executor attestation invalid")
    if observer.get("accepted") is not True or not _fullmatch(OBSERVER_PATTERN, observer_id) or \
            observer.get("statement_version") != "phase4_manual_play_attestation_v1" or \
            _parse_utc(observer.get("signed_utc")) is None:
        errors.append(context + " observer attestation invalid")
    if observer_id == executor_id:
        errors.append(context + " executor and observer cannot self-attest")
    pii = record.get("pii_review", {})
    if pii.get("performed") is not True or pii.get("status") != "CLEAR" or \
            pii.get("reviewer_observer_id") != observer_id:
        errors.append(context + " PII review is missing or not bound to observer")


def _compute_aggregate(records, record_pass):
    manual = sum(value["manual"] for value in record_pass.values())
    replay = sum(value["replay"] for value in record_pass.values())
    trajectory = sum(value["trajectory"] for value in record_pass.values())
    automation = sum(value["automation"] for value in record_pass.values())
    complete = sum(record.get("status") == "COMPLETE" for record in records)
    all_pass = complete == manual == replay == trajectory == automation == 12
    return {
        "status": "PASS" if all_pass else "FAIL",
        "scenario_count": len(records),
        "complete_record_count": complete,
        "manual_play_pass_count": manual,
        "replay_determinism_and_rejection_pass_count": replay,
        "trajectory_and_authority_pass_count": trajectory,
        "automation_binding_pass_count": automation,
        "scenario_results": {
            scenario_id: "PASS" if all(values.values()) else "FAIL"
            for scenario_id, values in record_pass.items()
        },
        "gate_pass": all_pass,
    }


def validate_actual_matrix(value, pack_path: Path = PACK_PATH):
    errors = []
    pending = build_pending_matrix(pack_path)
    if value.get("schema") != "phase4_manual_play_v1" or \
            value.get("matrix_id") != "phase4_launch12_human_manual_play_v1" or \
            value.get("status") not in {"PASS", "FAIL"}:
        errors.append("actual Phase4 matrix identity/status invalid")
    for key in ("protocol_path", "pack_contract", "scenario_contract", "manual_check_contract",
                "replay_rejection_contract", "trajectory_contract", "evidence_boundary"):
        if value.get(key) != pending[key]:
            errors.append("Phase4 matrix contract differs: " + key)
    records = value.get("records")
    if not isinstance(records, list) or len(records) != 12:
        return errors + ["actual Phase4 matrix requires exactly 12 records"], None
    expected = {item["scenario_id"]: item for item in pending["scenario_contract"]}
    actual_ids = [record.get("scenario_id") for record in records if isinstance(record, dict)]
    if len(actual_ids) != 12 or len(set(actual_ids)) != 12 or set(actual_ids) != set(expected):
        return errors + ["manual-play records must exactly cover the 12 manifest scenario IDs"], None
    record_pass = {}
    frozen_builds = set()
    frozen_assets = set()
    frozen_executables = set()
    frozen_profiles = set()
    for index, record in enumerate(records):
        context = "record[{}] {}".format(index, record.get("scenario_id"))
        contract = expected[record["scenario_id"]]
        if any(record.get(key) != value for key, value in contract.items()):
            errors.append(context + " immutable scenario contract differs from manifest")
        if record.get("status") != "COMPLETE":
            errors.append(context + " is not COMPLETE")
        executor = record.get("executor")
        if not isinstance(executor, dict) or not _fullmatch(EXECUTOR_PATTERN, executor.get("executor_id")) or \
                executor.get("real_external_human") is not True or executor.get("project_member") is not False or \
                executor.get("agent_or_robot") is not False:
            errors.append(context + " executor must be an anonymous external real human")
        environment = _validate_environment(record.get("environment"), errors, context) or {}
        _validate_session(record.get("session"), errors, context)
        manual_pass = _validate_manual_checks(record, errors, context)
        replay_pass = _validate_replay(record, environment, errors, context)
        trajectory_pass = _validate_trajectory(record, errors, context)
        automation_pass = _validate_automation(record, errors, context)
        _validate_attestations(record, errors, context)
        record_pass[record["scenario_id"]] = {
            "manual": manual_pass, "replay": replay_pass,
            "trajectory": trajectory_pass, "automation": automation_pass,
        }
        frozen_builds.add(environment.get("build_sha256"))
        frozen_assets.add(environment.get("asset_bundle_sha256"))
        frozen_executables.add(environment.get("executable_version"))
        frozen_profiles.add((environment.get("physics_profile_id"), environment.get("physics_profile_sha256")))
    if len(frozen_builds) != 1 or len(frozen_assets) != 1 or len(frozen_executables) != 1 or len(frozen_profiles) != 1:
        errors.append("all 12 records must use one frozen build/assets/executable/profile")
    aggregate = _compute_aggregate(records, record_pass)
    if value.get("aggregate") != aggregate:
        errors.append("aggregate differs from validator-computed 12-scenario result")
    if value.get("status") != aggregate["status"]:
        errors.append("matrix status differs from computed aggregate")
    return errors, aggregate


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("evidence", type=Path)
    parser.add_argument("--pack", type=Path, default=PACK_PATH)
    parser.add_argument("--pending-template", action="store_true")
    args = parser.parse_args()
    try:
        value = json.loads(args.evidence.read_text(encoding="utf-8"))
        if args.pending_template:
            errors = validate_pending_template(value, args.pack.resolve())
            aggregate = None
        else:
            errors, aggregate = validate_actual_matrix(value, args.pack.resolve())
    except (OSError, json.JSONDecodeError, ValueError) as error:
        print("Phase4 manual-play validation failed: " + str(error), file=sys.stderr)
        return 1
    if errors:
        for error in errors:
            print("ERROR: " + error, file=sys.stderr)
        return 1
    if args.pending_template:
        print("Phase4 human manual-play PENDING matrix valid: records=0/12 result=NOT_RUN")
        return 0
    print("Phase4 human manual-play actual matrix valid: records=12/12 result=" + aggregate["status"])
    return 0 if aggregate["status"] == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
