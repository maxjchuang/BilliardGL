from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from scripts.generate_phase5_beta_qa_templates import (
    AUDIO_DECISIONS, FROZEN_TOKENS, RULE_EVENTS, build_pending_matrix, write_templates,
)
from scripts.validate_phase5_beta_qa import validate_actual_matrix, validate_pending_template


ROOT = Path(__file__).resolve().parents[2]
PENDING = ROOT / "docs/art/phase5-beta-qa-v1.pending.json"


def sha(number: int) -> str:
    return "sha256:" + format(number, "064x")


def finalize(value: dict) -> None:
    _, aggregate = validate_actual_matrix(value)
    value["aggregate"] = aggregate
    value["status"] = aggregate["status"]


def complete_fixture() -> dict:
    value = build_pending_matrix()
    contract = value["source_contract"]
    for index, record in enumerate(value["records"], start=1):
        executor = "P5QA-A" + format(index, "09d")
        observer = "OBS-A" + format(index, "07d")
        record["status"] = "COMPLETE"
        record["executor"] = {"executor_id": executor, "real_external_human": True,
                              "project_member": False, "agent_or_robot": False}
        record["environment"] = {
            "build_sha256": sha(100), "content_sha256": sha(101),
            "physics_profile_id": "wpa_heyball_frozen_v1", "physics_profile_sha256": sha(102),
            "gpu_capture_manifest_sha256": sha(103), "gpu_manual_review_sha256": sha(104),
            "device_code": "DEVICE-A", "operating_system": "test-os", "gpu_device": "test-gpu",
            "input_devices": ["keyboard", "mouse"], "logical_viewport_px": "1920x1080",
            "physical_viewport_px": "1920x1080", "dpi": 96,
            **contract["source_hashes"], **FROZEN_TOKENS,
        }
        record["session"] = {"started_utc": "2026-07-27T09:00:00Z",
                             "ended_utc": "2026-07-27T09:20:00Z", "duration_ms": 1200000}
        for check in record["checks"]:
            check["result"] = "PASS"
            check["notes"] = "Observed by external Beta QA."
        record_id = record["record_id"]
        if record_id == "full_match_product":
            record["evidence"] = {"started_phase": "break", "ended_phase": "complete", "winner": 0,
                                  "physical_shot_count": 15, "match_trace_sha256": sha(200),
                                  "match_replay_sha256": sha(201), "used_scenario_shortcut": False,
                                  "used_state_injection": False}
        elif record_id == "rules_hud_reason_coverage":
            events = []
            for event_index, event in enumerate(RULE_EVENTS):
                text_keys = event["text_keys"]
                if event["event_id"] == "group_assignment":
                    text_keys = ["rules.chinese8.group.solids"]
                events.append({"event_id": event["event_id"], "observed": True,
                               "hud_player_readable": True, "reason_codes": event["reason_codes"],
                               "text_keys": text_keys,
                               "ball_in_hand_visible": event["event_id"] == "ball_in_hand",
                               "turn_handoff_visible": event["event_id"] == "turn_handoff",
                               "screenshot_sha256": sha(300 + event_index)})
            record["evidence"] = {"events": events}
        elif record_id.startswith("ai_"):
            level = record_id.split("_")[1]
            record["evidence"] = {"difficulty_id": level, "product_entry": True,
                                  "challenge_started": True, "challenge_completed": True,
                                  "hidden_physics_interface_used": False,
                                  "main_thread_max_block_ms": 4.0,
                                  "automation_trace_sha256": sha(400 + index)}
        elif record_id == "settings_lifecycle":
            record["evidence"] = {
                "startup_loaded": True, "bad_file_fell_back_to_defaults": True,
                "bad_file_warning_code": "E_SETTINGS_FORMAT", "preview_visible_before_apply": True,
                "apply_committed": True, "cancel_restored_live_values": True,
                "atomic_save_succeeded": True, "saved_file_sha256": sha(500),
                "restart_loaded_same_sha256": sha(500), "context_rebind_succeeded": True,
                "conflict_rejected": True, "conflict_error_code": "E_BINDING_CONFLICT",
                "conflict_rollback_preserved_binding": True}
        elif record_id == "accessibility_and_controls":
            settings = contract["settings"]
            record["evidence"] = {
                "minimum_ui_scale": 0.85, "maximum_ui_scale": 1.35,
                "reachable_cvd_modes": settings["color_vision_modes"],
                "reachable_subtitle_modes": settings["subtitle_modes"],
                "reachable_assist_modes": settings["assist_modes"],
                "reachable_audio_buses": settings["audio_buses"],
                "reachable_display_effects": settings["display_effects"],
                "assist_auto_aimed": False, "assist_auto_shot": False}
        elif record_id in AUDIO_DECISIONS:
            record["evidence"] = {**contract["audio_decisions"][record_id],
                                  "reported_status": "degraded_asset_alias",
                                  "disclosed_to_content_review": True,
                                  "claimed_distinct_asset": False,
                                  "claimed_pitch_supported": False,
                                  "content_disposition": "ACCEPTED_DEGRADED",
                                  "review_notes": "Shared fallback explicitly accepted for this Beta."}
        else:
            record["evidence"] = {"actual_gpu_capture": True, "capture_manifest_status": "PASS",
                                  "manual_review_status": "PASS", "hud_cases_present": True,
                                  "shell_cases_present": True, "opaque_icon_count": 0,
                                  "unexpanded_icon_count": 0, "debug_node_count": 0,
                                  "capture_artifact_sha256": sha(600)}
        record["automation_binding"] = {"attached": True, "artifact_sha256": sha(700),
                                        "passed": True, "substitutes_for_human_qa": False}
        record["attestations"] = {
            "executor": {"accepted": True, "signer_code": executor,
                         "signed_utc": "2026-07-27T09:21:00Z",
                         "statement_version": "phase5_beta_qa_attestation_v1"},
            "observer": {"accepted": True, "observer_id": observer,
                         "signed_utc": "2026-07-27T09:22:00Z",
                         "statement_version": "phase5_beta_qa_attestation_v1"}}
        record["pii_review"] = {"performed": True, "status": "CLEAR",
                                "reviewer_observer_id": observer}
    finalize(value)
    return value


