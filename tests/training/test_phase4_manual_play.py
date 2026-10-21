from __future__ import annotations

import copy
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from scripts.generate_phase4_manual_play_templates import (
    MANUAL_CHECK_IDS,
    PACK_PATH,
    build_pending_matrix,
    write_templates,
)
from scripts.validate_phase4_manual_play import (
    _compute_aggregate,
    validate_actual_matrix,
    validate_pending_template,
)


ROOT = Path(__file__).resolve().parents[2]
PENDING = ROOT / "docs/art/phase4-manual-play-v1.pending.json"


def sha(value: int) -> str:
    return "sha256:" + format(value, "064x")


def recompute(value: dict) -> None:
    results = {}
    for record in value["records"]:
        manual = all(item["result"] == "PASS" for item in record["manual_checks"])
        replay = (
            record["replay_evidence"]["first_play_final_state_sha256"] ==
            record["replay_evidence"]["declared_final_state_sha256"] ==
            record["replay_evidence"]["second_play_final_state_sha256"]
            and all(record["replay_evidence"][field]["rejected"] for field in (
                "wrong_content_hash_rejection", "wrong_profile_id_rejection",
                "wrong_profile_hash_rejection"))
        )
        trajectory = all(record["trajectory_evidence"][key] is True for key in (
            "predicted_legend_readable", "actual_legend_readable",
            "tracks_distinguishable_without_color", "predicted_endpoint_readable",
            "actual_endpoint_readable", "preview_authority_unchanged"))
        automation = record["automation_binding"]["attached"] is True and \
            record["automation_binding"]["success_fixture_passed"] is True and \
            record["automation_binding"]["failure_fixture_passed"] is True and \
            record["automation_binding"]["substitutes_for_manual_play"] is False
        results[record["scenario_id"]] = {
            "manual": manual, "replay": replay,
            "trajectory": trajectory, "automation": automation,
        }
    value["aggregate"] = _compute_aggregate(value["records"], results)
    value["status"] = value["aggregate"]["status"]


def complete_fixture() -> dict:
    value = build_pending_matrix()
    for index, record in enumerate(value["records"], start=1):
        executor_id = "P4QA-A" + format(index, "09d")
        observer_id = "OBS-A" + format(index, "07d")
        record["status"] = "COMPLETE"
        record["executor"] = {
            "executor_id": executor_id,
            "real_external_human": True,
            "project_member": False,
            "agent_or_robot": False,
        }
        record["environment"] = {
            "build_sha256": sha(100),
            "asset_bundle_sha256": sha(101),
            "executable_version": "phase4-manual-test-build-v1",
            "physics_profile_id": "wpa_heyball_frozen_v1",
            "physics_profile_sha256": sha(102),
            "device_code": "DEVICE-A",
            "operating_system": "test-os",
            "gpu_device": "test-gpu",
            "input_devices": ["keyboard", "mouse"],
            "logical_viewport_px": "1920x1080",
            "physical_viewport_px": "1920x1080",
            "dpi": 96,
        }
        record["session"] = {
            "started_utc": "2026-07-27T08:00:00Z",
            "ended_utc": "2026-07-27T08:10:00Z",
            "duration_ms": 600000,
        }
        record["manual_checks"] = [
            {"check_id": check_id, "result": "PASS", "notes": "Observed by external QA."}
            for check_id in MANUAL_CHECK_IDS
        ]
        record["manual_open_response"] = "The exercise flow and outcome were understandable."
        final = sha(1000 + index)
        record["replay_evidence"] = {
            "file_sha256": sha(2000 + index),
            "format": "billiardgl_replay_v1",
            "format_version": 1,
            "executable_version": record["environment"]["executable_version"],
            "content_sha256": record["scenario_file_sha256"],
            "physics_profile_id": record["environment"]["physics_profile_id"],
            "physics_profile_sha256": record["environment"]["physics_profile_sha256"],
            "seed": record["attempt_seed"],
            "declared_final_state_sha256": final,
            "first_play_final_state_sha256": final,
            "second_play_final_state_sha256": final,
            "first_play_final_hash_matches": True,
            "second_play_final_hash_matches": True,
            "same_file_replayed_twice": True,
            "wrong_content_hash_rejection": {
                "rejected": True, "error_code": "replay_content_hash_mismatch"},
            "wrong_profile_id_rejection": {
                "rejected": True, "error_code": "replay_profile_id_mismatch"},
            "wrong_profile_hash_rejection": {
                "rejected": True, "error_code": "replay_profile_hash_mismatch"},
        }
        record["trajectory_evidence"] = {
            "predicted_legend_readable": True,
            "actual_legend_readable": True,
            "tracks_distinguishable_without_color": True,
            "predicted_endpoint_readable": True,
            "actual_endpoint_readable": True,
            "authority_before_sha256": sha(3000 + index),
            "authority_after_preview_sha256": sha(3000 + index),
            "preview_authority_unchanged": True,
        }
        record["automation_binding"] = {
            "attached": True,
            "artifact_sha256": sha(4000),
            "success_fixture_passed": True,
            "failure_fixture_passed": True,
            "substitutes_for_manual_play": False,
        }
        record["attestations"] = {
            "executor": {
                "accepted": True, "signer_code": executor_id,
                "signed_utc": "2026-07-27T08:11:00Z",
                "statement_version": "phase4_manual_play_attestation_v1",
            },
            "observer": {
                "accepted": True, "observer_id": observer_id,
                "signed_utc": "2026-07-27T08:12:00Z",
                "statement_version": "phase4_manual_play_attestation_v1",
            },
        }
        record["pii_review"] = {
            "performed": True, "status": "CLEAR", "reviewer_observer_id": observer_id}
    recompute(value)
    return value


