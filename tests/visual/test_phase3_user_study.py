import copy
from datetime import datetime, timedelta, timezone
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
DOCS = ROOT / "docs/art"
PENDING = DOCS / "phase3-user-study-v1.pending.json"
STUDY_SCHEMA = DOCS / "phase3-user-study-v1.schema.json"
PARTICIPANT_SCHEMA = DOCS / "phase3-user-study-participant-v1.schema.json"
GENERATOR = ROOT / "scripts/generate_phase3_user_study_templates.py"
VALIDATOR = ROOT / "scripts/validate_phase3_user_study.py"

sys.path.insert(0, str(ROOT / "scripts"))
from generate_phase3_user_study_templates import FROZEN_BINDINGS  # noqa: E402
from validate_phase3_user_study import (  # noqa: E402
    _compute_aggregate, validate_actual_study, validate_pending_template,
)


def _step(step_id, at_ms, help_opened=False, errors=None):
    return {"step_id": step_id, "at_ms": at_ms, "outcome": "COMPLETED",
            "help_opened": help_opened, "errors": errors or []}


def _task(task_id, start, end, steps, measurement, response):
    return {"task_id": task_id, "status": "COMPLETE", "start_ms": start,
            "end_ms": end, "steps": steps,
            "error_count": sum(len(step["errors"]) for step in steps),
            "help_open_count": sum(int(step["help_opened"]) for step in steps),
            "open_response": response, "measurement": measurement}


def passing_study():
    value = json.loads(PENDING.read_text())
    build = "sha256:" + "1" * 64
    content = "sha256:" + "2" * 64
    capture = "sha256:" + "3" * 64
    first_times = [20_000, 25_000, 30_000, 35_000, 15_000, 22_000, 24_000, 26_000]
    base_utc = datetime(2026, 7, 27, 15, 0, tzinfo=timezone.utc)
    for index, session in enumerate(value["sessions"]):
        participant_id = "P3-A%09d" % (index + 1)
        observer_id = "OBS-%08d" % (index + 1)
        experienced = session["group"] == "billiards_experienced_user"
        session["status"] = "COMPLETE"
        session["participant_id"] = participant_id
        session["eligibility"] = {
            "real_human": True, "project_member": False, "agent_or_robot": False,
            "first_time_billiardgl_user": True,
            "in_person_billiards_experience_years": 3 if experienced else 0,
            "group_qualification_attested": True,
        }
        session["evidence_bindings"] = {
            "build_sha256": build, "content_sha256": content, **FROZEN_BINDINGS,
            "gpu_capture_manifest_sha256": capture,
        }
        session["device"] = {
            "device_code": "DEV-%02d" % (index + 1), "operating_system": "macOS fixture",
            "gpu_device": "Fixture GPU", "input_devices": ["keyboard", "mouse"],
            "logical_viewport_px": [1280, 720], "physical_viewport_px": [1920, 1080],
            "dpi": 144,
        }
        started = base_utc + timedelta(minutes=index * 5)
        ended = started + timedelta(seconds=180)
        session["session"] = {"started_utc": started.isoformat().replace("+00:00", "Z"),
                              "ended_utc": ended.isoformat().replace("+00:00", "Z"),
                              "duration_ms": 180_000}
        valid_time = first_times[index]
        opens_help = index == 0
        first_steps = [
            _step("application_launched", 0), _step("hud_visible", 1000),
            _step("power_control_found", 3000, opens_help),
            _step("tip_control_found", 4000), _step("aim_entered", 6000),
            _step("shot_released", max(7000, valid_time - 1000)),
            _step("valid_shot_confirmed", valid_time),
        ]
        first_task = _task("first_valid_shot", 0, valid_time, first_steps, {
            "valid_shot": True, "valid_shot_at_ms": valid_time,
            "power_control_found_at_ms": 3000, "tip_control_found_at_ms": 4000,
            "help_opened_before_power_and_tip_found": opens_help,
        }, "The controls were located and one valid shot was completed.")
        spin_task = _task("low_draw_and_right_english", 40_000, 70_000, [
            _step("low_draw_selected", 42_000), _step("low_draw_submitted", 48_000),
            _step("right_english_selected", 56_000), _step("right_english_submitted", 66_000),
        ], {"low_draw_submitted": True, "low_draw_tip_offset_xy_R": [0.0, -0.5],
            "right_english_submitted": True, "right_english_tip_offset_xy_R": [0.5, 0.0]},
           "Low draw and right English were set and submitted separately.")
        intent = "sha256:" + ("%x" % (index + 4)) * 64
        camera_task = _task("overhead_to_player_low_roundtrip", 75_000, 95_000, [
            _step("shot_intent_recorded", 76_000), _step("overhead_selected", 80_000),
            _step("player_low_restored", 90_000), _step("shot_intent_rechecked", 94_000),
        ], {"overhead_selected": True, "player_low_restored": True,
            "shot_intent_before_sha256": intent, "shot_intent_after_sha256": intent},
           "The camera returned to player-low without changing the prepared shot.")
        rules_task = _task("foul_and_handoff_explanation", 100_000, 125_000, [
            _step("foul_presented", 101_000), _step("foul_explained", 108_000),
            _step("handoff_presented", 112_000), _step("handoff_explained", 120_000),
        ], {"foul_explanation": "The shot violated the displayed rail requirement.",
            "handoff_explanation": "Control passes to the opposing player.",
            "foul_reason_identified": True, "handoff_identified": True},
           "The displayed foul reason and the following handoff were understood.")
        session["tasks"] = [first_task, spin_task, camera_task, rules_task]
        if experienced:
            session["experienced_debrief"] = {
                "tip_contact_answer": "The marker is below and right of cue-ball center.",
                "shot_direction_answer": "The guide shows the initial shot direction.",
                "assist_level_answer": "Assist displays guidance but does not shoot.",
                "tip_contact_correct": True, "shot_direction_correct": True,
                "assist_level_correct": True,
            }
        else:
            session["experienced_debrief"] = {key: "N/A" for key in (
                "tip_contact_answer", "shot_direction_answer", "assist_level_answer",
                "tip_contact_correct", "shot_direction_correct", "assist_level_correct")}
        signed = (ended + timedelta(seconds=1)).isoformat().replace("+00:00", "Z")
        session["attestations"] = {
            "participant": {"accepted": True, "signer_code": participant_id,
                            "signed_utc": signed,
                            "statement_version": "phase3_user_study_attestation_v1"},
            "observer": {"accepted": True, "observer_id": observer_id,
                         "signed_utc": signed,
                         "statement_version": "phase3_user_study_attestation_v1"},
        }
        session["pii_review"] = {"performed": True, "status": "CLEAR",
                                 "reviewer_observer_id": observer_id}
    value["aggregate"] = _compute_aggregate(value["sessions"])
    value["status"] = value["aggregate"]["status"]
    return value


