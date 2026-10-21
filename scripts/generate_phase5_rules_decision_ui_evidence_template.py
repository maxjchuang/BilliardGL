#!/usr/bin/env python3
"""Generate the 60-row pending actual-GPU/manual evidence template."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / "docs/evidence/phase5/rules-decision-ui-gpu-manual-template-v1.json"
GATE_SHA = "26567d0547163561ed77e6e41b32753c4cc55efcd693c9ce22faf847d43f5a08"
STATES = (
    "group_choice_open_unselected", "group_choice_armed_confirm",
    "full_table_preview_valid", "full_table_preview_invalid",
    "behind_head_string_preview_valid", "behind_head_string_preview_invalid",
    "eight_legal_open_unselected", "eight_foul_open_unselected",
    "break_illegal_open_unselected", "break_shot_foul_open_unselected",
    "submitting_progress", "authority_error", "state_drift_refreshing",
    "ai_or_remote_pending", "section_6g_classification_pending",
)
PROFILES = (
    ("p1", "1280x720", "1280x720", 1, 0.85, "standard"),
    ("p2", "1280x720", "1280x720", 1, 1.35, "color_vision_safe"),
    ("p3", "1920x1080", "1920x1080", 1, 1.0, "color_vision_safe"),
    ("p4", "1280x720", "2560x1440", 2, 1.0, "standard"),
)


def build() -> dict:
    cases: list[dict] = []
    for state_index, state in enumerate(STATES):
        for profile_index, profile in enumerate(PROFILES):
            profile_id, logical, physical, density, scale, palette = profile
            locale = "en" if (state_index + profile_index) % 2 == 0 else "zh_cn"
            cases.append({
                "case_id": f"{state}__{profile_id}", "state": state,
                "profile_id": profile_id, "logical_viewport": logical,
                "physical_viewport": physical, "density": density,
                "ui_scale": scale, "palette": palette, "locale": locale,
                "production_core41_after_tonemap_png": None,
                "png_sha256": None, "recorded_physical_dimensions": None,
                "machine_validation": "PENDING_ACTUAL_GPU_CAPTURE",
                "independent_manual_review": "PENDING_REAL_REVIEWER",
                "reviewer_identity": None, "reviewed_at": None,
            })
    return {
        "schema": "phase5_rules_decision_ui_gpu_manual_template_v1",
        "schema_version": 1, "design_gate_sha256": GATE_SHA,
        "status": "TEMPLATE_ONLY_PENDING_NO_GPU_NO_HUMAN",
        "product_exposed": False, "profile_complete": False,
        "actual_gpu_claimed": False, "manual_review_claimed": False,
        "case_count": len(cases), "cases": cases,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(build(), ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8")
    print(f"Phase5 rules decision UI pending GPU/manual template: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
