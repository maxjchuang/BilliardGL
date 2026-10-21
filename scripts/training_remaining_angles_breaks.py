#!/usr/bin/env python3
"""Pure scenario specs for the four revised angle/break training lessons.

The Phase 4 pack generator owns materialization, hashing, and replay binding.
This module deliberately exports JSON-only kwargs and independently calibrated
semantic actions so it can be imported without creating a generator cycle.
"""

from __future__ import annotations

import copy
import json


BALL_Y_CM = 89.341476


def ball(index: int, x: float, z: float) -> dict:
    return {
        "angular_velocity_rad_s": [0.0, 0.0, 0.0],
        "index": index,
        "pocketed": False,
        "position_cm": [x, BALL_Y_CM, z],
        "velocity_cm_s": [0.0, 0.0, 0.0],
    }


def cond(predicate: str, **kwargs: object) -> dict:
    return {"predicate": predicate, **kwargs}


def zone(ball_index: int, x: float, z: float, radius: float) -> dict:
    return cond("ball_final_position_in_zone", ball_index=ball_index,
                center_cm=[x, z], radius_cm=radius)


def rack_balls(cue_x: float = 0.0) -> list[dict]:
    result = [ball(0, cue_x, 76.0)]
    index = 1
    for row in range(5):
        z = -54.0 - row * 5.08
        for column in range(row + 1):
            x = (column - row / 2.0) * 5.86
            result.append(ball(index, round(x, 3), round(z, 3)))
            index += 1
    return result


def break_cluster_balls(cue_x: float = 0.0) -> list[dict]:
    """A compact three-object teaching rack with an unambiguous head ball."""
    return [
        ball(0, cue_x, 76.0),
        ball(1, 0.0, -54.0),
        ball(2, -2.93, -59.08),
        ball(3, 2.93, -59.08),
    ]


def hints(*, goal: str, power: tuple[float, float, float],
          tip: tuple[float, float, str], aim: tuple[float, float],
          target_pocket: tuple[bool, str, float, float],
          cue_zone: tuple[bool, float, float, float],
          shot_line: tuple[int, int, float, float, float, float],
          aim_at_pocket: bool = False) -> dict:
    enabled, pocket_id, pocket_x, pocket_z = target_pocket
    zone_visible, zone_x, zone_z, zone_radius = cue_zone
    cue_index, object_index, start_x, start_z, object_x, object_z = shot_line
    return {
        "goal_en": goal,
        "target_pocket_enabled": enabled,
        "aim_at_pocket": aim_at_pocket,
        "recommended_power": {
            "minimum_percent": power[0],
            "maximum_percent": power[1],
            "default_percent": power[2],
        },
        "recommended_tip": {
            "x_ball_radii": tip[0],
            "y_ball_radii": tip[1],
            "label": tip[2],
        },
        "aim": {
            "yaw_radians": aim[0],
            "tolerance_degrees": aim[1],
        },
        "target_pocket_id": pocket_id,
        "target_pocket_center_cm": [pocket_x, pocket_z],
        "cue_ball_target_zone": {
            "visible": zone_visible,
            "center_cm": [zone_x, zone_z],
            "radius_cm": zone_radius,
        },
        "shot_line": {
            "cue_ball_index": cue_index,
            "object_ball_index": object_index,
            "start_cm": [start_x, start_z],
            "object_center_cm": [object_x, object_z],
        },
    }


