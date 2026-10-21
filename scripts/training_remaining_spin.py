"""Revised side-English lessons and real public-physics calibration."""

from __future__ import annotations

import copy
import hashlib
import json
import math
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "scripts"))

from calibrate_phase4_reference_replays import (  # noqa: E402
    AutomationClient,
    all_trace_frames,
    evaluate_candidate,
    facts_from_frames,
    teaching_range_universe,
)


LEFT_ID = "launch_left_english_one_rail_v2"
RIGHT_ID = "launch_right_english_one_rail_v2"
TWO_RAIL_ID = "launch_side_english_two_rail_v2"
IDS = [LEFT_ID, RIGHT_ID, TWO_RAIL_ID]

LEFT_YAW = 0.20483780000248009
RIGHT_YAW = math.pi - LEFT_YAW
TWO_RAIL_YAW = 0.19983780000248009

REFERENCE_ACTIONS = {
    LEFT_ID: [(LEFT_YAW, 80.0, -0.35, 0.0)],
    RIGHT_ID: [(RIGHT_YAW, 80.0, 0.35, 0.0)],
    TWO_RAIL_ID: [(TWO_RAIL_YAW, 95.0, -0.35, 0.0)],
}
LEGACY_ACTIONS = {
    "launch_left_english_one_rail_v1":
        [(-1.6859450042095234, 140.0, -0.4, 0.0)],
    "launch_right_english_one_rail_v1":
        [(-1.45564764938027, 140.0, 0.4, 0.0)],
    "launch_side_english_two_rail_v1":
        [(-1.7734456551901618, 180.0, -0.25, 0.0)],
}


def condition(predicate: str, **fields: object) -> dict:
    return {"predicate": predicate, **fields}


def zone(ball_index: int, x: float, z: float, radius: float) -> dict:
    return condition("ball_final_position_in_zone", ball_index=ball_index,
                     center_cm=[x, z], radius_cm=radius)


def hints(*, power_min: float, power_default: float, power_max: float,
          tip_x: float, tip_label: str, yaw: float, tolerance_degrees: float,
          pocket_id: str, pocket_center: tuple[float, float],
          cue_start: tuple[float, float], object_index: int,
          object_center: tuple[float, float], target: tuple[float, float],
          target_radius: float, goal: str) -> dict:
    return {
        "goal_en": goal,
        "recommended_power": {
            "minimum_percent": power_min,
            "default_percent": power_default,
            "maximum_percent": power_max,
        },
        "recommended_tip": {
            "x_ball_radii": tip_x,
            "y_ball_radii": 0.0,
            "label": tip_label,
        },
        "aim": {"yaw_radians": yaw,
                "tolerance_degrees": tolerance_degrees},
        # These routes use calibrated cut angles; recomputing a centre-ball
        # ghost aim would erase the demonstrated English response.
        "aim_at_pocket": False,
        "target_pocket_enabled": True,
        "target_pocket_id": pocket_id,
        "target_pocket_center_cm": list(pocket_center),
        "cue_ball_target_zone": {
            "center_cm": list(target),
            "radius_cm": target_radius,
            "visible": True,
        },
        "shot_line": {
            "cue_ball_index": 0,
            "object_ball_index": object_index,
            "start_cm": list(cue_start),
            "object_center_cm": list(object_center),
        },
    }


