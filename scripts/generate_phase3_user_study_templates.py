#!/usr/bin/env python3
"""Generate PENDING-only Phase-3 real-user-study templates and JSON schemas."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


PENDING_NAME = "phase3-user-study-v1.pending.json"
STUDY_SCHEMA_NAME = "phase3-user-study-v1.schema.json"
PARTICIPANT_SCHEMA_NAME = "phase3-user-study-participant-v1.schema.json"

GROUP_SLOTS = (
    ("new-user-01", "project_new_user"), ("new-user-02", "project_new_user"),
    ("new-user-03", "project_new_user"), ("new-user-04", "project_new_user"),
    ("new-user-05", "project_new_user"),
    ("billiards-01", "billiards_experienced_user"),
    ("billiards-02", "billiards_experienced_user"),
    ("billiards-03", "billiards_experienced_user"),
)

TASK_IDS = ("first_valid_shot", "low_draw_and_right_english",
            "overhead_to_player_low_roundtrip", "foul_and_handoff_explanation")

TASK_STEP_IDS = {
    "first_valid_shot": ("application_launched", "hud_visible", "power_control_found",
                         "tip_control_found", "aim_entered", "shot_released",
                         "valid_shot_confirmed"),
    "low_draw_and_right_english": ("low_draw_selected", "low_draw_submitted",
                                   "right_english_selected", "right_english_submitted"),
    "overhead_to_player_low_roundtrip": ("shot_intent_recorded", "overhead_selected",
                                         "player_low_restored", "shot_intent_rechecked"),
    "foul_and_handoff_explanation": ("foul_presented", "foul_explained",
                                     "handoff_presented", "handoff_explained"),
}

FROZEN_BINDINGS = {
    "hud_token_sha256": "sha256:abf73aa283279c28677b5c97a9b02b3130b0d0125048c3124c85a9c0e72df860",
    "shell_token_sha256": "sha256:c144f1a25d3e7ced14e385bd6f76ca7e0f05b06597dcc8a4bacb4c570fc6bad3",
    "capture_matrix_sha256": "sha256:835210fb2a458edda80e6ba54d0dcda14805e15b5ca54c5a3c3747d154192766",
}

THRESHOLDS = {
    "new_user_first_valid_shot_median_seconds_max": 30.0,
    "new_user_power_and_tip_without_help_rate_min": 0.8,
    "experienced_tip_direction_assist_explanation_rate_min": 1.0,
    "camera_roundtrip_shot_intent_unchanged_rate_min": 1.0,
    "all_four_tasks_completion_rate_min": 1.0,
}


def _pending_step(step_id):
    return {"step_id": step_id, "at_ms": "PENDING", "outcome": "PENDING",
            "help_opened": "PENDING", "errors": "PENDING"}


def _pending_task(task_id):
    value = {
        "task_id": task_id, "status": "PENDING", "start_ms": "PENDING",
        "end_ms": "PENDING", "steps": [_pending_step(step) for step in TASK_STEP_IDS[task_id]],
        "error_count": "PENDING", "help_open_count": "PENDING",
        "open_response": "PENDING",
    }
    if task_id == "first_valid_shot":
        value["measurement"] = {
            "valid_shot": "PENDING", "valid_shot_at_ms": "PENDING",
            "power_control_found_at_ms": "PENDING", "tip_control_found_at_ms": "PENDING",
            "help_opened_before_power_and_tip_found": "PENDING",
        }
    elif task_id == "low_draw_and_right_english":
        value["measurement"] = {
            "low_draw_submitted": "PENDING", "low_draw_tip_offset_xy_R": "PENDING",
            "right_english_submitted": "PENDING", "right_english_tip_offset_xy_R": "PENDING",
        }
    elif task_id == "overhead_to_player_low_roundtrip":
        value["measurement"] = {
            "overhead_selected": "PENDING", "player_low_restored": "PENDING",
            "shot_intent_before_sha256": "PENDING", "shot_intent_after_sha256": "PENDING",
        }
    else:
        value["measurement"] = {
            "foul_explanation": "PENDING", "handoff_explanation": "PENDING",
            "foul_reason_identified": "PENDING", "handoff_identified": "PENDING",
        }
    return value


def _pending_participant(slot_id, group):
    return {
        "slot_id": slot_id, "status": "PENDING", "participant_id": "PENDING",
        "group": group,
        "eligibility": {
            "real_human": "PENDING", "project_member": "PENDING",
            "agent_or_robot": "PENDING", "first_time_billiardgl_user": "PENDING",
            "in_person_billiards_experience_years": "PENDING",
            "group_qualification_attested": "PENDING",
        },
        "evidence_bindings": {
            "build_sha256": "PENDING", "content_sha256": "PENDING",
            **FROZEN_BINDINGS, "gpu_capture_manifest_sha256": "PENDING",
        },
        "device": {
            "device_code": "PENDING", "operating_system": "PENDING",
            "gpu_device": "PENDING", "input_devices": "PENDING",
            "logical_viewport_px": "PENDING", "physical_viewport_px": "PENDING",
            "dpi": "PENDING",
        },
        "session": {"started_utc": "PENDING", "ended_utc": "PENDING",
                    "duration_ms": "PENDING"},
        "tasks": [_pending_task(task_id) for task_id in TASK_IDS],
        "experienced_debrief": {
            "tip_contact_answer": "PENDING", "shot_direction_answer": "PENDING",
            "assist_level_answer": "PENDING", "tip_contact_correct": "PENDING",
            "shot_direction_correct": "PENDING", "assist_level_correct": "PENDING",
        },
        "attestations": {
            "participant": {"accepted": "PENDING", "signer_code": "PENDING",
                            "signed_utc": "PENDING",
                            "statement_version": "phase3_user_study_attestation_v1"},
            "observer": {"accepted": "PENDING", "observer_id": "PENDING",
                         "signed_utc": "PENDING",
                         "statement_version": "phase3_user_study_attestation_v1"},
        },
        "pii_review": {"performed": "PENDING", "status": "PENDING",
                       "reviewer_observer_id": "PENDING"},
    }


def build_pending_study():
    return {
        "schema": "phase3_user_study_v1", "status": "NOT_RUN",
        "study_id": "phase3_first_shot_and_controls_v1",
        "protocol_path": "docs/art/phase3-user-study-protocol.md",
        "research_ethics": {
            "real_humans_only": True, "agents_or_robots_allowed": False,
            "project_members_allowed": False, "pii_collection_allowed": False,
            "anonymous_ids_required": True, "participant_and_observer_attestation_required": True,
        },
        "group_contract": {"project_new_user": 5, "billiards_experienced_user": 3,
                           "total": 8, "duplicate_participant_ids_allowed": False},
        "task_contract": {task_id: {"required_steps": list(TASK_STEP_IDS[task_id])}
                          for task_id in TASK_IDS},
        "thresholds": THRESHOLDS,
        "sessions": [_pending_participant(slot_id, group) for slot_id, group in GROUP_SLOTS],
        "aggregate": {"status": "NOT_RUN"},
        "evidence_boundary": {
            "agent_generated_participant_evidence_allowed": False,
            "synthetic_or_simulated_participant_allowed": False,
            "flat_proof_is_user_study": False, "gpu_capture_is_user_study": False,
            "aggregate_before_eight_complete_sessions_allowed": False,
        },
    }


def build_participant_schema():
    return {
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "$id": "billiardgl://schemas/phase3-user-study-participant-v1",
        "title": "Anonymous Phase 3 participant evidence",
        "type": "object", "additionalProperties": False,
        "required": ["slot_id", "status", "participant_id", "group", "eligibility",
                     "evidence_bindings", "device", "session", "tasks",
                     "experienced_debrief", "attestations", "pii_review"],
        "properties": {
            "slot_id": {"type": "string"}, "status": {"enum": ["PENDING", "COMPLETE"]},
            "participant_id": {"type": "string"},
            "group": {"enum": ["project_new_user", "billiards_experienced_user"]},
            "eligibility": {"type": "object"}, "evidence_bindings": {"type": "object"},
            "device": {"type": "object"}, "session": {"type": "object"},
            "tasks": {"type": "array", "minItems": 4, "maxItems": 4},
            "experienced_debrief": {"type": "object"},
            "attestations": {"type": "object"}, "pii_review": {"type": "object"},
        },
    }


def build_study_schema():
    return {
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "$id": "billiardgl://schemas/phase3-user-study-v1",
        "title": "Phase 3 real-user study evidence",
        "type": "object", "additionalProperties": False,
        "required": ["schema", "status", "study_id", "protocol_path", "research_ethics",
                     "group_contract", "task_contract", "thresholds", "sessions",
                     "aggregate", "evidence_boundary"],
        "properties": {
            "schema": {"const": "phase3_user_study_v1"},
            "status": {"enum": ["NOT_RUN", "PASS", "FAIL"]},
            "study_id": {"const": "phase3_first_shot_and_controls_v1"},
            "protocol_path": {"type": "string"}, "research_ethics": {"type": "object"},
            "group_contract": {"type": "object"}, "task_contract": {"type": "object"},
            "thresholds": {"type": "object"},
            "sessions": {"type": "array", "minItems": 8, "maxItems": 8},
            "aggregate": {"type": "object"}, "evidence_boundary": {"type": "object"},
        },
    }


def write_templates(output_dir):
    output_dir.mkdir(parents=True, exist_ok=True)
    values = {PENDING_NAME: build_pending_study(), STUDY_SCHEMA_NAME: build_study_schema(),
              PARTICIPANT_SCHEMA_NAME: build_participant_schema()}
    for name, value in values.items():
        (output_dir / name).write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", type=Path, default=Path("docs/art"))
    args = parser.parse_args()
    write_templates(args.output_dir)
    print("Generated Phase3 real-user-study template: new_users=5 billiards_experienced=3 status=NOT_RUN")


if __name__ == "__main__":
    main()
