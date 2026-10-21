#!/usr/bin/env python3
"""Validate the fail-closed WPA fixed-seed rack visual rebaseline gate."""

from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GATE = ROOT / "docs/art/wpa-official-fixed-seed-rack-visual-rebaseline-design-gate-v1.json"

PHASE2_IDS = [
    "break", "low_aim", "ball_closeup", "pocket_closeup",
    "tactical_overhead", "post_shot_observe",
]
PHASE4_IDS = [
    "camera_player_low_1080", "camera_elevated_1080",
    "camera_overhead_1080", "camera_broadcast_1080",
    "actual_retina_ui135_cvd",
]
PHASE5_IDS = [
    "group_choice_armed_confirm--p1--zh_cn",
    "group_choice_armed_confirm--p2--en",
    "group_choice_armed_confirm--p3--zh_cn",
    "group_choice_armed_confirm--p4--en",
    "group_choice_open_unselected--p1--en",
    "group_choice_open_unselected--p2--zh_cn",
    "group_choice_open_unselected--p3--en",
    "group_choice_open_unselected--p4--zh_cn",
    "eight_legal_open_unselected--p1--en",
    "break_illegal_open_unselected--p2--zh_cn",
    "submitting_progress--p3--en",
    "authority_error--p4--en",
    "full_table_preview_valid--p1--en",
    "full_table_preview_invalid--p2--en",
    "behind_head_string_preview_valid--p3--en",
    "behind_head_string_preview_invalid--p4--en",
]
RACK_AXES = [
    "base_corners_opposite_groups",
    "eight_ball_center_slot",
    "apex_and_foot_spot_readability",
    "rack_tightness_and_no_overlap",
    "numbered_texture_orientation_and_readability",
    "lighting_contact_shadow_and_highlight_regression",
    "camera_composition_and_occlusion_regression",
]
EXPECTED_BINDINGS = {
    "docs/rules/wpa-ii-04-official-rack-product-migration-gate-v1.json":
        "bf02b866e6017278c60366718535053049738555d72a573c8afc1f675d5c5545",
    "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v3.json":
        "d7ab2104fac9fd85fe8d7336ae658cc9baaaa6b4fa866cd4e7815c51404ad033",
    "docs/art/phase2-v10i-formal-full-six-ta-review-template-v1.json":
        "6adc89b2b9030426287f432edfa3c33a6431af545cbaedae80a446c2b9e81f49",
    "docs/art/phase4-v12-targeted5-independent-ta-review-template-v1.json":
        "e4ef1fbfb92536d6a02b6c3287b4a453f84e7e21bfed8c594414401d937e98df",
    "docs/art/phase5-rules-decision-ui-v5-exact16-independent-ta-review-template-v1.json":
        "917ef40bdf96ccca631117363c2d7d92eadade3bc9629d3d7559067e53164e2a",
    "assets/training/phase4/scenarios/launch_stop_shot_center_v1.training.json":
        "5d93df130f888790ac8298394c28cd9aa8147b39c4ea587f8b9ffc063e25cd99",
    "docs/art/phase4-training-replay-overlay-fixtures-v3/launch_stop_shot_center_v1.overlay.replay.json":
        "587bef29abe6167e8efbac2630963d6924ee8984c729e586adb313a806167b0e",
    "assets/modern/phase2-v10i-formal/focused_practice_hall_ball_identity_atlas_v1.png":
        "4958d3b8443ddd3f9a3b559ca1e22b6130c169d733ec73c7b918aadcd14fd60e",
    "src/Billiards/game_state.cpp":
        "a91185ba5fad8c8571aa1b0efd36de967d0fc3c307fef01b2903c2f910de88c3",
    "src/Billiards/rules/rack_executor.cpp":
        "d1805c8e197f6b1a8f712f80ba8be807217e875d56def3c7d00186293cd9fb17",
    "src/Billiards/rules/rack_executor.h":
        "d42dc8ae486ad9df8911e4f82cb3d657c0e51e76b35082edb3fd909ee37ef242",
    "src/Billiards/product_shell_controller.cpp":
        "eb1a754778808e23232ff14ab1a411be9189da876070910961ec6f4cbdf69a6c",
    "src/Billiards/product_shell_controller.h":
        "b90565640c7770cd1058b39cd42c03ad9bc81d6fcaf1c3a70882345d6a5a63f5",
    "src/Billiards/game_runtime.cpp":
        "3292df4f4546d9fd1639de8d5db33b7dab526f13b129f496d1420a489253fb9a",
    "src/Billiards/game_runtime.h":
        "deacceaa4ffff8b0e5080963d958f5fc49a77a197c744c2693be6f2d82b96ec3",
    "src/Billiards/render/modern/modern_product_session.cpp":
        "8e962dfc76149a4461e011941231e5e72860aed9f8cb6f006169e269455cd174",
    "src/Billiards/render/modern/modern_product_session.h":
        "f9b5ecef2223943bc8cb278b081b8f7f7fb886e7b5a0cc4db4c1fe2bac8528c4",
}
EXPECTED_SLOTS = {
    0: None, 1: 7, 2: 13, 3: 3, 4: 5, 5: 10, 6: 9, 7: 0,
    8: 4, 9: 8, 10: 14, 11: 12, 12: 6, 13: 2, 14: 1, 15: 11,
}
EXPECTED_POSITIONS = {
    0: [0.0, 89.34147644042969, -63.5],
    1: [-2.8575000762939453, 89.34147644042969, 78.34800720214844],
    2: [5.715000152587891, 89.34147644042969, 83.2973403930664],
    3: [-5.715000152587891, 89.34147644042969, 73.39867401123047],
    4: [5.715000152587891, 89.34147644042969, 73.39867401123047],
    5: [-11.430000305175781, 89.34147644042969, 83.2973403930664],
    6: [8.572500228881836, 89.34147644042969, 78.34800720214844],
    7: [0.0, 89.34147644042969, 63.5],
    8: [0.0, 89.34147644042969, 73.39867401123047],
    9: [2.8575000762939453, 89.34147644042969, 78.34800720214844],
    10: [11.430000305175781, 89.34147644042969, 83.2973403930664],
    11: [0.0, 89.34147644042969, 83.2973403930664],
    12: [-8.572500228881836, 89.34147644042969, 78.34800720214844],
    13: [2.8575000762939453, 89.34147644042969, 68.44933319091797],
    14: [-2.8575000762939453, 89.34147644042969, 68.44933319091797],
    15: [-5.715000152587891, 89.34147644042969, 83.2973403930664],
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate(path: Path = DEFAULT_GATE) -> list[str]:
    errors: list[str] = []
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        return [f"gate unreadable: {error}"]

    def require(condition: bool, message: str) -> None:
        if not condition:
            errors.append(message)

    require(value.get("schema") ==
            "wpa_official_fixed_seed_rack_visual_rebaseline_design_gate_v1",
            "schema mismatch")
    require(value.get("status") ==
            "DESIGN_GATE_APPROVED_NOT_RUN_NO_GPU_NO_AUTH_NO_READINESS",
            "status mismatch")

    observed_bindings: dict[str, str] = {}
    for section in ("frozen_bindings", "post_migration_production_bindings"):
        for binding in value.get(section, {}).values():
            if isinstance(binding, dict) and isinstance(binding.get("path"), str):
                observed_bindings[binding["path"]] = binding.get("sha256")
    require(observed_bindings == EXPECTED_BINDINGS,
            "binding inventory or declared SHA mismatch")
    for relative, digest in EXPECTED_BINDINGS.items():
        target = ROOT / relative
        require(target.is_file(), f"binding missing: {relative}")
        if target.is_file():
            require(sha256(target) == digest, f"binding drift: {relative}")

    semantics = value.get("official_contract_semantics", {})
    require(semantics.get("fixed_seed_hex") == "0x48455942414c4c25",
            "fixed seed drift")
    require(semantics.get("numbered_ball_order_is_official") is False,
            "seeded numbered order must remain non-normative")
    require(semantics.get("texture_asset_change_allowed_by_migration") is False,
            "migration cannot authorize texture asset changes")
    require("ball 7 at the apex for this algorithm and seed" in
            semantics.get("implementation_deterministic_non_normative", []),
            "seeded apex non-normative guard missing")

    rows = value.get("ball_id_position_comparison", [])
    require(len(rows) == 16, "comparison must contain exactly 16 balls")
    by_id = {row.get("ball_id"): row for row in rows if isinstance(row, dict)}
    require(set(by_id) == set(range(16)), "comparison ball IDs not exact 0..15")
    for ball in range(16):
        row = by_id.get(ball, {})
        require(row.get("official_slot") == EXPECTED_SLOTS[ball],
                f"official slot mismatch: ball {ball}")
        require(row.get("official_position_cm") == EXPECTED_POSITIONS[ball],
                f"official position mismatch: ball {ball}")
    exact_ids = sorted(ball for ball, row in by_id.items()
                       if row.get("position_byte_exact") is True)
    require(exact_ids == [0, 9], "exact current-to-official match set mismatch")

    summary = value.get("coordinate_contract", {}).get("comparison_summary", {})
    require(summary == {
        "exact_position_match_ball_ids": [0, 9],
        "exact_position_match_count": 2,
        "exact_position_mismatch_count": 14,
        "current_base_corner_ball_ids": [11, 15],
        "current_base_corners_are_opposite_groups": False,
        "official_base_corner_ball_ids": [5, 10],
        "official_base_corners_are_opposite_groups": True,
        "official_apex_ball_id_for_fixed_seed": 7,
        "official_eight_ball_slot": 4,
    }, "comparison summary mismatch")

    diameter = value.get("coordinate_contract", {}).get("ball_diameter_cm")
    require(isinstance(diameter, (int, float)) and diameter > 0,
            "ball diameter invalid")
    if isinstance(diameter, (int, float)) and set(by_id) == set(range(16)):
        contacts = 0
        object_rows = [by_id[ball] for ball in range(1, 16)]
        for index, first in enumerate(object_rows):
            for second in object_rows[index + 1:]:
                a = first.get("official_position_cm", [math.nan] * 3)
                b = second.get("official_position_cm", [math.nan] * 3)
                distance = math.hypot(a[0] - b[0], a[2] - b[2])
                require(distance >= diameter - 0.0001,
                        "official object-ball overlap detected")
                if abs(distance - diameter) <= 0.0001:
                    contacts += 1
        require(contacts == 30, "tight triangle contact graph must have 30 edges")

    inheritance = value.get("evidence_inheritance", {})
    require(all(inheritance.get(phase) is False
                for phase in ("phase2", "phase4", "phase5")),
            "historical evidence inheritance must be false")
    require(inheritance.get("classification") ==
            "HISTORICAL_ONLY_NOT_ACCEPTANCE_EVIDENCE",
            "historical evidence classification mismatch")

    inventory = value.get("affected_case_inventory", {})
    phase2 = inventory.get("phase2_full_six", {})
    phase4 = inventory.get("phase4_targeted5", {})
    phase5 = inventory.get("phase5_exact16", {})
    require(phase2.get("case_ids_in_order") == PHASE2_IDS and
            phase2.get("directly_affected_case_ids") == PHASE2_IDS and
            phase2.get("direct_rack_texture_or_composition_impact") is True,
            "Phase2 affected inventory mismatch")
    require(phase4.get("case_ids_in_order") == PHASE4_IDS and
            phase4.get("directly_affected_case_ids") == [] and
            phase4.get("mandatory_rebaseline_control_case_ids") == PHASE4_IDS and
            phase4.get("direct_rack_texture_or_composition_impact") is False,
            "Phase4 isolation/control inventory mismatch")
    require(phase5.get("case_ids_in_order") == PHASE5_IDS and
            phase5.get("directly_affected_case_ids") == PHASE5_IDS and
            phase5.get("direct_rack_texture_or_composition_impact") is True,
            "Phase5 affected inventory mismatch")

    minimum = value.get("minimum_fresh_shared_build_rebaseline", {})
    require(minimum.get("fresh_shared_build_required") is True and
            minimum.get("build_must_postdate_and_bind_all_post_migration_production_shas") is True,
            "fresh shared build contract missing")
    require(minimum.get("minimum_total_capture_count") == 27 and
            minimum.get("minimum_total_machine_pass_count") == 27 and
            minimum.get("minimum_total_ta_case_count") == 27,
            "minimum 27-case rebaseline count mismatch")
    sets = minimum.get("capture_sets", [])
    require([(item.get("set_id"), item.get("required_case_count"),
              item.get("required_machine_pass_count"),
              item.get("required_ta_case_count")) for item in sets] == [
                  ("phase2_full_six", 6, 6, 6),
                  ("phase4_targeted5", 5, 5, 5),
                  ("phase5_exact16", 16, 16, 16),
              ], "capture-set minimums mismatch")
    preflight = minimum.get("cpu_static_machine_preflight", {})
    require(preflight.get("status") == "NOT_RUN" and
            preflight.get("required") is True,
            "CPU/static preflight must remain required and NOT_RUN")
    require("not by initializeBalls legacy layout" in
            minimum.get("product_match_state_attestation", ""),
            "product Match official-rack provenance guard missing")

    ta = value.get("original_resolution_ta_contract", {})
    require(ta.get("canonical_rack_axes_in_order") == RACK_AXES,
            "canonical rack TA axes mismatch")
    require(set(ta.get("axis_contracts", {})) == set(RACK_AXES),
            "rack TA axis contracts incomplete")
    require(ta.get("phase2", {}).get("rack_authority_views") ==
            ["break", "tactical_overhead"],
            "Phase2 rack authority views mismatch")
    require(ta.get("phase4", {}).get("additional_isolation_axes") == [
        "two_ball_fixture_did_not_inherit_product_rack",
        "ball_1_texture_orientation_and_readability_unchanged",
        "trajectory_local_contrast_and_ball_occlusion_no_regression",
    ], "Phase4 isolation axes mismatch")
    require(ta.get("phase5", {}).get("placement_cases_require_world_overlay_occlusion_recheck") ==
            PHASE5_IDS[-4:], "Phase5 placement regression inventory mismatch")

    boundaries = value.get("unchanged_machine_boundaries", {})
    require(boundaries and all(item is False for item in boundaries.values()),
            "threshold/workaround boundaries must all remain false")
    future = value.get("future_evidence", {})
    require(len(future) == 8, "future evidence inventory mismatch")
    for key, item in future.items():
        require(item.get("status") in {"NOT_AVAILABLE", "NOT_RUN"},
                f"future evidence must be unexecuted: {key}")
        require(item.get("path") is None and item.get("sha256") is None,
                f"future evidence path/SHA must be null: {key}")
    boundary = value.get("evidence_boundary", {})
    require(boundary and all(item is False for item in boundary.values()),
            "evidence boundary must remain entirely false")

    forbidden = value.get("forbidden_in_this_gate", [])
    for phrase in (
        "production or CMake edit", "GPU execution",
        "readiness generation", "authorization issuance or consumption",
        "claim that the seeded complete numbered-ball order is WPA normative",
    ):
        require(phrase in forbidden, f"forbidden boundary missing: {phrase}")
    return errors


def main() -> int:
    path = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT_GATE
    errors = validate(path)
    if errors:
        for error in errors:
            print("FAIL: " + error, file=sys.stderr)
        return 1
    print("PASS_WPA_OFFICIAL_FIXED_SEED_RACK_VISUAL_REBASELINE_DESIGN_GATE")
    print("bindings=17 balls=16 historical_inheritance=0 minimum_captures=27")
    print("cpu_static=NOT_RUN gpu=false readiness=false auth=false ta=false")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
