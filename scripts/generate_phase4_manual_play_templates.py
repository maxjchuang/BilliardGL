#!/usr/bin/env python3
"""Generate PENDING-only Phase-4 human manual-play acceptance artifacts."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PACK_PATH = ROOT / "assets/training/phase4/training_pack_v1.json"
PENDING_NAME = "phase4-manual-play-v1.pending.json"
MATRIX_SCHEMA_NAME = "phase4-manual-play-v1.schema.json"
RECORD_SCHEMA_NAME = "phase4-manual-play-record-v1.schema.json"

EXPECTED_CATEGORY_COUNTS = {
    "stun_follow_draw": 3,
    "side_spin_cushion": 3,
    "pocket_angle": 2,
    "continuous_position": 2,
    "break_cue_control": 2,
}

MANUAL_CHECK_IDS = (
    "product_entry_reachable",
    "instructions_clear",
    "objective_clear",
    "recommended_assist_clear",
    "scenario_loaded",
    "legal_operation_completed",
    "success_state_observed",
    "failure_state_observed",
    "score_observed",
    "reset_observed",
    "retry_observed",
    "replay_v1_saved",
)

REPLAY_REJECTION_CONTRACT = {
    "wrong_content_hash": "replay_content_hash_mismatch",
    "wrong_profile_id": "replay_profile_id_mismatch",
    "wrong_profile_hash": "replay_profile_hash_mismatch",
}

TRAJECTORY_CONTRACT = {
    "actual": {
        "legend_label": "training.trajectory.replay_actual",
        "line_style": "SolidWithMarkers",
        "marker": "tick_dot",
    },
    "predicted": {
        "legend_label": "training.trajectory.predicted",
        "line_style": "Dashed",
        "marker": "endpoint_ring",
    },
    "preview_may_mutate_authority": False,
}


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def extract_pack_contract(pack_path: Path = PACK_PATH):
    pack = _load_json(pack_path)
    if pack.get("schema") != "training_pack_v1" or pack.get("schema_version") != 1:
        raise ValueError("Phase4 manual play requires training_pack_v1 schema version 1")
    scenarios = pack.get("scenarios")
    if not isinstance(scenarios, list) or len(scenarios) != 12:
        raise ValueError("Phase4 manual play requires exactly 12 manifest scenarios")
    counts = {category: 0 for category in EXPECTED_CATEGORY_COUNTS}
    ids = set()
    records = []
    for entry in scenarios:
        scenario_id = entry.get("id")
        category = entry.get("category")
        relative = entry.get("file")
        declared_sha = entry.get("sha256")
        if not isinstance(scenario_id, str) or not scenario_id or scenario_id in ids:
            raise ValueError("Phase4 manifest scenario IDs must be unique non-empty strings")
        if category not in counts:
            raise ValueError("Phase4 manual-play category differs from launch contract: " + str(category))
        scenario_path = (pack_path.parent / str(relative)).resolve()
        if pack_path.parent.resolve() not in scenario_path.parents:
            raise ValueError("Phase4 scenario path escapes manifest root")
        actual_sha = _sha256(scenario_path)
        if declared_sha != actual_sha:
            raise ValueError("Phase4 manifest scenario SHA mismatch: " + scenario_id)
        scenario = _load_json(scenario_path)
        if scenario.get("id") != scenario_id or scenario.get("category") != category:
            raise ValueError("Phase4 scenario identity/category differs from manifest: " + scenario_id)
        locale = scenario.get("locale", {})
        assist = scenario.get("assist", {})
        reset = scenario.get("reset", {})
        if not all(isinstance(locale.get(key), str) and locale[key].strip() for key in
                   ("title_en", "title_zh_cn", "description_en", "description_zh_cn")):
            raise ValueError("Phase4 scenario locale is incomplete: " + scenario_id)
        if assist.get("default") is None or reset.get("attempt_seed") != 0:
            raise ValueError("Phase4 assist/seed contract differs: " + scenario_id)
        ids.add(scenario_id)
        counts[category] += 1
        records.append({
            "scenario_id": scenario_id,
            "category": category,
            "scenario_file": str(relative),
            "scenario_file_sha256": "sha256:" + actual_sha,
            "initial_scenario_sha256": "sha256:" + scenario["initial_scenario"]["canonical_sha256"],
            "title_zh_cn": locale["title_zh_cn"],
            "description_zh_cn": locale["description_zh_cn"],
            "recommended_assist": assist["default"],
            "required_capabilities": scenario["runtime_gate"]["required_capabilities"],
            "attempt_seed": reset["attempt_seed"],
        })
    if counts != EXPECTED_CATEGORY_COUNTS:
        raise ValueError("Phase4 category counts differ from launch contract: " + str(counts))
    coverage_path = pack_path.parent / pack["coverage_matrix"]["file"]
    if _sha256(coverage_path) != pack["coverage_matrix"]["sha256"]:
        raise ValueError("Phase4 coverage matrix SHA differs from manifest")
    return {
        "pack_id": pack["id"],
        "pack_path": pack_path.relative_to(ROOT).as_posix(),
        "pack_sha256": "sha256:" + _sha256(pack_path),
        "coverage_matrix_path": coverage_path.relative_to(ROOT).as_posix(),
        "coverage_matrix_sha256": "sha256:" + _sha256(coverage_path),
        "category_counts": counts,
        "scenarios": records,
    }


def _pending_check(check_id):
    return {"check_id": check_id, "result": "PENDING", "notes": "PENDING"}


def _pending_record(scenario):
    return {
        **scenario,
        "status": "PENDING",
        "executor": {
            "executor_id": "PENDING",
            "real_external_human": "PENDING",
            "project_member": "PENDING",
            "agent_or_robot": "PENDING",
        },
        "environment": {
            "build_sha256": "PENDING",
            "asset_bundle_sha256": "PENDING",
            "executable_version": "PENDING",
            "physics_profile_id": "PENDING",
            "physics_profile_sha256": "PENDING",
            "device_code": "PENDING",
            "operating_system": "PENDING",
            "gpu_device": "PENDING",
            "input_devices": "PENDING",
            "logical_viewport_px": "PENDING",
            "physical_viewport_px": "PENDING",
            "dpi": "PENDING",
        },
        "session": {"started_utc": "PENDING", "ended_utc": "PENDING", "duration_ms": "PENDING"},
        "manual_checks": [_pending_check(check_id) for check_id in MANUAL_CHECK_IDS],
        "manual_open_response": "PENDING",
        "replay_evidence": {
            "file_sha256": "PENDING",
            "format": "PENDING",
            "format_version": "PENDING",
            "executable_version": "PENDING",
            "content_sha256": "PENDING",
            "physics_profile_id": "PENDING",
            "physics_profile_sha256": "PENDING",
            "seed": "PENDING",
            "declared_final_state_sha256": "PENDING",
            "first_play_final_state_sha256": "PENDING",
            "second_play_final_state_sha256": "PENDING",
            "first_play_final_hash_matches": "PENDING",
            "second_play_final_hash_matches": "PENDING",
            "same_file_replayed_twice": "PENDING",
            "wrong_content_hash_rejection": {"rejected": "PENDING", "error_code": "PENDING"},
            "wrong_profile_id_rejection": {"rejected": "PENDING", "error_code": "PENDING"},
            "wrong_profile_hash_rejection": {"rejected": "PENDING", "error_code": "PENDING"},
        },
        "trajectory_evidence": {
            "predicted_legend_readable": "PENDING",
            "actual_legend_readable": "PENDING",
            "tracks_distinguishable_without_color": "PENDING",
            "predicted_endpoint_readable": "PENDING",
            "actual_endpoint_readable": "PENDING",
            "authority_before_sha256": "PENDING",
            "authority_after_preview_sha256": "PENDING",
            "preview_authority_unchanged": "PENDING",
        },
        "automation_binding": {
            "attached": "PENDING",
            "artifact_sha256": "PENDING",
            "success_fixture_passed": "PENDING",
            "failure_fixture_passed": "PENDING",
            "substitutes_for_manual_play": False,
        },
        "attestations": {
            "executor": {
                "accepted": "PENDING", "signer_code": "PENDING", "signed_utc": "PENDING",
                "statement_version": "phase4_manual_play_attestation_v1",
            },
            "observer": {
                "accepted": "PENDING", "observer_id": "PENDING", "signed_utc": "PENDING",
                "statement_version": "phase4_manual_play_attestation_v1",
            },
        },
        "pii_review": {"performed": "PENDING", "status": "PENDING", "reviewer_observer_id": "PENDING"},
    }


def build_pending_matrix(pack_path: Path = PACK_PATH):
    contract = extract_pack_contract(pack_path)
    return {
        "schema": "phase4_manual_play_v1",
        "status": "NOT_RUN",
        "matrix_id": "phase4_launch12_human_manual_play_v1",
        "protocol_path": "docs/art/phase4-manual-play-protocol.md",
        "pack_contract": {key: value for key, value in contract.items() if key != "scenarios"},
        "scenario_contract": contract["scenarios"],
        "manual_check_contract": list(MANUAL_CHECK_IDS),
        "replay_rejection_contract": REPLAY_REJECTION_CONTRACT,
        "trajectory_contract": TRAJECTORY_CONTRACT,
        "evidence_boundary": {
            "real_external_human_required_for_every_scenario": True,
            "agents_or_robots_allowed": False,
            "project_members_allowed": False,
            "pii_collection_allowed": False,
            "automation_may_substitute_for_manual_play": False,
            "aggregate_before_twelve_complete_records_allowed": False,
            "observer_and_executor_attestation_required": True,
        },
        "records": [_pending_record(scenario) for scenario in contract["scenarios"]],
        "aggregate": {"status": "NOT_RUN"},
    }


def build_record_schema():
    return {
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "$id": "billiardgl://schemas/phase4-manual-play-record-v1",
        "title": "Phase 4 anonymous human manual-play record",
        "type": "object", "additionalProperties": False,
        "required": ["scenario_id", "category", "scenario_file", "scenario_file_sha256",
                     "initial_scenario_sha256", "title_zh_cn", "description_zh_cn",
                     "recommended_assist", "required_capabilities", "attempt_seed", "status",
                     "executor", "environment", "session", "manual_checks", "manual_open_response",
                     "replay_evidence", "trajectory_evidence", "automation_binding", "attestations",
                     "pii_review"],
        "properties": {
            "scenario_id": {"type": "string"}, "category": {"type": "string"},
            "scenario_file": {"type": "string"}, "scenario_file_sha256": {"type": "string"},
            "initial_scenario_sha256": {"type": "string"}, "title_zh_cn": {"type": "string"},
            "description_zh_cn": {"type": "string"}, "recommended_assist": {"type": "string"},
            "required_capabilities": {"type": "array"}, "attempt_seed": {"type": "integer"},
            "status": {"enum": ["PENDING", "COMPLETE"]}, "executor": {"type": "object"},
            "environment": {"type": "object"}, "session": {"type": "object"},
            "manual_checks": {"type": "array", "minItems": 12, "maxItems": 12},
            "manual_open_response": {"type": "string"}, "replay_evidence": {"type": "object"},
            "trajectory_evidence": {"type": "object"}, "automation_binding": {"type": "object"},
            "attestations": {"type": "object"}, "pii_review": {"type": "object"},
        },
    }


def build_matrix_schema():
    return {
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "$id": "billiardgl://schemas/phase4-manual-play-v1",
        "title": "Phase 4 launch-12 human manual-play acceptance",
        "type": "object", "additionalProperties": False,
        "required": ["schema", "status", "matrix_id", "protocol_path", "pack_contract",
                     "scenario_contract", "manual_check_contract", "replay_rejection_contract",
                     "trajectory_contract", "evidence_boundary", "records", "aggregate"],
        "properties": {
            "schema": {"const": "phase4_manual_play_v1"},
            "status": {"enum": ["NOT_RUN", "PASS", "FAIL"]},
            "matrix_id": {"const": "phase4_launch12_human_manual_play_v1"},
            "protocol_path": {"type": "string"}, "pack_contract": {"type": "object"},
            "scenario_contract": {"type": "array", "minItems": 12, "maxItems": 12},
            "manual_check_contract": {"type": "array", "minItems": 12, "maxItems": 12},
            "replay_rejection_contract": {"type": "object"},
            "trajectory_contract": {"type": "object"}, "evidence_boundary": {"type": "object"},
            "records": {"type": "array", "minItems": 12, "maxItems": 12},
            "aggregate": {"type": "object"},
        },
    }


def write_templates(output_dir: Path, pack_path: Path = PACK_PATH):
    output_dir.mkdir(parents=True, exist_ok=True)
    values = {
        PENDING_NAME: build_pending_matrix(pack_path),
        MATRIX_SCHEMA_NAME: build_matrix_schema(),
        RECORD_SCHEMA_NAME: build_record_schema(),
    }
    for name, value in values.items():
        (output_dir / name).write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", type=Path, default=ROOT / "docs/art")
    parser.add_argument("--pack", type=Path, default=PACK_PATH)
    args = parser.parse_args()
    write_templates(args.output_dir, args.pack.resolve())
    print("Generated Phase4 human manual-play matrix: scenarios=12 status=NOT_RUN")


if __name__ == "__main__":
    main()