_SPECS = [
    {
        "scenario_id": "launch_corner_pocket_angle_v2",
        "category": "pocket_angle",
        "title_zh": "角袋切球入门",
        "title_en": "Corner-pocket cut basics",
        "goal_zh": "将4号球直接切入左下角袋，母球停入标记区且不碰库。",
        "goal_en": "Pot 4 in corner / cue in ring / no cue rails",
        "tags": ["launch13", "corner_pocket", "cut_angle", "beginner"],
        "balls": [ball(0, -15.0, -70.0), ball(4, -45.0, -100.0)],
        "success_conditions": [
            cond("first_contact_ball", ball_index=4),
            cond("ball_pocketed_in_pocket", ball_index=4, pocket_id=0),
            cond("ball_not_pocketed", ball_index=0),
            cond("rail_contact_count_lte", ball_index=0, maximum=0),
            cond("rail_contact_count_lte", ball_index=4, maximum=0),
            zone(0, -52.0, -97.0, 10.0),
        ],
        "par_shots": 1,
        "tip_axis": "none",
        "preferred_tip": [0.0, 0.0],
        "content_version": 2,
        "teaching_hints": hints(
            goal="Pot 4 in corner / cue in ring / no cue rails",
            power=(27.5, 32.5, 30.0), tip=(0.0, 0.0, "center_ball"),
            aim=(-2.3846986252222946, 1.0),
            target_pocket=(True, "corner_nx_nz", -63.5, -127.0),
            cue_zone=(True, -52.0, -97.0, 10.0),
            shot_line=(0, 4, -15.0, -70.0, -45.0, -100.0),
            aim_at_pocket=True),
    },
    {
        "scenario_id": "launch_side_pocket_angle_v2",
        "category": "pocket_angle",
        "title_zh": "中袋切球入门",
        "title_en": "Side-pocket cut basics",
        "goal_zh": "将5号球直接切入右侧中袋，母球停入右侧标记区且双方不碰库。",
        "goal_en": "Pot 5 in side / cue in ring / no cue rails",
        "tags": ["launch13", "side_pocket", "cut_angle", "beginner"],
        "balls": [ball(0, -5.0, 38.0), ball(5, 30.0, 5.0)],
        "success_conditions": [
            cond("first_contact_ball", ball_index=5),
            cond("ball_pocketed_in_pocket", ball_index=5, pocket_id=5),
            cond("ball_not_pocketed", ball_index=0),
            cond("rail_contact_count_lte", ball_index=0, maximum=0),
            cond("rail_contact_count_lte", ball_index=5, maximum=0),
            zone(0, 31.0, -36.0, 16.0),
        ],
        "par_shots": 1,
        "tip_axis": "none",
        "preferred_tip": [0.0, 0.0],
        "content_version": 2,
        "teaching_hints": hints(
            goal="Pot 5 in side / cue in ring / no cue rails",
            power=(32.5, 35.0, 32.5), tip=(0.0, 0.0, "center_ball"),
            aim=(-0.8240532136025128, 0.25),
            target_pocket=(True, "side_px", 63.5, 0.0),
            cue_zone=(True, 31.0, -36.0, 16.0),
            shot_line=(0, 5, -5.0, 38.0, 30.0, 5.0),
            aim_at_pocket=True),
    },
    {
        "scenario_id": "launch_head_ball_break_control_v2",
        "category": "break_cue_control",
        "title_zh": "正面散球入门",
        "title_en": "Head-on break basics",
        "goal_zh": "先碰1号球，至少一颗目标球碰库，母球不落袋即可通过；母球停进圈内额外加20分。",
        "goal_en": "Hit 1 / 1+ object rail / no scratch; ring bonus +20",
        "tags": ["launch13", "break", "head_ball", "cue_ball_control", "beginner"],
        "balls": break_cluster_balls(),
        "success_conditions": [
            cond("first_contact_ball", ball_index=1),
            cond("distinct_object_balls_with_rail_contact_gte", minimum=1),
            cond("ball_not_pocketed", ball_index=0),
        ],
        "optional_bonus_zone": zone(0, 0.0, -5.0, 70.0),
        "par_shots": 1,
        "tip_axis": "none",
        "preferred_tip": [0.0, 0.0],
        "content_version": 2,
        "teaching_hints": hints(
            goal="Hit 1 / 1+ object rail / no scratch; ring bonus +20",
            power=(62.5, 80.0, 70.0), tip=(0.0, 0.0, "center_ball"),
            aim=(-1.5707963267948966, 0.25),
            target_pocket=(False, "none", 0.0, 0.0),
            cue_zone=(True, 0.0, -5.0, 70.0),
            shot_line=(0, 1, 0.0, 76.0, 0.0, -54.0)),
    },
    {
        "scenario_id": "launch_cut_break_control_v2",
        "category": "break_cue_control",
        "title_zh": "侧位切薄开球控白",
        "title_en": "Cut-break cue control",
        "goal_zh": "从右侧开球位切1号顶球，让至少两颗目标球碰库，并将母球留在中台标记区。",
        "goal_en": "Cut 1 / 2+ object rails / cue in ring",
        "tags": ["launch13", "break", "cut_break", "cue_ball_control", "advanced"],
        "balls": break_cluster_balls(18.0),
        "success_conditions": [
            cond("first_contact_ball", ball_index=1),
            cond("distinct_object_balls_with_rail_contact_gte", minimum=2),
            cond("ball_not_pocketed", ball_index=0),
            zone(0, 0.0, -28.0, 72.0),
        ],
        "par_shots": 1,
        "tip_axis": "none",
        "preferred_tip": [0.0, 0.0],
        "content_version": 2,
        "teaching_hints": hints(
            goal="Cut 1 / 2+ object rails / cue in ring",
            power=(67.5, 72.5, 70.0), tip=(0.0, 0.0, "center_ball"),
            aim=(-1.7163830632652467, 0.25),
            target_pocket=(False, "none", 0.0, 0.0),
            cue_zone=(True, 0.0, -28.0, 72.0),
            shot_line=(0, 1, 18.0, 76.0, 0.0, -54.0)),
    },
]


REFERENCE_ACTIONS: dict[str, list[tuple[float, float, float, float]]] = {
    "launch_corner_pocket_angle_v2": [(-2.3846986252222946, 60.0, 0.0, 0.0)],
    "launch_side_pocket_angle_v2": [(-0.8240532136025128, 65.0, 0.0, 0.0)],
    "launch_head_ball_break_control_v2": [(-1.5707963267948966, 140.0, 0.0, 0.0)],
    "launch_cut_break_control_v2": [(-1.7163830632652467, 140.0, 0.0, 0.0)],
}


def build_remaining_angles_breaks() -> list[dict]:
    """Return a mutation-safe copy of the four generator kwargs specs."""
    return copy.deepcopy(_SPECS)


def main() -> int:
    print(json.dumps({
        "schema": "billiardgl.training.remaining_angles_breaks_specs.v1",
        "scenarios": build_remaining_angles_breaks(),
        "reference_actions": REFERENCE_ACTIONS,
    }, ensure_ascii=False, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