def build_remaining_spin() -> list[dict]:
    common_one_rail = dict(
        category="side_spin_cushion",
        tags=["launch13", "side_english", "one_rail", "beginner"],
        balls=[(0, 10.0, 80.0), (2, 40.0, 90.0)],
        par_shots=1,
        tip_axis="horizontal",
        content_version=2,
    )
    left = dict(
        scenario_id=LEFT_ID,
        title_zh="左塞一库定线",
        title_en="Left-English one-rail line",
        description_zh="将2号球打入右上角袋，母球恰好吃一库后停入目标圈。",
        description_en="Pocket the 2-ball in the upper-right corner; use exactly one cue-ball rail to reach the ring.",
        preferred_tip=[-0.35, 0.0],
        success_conditions=[
            condition("ball_pocketed_in_pocket", ball_index=2, pocket_id=3),
            condition("ball_not_pocketed", ball_index=0),
            condition("rail_contact_count_gte", ball_index=0, minimum=1),
            condition("rail_contact_count_lte", ball_index=0, maximum=1),
            zone(0, 6.0, 54.0, 20.0),
        ],
        teaching_hints=hints(
            power_min=38.75, power_default=40.0, power_max=41.25,
            tip_x=-0.35, tip_label="left_english", yaw=LEFT_YAW,
            tolerance_degrees=0.25,
            pocket_id="corner_px_pz", pocket_center=(63.5, 127.0),
            cue_start=(10.0, 80.0), object_index=2,
            object_center=(40.0, 90.0), target=(6.0, 54.0),
            target_radius=20.0,
            goal="Pot ball 2; cue: exactly 1 rail, then finish in ring"),
        **common_one_rail,
    )
    right = copy.deepcopy(left)
    right.update(
        scenario_id=RIGHT_ID,
        title_zh="右塞一库定线",
        title_en="Right-English one-rail line",
        description_zh="将2号球打入左上角袋，母球恰好吃一库后停入目标圈。",
        description_en="Pocket the 2-ball in the upper-left corner; use exactly one cue-ball rail to reach the ring.",
        balls=[(0, -10.0, 80.0), (2, -40.0, 90.0)],
        preferred_tip=[0.35, 0.0],
        success_conditions=[
            condition("ball_pocketed_in_pocket", ball_index=2, pocket_id=2),
            condition("ball_not_pocketed", ball_index=0),
            condition("rail_contact_count_gte", ball_index=0, minimum=1),
            condition("rail_contact_count_lte", ball_index=0, maximum=1),
            zone(0, -6.0, 54.0, 20.0),
        ],
        teaching_hints=hints(
            power_min=38.75, power_default=40.0, power_max=41.25,
            tip_x=0.35, tip_label="right_english", yaw=RIGHT_YAW,
            tolerance_degrees=0.25,
            pocket_id="corner_nx_pz", pocket_center=(-63.5, 127.0),
            cue_start=(-10.0, 80.0), object_index=2,
            object_center=(-40.0, 90.0), target=(-6.0, 54.0),
            target_radius=20.0,
            goal="Pot ball 2; cue: exactly 1 rail, then finish in ring"),
    )
    two_rail = dict(
        scenario_id=TWO_RAIL_ID,
        category="side_spin_cushion",
        title_zh="侧塞两库回位",
        title_en="Side-English two-rail return",
        description_zh="将2号球打入右上角袋，母球恰好连续吃两库后回到目标圈。",
        description_en="Pocket the 2-ball in the upper-right corner; use exactly two cue-ball rails to return to the ring.",
        tags=["launch13", "side_english", "two_rail", "intermediate"],
        balls=[(0, 10.0, 80.0), (2, 40.0, 90.0)],
        success_conditions=[
            condition("ball_pocketed_in_pocket", ball_index=2, pocket_id=3),
            condition("ball_not_pocketed", ball_index=0),
            condition("rail_contact_count_gte", ball_index=0, minimum=2),
            condition("rail_contact_count_lte", ball_index=0, maximum=2),
            zone(0, -12.0, 4.0, 32.0),
        ],
        par_shots=1,
        tip_axis="horizontal",
        preferred_tip=[-0.35, 0.0],
        content_version=2,
        teaching_hints=hints(
            power_min=46.25, power_default=47.5, power_max=48.75,
            tip_x=-0.35, tip_label="left_english", yaw=TWO_RAIL_YAW,
            tolerance_degrees=0.25,
            pocket_id="corner_px_pz", pocket_center=(63.5, 127.0),
            cue_start=(10.0, 80.0), object_index=2,
            object_center=(40.0, 90.0), target=(-12.0, 4.0),
            target_radius=32.0,
            goal="Pot ball 2; cue: exactly 2 rails, then finish in ring"),
    )
    return [left, right, two_rail]


