#!/usr/bin/env python3
"""Validate the Phase5 v4 external real-input/accessibility gate and template."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GATE = ROOT / (
    "docs/art/phase5-rules-decision-ui-v4-real-input-"
    "accessibility-external-gate-v1.json"
)
DEFAULT_TEMPLATE = ROOT / (
    "docs/art/phase5-rules-decision-ui-v4-real-input-"
    "accessibility-session-template-v1.json"
)
GATE_SHA256 = (
    "7a17d0488272466bc93e169aad1b33fbde88224be4ec679861d19b7ef61f7db8"
)
V3_MANUAL_PATH = (
    "docs/evidence/phase5/rules-decision-ui-v3-binding-corrected-"
    "targeted28-independent-ta-manual-review-v1.json"
)
V3_MANUAL_SHA256 = (
    "f044e39c7765f3026bd53521e7cca8cf11b619ca5e6369c6e37e338da0afb954"
)
V4_GATE_PATH = (
    "docs/art/phase5-rules-decision-ui-v4-exact-shape-"
    "correction-design-gate-v1.json"
)
V4_GATE_SHA256 = (
    "f93ca59634875ed5256f5c22678a55817e0f3e8bf8df582b3b04ac527b886051"
)
SUBJECT_IDS = [f"P{number:02d}" for number in range(1, 13)]
DEVICE_IDS = [f"D{number:02d}" for number in range(1, 5)]
SCREEN_READERS = ["VoiceOver", "NVDA"]
CATEGORY_COUNTS = {
    "keyboard_only": 7,
    "pointer_only": 7,
    "bilingual_copy": 4,
    "standard_cvd": 4,
    "screen_reader": 8,
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(value, dict), f"{path}: object required")
    return value


def bound_file(relative: Any, digest: Any, label: str) -> Path:
    require(isinstance(relative, str) and relative
            and not Path(relative).is_absolute(),
            label + ": repository-relative path required")
    require(isinstance(digest, str) and len(digest) == 64
            and all(character in "0123456789abcdef" for character in digest),
            label + ": lowercase SHA-256 required")
    try:
        path = (ROOT / relative).resolve(strict=True)
        path.relative_to(ROOT.resolve())
    except (OSError, ValueError) as error:
        raise RuntimeError(label + ": missing/outside repository") from error
    require(path.is_file() and sha256(path) == digest,
            label + ": byte binding mismatch")
    return path


def expected_ta_prerequisite() -> dict[str, Any]:
    return {
        "status": "NOT_AVAILABLE",
        "manual_path": None,
        "manual_sha256": None,
        "required_schema":
            "phase5_rules_decision_ui_v4_targeted16_"
            "independent_ta_manual_review_v1",
        "required_result": "PASS",
        "required_cases_reviewed": 16,
        "required_axes_reviewed": 160,
        "required_failed_axes": 0,
    }


def validate_gate(gate: dict[str, Any]) -> list[dict[str, Any]]:
    require(gate.get("schema") ==
            "phase5_rules_decision_ui_v4_real_input_"
            "accessibility_external_gate_v1"
            and gate.get("schema_version") == 1
            and gate.get("status") ==
            "EXTERNAL_SESSION_PROTOCOL_APPROVED_NOT_RUN",
            "external gate schema/version/status")
    require(gate.get("read_only_bindings") == [
        {"role": "v3_corrected_targeted28_final_manual",
         "path": V3_MANUAL_PATH, "sha256": V3_MANUAL_SHA256},
        {"role": "v4_exact_shape_correction_design_gate",
         "path": V4_GATE_PATH, "sha256": V4_GATE_SHA256},
    ], "external gate exact frozen bindings")
    manual = load_json(bound_file(
        V3_MANUAL_PATH, V3_MANUAL_SHA256, "v3 manual"))
    require(manual.get("overall_result") == "FAIL"
            and manual.get("review_counts", {}).get("axes_fail") == 8,
            "v3 manual failure identity")
    v4 = load_json(bound_file(V4_GATE_PATH, V4_GATE_SHA256, "v4 gate"))
    require(v4.get("status") ==
            "APPROVED_FOR_SCOPED_EXACT_SHAPE_IMPLEMENTATION_ONLY"
            and v4.get("future_targeted_actual_gpu_gate", {}).get(
                "minimum_case_count") == 16
            and v4.get("technical_art_re_review_gate", {}).get(
                "axis_review_count") == 160,
            "v4 targeted16/TA160 prerequisite contract")
    prerequisite = gate.get("future_targeted16_ta_prerequisite", {})
    expected = expected_ta_prerequisite()
    require({key: prerequisite.get(key) for key in expected} == expected
            and "does not reference or predict a nonexistent SHA" in
            prerequisite.get("policy", ""),
            "future TA prerequisite must remain unbound")
    participants = gate.get("participant_contract", {})
    require(participants.get("required_unique_subject_count") == 12
            and participants.get("locale_counts") == {"en": 6, "zh_cn": 6}
            and participants.get("minimum_real_input_session_count") == 16
            and participants.get("minimum_screen_reader_session_count") == 4,
            "participant/session counts")
    cohorts = participants.get("cohorts", [])
    require(len(cohorts) == 3
            and cohorts[0].get("subject_slots") == SUBJECT_IDS[:4]
            and cohorts[1].get("subject_slots") == SUBJECT_IDS[4:8]
            and cohorts[2].get("subject_slots") == SUBJECT_IDS[8:]
            and cohorts[0].get("required_cvd_types") == ["protan", "deutan"]
            and cohorts[1].get("required_cvd_types") == ["protan", "deutan"]
            and cohorts[2].get(
                "minimum_regular_screen_reader_use_months") == 12,
            "participant cohort composition")
    device = gate.get("device_contract", {})
    require(device.get("required_distinct_physical_device_count") == 4
            and [item.get("device_slot") for item in
                 device.get("device_slots", [])] == DEVICE_IDS,
            "four physical device slots")
    screen = gate.get("screen_reader_contract", {})
    require(screen.get("required_distinct_screen_reader_family_count") == 2
            and [item.get("family") for item in
                 screen.get("screen_reader_families", [])] == SCREEN_READERS
            and all(item.get("required_locale_sessions") == ["en", "zh_cn"]
                    for item in screen.get("screen_reader_families", [])),
            "VoiceOver/NVDA bilingual family coverage")
    catalog = gate.get("protocol_catalog", [])
    require(isinstance(catalog, list) and len(catalog) == 30
            and len({item.get("item_id") for item in catalog}) == 30,
            "exact 30 unique protocol items")
    actual_counts: dict[str, int] = {}
    for item in catalog:
        category = item.get("category")
        actual_counts[category] = actual_counts.get(category, 0) + 1
        required = item.get("required_subject_slots", [])
        require(required and all(slot in SUBJECT_IDS for slot in required)
                and isinstance(item.get("acceptance"), str)
                and len(item["acceptance"]) >= 50,
                f"{item.get('item_id')}: subject/acceptance contract")
    require(actual_counts == CATEGORY_COUNTS
            and gate.get("protocol_counts") == {
                "keyboard_items": 7, "pointer_items": 7,
                "bilingual_copy_items": 4, "standard_cvd_items": 4,
                "screen_reader_items": 8, "total_items": 30},
            "protocol category counts")
    result_contract = gate.get("item_result_contract", {})
    require(result_contract.get("allowed_statuses") == [
                "PASS", "FAIL", "NOT_RUN"]
            and all(term in result_contract.get("pass_policy", "")
                    for term in ("Every required subject", "No averaging",
                                 "inherited machine/TA PASS"))
            and "overall external gate FAIL" in
            result_contract.get("fail_policy", "")
            and "prevents overall PASS" in
            result_contract.get("not_run_policy", ""),
            "fail-closed item result policy")
    independence = gate.get("independence_and_privacy", {})
    require(independence.get(
                "implementation_team_members_allowed_as_subjects") is False
            and independence.get(
                "technical_art_reviewer_allowed_as_subject") is False
            and independence.get("external_moderator_required") is True
            and independence.get("pseudonymous_subject_codes_only") is True,
            "independence/privacy contract")
    require(gate.get("evidence_boundary") == {
        "design_protocol_only": True,
        "future_targeted16_ta_pass_bound": False,
        "participant_recruitment_performed": False,
        "real_keyboard_sessions_performed": 0,
        "real_pointer_sessions_performed": 0,
        "cvd_human_sessions_performed": 0,
        "screen_reader_sessions_performed": 0,
        "external_human_review_pass": False,
        "product_source_modified": False,
        "gpu_executed": False,
        "authority_or_rules_modified": False,
        "threshold_modified": False,
        "product_exposed": False,
        "product_pass": False,
        "golden_promotion": False,
        "m2_promotion": False,
        "beta_pass": False,
    }, "gate design-only/no-session/no-exposure boundary")
    return catalog


def validate_template_common(template: dict[str, Any],
                             catalog: list[dict[str, Any]]) -> None:
    require(template.get("schema") ==
            "phase5_rules_decision_ui_v4_real_input_"
            "accessibility_session_template_v1"
            and template.get("schema_version") == 1,
            "session template schema/version")
    require(template.get("external_gate_binding") == {
        "path":
            "docs/art/phase5-rules-decision-ui-v4-real-input-"
            "accessibility-external-gate-v1.json",
        "sha256": GATE_SHA256,
    }, "session template gate binding")
    participants = template.get("participant_slots", [])
    require(len(participants) == 12
            and [item.get("slot_id") for item in participants] == SUBJECT_IDS
            and [item.get("locale") for item in participants].count("en") == 6
            and [item.get("locale") for item in participants].count("zh_cn") == 6,
            "template exact 12 subject slots/locales")
    require([item.get("vision_profile") for item in participants].count(
                "cvd_protan") == 2
            and [item.get("vision_profile") for item in participants].count(
                "cvd_deutan") == 2
            and [item.get("vision_profile") for item in participants].count(
                "regular_screen_reader_user") == 4,
            "template CVD/screen-reader subject slots")
    devices = template.get("device_slots", [])
    require(len(devices) == 4
            and [item.get("slot_id") for item in devices] == DEVICE_IDS,
            "template exact four device slots")
    readers = template.get("screen_reader_configuration_slots", [])
    require(len(readers) == 2
            and [item.get("family") for item in readers] == SCREEN_READERS,
            "template exact two screen-reader families")
    results = template.get("protocol_results", [])
    require(len(results) == 30
            and [item.get("item_id") for item in results] ==
            [item["item_id"] for item in catalog],
            "template exact protocol item order")


def validate_template_not_run(template: dict[str, Any]) -> dict[str, Any]:
    require(template.get("status") ==
            "TEMPLATE_NOT_RUN_WAITING_FOR_TARGETED16_TA_PASS"
            and template.get("overall_result") == "NOT_RUN"
            and template.get("future_targeted16_ta_pass_binding") ==
            expected_ta_prerequisite(),
            "template NOT_RUN/future TA unbound")
    for participant in template["participant_slots"]:
        require(participant.get("status") == "NOT_RUN"
                and participant.get("future_subject_code") is None
                and participant.get("future_consent_receipt_sha256") is None,
                participant["slot_id"] + ": participant must be unfilled")
    for device in template["device_slots"]:
        require(device.get("status") == "NOT_RUN"
                and all(device.get(field) is None for field in (
                    "future_device_code", "future_manufacturer_model",
                    "future_physical_display", "future_os_build",
                    "future_input_hardware", "future_app_binary_sha256",
                    "future_configuration_sha256")),
                device["slot_id"] + ": device must be unfilled")
    for reader in template["screen_reader_configuration_slots"]:
        require(reader.get("status") == "NOT_RUN"
                and all(reader.get(field) is None for field in (
                    "future_version_build",
                    "future_synthesizer_voice_by_locale",
                    "future_verbosity_and_mode",
                    "future_configuration_sha256")),
                reader["family"] + ": screen reader must be unfilled")
    for result in template["protocol_results"]:
        require(result == {
            "item_id": result["item_id"], "status": "NOT_RUN",
            "future_per_subject_results": [], "evidence_refs": [],
            "moderator_observation": None},
            result["item_id"] + ": item must remain NOT_RUN/unfilled")
    require(template.get("session_counts") == {
        "participant_slots_total": 12,
        "participant_slots_not_run": 12,
        "physical_device_slots_total": 4,
        "physical_device_slots_not_run": 4,
        "screen_reader_family_slots_total": 2,
        "screen_reader_family_slots_not_run": 2,
        "required_real_input_sessions": 16,
        "completed_real_input_sessions": 0,
        "required_screen_reader_sessions": 4,
        "completed_screen_reader_sessions": 0,
        "protocol_items_total": 30,
        "protocol_items_pass": 0,
        "protocol_items_fail": 0,
        "protocol_items_not_run": 30,
    } and template.get("failed_item_ids") == []
       and template.get("future_session_evidence_manifest") == {
           "path": None, "sha256": None,
           "stable_aggregate_sha256": None},
       "template counts/evidence must be zero/unbound")
    require(template.get("evidence_boundary") == {
        "template_only": True,
        "future_targeted16_ta_pass_bound": False,
        "participant_recruitment_performed": False,
        "real_keyboard_sessions_performed": 0,
        "real_pointer_sessions_performed": 0,
        "cvd_human_sessions_performed": 0,
        "screen_reader_sessions_performed": 0,
        "external_human_review_pass": False,
        "product_source_modified": False,
        "gpu_executed": False,
        "authority_or_rules_modified": False,
        "threshold_modified": False,
        "product_exposed": False,
        "product_pass": False,
        "golden_promotion": False,
        "m2_promotion": False,
        "beta_pass": False,
    }, "template no-session/no-exposure boundary")
    return {
        "status": "PASS_EXTERNAL_SESSION_TEMPLATE_NOT_RUN",
        "mode": "TEMPLATE",
        "participants": 12,
        "devices": 4,
        "screen_reader_families": 2,
        "protocol_items": 30,
        "items_not_run": 30,
        "targeted16_ta_pass_bound": False,
        "external_human_review_pass": False,
    }


def validate_completed(template: dict[str, Any],
                       catalog: list[dict[str, Any]]) -> dict[str, Any]:
    binding = template.get("future_targeted16_ta_pass_binding", {})
    require(binding.get("status") == "BOUND_TARGETED16_TA_PASS"
            and binding.get("required_schema") ==
            expected_ta_prerequisite()["required_schema"]
            and binding.get("required_result") == "PASS"
            and binding.get("required_cases_reviewed") == 16
            and binding.get("required_axes_reviewed") == 160
            and binding.get("required_failed_axes") == 0,
            "completed sessions require targeted16 TA PASS binding")
    ta_path = bound_file(binding.get("manual_path"),
                         binding.get("manual_sha256"), "targeted16 TA manual")
    ta = load_json(ta_path)
    require(ta.get("schema") == binding["required_schema"]
            and ta.get("overall_result") == "PASS"
            and ta.get("review_counts", {}).get("cases_reviewed") == 16
            and ta.get("review_counts", {}).get("axes_total") == 160
            and ta.get("review_counts", {}).get("axes_fail") == 0,
            "targeted16 TA manual exact PASS/counts")
    subject_codes = []
    for participant in template["participant_slots"]:
        require(participant.get("status") == "COMPLETED"
                and isinstance(participant.get("future_subject_code"), str)
                and participant["future_subject_code"].startswith("EXT-")
                and isinstance(participant.get(
                    "future_consent_receipt_sha256"), str)
                and len(participant["future_consent_receipt_sha256"]) == 64,
                participant["slot_id"] + ": completed pseudonymous subject")
        subject_codes.append(participant["future_subject_code"])
    require(len(set(subject_codes)) == 12,
            "completed subjects must be 12 unique codes")
    device_codes = []
    for device in template["device_slots"]:
        require(device.get("status") == "VERIFIED"
                and all(isinstance(device.get(field), str)
                        and device[field] for field in (
                            "future_device_code", "future_manufacturer_model",
                            "future_physical_display", "future_os_build",
                            "future_input_hardware", "future_app_binary_sha256",
                            "future_configuration_sha256")),
                device["slot_id"] + ": completed device evidence")
        device_codes.append(device["future_device_code"])
    require(len(set(device_codes)) == 4,
            "four distinct physical devices required")
    for reader in template["screen_reader_configuration_slots"]:
        require(reader.get("status") == "VERIFIED"
                and all(reader.get(field) for field in (
                    "future_version_build",
                    "future_synthesizer_voice_by_locale",
                    "future_verbosity_and_mode",
                    "future_configuration_sha256")),
                reader["family"] + ": completed screen-reader configuration")
    evidence = template.get("future_session_evidence_manifest", {})
    evidence_path = bound_file(evidence.get("path"), evidence.get("sha256"),
                               "external session evidence manifest")
    evidence_manifest = load_json(evidence_path)
    require(evidence_manifest.get("schema") ==
            "phase5_rules_decision_ui_v4_external_session_evidence_v1"
            and evidence_manifest.get("subject_count") == 12
            and evidence_manifest.get("device_count") == 4
            and evidence_manifest.get("screen_reader_family_count") == 2
            and evidence_manifest.get("protocol_item_count") == 30
            and evidence_manifest.get("stable_aggregate_sha256") ==
            evidence.get("stable_aggregate_sha256"),
            "external evidence manifest identity/count/aggregate")
    catalog_by_id = {item["item_id"]: item for item in catalog}
    item_pass = item_fail = 0
    failed_items = []
    for result in template["protocol_results"]:
        item_id = result["item_id"]
        required_subjects = catalog_by_id[item_id]["required_subject_slots"]
        per_subject = result.get("future_per_subject_results", [])
        require([item.get("slot_id") for item in per_subject] ==
                required_subjects
                and result.get("status") in ("PASS", "FAIL")
                and isinstance(result.get("moderator_observation"), str)
                and len(result["moderator_observation"].strip()) >= 30
                and isinstance(result.get("evidence_refs"), list)
                and result["evidence_refs"],
                item_id + ": completed item evidence/results")
        subject_statuses = []
        for subject in per_subject:
            require(subject.get("status") in ("PASS", "FAIL")
                    and isinstance(subject.get("observation"), str)
                    and len(subject["observation"].strip()) >= 20
                    and subject.get("evidence_refs"),
                    item_id + ": per-subject PASS/FAIL/evidence")
            subject_statuses.append(subject["status"])
        expected_status = (
            "PASS" if all(status == "PASS" for status in subject_statuses)
            else "FAIL")
        require(result["status"] == expected_status,
                item_id + ": item/per-subject status consistency")
        if expected_status == "PASS":
            item_pass += 1
        else:
            item_fail += 1
            failed_items.append(item_id)
    overall = template.get("overall_result")
    require(overall in ("PASS", "FAIL")
            and template.get("status") ==
            ("EXTERNAL_SESSION_REVIEW_PASS" if overall == "PASS"
             else "EXTERNAL_SESSION_REVIEW_FAIL")
            and (overall == "PASS") == (item_fail == 0),
            "completed overall/item consistency")
    require(template.get("session_counts") == {
        "participant_slots_total": 12,
        "participant_slots_not_run": 0,
        "physical_device_slots_total": 4,
        "physical_device_slots_not_run": 0,
        "screen_reader_family_slots_total": 2,
        "screen_reader_family_slots_not_run": 0,
        "required_real_input_sessions": 16,
        "completed_real_input_sessions": 16,
        "required_screen_reader_sessions": 4,
        "completed_screen_reader_sessions": 4,
        "protocol_items_total": 30,
        "protocol_items_pass": item_pass,
        "protocol_items_fail": item_fail,
        "protocol_items_not_run": 0,
    } and template.get("failed_item_ids") == failed_items,
       "completed session counts/failures")
    require(template.get("evidence_boundary") == {
        "template_only": False,
        "future_targeted16_ta_pass_bound": True,
        "participant_recruitment_performed": True,
        "real_keyboard_sessions_performed": 8,
        "real_pointer_sessions_performed": 8,
        "cvd_human_sessions_performed": 8,
        "screen_reader_sessions_performed": 4,
        "external_human_review_pass": overall == "PASS",
        "product_source_modified": False,
        "gpu_executed": False,
        "authority_or_rules_modified": False,
        "threshold_modified": False,
        "product_exposed": False,
        "product_pass": False,
        "golden_promotion": False,
        "m2_promotion": False,
        "beta_pass": False,
    }, "completed external evidence/exposure boundary")
    return {
        "status": "PASS_COMPLETED_EXTERNAL_SESSION_REVIEW_STRUCTURE",
        "mode": "COMPLETED", "overall_result": overall,
        "participants": 12, "devices": 4,
        "screen_reader_families": 2,
        "protocol_items": 30, "items_pass": item_pass,
        "items_fail": item_fail,
        "external_human_review_pass": overall == "PASS",
    }


def validate(gate_path: Path = DEFAULT_GATE,
             template_path: Path = DEFAULT_TEMPLATE) -> dict[str, Any]:
    gate_path = gate_path.resolve(strict=True)
    template_path = template_path.resolve(strict=True)
    require(sha256(gate_path) == GATE_SHA256,
            "external gate frozen SHA drift")
    gate = load_json(gate_path)
    catalog = validate_gate(gate)
    template = load_json(template_path)
    validate_template_common(template, catalog)
    if template.get("status") == (
            "TEMPLATE_NOT_RUN_WAITING_FOR_TARGETED16_TA_PASS"):
        result = validate_template_not_run(template)
    else:
        result = validate_completed(template, catalog)
    result["gate_sha256"] = GATE_SHA256
    result["template_sha256"] = sha256(template_path)
    result["product_exposed"] = False
    result["product_pass"] = False
    result["golden_promotion"] = False
    result["m2_promotion"] = False
    result["beta_pass"] = False
    result["gpu_executed"] = False
    result["production_modified"] = False
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--gate", type=Path, default=DEFAULT_GATE)
    parser.add_argument("--template", type=Path, default=DEFAULT_TEMPLATE)
    arguments = parser.parse_args()
    try:
        print(json.dumps(validate(arguments.gate, arguments.template),
                         indent=2, sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
