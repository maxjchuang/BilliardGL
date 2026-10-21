#!/usr/bin/env python3
"""Bounded search for real shipping-profile Phase 4 training actions.

This drives only public automation commands. It never edits a scenario,
injects AttemptFacts, or treats a predicted path as a successful attempt.
"""

from __future__ import annotations

import argparse
import hashlib
import itertools
import json
import math
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests" / "e2e"))
from automation_client import AutomationClient  # noqa: E402

RADIUS = 2.8575
POCKETS = [(-63.5, -127.0), (63.5, -127.0),
           (-63.5, 127.0), (63.5, 127.0),
           (-63.5, 0.0), (63.5, 0.0)]


def file_sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def angle_to(cue: tuple[float, float], target: tuple[float, float]) -> float:
    return math.atan2(target[1] - cue[1], target[0] - cue[0])


def ghost_yaw(cue: tuple[float, float], ball: tuple[float, float],
              pocket: tuple[float, float]) -> float:
    dx, dz = pocket[0] - ball[0], pocket[1] - ball[1]
    length = math.hypot(dx, dz)
    ghost = (ball[0] - 2.0 * RADIUS * dx / length,
             ball[1] - 2.0 * RADIUS * dz / length)
    return angle_to(cue, ghost)


def candidate_universe(scenario: dict, maximum: int):
    balls = {item["index"]: (item["position_cm"][0], item["position_cm"][2])
             for item in scenario["initial_scenario"]["scenario"]["balls"]}
    cue = balls[0]
    objects = [balls[index] for index in sorted(balls) if index]
    bases: list[float] = []
    for ball in objects[:3]:
        bases.append(angle_to(cue, ball))
        bases.extend(ghost_yaw(cue, ball, pocket) for pocket in POCKETS)
    offsets = [0.0, -0.012, 0.012, -0.025, 0.025, -0.05, 0.05,
               -0.085, 0.085, -0.13, 0.13]
    powers = [55.0, 70.0, 85.0, 100.0, 120.0, 140.0, 160.0, 180.0, 200.0]
    tip = scenario["allowed_controls"]["tip_offset"]
    axis = tip["axis"]
    preferred = tip["preferred_radius_units"]
    tips = [(0.0, 0.0)]
    if axis == "horizontal":
        sign = -1.0 if preferred[0] < 0.0 else 1.0
        tips = [(sign * value, 0.0) for value in (0.25, 0.4, 0.55)]
    elif axis == "vertical":
        sign = -1.0 if preferred[1] < 0.0 else 1.0
        tips = [(0.0, sign * value) for value in (0.25, 0.4)]
    seen: set[tuple[float, float, float, float]] = set()
    produced = 0
    for base, offset, power, (tip_x, tip_y) in itertools.product(
            bases, offsets, powers, tips):
        candidate = (base + offset, power, tip_x, tip_y)
        key = tuple(round(value, 12) for value in candidate)
        if key in seen:
            continue
        seen.add(key)
        yield candidate
        produced += 1
        if produced >= maximum:
            return


def teaching_range_universe(scenario: dict, maximum: int):
    """Enumerate a fixed lattice over the advertised beginner range.

    Teaching power is stored as a percentage while the public control is
    0..200 requested-power units.  Endpoints, midpoint, and 5-unit interior
    steps are all exercised; aim checks both endpoints and the center.
    """
    hints = scenario["teaching_hints"]
    power = hints["recommended_power"]
    minimum = 2.0 * float(power["minimum_percent"])
    maximum_power = 2.0 * float(power["maximum_percent"])
    values = [minimum]
    cursor = math.ceil(minimum / 5.0) * 5.0
    while cursor < maximum_power:
        if cursor > minimum:
            values.append(cursor)
        cursor += 5.0
    if maximum_power != values[-1]:
        values.append(maximum_power)
    # A narrow beginner range can otherwise contain endpoints only.  Preserve
    # the existing five-unit lattice and add one midpoint so calibration still
    # exercises the interior response curve.
    if len(values) == 2:
        values.insert(1, (minimum + maximum_power) * 0.5)
    tolerance = math.radians(float(hints["aim"]["tolerance_degrees"]))
    center = float(hints["aim"]["yaw_radians"])
    yaws = [center - tolerance, center - tolerance * 0.5, center,
            center + tolerance * 0.5, center + tolerance]
    tip = hints["recommended_tip"]
    candidate_count = 0
    for power_value, yaw in itertools.product(values, yaws):
        yield (yaw, power_value, float(tip["x_ball_radii"]),
               float(tip["y_ball_radii"]))
        candidate_count += 1
        if candidate_count >= maximum:
            return


def all_trace_frames(client: AutomationClient, final_tick: int) -> list[dict]:
    frames: list[dict] = []
    after = 0
    while after < final_tick:
        page = client.physics_trace(after, 1000).get("frames", [])
        if not page:
            break
        frames.extend(page)
        next_after = int(page[-1]["tick"])
        if next_after <= after:
            break
        after = next_after
    return frames


