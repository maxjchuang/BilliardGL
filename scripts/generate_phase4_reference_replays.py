#!/usr/bin/env python3
"""Record and bind the twelve verified Phase 4 reference replays.

Both passes execute the real public automation surface and Replay Recorder.
The first pass proves the action/trace digests without depending on the
enclosing training-file hash.  The second pass replays the same semantic
actions from a fresh scenario and binds the finalized file hash from tick 0.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tests" / "e2e"))
from automation_client import AutomationClient  # noqa: E402
from scripts.generate_phase4_training_pack import (  # noqa: E402
    REFERENCE_AUDITS,
    REFERENCE_AUDITS_V2,
    build_scenarios,
    build_scenarios_v2,
    generate_v1,
    generate_v2,
    write_json,
)
from scripts.validate_phase4_training_pack import validate_pack  # noqa: E402
from scripts.training_remaining_angles_breaks import (  # noqa: E402
    REFERENCE_ACTIONS as ANGLES_BREAKS_ACTIONS,
)
from scripts.training_remaining_routes import (  # noqa: E402
    REFERENCE_ACTIONS as ROUTE_ACTIONS,
)
from scripts.training_remaining_spin import (  # noqa: E402
    REFERENCE_ACTIONS as SPIN_ACTIONS,
)

EXECUTABLE_VERSION = "billiardgl-product-source-v1"
ACTIONS = {
    "launch_stop_shot_center_v1": [(-1.5957963267948965, 160, 0, 0)],
    "launch_follow_shot_v1": [(-1.6127963267948966, 160, 0, .25)],
    "launch_draw_shot_v1": [(-1.6167963267948966, 159, 0, -.4)],
    "launch_left_english_one_rail_v1": [(-1.6859450042095234, 140, -.4, 0)],
    "launch_right_english_one_rail_v1": [(-1.45564764938027, 140, .4, 0)],
    "launch_side_english_two_rail_v1": [(-1.7734456551901618, 180, -.25, 0)],
    "launch_corner_pocket_angle_v1": [(-2.029726989647297, 85, 0, 0)],
    "launch_side_pocket_angle_v1": [(-.9171604298753748, 160, 0, 0)],
    "launch_two_ball_lane_route_v1": [
        (-1.9334165703363158, 200, 0, 0),
        (-2.2174037217455727, 160, 0, 0),
    ],
    "launch_three_ball_corner_route_v1": [
        (-2.0969540466522996, 100, 0, 0),
        (1.3450324790464807, 200, 0, 0),
        (1.2116120197536073, 180, 0, 0),
    ],
    "launch_head_ball_break_control_v1": [(-1.5707963267948966, 140, 0, 0)],
    "launch_cut_break_control_v1": [(-1.5450235670294887, 178, 0, 0)],
}
ACTIONS_V2 = {
    "launch_straight_pot_intro_v2": [(0.0, 55, 0, 0)],
    "launch_straight_stop_intro_v2": [(0.0, 50, 0, 0)],
    "launch_straight_follow_intro_v2": [(0.0, 65, 0, .25)],
    "launch_straight_draw_intro_v2": [(0.0, 100, 0, -.5)],
    **SPIN_ACTIONS,
    **ANGLES_BREAKS_ACTIONS,
    **ROUTE_ACTIONS,
}


def file_sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_staging_scenarios(root: Path, scenarios: list[dict]) -> dict[str, Path]:
    paths = {}
    for item in scenarios:
        path = root / "scenarios" / f"{item['id']}.training.json"
        write_json(path, item)
        paths[item["id"]] = path
    return paths


def record_pass(binary: Path, scenarios: dict[str, Path], actions_by_id: dict,
                output: Path, bind_final_content: bool) -> dict[str, Path]:
    output.mkdir(parents=True, exist_ok=True)
    result = {}
    with AutomationClient(str(binary)) as client:
        for scenario_id, actions in actions_by_id.items():
            scenario = scenarios[scenario_id]
            replay = output / f"{scenario_id}.replay.json"
            client.reset()
            client.command("training_load", {"schema_version": 1,
                                              "path": str(scenario)})
            client.command("replay_record_begin", {
                "schema_version": 1,
                "executable_version": EXECUTABLE_VERSION,
                "content_sha256": file_sha(scenario) if bind_final_content
                    else "0" * 64,
                "seed": 0,
            })
            for yaw, power, tip_x, tip_y in actions:
                client.set_aim_yaw(yaw)
                client.set_shot_power(power)
                client.command("set_tip_offset", {
                    "schema_version": 2, "x": tip_x, "y": tip_y})
                client.shoot()
                client.run_until("balls_stopped", max_steps=6000)
            client.command("replay_record_finish", {"schema_version": 1})
            client.command("replay_record_save", {
                "schema_version": 1, "path": str(replay)})
            result[scenario_id] = replay
    return result


def audit_pass(audit_binary: Path, scenarios: dict[str, Path],
               replays: dict[str, Path], expected: dict[str, tuple[str, str]] | None
               ) -> dict[str, tuple[str, str]]:
    result = {}
    for scenario_id, replay in replays.items():
        completed = subprocess.run([
            str(audit_binary), "--audit-reference",
            str(scenarios[scenario_id]), str(replay),
        ], check=True, capture_output=True, text=True)
        audit = json.loads(completed.stdout)
        if not audit["ok"]:
            raise RuntimeError(f"{scenario_id}: {audit['error_code']}")
        pair = (audit["action_log_sha256"], audit["trace_sha256"])
        if expected is not None and pair != expected[scenario_id]:
            raise RuntimeError(f"{scenario_id}: frozen reference digest drift")
        result[scenario_id] = pair
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--audit-binary", type=Path, required=True)
    parser.add_argument("--build-type", required=True)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--compiler-flags", required=True)
    parser.add_argument("--output", type=Path,
        default=ROOT / "assets" / "training" / "phase4")
    parser.add_argument("--pack-version", type=int, choices=(1, 2), default=1)
    parser.add_argument("--discover-audits", action="store_true")
    args = parser.parse_args()
    binary = args.binary.resolve()
    audit_binary = args.audit_binary.resolve()
    pack_version = args.pack_version
    scenario_values = build_scenarios_v2() if pack_version == 2 else build_scenarios()
    actions_by_id = ACTIONS_V2 if pack_version == 2 else ACTIONS
    expected_audits = REFERENCE_AUDITS_V2 if pack_version == 2 else REFERENCE_AUDITS
    if set(actions_by_id) != {item["id"] for item in scenario_values}:
        raise RuntimeError("reference action/scenario set mismatch")
    with tempfile.TemporaryDirectory(prefix="billiardgl-phase4-reference-") as raw:
        temporary = Path(raw)
        scenarios = write_staging_scenarios(
            temporary / "staging", scenario_values)
        first = record_pass(binary, scenarios, actions_by_id,
            temporary / "pass1", False)
        first_audit = audit_pass(audit_binary, scenarios, first,
            None if args.discover_audits else expected_audits)
        second = record_pass(binary, scenarios, actions_by_id,
            temporary / "pass2", True)
        second_audit = audit_pass(audit_binary, scenarios, second,
            None if args.discover_audits else expected_audits)
        if first_audit != second_audit:
            raise RuntimeError("reference two-pass digest mismatch")
        if args.discover_audits:
            print(json.dumps({key: list(value) for key, value in
                sorted(first_audit.items())}, indent=2, sort_keys=True))
            return 0
        if pack_version == 2:
            generate_v2(args.output.resolve(), temporary / "pass2")
        else:
            generate_v1(args.output.resolve(), temporary / "pass2")
        compiler = args.compiler.resolve()
        compiler_version = subprocess.run(
            [str(compiler), "--version"], check=True, capture_output=True,
            text=True).stdout.splitlines()[0]
        generation = {
            "audit_binary_sha256": file_sha(audit_binary),
            "binary_sha256": file_sha(binary),
            "build_type": args.build_type,
            "compiler": str(compiler),
            "compiler_flags": args.compiler_flags,
            "compiler_version": compiler_version,
            "executable_version": EXECUTABLE_VERSION,
            "method": "fresh_runtime_real_actions_two_pass_recorder",
            "portability": "terminal_hash_verified_only_with_matching_build_configuration",
            "scenario_count": len(actions_by_id),
            "schema": "training_reference_generation_v1",
            "schema_version": 1,
        }
        generation_name = ("reference_generation_v2.json"
                           if pack_version == 2 else "reference_generation.json")
        generation_path = args.output.resolve() / generation_name
        generation_sha = write_json(generation_path, generation)
        pack_path = args.output.resolve() / (
            "training_pack_v2.json" if pack_version == 2 else
            "training_pack_v1.json")
        pack = json.loads(pack_path.read_text(encoding="utf-8"))
        pack["reference_generation"] = {
            "file": generation_path.name,
            "sha256": generation_sha,
        }
        write_json(pack_path, pack)
        validated = validate_pack(pack_path)
        print(json.dumps({"scenarios": validated["scenarios"],
                          "two_pass_match": True,
                          "output": str(args.output.resolve())},
                         sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