def by_id(value: dict, record_id: str) -> dict:
    return next(record for record in value["records"] if record["record_id"] == record_id)


class Phase5BetaQaTests(unittest.TestCase):
    def test_committed_pending_is_exact_not_run_and_unauthorized(self):
        value = json.loads(PENDING.read_text(encoding="utf-8"))
        self.assertEqual(validate_pending_template(value), [])
        self.assertEqual(value["status"], "NOT_RUN")
        self.assertEqual(value["aggregate"], {
            "status": "NOT_RUN", "product_mode_default_switch_authorized": False})

    def test_generator_is_byte_deterministic(self):
        with tempfile.TemporaryDirectory() as first, tempfile.TemporaryDirectory() as second:
            write_templates(Path(first)); write_templates(Path(second))
            for path in Path(first).iterdir():
                self.assertEqual(path.read_bytes(), (Path(second) / path.name).read_bytes())

    def test_source_contract_has_exact_rules_ai_settings_audio_and_tokens(self):
        contract = build_pending_matrix()["source_contract"]
        self.assertEqual(contract["ruleset_id"], "wpa_heyball_2025_08_16_v1")
        self.assertEqual(len(contract["reason_codes"]), 8)
        self.assertEqual(set(contract["ai_levels"]), {"rookie", "club", "expert"})
        self.assertEqual(contract["settings"]["ui_scale_range"], [0.85, 1.35])
        self.assertEqual(len(contract["audio_decisions"]), 4)

    def test_synthetic_fixture_exercises_pass_and_default_switch_computation(self):
        errors, aggregate = validate_actual_matrix(complete_fixture())
        self.assertEqual(errors, [])
        self.assertEqual(aggregate["passing_record_count"], 12)
        self.assertTrue(aggregate["product_mode_default_switch_authorized"])

    def test_external_human_and_dual_attestation_are_required(self):
        value = complete_fixture()
        value["records"][0]["executor"]["agent_or_robot"] = True
        value["records"][0]["attestations"]["observer"]["accepted"] = False
        errors, _ = validate_actual_matrix(value)
        self.assertTrue(any("external real human" in error for error in errors))
        self.assertTrue(any("observer attestation" in error for error in errors))

    def test_full_match_rejects_scenario_or_state_shortcuts(self):
        value = complete_fixture()
        match = by_id(value, "full_match_product")["evidence"]
        match["used_scenario_shortcut"] = True
        finalize(value)
        errors, aggregate = validate_actual_matrix(value)
        self.assertEqual(errors, [])
        self.assertEqual(aggregate["record_results"]["full_match_product"], "FAIL")
        self.assertFalse(aggregate["product_mode_default_switch_authorized"])

    def test_rules_reason_codes_are_exact_and_fail_closed(self):
        value = complete_fixture()
        events = by_id(value, "rules_hud_reason_coverage")["evidence"]["events"]
        next(item for item in events if item["event_id"] == "wrong_first_contact")["reason_codes"] = ["wrong_ball"]
        errors, _ = validate_actual_matrix(value)
        self.assertTrue(any("reason code differs" in error for error in errors))

    def test_unreadable_rule_hud_computes_fail(self):
        value = complete_fixture()
        by_id(value, "rules_hud_reason_coverage")["evidence"]["events"][0]["hud_player_readable"] = False
        finalize(value)
        errors, aggregate = validate_actual_matrix(value)
        self.assertEqual(errors, [])
        self.assertEqual(aggregate["record_results"]["rules_hud_reason_coverage"], "FAIL")

    def test_all_three_ai_product_levels_reject_hidden_physics_and_stalls(self):
        value = complete_fixture()
        ai = by_id(value, "ai_expert_product_challenge")["evidence"]
        ai["hidden_physics_interface_used"] = True
        ai["main_thread_max_block_ms"] = 25.0
        finalize(value)
        errors, aggregate = validate_actual_matrix(value)
        self.assertEqual(errors, [])
        self.assertEqual(aggregate["record_results"]["ai_expert_product_challenge"], "FAIL")

    def test_settings_error_codes_save_hash_and_conflict_rollback_are_enforced(self):
        value = complete_fixture()
        settings = by_id(value, "settings_lifecycle")["evidence"]
        settings["bad_file_warning_code"] = "generic"
        settings["restart_loaded_same_sha256"] = sha(999)
        errors, _ = validate_actual_matrix(value)
        self.assertTrue(any("stable error code" in error for error in errors))

    def test_accessibility_sets_and_assist_authority_are_enforced(self):
        value = complete_fixture()
        access = by_id(value, "accessibility_and_controls")["evidence"]
        access["reachable_cvd_modes"] = ["default"]
        access["assist_auto_shot"] = True
        errors, _ = validate_actual_matrix(value)
        self.assertTrue(any("reachable set differs" in error for error in errors))

    def test_four_audio_decisions_disclose_degraded_alias_and_no_pitch(self):
        value = complete_fixture()
        for record_id in AUDIO_DECISIONS:
            evidence = by_id(value, record_id)["evidence"]
            self.assertEqual(evidence["reported_status"], "degraded_asset_alias")
            self.assertFalse(evidence["claimed_distinct_asset"])
            self.assertFalse(evidence["claimed_pitch_supported"])

    def test_blocked_audio_disposition_is_valid_fail_not_fake_acceptance(self):
        value = complete_fixture()
        by_id(value, "audio_alias_ambience")["evidence"]["content_disposition"] = "BLOCKED"
        finalize(value)
        errors, aggregate = validate_actual_matrix(value)
        self.assertEqual(errors, [])
        self.assertEqual(aggregate["audio_accepted_degraded_count"], 3)
        self.assertFalse(aggregate["product_mode_default_switch_authorized"])

    def test_audio_cannot_claim_distinct_asset_or_pitch_support(self):
        value = complete_fixture()
        evidence = by_id(value, "audio_alias_notifications")["evidence"]
        evidence["claimed_distinct_asset"] = True
        evidence["claimed_pitch_supported"] = True
        finalize(value)
        errors, aggregate = validate_actual_matrix(value)
        self.assertEqual(errors, [])
        self.assertEqual(aggregate["record_results"]["audio_alias_notifications"], "FAIL")

    def test_actual_gpu_capture_requires_shell_hud_and_zero_opaque_debug(self):
        value = complete_fixture()
        gpu = by_id(value, "actual_gpu_shell_hud")["evidence"]
        gpu["opaque_icon_count"] = 1
        gpu["debug_node_count"] = 1
        finalize(value)
        errors, aggregate = validate_actual_matrix(value)
        self.assertEqual(errors, [])
        self.assertEqual(aggregate["record_results"]["actual_gpu_shell_hud"], "FAIL")

    def test_frozen_build_capture_and_source_binding_mismatch_is_rejected(self):
        value = complete_fixture()
        value["records"][0]["environment"]["build_sha256"] = sha(999)
        value["records"][1]["environment"]["rules_source_sha256"] = sha(998)
        errors, _ = validate_actual_matrix(value)
        self.assertTrue(any("one frozen" in error for error in errors))
        self.assertTrue(any("source/token" in error for error in errors))

    def test_automation_or_model_test_cannot_replace_human_qa(self):
        value = complete_fixture()
        value["records"][0]["automation_binding"]["substitutes_for_human_qa"] = True
        errors, _ = validate_actual_matrix(value)
        self.assertTrue(any("cannot substitute" in error for error in errors))

    def test_pii_and_premature_aggregate_are_rejected(self):
        value = complete_fixture()
        value["records"][0]["checks"][0]["notes"] = "my name is private"
        value["aggregate"]["product_mode_default_switch_authorized"] = False
        errors, _ = validate_actual_matrix(value)
        self.assertTrue(any("PII" in error for error in errors))
        self.assertTrue(any("authorization differs" in error for error in errors))

    def test_pending_cli_reports_not_run_and_unauthorized(self):
        result = subprocess.run(
            [sys.executable, str(ROOT / "scripts/validate_phase5_beta_qa.py"),
             str(PENDING), "--pending-template"], cwd=ROOT, text=True,
            capture_output=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("records=0/12 result=NOT_RUN default_switch=UNAUTHORIZED", result.stdout)


if __name__ == "__main__":
    unittest.main()