def facts_from_frames(frames: list[dict]) -> dict:
    ordered = []
    for frame in frames:
        for contact in frame.get("contacts", []):
            ordered.append((int(frame["tick"]),
                float(contact.get("whole_tick_time_of_impact_seconds", 0.0)),
                contact))
    ordered.sort(key=lambda item: (item[0], item[1],
                                   int(item[2].get("solver_event_id", -1))))
    first = -1
    cue_position_at_first_contact = None
    rails = [0] * 16
    pockets: list[int] = []
    pocket_seen: set[int] = set()
    for _, _, contact in ordered:
        kind = contact.get("kind")
        a, b = int(contact.get("first_ball", -1)), int(contact.get("second_ball", -1))
        if kind == "ball_ball" and first < 0:
            if a == 0 and b > 0:
                first = b
                cue_position_at_first_contact = contact.get(
                    "first_position_at_contact_cm")
            elif b == 0 and a > 0:
                first = a
                cue_position_at_first_contact = contact.get(
                    "second_position_at_contact_cm")
        elif kind == "rail" and 0 <= a < 16:
            rails[a] += 1
        elif (kind == "pocket" and 0 <= a < 16 and
              contact.get("pocket_phase_after") == "captured" and
              a not in pocket_seen):
            pocket_seen.add(a)
            pockets.append(a)
    result = {
        "first_contact_ball": first,
        "rail_contacts": rails,
        "distinct_object_balls_with_rail_contact": sum(
            count > 0 for count in rails[1:]),
        "pocket_order": pockets,
    }
    if cue_position_at_first_contact is not None:
        result["cue_ball_position_at_first_contact_cm"] = [
            cue_position_at_first_contact["x"],
            cue_position_at_first_contact["y"],
            cue_position_at_first_contact["z"],
        ]
    return result


def evaluate_candidate(client: AutomationClient, scenario_path: Path,
                       candidate: tuple[float, float, float, float]) -> dict:
    yaw, power, tip_x, tip_y = candidate
    # training_load replaces the scenario GameState, but a long-lived product
    # runtime also owns journals, capture generations, and other lifecycle
    # state.  Reset that authority before every lattice point so one failed or
    # scratched candidate cannot influence the next candidate.
    client.reset()
    client.command("training_load", {"schema_version": 1,
                                     "path": str(scenario_path)})
    client.start_physics_trace()
    client.toggle_aim()
    client.set_aim_yaw(yaw)
    client.set_shot_power(power)
    client.command("set_tip_offset", {"schema_version": 2,
                                      "x": tip_x, "y": tip_y})
    client.shoot()
    settled = client.run_until("balls_stopped", max_steps=6000)
    state = client.state()
    facts = facts_from_frames(all_trace_frames(client, int(state["tick"])))
    score = client.command("training_score", {
        "schema_version": 1, "shot_count": 1,
        "elapsed_ticks": int(state["tick"]), **facts})
    return {
        "actions": {"yaw": yaw, "power": power,
                    "tip_x": tip_x, "tip_y": tip_y},
        "settled": settled, "facts": facts, "score": score,
        "final_tick": int(state["tick"]),
        "final_cue_position": state["balls"][0]["position"],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--scenario", required=True)
    parser.add_argument("--pack-root", type=Path,
        default=ROOT / "assets/training/phase4")
    parser.add_argument("--max-candidates", type=int, default=384)
    parser.add_argument("--verify-teaching-range", action="store_true")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    scenario_directory = "scenarios_v2" if args.scenario.endswith("_v2") \
        else "scenarios"
    scenario_path = (args.pack_root / scenario_directory /
                     f"{args.scenario}.training.json").resolve()
    scenario = json.loads(scenario_path.read_text(encoding="utf-8"))
    attempts = []
    winner = None
    with AutomationClient(str(args.binary.resolve()), mode="headless") as client:
        universe = (teaching_range_universe(scenario, args.max_candidates)
                    if args.verify_teaching_range else
                    candidate_universe(scenario, args.max_candidates))
        for ordinal, candidate in enumerate(universe, 1):
            try:
                result = evaluate_candidate(client, scenario_path, candidate)
            except Exception as error:
                result = {"actions": {"yaw": candidate[0],
                    "power": candidate[1], "tip_x": candidate[2],
                    "tip_y": candidate[3]}, "error": str(error)}
            result["ordinal"] = ordinal
            attempts.append(result)
            if result.get("score", {}).get("succeeded") and not args.verify_teaching_range:
                winner = result
                break
        if args.verify_teaching_range and attempts and all(
                item.get("score", {}).get("succeeded") for item in attempts):
            winner = attempts[len(attempts) // 2]
    report = {
        "schema": "billiardgl.phase4.reference_calibration.v1",
        "scenario_id": args.scenario,
        "scenario_sha256": file_sha(scenario_path),
        "binary_sha256": file_sha(args.binary.resolve()),
        "candidate_limit": args.max_candidates,
        "attempt_count": len(attempts),
        "mode": "verify_teaching_range" if args.verify_teaching_range else
            "first_success_search",
        "all_candidates_succeeded": bool(attempts) and all(
            item.get("score", {}).get("succeeded") for item in attempts),
        "winner": winner,
        "attempts": attempts,
    }
    payload = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(payload, encoding="utf-8")
    print(payload, end="")
    return 0 if winner else 2


if __name__ == "__main__":
    raise SystemExit(main())
