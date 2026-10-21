#!/usr/bin/env python3
"""Generate the Phase 4 launch pack and bind verified reference replays."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import shutil

from scripts.training_remaining_angles_breaks import (
    build_remaining_angles_breaks,
)
from scripts.training_remaining_routes import build_remaining_routes
from scripts.training_remaining_spin import build_remaining_spin


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / "assets" / "training" / "phase4"
BALL_Y_CM = 89.341476
BALL_RADIUS_CM = 2.8575

# These digests were produced by two independent fixed-step runs of the
# shipping Release actions in scripts/generate_phase4_reference_replays.py.
# They do not include the enclosing training-file hash, so that file can be
# finalized before the second Recorder pass binds it.  Replay terminal hashes
# are build-local; debug/alternate-optimization tests record their own replay.
REFERENCE_AUDITS = {
    "launch_stop_shot_center_v1": ("4e60f3632fe7818a2e0e42351f278539e65a5b266548afefa9e8c618a627c087", "26c93f013e7785d20fa2ef1d8e36d92fe7eff62c516d8c70d3f19ed5ab2a2a24"),
    "launch_follow_shot_v1": ("aee5026984bcd7d4d71aa12a99d275b095c76e242856e69415baec46a3b48c59", "9df52f22f8a0621c8185bbd7f054e3d934b8c1966093c656555923dd510650dc"),
    "launch_draw_shot_v1": ("ebeec532494495c755d44391a19c3c23fe657262f83a77e18e6aec14407e3f2e", "c398611f2e43e3c003fe3f94db73ef3abe924a80790bffd8b70c432ce11af284"),
    "launch_left_english_one_rail_v1": ("7810aaba2b93fa7048691cbb133f2672b485370cb30ffa78523434e3f4b61420", "89bc4f0810465a6ef80ab66153a7fecd766fbfef95ae0238eac563c2aa503f8b"),
    "launch_right_english_one_rail_v1": ("384972d546951d4486a68fb506266a9b862a503ee61977ff2a44f4ac8f8db87b", "40e719d7875cca027cc43801abdfb19d20b9eed392c067e5c2aba60aaf4c7d01"),
    "launch_side_english_two_rail_v1": ("4f03203b240939ed24cb4a38a5851fb5caa7b15d764499d757d7cbea03ccbb0e", "85bc743ba3939ce8b237ff6cd991c4617cb0452e81453fab8db902bfab6cd587"),
    "launch_corner_pocket_angle_v1": ("3e982e622144818571a6c2d5a7edbb862e27a401c94d3788d573e69c9312e105", "89eae82f5a4edea299ab8d6b542f3aa2b8f30efb40c3564f4761ecc9f7ad11b4"),
    "launch_side_pocket_angle_v1": ("f9b81b9c21d508689c45439ada4195bf16c351370d23714305266c28b381f8ca", "4b1294d77fb287ff098b1e51f8a03dfbb7ff8bf8ce8b4446f1a66679ca838425"),
    "launch_two_ball_lane_route_v1": ("424d7e18999458d6e08fb516a1ba144cb655a2aa9f9cf07f589e500fc3b1eced", "edf44aaa64ea3b0c9c912b74bd8834635b2e2b3c624985eb896b4b9d43973ac7"),
    "launch_three_ball_corner_route_v1": ("444633fa7207ebea039ae9b45446fe4978d8dbca9f0468c21fc965e1e3826d61", "6d4e8caacb4b8e644f8ffba200c6670291e7f51bec985352c144580bc0c11c56"),
    "launch_head_ball_break_control_v1": ("8e85310608dd2c369c31effa85ce3cf093abf225e619ba2db6c829e2b2621700", "dd55078ec91fe4065e18a7a4f0d675698da3b2c1315d379f26a071567101c0fe"),
    "launch_cut_break_control_v1": ("f6df6d92a7c9f772e5528de1f86375102024e065f9ea4cf49d141c4512b4ce61", "5a109030756de31f21a66067781d2df538e51183415b642d3b5a855abd8ec71d"),
}

# Filled by the real two-pass Recorder before the v2 pack is promoted.  V1
# remains available separately; every v2 catalog entry owns its verified
# action and trace binding.
REFERENCE_AUDITS_V2 = {
    **REFERENCE_AUDITS,
    "launch_straight_pot_intro_v2": ("cf9d9724fdf39e34687eb0aa5601bffd3c4123f2f364f57357c738a4372f44d9", "30e61e58b9f90c0849317e2d2ccb0612c2bf4e7aa71ebe3eb782e58059b55cad"),
    "launch_straight_stop_intro_v2": ("be990de3eef24d0ec2fc787abb11e560a8583d061bc94036578ea10364051e55", "b8d8e5397fc319f40e3a066f93b1c71e6fcff2704f0f0a4ca877bd24122cc82c"),
    "launch_straight_follow_intro_v2": ("02e1ed99b4f8f83d0e1a13b5d2b403b98c2b8d6ba7fd2a706db48f8cb45a20a6", "fe1735ca3227b5a7163e6bf0b4990b475e971f097f41e4905d2504af304bf275"),
    "launch_straight_draw_intro_v2": ("fa268950dfc170c74ffb984d93537cc16af6af1e5c4f59a648ef66046e1e83d6", "3b552526a56ae5b88da37c1f28206138695d856278d866a651990b8e2b0c5a6d"),
    "launch_left_english_one_rail_v2": ("3b72af0f0bc6008f63388cf82ce783a4fc23a7e493840ad98b4959b2524e10f5", "e4791ca3fe083e62e51c3cb35031c7d0c81c11e3c7799209d61def6334e7f220"),
    "launch_right_english_one_rail_v2": ("5f288c5c1cd37d658eb6dcf46a9b0bf92f3ca91a0a837b09bf45ecd043818151", "33752e43a587fa73903f6b97f5ce10ce6b93e454b1e7df44e9400e62cf9c01a4"),
    "launch_side_english_two_rail_v2": ("d688f4d683c93f7025a6d5162381168bd42d25b8b93822dc24ebff9bc6e46680", "763926c031f17875cbeab3f456f8e9b6b454ed7ab585d317dd99eb8e119441bb"),
    "launch_corner_pocket_angle_v2": ("38a64491a60fd94430a478c7fa59b72045f1f20ba1180a614aba559a90d76c5f", "e7b211264f4e72f6cf7b1bcf5705619b01465cf7317d8834e1277f29eb0ed05b"),
    "launch_side_pocket_angle_v2": ("2c945b0484f70091cad75b7a4815c0f1963e231e9d7df40ccc3ae75427a22037", "e6d9a19cda40bcece5ddb6c424883b4d474cf9e4e13a8b809d5d9667a5c60a7a"),
    "launch_two_ball_lane_route_v2": ("940622252c7ccbf1dd46cc4a5759b1566da261e7767b375e0f2d1aa40a1ec886", "7e334753a832dd3857f4b3e2982243b056c118e2556836327d7322f3a65a133e"),
    "launch_three_ball_corner_route_v2": ("a748ecb665dd04aae466575aee31c238c878cb756527028b6c097ca54b3b9b73", "6ca3921233b7b0f71f563c753c1449bff721a4aa7821766b7aacb81193e8443e"),
    "launch_head_ball_break_control_v2": ("8e85310608dd2c369c31effa85ce3cf093abf225e619ba2db6c829e2b2621700", "8f0ba89de758ca30639b5b661fa727ddc6603d457a21c84ccf4bb059328fd632"),
    "launch_cut_break_control_v2": ("301b44aef72f7f1d3175ef8e6a2ea8baa23afb468bcd6a940708116419247da4", "3efd38f2956c532aac75cfa3a73002f826b93cd2745805f591b833642df7142e"),
}


def canonical_bytes(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n").encode()


def canonical_physics_bytes(value: object) -> bytes:
    """Mirror training_scenario.cpp canonicalScenarioValue exactly."""
    def encode(item: object, parent: str = "") -> str:
        if item is None:
            return "null"
        if isinstance(item, bool):
            return "true" if item else "false"
        if isinstance(item, str):
            return json.dumps(item, ensure_ascii=False, separators=(",", ":"))
        if isinstance(item, (int, float)):
            if parent in {"index", "schema_version", "ticks"}:
                return str(int(item))
            if float(item) == 12500000.0:
                return "1.25e+07"
            number = repr(float(item))
            return number if any(marker in number for marker in ".eE") else number + ".0"
        if isinstance(item, list):
            return "[" + ",".join(encode(child, parent) for child in item) + "]"
        if isinstance(item, dict):
            return "{" + ",".join(
                json.dumps(key, ensure_ascii=False) + ":" + encode(item[key], key)
                for key in sorted(item)) + "}"
        raise TypeError(type(item))
    return (encode(value) + "\n").encode()


def pretty_bytes(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n").encode()


def sha256_bytes(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def ball(index: int, x: float, z: float) -> dict:
    return {
        "angular_velocity_rad_s": [0.0, 0.0, 0.0],
        "index": index,
        "pocketed": False,
        "position_cm": [x, BALL_Y_CM, z],
        "velocity_cm_s": [0.0, 0.0, 0.0],
    }


def rack_balls() -> list[dict]:
    result = [ball(0, 0.0, 76.0)]
    index = 1
    for row in range(5):
        z = -54.0 - row * 5.08
        for column in range(row + 1):
            x = (column - row / 2.0) * 5.86
            result.append(ball(index, round(x, 3), round(z, 3)))
            index += 1
    return result


def shipping_physics_profile() -> dict:
    """Exact physics_scenario_v12 input for the shipping 120 Hz profile."""
    return {
        "id": "chinese_pool_interactive_120hz_v5",
        "formula_version": "legacy_v1",
        "ball": {
            "mass_kg": 0.17, "radius_cm": 2.8575,
            "inertia_factor": 0.4, "normal_restitution": 0.97,
            "friction_coefficient": 0.10, "material": "phenolic_resin",
        },
        "surface": {
            "legacy_friction_acceleration_cm_s2": 4.0,
            "sliding_friction_coefficient": 0.20,
            "rolling_resistance_acceleration_cm_s2": 12.5,
            "torsional_spin_deceleration_rad_s2": 12.0,
            "slip_speed_epsilon_cm_s": 0.0001,
            "stop_energy_threshold_j": 0.000000001,
            "material": "mathavan_interactive_surface_v2",
        },
        "cue": {
            "effective_mass_kg": 0.5, "normal_restitution": 0.0,
            "chalked_friction_coefficient": 0.6,
            "unchalked_friction_coefficient": 0.1,
            "maximum_reliable_offset_radius": 0.8,
            "cue_speed_per_power_unit_cm_s": 1.675,
        },
        "cushion": {
            "normal_restitution": 0.93, "restitution_intercept": 1.0,
            "restitution_slope_per_mps": 0.056,
            "minimum_restitution": 0.0, "maximum_restitution": 0.93,
            "friction_coefficient": 0.14, "nose_height_ratio": 1.4,
            "maximum_rigid_incident_speed_cm_s": 250.0,
            "material": "mathavan_speed_dependent_cushion_v2",
        },
        "table_boundary": {
            "playfield_width_cm": 127.0, "playfield_length_cm": 254.0,
            "corner_mouth_width_cm": 13.2, "side_mouth_width_cm": 8.6,
            "corner_throat_width_cm": 13.2, "side_throat_width_cm": 8.6,
            "jaw_radius_cm": 0.0001, "throat_depth_cm": 6.0,
            "capture_depth_cm": 6.0, "geometry_id": "legacy_opening_band",
            "material": "legacy_table_boundary",
        },
        "solver": {
            "time_step_seconds": 1.0 / 120.0,
            "maximum_events_per_tick": 64,
            "toi_tolerance_seconds": 0.0000001,
            "maximum_island_size": 16, "velocity_iterations": 64,
            "position_iterations": 4, "penetration_slop_cm": 0.001,
            "maximum_penetration_cm": 2.75,
            "residual_tolerance_cm_s": 0.001,
            "passive_energy_tolerance_j": 0.0000000001,
        },
        "frozen_cue_contact": {
            "enabled": False,
            "normal_stiffness_n_per_m32": 12500000.0,
            "normal_dissipation_s_per_m": 0.05,
            "tangential_stiffness_n_per_m": 400000.0,
            "tangential_damping_ns_per_m": 25.0,
            "microstep_seconds": 0.00001,
            "maximum_contact_seconds": 0.006,
            "release_compression_m": 0.00000001,
            "maximum_compression_m": 0.004,
            "maximum_normal_force_n": 10000.0,
        },
    }


def canonical_scenario(scenario_id: str, balls: list[dict],
                       content_version: int = 1) -> dict:
    profile = shipping_physics_profile()
    return {
        "balls": balls,
        "boundary_mode": "production_table",
        "description": f"BilliardGL training initial state for {scenario_id}",
        "evidence": {
            "equipment": "CHINESE_EIGHT_BALL",
            "grade": "C",
            "source": "original_training_design",
        },
        "expectations": [{"metric": "finite_state", "operator": "eq", "value": True}],
        "id": f"{scenario_id}_initial_v{content_version}",
        "physics_profile": profile,
        "schema_version": 12,
        "simulation": {"ticks": 1, "time_step_seconds": 1.0 / 120.0},
    }


def cond(predicate: str, **kwargs: object) -> dict:
    return {"predicate": predicate, **kwargs}


def zone(ball_index: int, x: float, z: float, radius: float) -> dict:
    return cond(
        "ball_final_position_in_zone",
        ball_index=ball_index,
        center_cm=[x, z],
        radius_cm=radius,
    )


def scenario(
    scenario_id: str,
    category: str,
    title_zh: str,
    title_en: str,
    description_zh: str,
    description_en: str,
    tags: list[str],
    balls: list[dict],
    success: list[dict],
    *,
    par_shots: int = 1,
    tip_axis: str = "none",
    preferred_tip: list[float] | None = None,
    aim_assist: str = "basic",
    content_version: int = 1,
    teaching_hints: dict | None = None,
    reference_audits: dict[str, tuple[str, str]] = REFERENCE_AUDITS,
    reference_directory: str = "replays",
    reject_cue_ball_pocketed_during_attempt: bool = False,
    optional_bonus_zone: dict | None = None,
) -> dict:
    initial = canonical_scenario(scenario_id, balls, content_version)
    spin_capability = {
        "vertical": "authoritative_longitudinal_tip_offset_v1",
        "horizontal": "authoritative_horizontal_tip_offset_v1",
    }.get(tip_axis)
    required = ["training_scenario_v1"] + ([spin_capability] if spin_capability else [])
    controls = {
        "actions": ["aim_yaw", "shot_power", "camera_cycle", "reset_attempt"],
        "camera_presets": ["player_shot", "tactical_overhead"],
        "tip_offset": {
            "axis": tip_axis,
            "enabled": tip_axis != "none",
            "preferred_radius_units": preferred_tip or [0.0, 0.0],
        },
        "explicitly_forbidden": ["cue_elevation", "chalk", "auto_shot", "auto_aim"],
    }
    action_hash, trace_hash = reference_audits[scenario_id]
    failure_predicates = [
        cond("numerical_failure"),
        cond("cue_ball_pocketed"),
        cond("shot_limit_exceeded", maximum=par_shots),
        cond("timeout_ticks", maximum=6000 * par_shots),
    ]
    failure_precedence = [
        "numerical_failure",
        "cue_ball_pocketed",
        "shot_limit_exceeded",
        "timeout_ticks",
    ]
    if reject_cue_ball_pocketed_during_attempt:
        failure_predicates.insert(2, cond(
            "cue_ball_pocketed_during_attempt"))
        failure_precedence.insert(2, "cue_ball_pocketed_during_attempt")
    result = {
        "allowed_controls": controls,
        "assist": {
            "allowed": ["aim_line", "first_contact_marker", "target_zone"],
            "default": aim_assist,
            "maximum_level": "training_guides_no_auto_execution",
        },
        "authorship": {
            "asset_policy": "original_billiardgl_content_no_third_party_assets",
            "owner": "BilliardGL",
        },
        "category": category,
        "failure": {
            "any": failure_predicates,
            "evaluation": "after_each_shot_then_at_attempt_end",
            "precedence": failure_precedence,
        },
        "id": scenario_id,
        "initial_scenario": {
            "binding": "embedded_canonical_physics_scenario_v1",
            "canonical_sha256": sha256_bytes(canonical_physics_bytes(initial)),
            "physics_profile_sha256": sha256_bytes(
                canonical_bytes(initial["physics_profile"])),
            "scenario": initial,
        },
        "locale": {
            "description_en": description_en,
            "description_zh_cn": description_zh,
            "title_en": title_en,
            "title_zh_cn": title_zh,
        },
        "reference_replay": {
            "binding_rule": {
                "activation_requires": [
                    "replay_id",
                    "scenario_sha256",
                    "physics_profile_id",
                    "action_log_sha256",
                    "trace_sha256",
                    "review_status_automated_verified",
                ],
                "scenario_sha256_must_equal": sha256_bytes(canonical_physics_bytes(initial)),
                "physics_profile_id": "chinese_pool_interactive_120hz_v5",
                "action_log_sha256": action_hash,
                "trace_sha256": trace_hash,
                "review_status": "automated_verified",
                "trace_must_be_generated_by": "deterministic_fixed_step_runtime",
            },
            "file": f"{reference_directory}/{scenario_id}.replay.json",
            "id": f"training-replay://phase4/{scenario_id}/v{content_version}",
            "status": "bound",
        },
        "reset": {
            "attempt_seed": 0,
            "mode": "atomic_reload_embedded_initial_scenario",
            "preserve": ["accessibility", "camera_preference", "assist_preference"],
            "reset_tick_to": 0,
        },
        "runtime_gate": {
            "on_missing": "visible_locked_no_score",
            "production_default_enabled": spin_capability is None,
            "required_capabilities": required,
        },
        "schema": "training_scenario_v1",
        "schema_version": 1,
        "scoring": {
            "base_points": 100,
            "deductions": [
                {
                    "cap_points": 35,
                    "metric": "cue_ball_target_error_cm",
                    "points_per_unit": 1,
                    "quantization": 0.1,
                },
                {
                    "cap_points": 30,
                    "metric": "shot_count_over_par",
                    "points_per_unit": 30,
                    "quantization": 1,
                },
            ],
            "deterministic": True,
            "maximum": 100,
            "minimum": 0,
            "par_shots": par_shots,
            "rounding": "nearest_integer_half_away_from_zero",
            "tie_break": ["higher_score", "lower_attempt_ticks", "lexicographic_trace_sha256"],
        },
        "success": {
            "all": success + [cond("shot_count_lte", maximum=par_shots)],
            "evaluation": "attempt_end_after_balls_stopped",
        },
        "tags": tags,
        "version": content_version,
    }
    if teaching_hints is not None:
        result["teaching_hints"] = teaching_hints
    if optional_bonus_zone is not None:
        result["scoring"]["optional_bonus_zone"] = optional_bonus_zone
        result["scoring"]["deductions"] = [{
            "metric": "cue_ball_bonus_zone_missed", "quantization": 1,
            "points_per_unit": 20, "cap_points": 20}]
    return result


def build_scenarios() -> list[dict]:
    common = [ball(0, 0.0, 58.0), ball(1, 0.0, -12.0)]
    scenarios = [
        scenario(
            "launch_stop_shot_center_v1", "stun_follow_draw", "中杆定球", "Center-ball stop shot",
            "将1号球打入底袋，并让母球停在碰撞点附近。", "Pocket the 1-ball and stop the cue ball near impact.",
            ["launch12", "stop", "center_ball", "beginner"], common,
            [cond("ball_pocketed", ball_index=1), zone(0, 0.0, -6.0, 13.0)],
        ),
        scenario(
            "launch_follow_shot_v1", "stun_follow_draw", "高杆跟进", "Follow-shot position",
            "使用高杆进球，并让母球越过碰撞点进入前方目标区。", "Use follow to pocket the object ball and enter the forward cue-ball zone.",
            ["launch12", "follow", "top_spin", "intermediate"], common,
            [cond("ball_pocketed", ball_index=1), zone(0, 0.0, -39.0, 15.0)],
            tip_axis="vertical", preferred_tip=[0.0, 0.55],
        ),
        scenario(
            "launch_draw_shot_v1", "stun_follow_draw", "低杆拉回", "Draw-shot recovery",
            "使用低杆进球，并把母球拉回起始方向的目标区。", "Use draw to pocket the object ball and recover into the rear cue-ball zone.",
            ["launch12", "draw", "back_spin", "intermediate"], common,
            [cond("ball_pocketed", ball_index=1), zone(0, 0.0, 25.0, 15.0)],
            tip_axis="vertical", preferred_tip=[0.0, -0.55],
        ),
        scenario(
            "launch_left_english_one_rail_v1", "side_spin_cushion", "左塞一库走位", "Left-English one-rail route",
            "使用左塞完成目标球，并让母球吃一库进入左侧目标区。", "Use left English and one rail to reach the left cue-ball zone.",
            ["launch12", "left_english", "one_rail", "intermediate"],
            [ball(0, 18.0, 52.0), ball(2, 8.0, -8.0)],
            [cond("ball_pocketed", ball_index=2), cond("rail_contact_count_gte", ball_index=0, minimum=1), zone(0, -30.0, 4.0, 18.0)],
            tip_axis="horizontal", preferred_tip=[-0.55, 0.0],
        ),
        scenario(
            "launch_right_english_one_rail_v1", "side_spin_cushion", "右塞一库走位", "Right-English one-rail route",
            "使用右塞完成目标球，并让母球吃一库进入右侧目标区。", "Use right English and one rail to reach the right cue-ball zone.",
            ["launch12", "right_english", "one_rail", "intermediate"],
            [ball(0, -18.0, 52.0), ball(2, -8.0, -8.0)],
            [cond("ball_pocketed", ball_index=2), cond("rail_contact_count_gte", ball_index=0, minimum=1), zone(0, 30.0, 4.0, 18.0)],
            tip_axis="horizontal", preferred_tip=[0.55, 0.0],
        ),
        scenario(
            "launch_side_english_two_rail_v1", "side_spin_cushion", "侧塞两库回位", "Side-English two-rail return",
            "用侧塞控制母球连续吃两库后回到中线目标区。", "Control two cushion contacts with side English and return to center.",
            ["launch12", "side_english", "two_rail", "advanced"],
            [ball(0, 22.0, 62.0), ball(3, 12.0, -3.0)],
            [cond("ball_pocketed", ball_index=3), cond("rail_contact_count_gte", ball_index=0, minimum=2), zone(0, 0.0, 30.0, 17.0)],
            tip_axis="horizontal", preferred_tip=[-0.45, 0.0],
        ),
        scenario(
            "launch_corner_pocket_angle_v1", "pocket_angle", "角袋薄球", "Corner-pocket cut",
            "将目标球切入左下角袋，母球不得落袋。", "Cut the object ball into the lower-left corner without scratching.",
            ["launch12", "corner_pocket", "cut_angle", "beginner"],
            [ball(0, 24.0, 61.0), ball(4, -18.0, -24.0)],
            [cond("ball_pocketed", ball_index=4), cond("ball_not_pocketed", ball_index=0)],
        ),
        scenario(
            "launch_side_pocket_angle_v1", "pocket_angle", "中袋角度", "Side-pocket angle",
            "将目标球切入右侧中袋，并控制母球留在中台。", "Cut the object ball into the right side pocket and hold center table.",
            ["launch12", "side_pocket", "cut_angle", "beginner"],
            [ball(0, -28.0, 55.0), ball(5, 8.0, 8.0)],
            [cond("ball_pocketed", ball_index=5), zone(0, -4.0, 18.0, 24.0)],
        ),
        scenario(
            "launch_two_ball_lane_route_v1", "continuous_position", "两球直线走位", "Two-ball lane route",
            "按1号、2号顺序完成两颗球，并让母球保留在中线。", "Clear balls 1 then 2 while keeping the cue ball in the center lane.",
            ["launch12", "two_shot", "position_play", "intermediate"],
            [ball(0, 0.0, 76.0), ball(1, -22.0, 4.0), ball(2, 24.0, -43.0)],
            [cond("ordered_ball_pockets", ball_indices=[1, 2]), zone(0, 0.0, -5.0, 28.0)], par_shots=2,
        ),
        scenario(
            "launch_three_ball_corner_route_v1", "continuous_position", "三球角袋路线", "Three-ball corner route",
            "按3、4、5号顺序连续清台，限制为三杆。", "Clear balls 3, 4, and 5 in order within three shots.",
            ["launch12", "three_shot", "position_play", "advanced"],
            [ball(0, 0.0, 78.0), ball(3, -28.0, 26.0), ball(4, 26.0, -20.0), ball(5, -20.0, -70.0)],
            [cond("ordered_ball_pockets", ball_indices=[3, 4, 5]), zone(0, 8.0, -45.0, 30.0)], par_shots=3,
        ),
        scenario(
            "launch_head_ball_break_control_v1", "break_cue_control", "正碰开球控白", "Head-ball break control",
            "正碰开球，至少两颗目标球碰库，母球停在中台控制区。", "Hit the head ball on the break, send two object balls to rails, and hold center table.",
            ["launch12", "break", "head_ball", "cue_ball_control", "intermediate"], rack_balls(),
            [cond("first_contact_ball", ball_index=1), cond("distinct_object_balls_with_rail_contact_gte", minimum=2), zone(0, 0.0, 15.0, 29.0)],
        ),
        scenario(
            "launch_cut_break_control_v1", "break_cue_control", "切薄开球控白", "Cut-break cue control",
            "切薄顶球完成有效开球，并将母球留在开球线附近。", "Cut the head ball for a legal break and retain the cue ball near the head string.",
            ["launch12", "break", "cut_break", "cue_ball_control", "advanced"], rack_balls(),
            [cond("first_contact_ball", ball_index=1), cond("distinct_object_balls_with_rail_contact_gte", minimum=2), zone(0, 0.0, 55.0, 24.0)],
        ),
    ]
    return scenarios


def build_scenarios_v2() -> list[dict]:
    """The revised launch catalog with thirteen independently bound drills.

    V1 remains byte-for-byte available.  V2 adds four short, collinear
    introductions and then reuses the remaining nine verified launch drills
    in their existing order.
    """
    short_straight = [ball(0, 10.0, 0.0), ball(1, 40.0, 0.0)]
    first_contact_center_x = 40.0 - 2.0 * BALL_RADIUS_CM
    common_hints = {
        "recommended_tip": {
            "x_ball_radii": 0.0,
            "y_ball_radii": 0.0,
            "label": "center_ball",
        },
        "aim": {"yaw_radians": 0.0, "tolerance_degrees": 1.0},
        "target_pocket_id": "side_px",
        "target_pocket_center_cm": [63.5, 0.0],
        "cue_ball_target_zone": {
            # The visual guide is anchored to the collinear cue-ball centre
            # at first sphere contact.  Scoring below uses the recorded
            # contact position, so this is a guide rather than a fixed oracle.
            "center_cm": [first_contact_center_x, 0.0],
            "radius_cm": 4.5,
            "visible": False,
        },
        "shot_line": {
            "cue_ball_index": 0,
            "object_ball_index": 1,
            "start_cm": [10.0, 0.0],
            "object_center_cm": [40.0, 0.0],
        },
    }
    straight_pot_hints = copy.deepcopy(common_hints)
    straight_pot_hints["recommended_power"] = {
        # The public shot control uses 0..200 requested-power units.  The HUD
        # metadata is percentage-based, hence 45..75 units => 22.5..37.5%.
        "minimum_percent": 22.5,
        "maximum_percent": 37.5,
        "default_percent": 27.5,
    }
    straight_stop_hints = copy.deepcopy(common_hints)
    straight_stop_hints["recommended_power"] = {
        "minimum_percent": 22.5,
        "maximum_percent": 27.5,
        "default_percent": 25.0,
    }
    straight_stop_hints["aim"]["tolerance_degrees"] = 0.45
    straight_stop_hints["cue_ball_target_zone"]["visible"] = True
    straight_follow_hints = copy.deepcopy(common_hints)
    straight_follow_hints["recommended_power"] = {
        "minimum_percent": 27.5,
        "maximum_percent": 35.0,
        "default_percent": 32.5,
    }
    straight_follow_hints["recommended_tip"] = {
        "x_ball_radii": 0.0,
        "y_ball_radii": 0.25,
        "label": "high_tip",
    }
    straight_follow_hints["aim"]["tolerance_degrees"] = 0.5
    straight_follow_hints["cue_ball_target_zone"] = {
        "center_cm": [42.0, 0.0],
        "radius_cm": 5.5,
        "visible": True,
    }
    straight_draw_hints = copy.deepcopy(common_hints)
    straight_draw_hints["recommended_power"] = {
        "minimum_percent": 47.5,
        "maximum_percent": 50.0,
        "default_percent": 50.0,
    }
    straight_draw_hints["recommended_tip"] = {
        "x_ball_radii": 0.0,
        "y_ball_radii": -0.5,
        "label": "low_tip",
    }
    straight_draw_hints["aim"]["tolerance_degrees"] = 0.35
    straight_draw_hints["cue_ball_target_zone"] = {
        "center_cm": [28.0, 0.0],
        "radius_cm": 5.75,
        "visible": True,
    }

    introductions = [
        scenario(
            "launch_straight_pot_intro_v2", "stun_follow_draw",
            "直线进球入门", "Straight pot basics",
            "短距离直线击打1号球入右侧中袋，先建立瞄准与力度的基本手感。",
            "Pocket the 1-ball in the right side pocket on a short straight line.",
            ["launch13", "straight_pot", "center_ball", "beginner"],
            short_straight,
            [cond("ball_pocketed", ball_index=1),
             cond("ball_not_pocketed", ball_index=0)],
            content_version=2,
            teaching_hints=straight_pot_hints,
            reference_audits=REFERENCE_AUDITS_V2,
            reference_directory="replays_v2",
        ),
        scenario(
            "launch_straight_stop_intro_v2", "stun_follow_draw",
            "直线定杆入门", "Straight stop-shot basics",
            "短距离直线进球，并用中杆让母球停在碰撞点附近的目标区。",
            "Pocket the 1-ball and stop the cue ball in the nearby target zone.",
            ["launch13", "straight_stop", "center_ball", "beginner"],
            short_straight,
            [cond("ball_pocketed", ball_index=1),
             cond("ball_not_pocketed", ball_index=0),
             cond("cue_ball_final_near_first_contact_point",
                  radius_cm=4.5)],
            tip_axis="vertical", preferred_tip=[0.0, 0.0],
            content_version=2,
            teaching_hints=straight_stop_hints,
            reference_audits=REFERENCE_AUDITS_V2,
            reference_directory="replays_v2",
        ),
        scenario(
            "launch_straight_follow_intro_v2", "stun_follow_draw",
            "短直线高杆跟进", "Straight follow-shot basics",
            "短距离高杆直线进球，让母球向前跟进且不碰库。",
            "Pocket the 1-ball, then follow forward without touching a rail.",
            ["launch13", "follow", "top_spin", "beginner"],
            short_straight,
            [cond("ball_pocketed", ball_index=1),
             cond("ball_not_pocketed", ball_index=0),
             cond("rail_contact_count_lte", ball_index=0, maximum=0),
             zone(0, 42.0, 0.0, 5.5)],
            tip_axis="vertical", preferred_tip=[0.0, 0.25],
            content_version=2,
            teaching_hints=straight_follow_hints,
            reference_audits=REFERENCE_AUDITS_V2,
            reference_directory="replays_v2",
        ),
        scenario(
            "launch_straight_draw_intro_v2", "stun_follow_draw",
            "短直线低杆拉回", "Straight draw-shot basics",
            "短距离低杆直线进球，让母球向后拉回目标区且不碰库。",
            "Pocket the 1-ball, then draw back into the rear zone without touching a rail.",
            ["launch13", "draw", "back_spin", "beginner"],
            short_straight,
            [cond("ball_pocketed", ball_index=1),
             cond("ball_not_pocketed", ball_index=0),
             cond("rail_contact_count_lte", ball_index=0, maximum=0),
             zone(0, 28.0, 0.0, 5.75)],
            tip_axis="vertical", preferred_tip=[0.0, -0.5],
            content_version=2,
            teaching_hints=straight_draw_hints,
            reference_audits=REFERENCE_AUDITS_V2,
            reference_directory="replays_v2",
        ),
    ]
    def materialize(spec: dict) -> dict:
        item = copy.deepcopy(spec)
        raw_balls = item.pop("balls")
        item["balls"] = [
            value if isinstance(value, dict) else ball(*value)
            for value in raw_balls
        ]
        item["success"] = item.pop("success_conditions")
        if "description_zh" not in item:
            item["description_zh"] = item.pop("goal_zh")
        if "description_en" not in item:
            item["description_en"] = item.pop("goal_en")
        item["reference_audits"] = REFERENCE_AUDITS_V2
        item["reference_directory"] = "replays_v2"
        item["reject_cue_ball_pocketed_during_attempt"] = True
        return scenario(**item)

    spin = [materialize(item) for item in build_remaining_spin()]
    angles_breaks = [materialize(item)
                     for item in build_remaining_angles_breaks()]
    routes = [materialize(item) for item in build_remaining_routes()]
    # Preserve the original curriculum categories/order while replacing every
    # legacy lesson with a calibrated v2 counterpart.
    return (introductions + spin + angles_breaks[:2] + routes +
            angles_breaks[2:])


def build_bad_inputs(scenarios: list[dict]) -> dict[str, dict]:
    fixtures: dict[str, dict] = {}
    value = copy.deepcopy(scenarios[0]); del value["success"]
    fixtures["missing_success.training.invalid.json"] = {"expected_error_code": "E_REQUIRED_SUCCESS", "scenario": value}
    value = copy.deepcopy(scenarios[0]); value["initial_scenario"]["scenario"]["balls"][1]["position_cm"] = value["initial_scenario"]["scenario"]["balls"][0]["position_cm"]
    value["initial_scenario"]["canonical_sha256"] = sha256_bytes(canonical_physics_bytes(value["initial_scenario"]["scenario"]))
    value["reference_replay"]["binding_rule"]["scenario_sha256_must_equal"] = value["initial_scenario"]["canonical_sha256"]
    fixtures["overlapping_balls.training.invalid.json"] = {"expected_error_code": "E_GEOMETRY_OVERLAP", "scenario": value}
    spin_scenario = next(item for item in scenarios
        if item["allowed_controls"]["tip_offset"]["enabled"])
    value = copy.deepcopy(spin_scenario); value["runtime_gate"]["required_capabilities"] = ["training_scenario_v1"]; value["runtime_gate"]["production_default_enabled"] = True
    fixtures["ungated_spin.training.invalid.json"] = {"expected_error_code": "E_SPIN_CAPABILITY_GATE", "scenario": value}
    value = copy.deepcopy(scenarios[0]); del value["reference_replay"]["binding_rule"]["trace_sha256"]
    fixtures["replay_bound_without_hash.training.invalid.json"] = {"expected_error_code": "E_REPLAY_BINDING", "scenario": value}
    value = copy.deepcopy(scenarios[0]); value["scoring"]["deterministic"] = False
    fixtures["nondeterministic_scoring.training.invalid.json"] = {"expected_error_code": "E_SCORING_DETERMINISM", "scenario": value}
    return fixtures


def write_json(path: Path, value: object) -> str:
    payload = pretty_bytes(value)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(payload)
    return sha256_bytes(payload)


def _generate_pack(output: Path, scenarios: list[dict],
                   reference_source: Path, *, pack_id: str,
                   pack_filename: str, coverage_filename: str,
                   generation_filename: str,
                   bad_input_directory: str) -> None:
    scenario_entries = []
    for item in scenarios:
        scenario_directory = "scenarios_v2" if item["version"] == 2 else "scenarios"
        relative = Path(scenario_directory) / f"{item['id']}.training.json"
        digest = write_json(output / relative, item)
        replay_relative = Path(item["reference_replay"]["file"])
        replay_source = reference_source / replay_relative.name
        if not replay_source.is_file() and item["version"] == 1:
            replay_source = DEFAULT_OUTPUT / "replays" / replay_relative.name
        if not replay_source.is_file():
            raise FileNotFoundError(f"missing verified reference replay: {replay_source}")
        replay_destination = output / replay_relative
        replay_destination.parent.mkdir(parents=True, exist_ok=True)
        if replay_source.resolve() != replay_destination.resolve():
            shutil.copyfile(replay_source, replay_destination)
        scenario_entries.append({"category": item["category"], "file": relative.as_posix(), "id": item["id"], "sha256": digest,
            "reference_replay": {"file": replay_relative.as_posix(),
                                 "sha256": sha256_bytes(replay_destination.read_bytes())}})

    bad_entries = []
    for filename, fixture in build_bad_inputs(scenarios).items():
        relative = Path(bad_input_directory) / filename
        digest = write_json(output / relative, fixture)
        bad_entries.append({"expected_error_code": fixture["expected_error_code"], "file": relative.as_posix(), "sha256": digest})

    categories = {
        name: sum(item["category"] == name for item in scenarios)
        for name in ("stun_follow_draw", "side_spin_cushion", "pocket_angle", "continuous_position", "break_cue_control")
    }
    coverage = {
        "category_counts": categories,
        "difficulty_counts": {
            level: sum(level in item["tags"] for item in scenarios)
            for level in ("beginner", "intermediate", "advanced")
        },
        "gated_scenarios": sorted(item["id"] for item in scenarios if not item["runtime_gate"]["production_default_enabled"]),
        "scenario_count": len(scenarios),
        "schema": "training_scenario_v1_coverage_matrix",
        "schema_version": 1,
        "success_predicates": sorted({c["predicate"] for item in scenarios for c in item["success"]["all"]}),
    }
    coverage_digest = write_json(output / coverage_filename, coverage)
    pack = {
        "bad_input_fixtures": bad_entries,
        "content_policy": "original_billiardgl_content_no_third_party_assets",
        "coverage_matrix": {"file": coverage_filename, "sha256": coverage_digest},
        "determinism_contract": {
            "canonical_json": "UTF-8_sorted_keys_LF",
            "forbidden_sources": ["wall_clock", "platform_rng", "unordered_iteration", "visual_frame_rate"],
            "initial_state_seed": 0,
            "static_checks": ["schema", "hashes", "geometry", "predicate_vocabulary", "scoring", "capability_gates", "replay_binding"],
        },
        "id": pack_id,
        "scenario_schema": "training_scenario_v1",
        "scenarios": scenario_entries,
        "schema": "training_pack_v1",
        "schema_version": 1,
    }
    generation_source = reference_source.parent / generation_filename
    if generation_source.is_file():
        generation_destination = output / generation_filename
        if generation_source.resolve() != generation_destination.resolve():
            shutil.copyfile(generation_source, generation_destination)
        pack["reference_generation"] = {
            "file": generation_destination.name,
            "sha256": sha256_bytes(generation_destination.read_bytes()),
        }
    write_json(output / pack_filename, pack)


def generate_v1(output: Path, reference_source: Path | None = None) -> None:
    _generate_pack(
        output, build_scenarios(),
        reference_source or (DEFAULT_OUTPUT / "replays"),
        pack_id="billiardgl_phase4_launch12_v1",
        pack_filename="training_pack_v1.json",
        coverage_filename="coverage_matrix.json",
        generation_filename="reference_generation.json",
        bad_input_directory="bad_inputs",
    )


def generate_v2(output: Path, reference_source: Path | None = None) -> None:
    _generate_pack(
        output, build_scenarios_v2(),
        reference_source or (DEFAULT_OUTPUT / "replays_v2"),
        pack_id="billiardgl_phase4_launch13_v2",
        pack_filename="training_pack_v2.json",
        coverage_filename="coverage_matrix_v2.json",
        generation_filename="reference_generation_v2.json",
        bad_input_directory="bad_inputs_v2",
    )


def generate(output: Path, reference_source: Path | None = None,
             include_v2: bool = True) -> None:
    generate_v1(output, reference_source)
    if include_v2:
        v2_source = None if reference_source is None else (
            reference_source.parent / "replays_v2"
            if reference_source.name == "replays" else reference_source)
        generate_v2(output, v2_source)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--reference-source", type=Path,
        default=DEFAULT_OUTPUT / "replays")
    parser.add_argument("--v1-only", action="store_true")
    args = parser.parse_args()
    generate(args.output.resolve(), args.reference_source.resolve(),
             include_v2=not args.v1_only)
    print(f"generated Phase 4 training pack: {args.output.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