class Phase4ManualPlayTests(unittest.TestCase):
    def test_committed_pending_matrix_is_exact_not_run(self):
        value = json.loads(PENDING.read_text(encoding="utf-8"))
        self.assertEqual(validate_pending_template(value), [])
        self.assertEqual(value["status"], "NOT_RUN")
        self.assertEqual(value["aggregate"], {"status": "NOT_RUN"})
        self.assertTrue(all(record["status"] == "PENDING" for record in value["records"]))

    def test_generator_is_byte_deterministic(self):
        with tempfile.TemporaryDirectory() as first, tempfile.TemporaryDirectory() as second:
            write_templates(Path(first))
            write_templates(Path(second))
            for path in Path(first).iterdir():
                self.assertEqual(path.read_bytes(), (Path(second) / path.name).read_bytes())

    def test_exact_manifest_ids_and_category_counts_are_bound(self):
        value = build_pending_matrix()
        manifest = json.loads(PACK_PATH.read_text(encoding="utf-8"))
        self.assertEqual([item["scenario_id"] for item in value["scenario_contract"]],
                         [item["id"] for item in manifest["scenarios"]])
        self.assertEqual(value["pack_contract"]["category_counts"], {
            "stun_follow_draw": 3, "side_spin_cushion": 3, "pocket_angle": 2,
            "continuous_position": 2, "break_cue_control": 2})

    def test_synthetic_complete_fixture_exercises_pass_computation(self):
        errors, aggregate = validate_actual_matrix(complete_fixture())
        self.assertEqual(errors, [])
        self.assertEqual(aggregate["status"], "PASS")
        self.assertEqual(aggregate["complete_record_count"], 12)

    def test_missing_duplicate_or_foreign_scenario_is_rejected(self):
        value = complete_fixture()
        value["records"][0]["scenario_id"] = value["records"][1]["scenario_id"]
        errors, _ = validate_actual_matrix(value)
        self.assertTrue(any("exactly cover" in error for error in errors))

    def test_manifest_contract_mutation_is_rejected(self):
        value = complete_fixture()
        value["records"][0]["category"] = "invented_category"
        errors, _ = validate_actual_matrix(value)
        self.assertTrue(any("immutable scenario contract" in error for error in errors))

    def test_agent_project_member_or_nonhuman_executor_is_rejected(self):
        for field, bad in (("agent_or_robot", True), ("project_member", True),
                           ("real_external_human", False)):
            value = complete_fixture()
            value["records"][0]["executor"][field] = bad
            errors, _ = validate_actual_matrix(value)
            self.assertTrue(any("external real human" in error for error in errors))

    def test_missing_attestation_and_pii_are_rejected(self):
        value = complete_fixture()
        value["records"][0]["attestations"]["observer"]["accepted"] = False
        value["records"][0]["manual_open_response"] = "my name is private"
        errors, _ = validate_actual_matrix(value)
        self.assertTrue(any("observer attestation" in error for error in errors))
        self.assertTrue(any("PII" in error for error in errors))

    def test_keyboard_mouse_viewport_and_frozen_build_are_enforced(self):
        value = complete_fixture()
        value["records"][0]["environment"]["input_devices"] = ["gamepad"]
        value["records"][1]["environment"]["logical_viewport_px"] = "wide"
        value["records"][2]["environment"]["build_sha256"] = sha(999)
        errors, _ = validate_actual_matrix(value)
        self.assertTrue(any("keyboard and mouse" in error for error in errors))
        self.assertTrue(any("viewport invalid" in error for error in errors))
        self.assertTrue(any("one frozen build" in error for error in errors))

    def test_manual_failure_computes_fail_instead_of_fabricating_pass(self):
        value = complete_fixture()
        value["records"][0]["manual_checks"][0]["result"] = "FAIL"
        recompute(value)
        errors, aggregate = validate_actual_matrix(value)
        self.assertEqual(errors, [])
        self.assertEqual(aggregate["status"], "FAIL")
        self.assertEqual(aggregate["manual_play_pass_count"], 11)

    def test_replay_content_profile_seed_and_two_runs_are_enforced(self):
        value = complete_fixture()
        replay = value["records"][0]["replay_evidence"]
        replay["content_sha256"] = sha(6000)
        replay["seed"] = 99
        replay["second_play_final_state_sha256"] = sha(6001)
        replay["second_play_final_hash_matches"] = False
        errors, _ = validate_actual_matrix(value)
        self.assertTrue(any("content hash" in error for error in errors))
        self.assertTrue(any("seed differs" in error for error in errors))

    def test_wrong_profile_and_hash_must_fail_closed_with_exact_codes(self):
        value = complete_fixture()
        rejection = value["records"][0]["replay_evidence"]["wrong_profile_hash_rejection"]
        rejection["rejected"] = False
        rejection["error_code"] = "generic_error"
        errors, _ = validate_actual_matrix(value)
        self.assertTrue(any("fail-closed" in error for error in errors))

    def test_trajectory_legends_and_preview_authority_are_enforced(self):
        value = complete_fixture()
        trajectory = value["records"][0]["trajectory_evidence"]
        trajectory["actual_legend_readable"] = False
        trajectory["authority_after_preview_sha256"] = sha(7000)
        trajectory["preview_authority_unchanged"] = False
        recompute(value)
        errors, aggregate = validate_actual_matrix(value)
        self.assertEqual(errors, [])
        self.assertEqual(aggregate["trajectory_and_authority_pass_count"], 11)
        self.assertEqual(aggregate["status"], "FAIL")

    def test_automation_cannot_replace_manual_play(self):
        value = complete_fixture()
        value["records"][0]["automation_binding"]["substitutes_for_manual_play"] = True
        errors, _ = validate_actual_matrix(value)
        self.assertTrue(any("claims to replace manual play" in error for error in errors))

    def test_premature_aggregate_invalidates_pending_and_claimed_actual(self):
        pending = build_pending_matrix()
        pending["aggregate"] = {"status": "PASS"}
        self.assertNotEqual(validate_pending_template(pending), [])
        actual = complete_fixture()
        actual["aggregate"]["manual_play_pass_count"] = 0
        errors, _ = validate_actual_matrix(actual)
        self.assertTrue(any("aggregate differs" in error for error in errors))

    def test_pending_cli_reports_zero_of_twelve_and_not_run(self):
        result = subprocess.run(
            [sys.executable, str(ROOT / "scripts/validate_phase4_manual_play.py"),
             str(PENDING), "--pending-template"],
            cwd=ROOT, text=True, capture_output=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("records=0/12 result=NOT_RUN", result.stdout)


if __name__ == "__main__":
    unittest.main()
