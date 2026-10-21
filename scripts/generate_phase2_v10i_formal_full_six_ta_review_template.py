#!/usr/bin/env python3
"""Generate the unexecuted Phase2 full-six independent-TA review template."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / (
    "docs/art/phase2-v10i-formal-full-six-ta-review-template-v1.json"
)
REENTRY_GATE_PATH = (
    "docs/art/phase2-v10i-formal-full-six-reentry-"
    "acceptance-design-gate-v1.json"
)
REENTRY_GATE_SHA256 = (
    "40fcca249cf1a307833a75972428bbcdae5bb642cef1f5d7637b96deb9bc3fa4"
)
SCENES = [
    ("break", "observe_free", "elevated", "table",
     [0.10, 0.16, 0.80, 0.68]),
    ("low_aim", "aim_player", "player_low", "cue_and_ball",
     [0.18, 0.36, 0.64, 0.42]),
    ("ball_closeup", "observe_free", "elevated", "ball",
     [0.25, 0.24, 0.50, 0.55]),
    ("pocket_closeup", "observe_free", "elevated", "pocket",
     [0.18, 0.18, 0.64, 0.64]),
    ("tactical_overhead", "aim_overhead", "overhead", "playfield",
     [0.20, 0.06, 0.60, 0.88]),
    ("post_shot_observe", "observe_free", "elevated", "moving_balls",
     [0.12, 0.15, 0.76, 0.68]),
]
CANONICAL_AXES = [
    "composition",
    "material_color",
    "contact_shadow",
    "geometry_alignment",
    "ball_identity_and_rotation_seam",
    "venue_and_background_focus",
    "camera_composition_and_occlusion",
]


def build_template() -> dict[str, Any]:
    cases = []
    for ordinal, (scene_id, state, preset, region, rect) in enumerate(
            SCENES, start=1):
        cases.append({
            "ordinal": ordinal,
            "scene_id": scene_id,
            "expected_semantic_camera": {
                "state": state,
                "preset": preset,
                "transitioning": False,
            },
            "interest_region": {
                "name": region,
                "rect_normalized": rect,
            },
            "future_ppm_evidence": {
                "path": None,
                "sha256": None,
                "dimensions": None,
            },
            "manual_case_result": "NOT_RUN",
            "blocking_axes": [],
            "axis_findings": {
                axis: {"status": "PENDING", "observation": None}
                for axis in CANONICAL_AXES
            },
        })
    return {
        "schema": "phase2_v10i_formal_full_six_independent_ta_review_v1",
        "status": "TEMPLATE_NOT_RUN_WAITING_FOR_FULL_SIX_MACHINE_PASS",
        "overall_result": "NOT_RUN",
        "template_version": 1,
        "review_scope": {
            "kind": "formal_full_six_independent_ta",
            "scene_ids_in_order": [scene[0] for scene in SCENES],
            "scene_count": 6,
            "canonical_axes_in_order": CANONICAL_AXES,
            "axis_count_per_scene": 7,
            "total_axis_count": 42,
            "original_resolution_required": [1920, 1080],
        },
        "frozen_reentry_gate_binding": {
            "path": REENTRY_GATE_PATH,
            "sha256": REENTRY_GATE_SHA256,
        },
        "future_machine_pass_binding": {
            "status": "NOT_AVAILABLE",
            "receipt_path": None,
            "receipt_sha256": None,
            "required_schema":
                "phase2_v10i_formal_full_six_machine_pass_receipt_v1",
            "required_status": "FULL_SIX_MACHINE_PASS",
            "required_scene_count": 6,
            "required_machine_pass_scene_count": 6,
            "required_failed_machine_check_count": 0,
        },
        "completion_preconditions": [
            "A future full-six machine receipt binds this reentry gate and reports exact 6/6 machine PASS.",
            "Each scene binds one existing immutable P6 RGB8 PPM by path and raw SHA-256.",
            "Each bound PPM is independently verified as 1920x1080.",
            "Only after all machine and image preconditions pass may every axis change from PENDING to PASS or FAIL.",
            "Partial or mixed PENDING/PASS/FAIL review state is forbidden."
        ],
        "allowed_statuses": {
            "template_case": ["NOT_RUN"],
            "template_axis": ["PENDING"],
            "completed_case": ["PASS", "FAIL"],
            "completed_axis": ["PASS", "FAIL"],
        },
        "cases": cases,
        "review_counts": {
            "cases_total": 6,
            "cases_not_run": 6,
            "cases_pass": 0,
            "cases_fail": 0,
            "axes_total": 42,
            "axes_pending": 42,
            "axes_pass": 0,
            "axes_fail": 0,
        },
        "failed_case_ids": [],
        "blocking_axes": [],
        "evidence_boundary": {
            "template_only": True,
            "machine_receipt_bound": False,
            "capture_ppm_bound_count": 0,
            "human_ta_review_performed": False,
            "full_six_ta_pass": False,
            "full_six_formal_pass": False,
            "authorization_prepared_by_template_work": False,
            "authorization_issued_by_template_work": False,
            "gpu_executed_by_template_work": False,
            "production_modified": False,
            "m2_promotion": False,
            "golden_promotion": False,
            "product_review_performed": False,
            "product_pass": False,
            "beta_pass": False,
        },
    }


def generate(output: Path = DEFAULT_OUTPUT) -> Path:
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(build_template(), indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8")
    return output


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    arguments = parser.parse_args()
    print(generate(arguments.output))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
