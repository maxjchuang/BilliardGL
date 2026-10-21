#!/usr/bin/env python3
"""Generate deterministic, validated Replay v1 fixtures for all 12 drills.

The authoritative simulation is executed by the built Billiards headless
automation binary. This script only drives public commands and verifies every
scenario/content/profile/executable/file binding written to the fixture index.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests" / "e2e"))
from automation_client import AutomationClient  # noqa: E402

FIXTURE_EXECUTABLE_VERSION = "phase4-overlay-gpu-fixture-v1"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_digest() -> str:
    paths = [
        ROOT / "src/Billiards/training/trajectory_overlay_model.cpp",
        ROOT / "src/Billiards/training/trajectory_overlay_model.h",
        ROOT / "src/Billiards/training/trajectory_overlay_renderer.cpp",
        ROOT / "src/Billiards/training/trajectory_overlay_renderer.h",
        ROOT / "src/Billiards/render/modern/camera_projection.h",
        ROOT / "src/Billiards/render/modern/modern_renderer.cpp",
        ROOT / "src/Billiards/render/modern/modern_product_ui_runtime.cpp",
        ROOT / "src/Billiards/render/ui/product_shell_draw_list.cpp",
        ROOT / "src/Billiards/render/ui/product_shell_layout.cpp",
        ROOT / "src/Billiards/render/ui/product_shell_layout.h",
        ROOT / "src/Billiards/render/ui/product_ui_frame_coordinator.cpp",
        ROOT / "src/Billiards/render/ui/product_ui_frame_coordinator.h",
        ROOT / "src/Billiards/render/ui/product_ui_gpu_renderer.cpp",
        ROOT / "src/Billiards/product_shell_controller.cpp",
        ROOT / "src/Billiards/product_shell_controller.h",
        ROOT / "src/Billiards/training/preview_runtime.cpp",
        ROOT / "src/Billiards/training/preview_runtime.h",
    ]
    digest = hashlib.sha256()
    for path in paths:
        digest.update(path.relative_to(ROOT).as_posix().encode())
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def shot_yaw(scenario: dict) -> float:
    balls = scenario["initial_scenario"]["scenario"]["balls"]
    by_index = {ball["index"]: ball for ball in balls if not ball["pocketed"]}
    cue = by_index[0]["position_cm"]
    objects = [ball for index, ball in sorted(by_index.items()) if index != 0]
    if not objects:
        return -math.pi / 2.0
    target = objects[0]["position_cm"]
    return math.atan2(target[2] - cue[2], target[0] - cue[0])


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve()
    if not executable.is_file():
        raise SystemExit("Billiards executable is missing")
    pack_path = ROOT / "assets/training/phase4/training_pack_v1.json"
    pack = json.loads(pack_path.read_text(encoding="utf-8"))
    if len(pack["scenarios"]) != 12:
        raise SystemExit("fixture generation requires exact launch 12")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    records = []
    with AutomationClient(str(executable), mode="headless") as client:
        for ordinal, entry in enumerate(pack["scenarios"], 1):
            scenario_path = pack_path.parent / entry["file"]
            if sha256(scenario_path) != entry["sha256"]:
                raise RuntimeError(f"scenario SHA mismatch: {entry['id']}")
            scenario = json.loads(scenario_path.read_text(encoding="utf-8"))
            client.command("training_load", {
                "schema_version": 1, "path": str(scenario_path)})
            client.command("replay_record_begin", {
                "schema_version": 1,
                "executable_version": FIXTURE_EXECUTABLE_VERSION,
                "content_sha256": entry["sha256"], "seed": ordinal})
            client.toggle_aim()
            client.set_aim_yaw(shot_yaw(scenario))
            client.set_shot_power(55.0)
            client.shoot()
            client.command("replay_record_get_state")
            client.run_until("balls_stopped", max_steps=6000)
            client.command("replay_record_finish", {"schema_version": 1})
            replay_path = (args.output_dir /
                f"{entry['id']}.overlay.replay.json").resolve()
            client.command("replay_record_save", {
                "schema_version": 1, "path": str(replay_path)})
            replay = json.loads(replay_path.read_text(encoding="utf-8"))
            if replay["format"] != "billiardgl_replay_v1" or \
                    replay["executable_version"] != FIXTURE_EXECUTABLE_VERSION or \
                    replay["content_sha256"] != entry["sha256"] or \
                    replay["final_summary"]["shot_count"] != 1:
                raise RuntimeError(f"Replay v1 binding invalid: {entry['id']}")
            records.append({
                "scenario_id": entry["id"],
                "scenario_path": str(scenario_path.resolve()),
                "scenario_sha256": entry["sha256"],
                "replay_path": str(replay_path),
                "replay_sha256": sha256(replay_path),
                "executable_version": FIXTURE_EXECUTABLE_VERSION,
                "executable_path": str(executable),
                "executable_sha256": sha256(executable),
                "content_sha256": replay["content_sha256"],
                "physics_profile_id": replay["physics_profile_id"],
                "physics_profile_sha256": replay["physics_profile_sha256"],
                "final_tick": replay["final_summary"]["tick"],
                "final_state_sha256": replay["final_summary"]["state_sha256"],
                "status": "FIXTURE_VALIDATED_NOT_GPU_EVIDENCE",
            })
    output = {
        "schema": "billiardgl.phase4.overlay_replay_fixtures.v1",
        "status": "FIXTURE_VALIDATED_NOT_GPU_EVIDENCE",
        "source_sha256": source_digest(),
        "training_pack_path": str(pack_path),
        "training_pack_sha256": sha256(pack_path),
        "executable_sha256": sha256(executable),
        "fixture_count": len(records),
        "external_real_human_records": [],
        "fixtures": records,
    }
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.write_text(json.dumps(output, indent=2) + "\n",
                             encoding="utf-8")
    print("generated 12 validated Replay v1 fixtures; no GPU/TA/manual claim")


if __name__ == "__main__":
    main()