class Phase3UserStudyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.pending = json.loads(PENDING.read_text())
        cls.passing = passing_study()

    def test_pending_template_is_exact_not_run_and_contains_no_participants(self):
        self.assertEqual(validate_pending_template(self.pending), [])
        self.assertEqual(self.pending["status"], "NOT_RUN")
        self.assertEqual(self.pending["aggregate"], {"status": "NOT_RUN"})
        self.assertTrue(all(session["participant_id"] == "PENDING"
                            for session in self.pending["sessions"]))

    def test_generator_is_byte_deterministic(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-study-") as directory:
            completed = subprocess.run([sys.executable, str(GENERATOR), "--output-dir", directory],
                                       cwd=ROOT, text=True, capture_output=True, timeout=20)
            self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
            for source in (PENDING, STUDY_SCHEMA, PARTICIPANT_SCHEMA):
                self.assertEqual((Path(directory) / source.name).read_bytes(), source.read_bytes())

    def test_pending_cli_reports_zero_of_eight_and_not_run(self):
        completed = subprocess.run([sys.executable, str(VALIDATOR), str(PENDING),
                                    "--pending-template"], cwd=ROOT, text=True,
                                   capture_output=True, timeout=20)
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        self.assertIn("participants=0/8", completed.stdout)
        self.assertIn("result=NOT_RUN", completed.stdout)

    def test_complete_real_human_fixture_computes_pass(self):
        errors, aggregate = validate_actual_study(self.passing)
        self.assertEqual(errors, [])
        self.assertTrue(aggregate["gate_pass"])
        self.assertEqual(aggregate["new_user_first_valid_shot_median_seconds"], 25.0)
        self.assertEqual(aggregate["new_user_power_and_tip_without_help_rate"], 0.8)

    def test_duplicate_or_nonanonymous_participant_id_is_rejected(self):
        invalid = copy.deepcopy(self.passing)
        invalid["sessions"][1]["participant_id"] = invalid["sessions"][0]["participant_id"]
        errors, _ = validate_actual_study(invalid)
        self.assertTrue(any("unique anonymous" in error for error in errors), errors)

    def test_agent_robot_or_project_member_is_rejected(self):
        invalid = copy.deepcopy(self.passing)
        invalid["sessions"][0]["eligibility"]["agent_or_robot"] = True
        invalid["sessions"][0]["eligibility"]["project_member"] = True
        errors, _ = validate_actual_study(invalid)
        self.assertTrue(any("real-human/non-project" in error for error in errors), errors)

    def test_missing_or_self_filled_dual_attestation_is_rejected(self):
        invalid = copy.deepcopy(self.passing)
        participant = invalid["sessions"][0]
        participant["attestations"]["observer"]["accepted"] = False
        participant["attestations"]["observer"]["observer_id"] = participant["participant_id"]
        errors, _ = validate_actual_study(invalid)
        self.assertTrue(any("dual attestation" in error for error in errors), errors)

    def test_pii_in_open_answer_is_rejected(self):
        invalid = copy.deepcopy(self.passing)
        invalid["sessions"][0]["tasks"][0]["open_response"] = "Contact me at user@example.com"
        errors, _ = validate_actual_study(invalid)
        self.assertTrue(any("contains PII" in error for error in errors), errors)

    def test_build_content_token_capture_binding_mismatch_is_rejected(self):
        invalid = copy.deepcopy(self.passing)
        invalid["sessions"][0]["evidence_bindings"]["hud_token_sha256"] = "sha256:" + "0" * 64
        errors, _ = validate_actual_study(invalid)
        self.assertTrue(any("SHA binding" in error for error in errors), errors)

    def test_missing_step_and_nonmonotonic_time_are_rejected(self):
        invalid = copy.deepcopy(self.passing)
        invalid["sessions"][0]["tasks"][0]["steps"].pop()
        invalid["sessions"][1]["tasks"][1]["steps"][2]["at_ms"] = 41_000
        errors, _ = validate_actual_study(invalid)
        self.assertTrue(any("required step/time coverage" in error for error in errors), errors)
        self.assertTrue(any("not monotonic" in error for error in errors), errors)

    def test_premature_aggregate_mutation_invalidates_pending_template(self):
        invalid = copy.deepcopy(self.pending)
        invalid["aggregate"]["participant_count"] = 0
        self.assertTrue(validate_pending_template(invalid))

    def test_failed_median_is_computed_as_fail_not_fabricated_pass(self):
        invalid = copy.deepcopy(self.passing)
        for session in invalid["sessions"][:5]:
            first = session["tasks"][0]
            first["end_ms"] = 40_000
            first["steps"][-2]["at_ms"] = 39_000
            first["steps"][-1]["at_ms"] = 40_000
            first["measurement"]["valid_shot_at_ms"] = 40_000
        invalid["aggregate"] = _compute_aggregate(invalid["sessions"])
        invalid["status"] = "FAIL"
        errors, aggregate = validate_actual_study(invalid)
        self.assertEqual(errors, [])
        self.assertFalse(aggregate["gate_pass"])
        self.assertFalse(aggregate["threshold_results"]["new_user_first_valid_shot_median"])

    def test_less_than_eighty_percent_without_help_computes_fail(self):
        invalid = copy.deepcopy(self.passing)
        for session in invalid["sessions"][:2]:
            first = session["tasks"][0]
            first["steps"][2]["help_opened"] = True
            first["help_open_count"] = 1
            first["measurement"]["help_opened_before_power_and_tip_found"] = True
        invalid["aggregate"] = _compute_aggregate(invalid["sessions"])
        invalid["status"] = "FAIL"
        errors, aggregate = validate_actual_study(invalid)
        self.assertEqual(errors, [])
        self.assertLess(aggregate["new_user_power_and_tip_without_help_rate"], 0.8)

    def test_experienced_explanation_failure_computes_fail(self):
        invalid = copy.deepcopy(self.passing)
        invalid["sessions"][5]["experienced_debrief"]["assist_level_correct"] = False
        invalid["aggregate"] = _compute_aggregate(invalid["sessions"])
        invalid["status"] = "FAIL"
        errors, aggregate = validate_actual_study(invalid)
        self.assertEqual(errors, [])
        self.assertFalse(aggregate["threshold_results"][
            "experienced_tip_direction_assist_explanation"])

    def test_camera_intent_change_and_invalid_spin_submission_are_rejected(self):
        invalid = copy.deepcopy(self.passing)
        invalid["sessions"][0]["tasks"][2]["measurement"]["shot_intent_after_sha256"] = \
            "sha256:" + "f" * 64
        invalid["sessions"][1]["tasks"][1]["measurement"]["low_draw_tip_offset_xy_R"] = [0, 0.5]
        errors, _ = validate_actual_study(invalid)
        self.assertTrue(any("camera roundtrip" in error for error in errors), errors)
        self.assertTrue(any("low-draw/right-English" in error for error in errors), errors)


if __name__ == "__main__":
    unittest.main()