def build_scenarios() -> list[dict]:
    """Materialize standalone fixtures without introducing an import cycle."""
    from generate_phase4_training_pack import ball, scenario

    result = []
    for spec in build_remaining_spin():
        item = copy.deepcopy(spec)
        item["balls"] = [ball(*value) for value in item["balls"]]
        item["success"] = item.pop("success_conditions")
        item["reference_audits"] = {key: ("0" * 64, "1" * 64)
                                      for key in IDS}
        item["reference_directory"] = "replays_v2"
        item["reject_cue_ball_pocketed_during_attempt"] = True
        result.append(scenario(**item))
    return result


def verify(binary: str, output: str) -> None:
    binary_path = Path(binary).resolve()
    binary_sha = hashlib.sha256(binary_path.read_bytes()).hexdigest()
    output_path = Path(output)
    output_path.mkdir(parents=True, exist_ok=True)
    reports = []
    legacy_reports = []
    with tempfile.TemporaryDirectory(prefix="billiardgl-spin-lessons-") as raw:
        fixture_root = Path(raw)
        for scenario_value in build_scenarios():
            scenario_path = fixture_root / f"{scenario_value['id']}.training.json"
            scenario_path.write_text(json.dumps(scenario_value, sort_keys=True))
            with AutomationClient(str(binary_path), mode="headless") as client:
                for candidate in teaching_range_universe(scenario_value, 1000):
                    result = evaluate_candidate(client, scenario_path, candidate)
                    result["scenario_id"] = scenario_value["id"]
                    reports.append(result)
                default = REFERENCE_ACTIONS[scenario_value["id"]][0]
                control = evaluate_candidate(
                    client, scenario_path,
                    (default[0], default[1], 0.0, 0.0))
                reports.append({"scenario_id": scenario_value["id"],
                                "center_tip_control": control})
        with AutomationClient(str(binary_path), mode="headless") as client:
            for scenario_id, actions in LEGACY_ACTIONS.items():
                scenario_path = (ROOT / "assets" / "training" / "phase4" /
                                 "scenarios" /
                                 f"{scenario_id}.training.json")
                result = evaluate_candidate(client, scenario_path, actions[0])
                result["scenario_id"] = scenario_id
                legacy_reports.append(result)
    if hashlib.sha256(binary_path.read_bytes()).hexdigest() != binary_sha:
        raise RuntimeError("Executable changed during calibration")
    attempts = [item for item in reports if "center_tip_control" not in item]
    controls = [item for item in reports if "center_tip_control" in item]
    expected_rails = {LEFT_ID: 1, RIGHT_ID: 1, TWO_RAIL_ID: 2}
    controls_distinct = all(
        (control["center_tip_control"]["facts"]["rail_contacts"][0] !=
         expected_rails[control["scenario_id"]]) or
        (control["center_tip_control"]["score"]["succeeded"] is False)
        for control in controls)
    legacy_exceeds_claim = all(
        item["facts"]["rail_contacts"][0] >
        (2 if item["scenario_id"].endswith("two_rail_v1") else 1)
        for item in legacy_reports)
    payload = {
        "schema": "billiardgl.phase4.remaining_spin_calibration.v1",
        "binary_sha256": binary_sha,
        "specs_sha256": hashlib.sha256(json.dumps(
            build_remaining_spin(), sort_keys=True,
            separators=(",", ":")).encode()).hexdigest(),
        "module_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "method": "real_public_automation_full_teaching_lattice_plus_center_tip_controls",
        "attempt_count": len(attempts),
        "all_succeeded": all(item["score"]["succeeded"] for item in attempts),
        "center_tip_controls_distinct": controls_distinct,
        "legacy_demos_exceed_named_rail_count": legacy_exceeds_claim,
        "attempts": attempts,
        "center_tip_controls": controls,
        "legacy_demo_findings": legacy_reports,
    }
    (output_path / "remaining_spin.range.json").write_text(
        json.dumps(payload, indent=2, sort_keys=True) + "\n")
    print(json.dumps({key: value for key, value in payload.items()
                      if key not in {"attempts", "center_tip_controls",
                                     "legacy_demo_findings"}},
                     sort_keys=True))
    if not (payload["all_succeeded"] and controls_distinct and
            legacy_exceeds_claim):
        raise RuntimeError("Side-English calibration contract failed")


if __name__ == "__main__":
    verify(sys.argv[1], sys.argv[2])
