#!/usr/bin/env python3
"""Run the bounded actual-Core-4.1 Phase 4 overlay preflight.

Seven GPU framebuffer captures cover all four cameras, 720/UI-135/CVD,
Retina, and explicit Advanced. This is machine preflight only, never TA/M3.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def png_dimensions(path: Path) -> tuple[int, int]:
    header = path.read_bytes()[:24]
    if header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise RuntimeError(f"not a PNG: {path}")
    return struct.unpack(">II", header[16:24])


def plan_cases(fixture: dict):
    common = dict(logical=(1920, 1080), physical=(1920, 1080), dpi=96,
                  device=1.0, palette="standard", ui=1.0, advanced=False)
    cases = []
    for camera in ("player_low", "elevated", "overhead", "broadcast"):
        profile = common.copy()
        profile.update(overlay_state="comparison_ready_collapsed",
                       terminal_outcome="failure")
        cases.append((f"camera_{camera}_1080", camera, profile))
    profile = common.copy()
    profile.update(logical=(1280, 720), physical=(1280, 720),
                   palette="color_vision_safe", ui=1.35,
                   overlay_state="comparison_terminal_collapsed",
                   terminal_outcome="failure")
    cases.append(("comparison_720_ui135_cvd", "overhead", profile))
    profile = common.copy()
    profile.update(logical=(1280, 720), physical=(1920, 1080), dpi=144,
                   device=1.5, palette="color_vision_safe", ui=1.35,
                   overlay_state="actual_replay_paused_collapsed",
                   terminal_outcome="not_terminal")
    cases.append(("actual_retina_ui135_cvd", "broadcast", profile))
    profile = common.copy()
    profile.update(advanced=True,
                   overlay_state=
                       "comparison_terminal_explicit_advanced_expanded",
                   terminal_outcome="success")
    cases.append(("advanced_1080_ui100_standard", "elevated", profile))
    rows = []
    for capture_id, camera, p in cases:
        values = [capture_id, fixture["scenario_id"],
            fixture["scenario_sha256"], fixture["replay_path"],
            fixture["replay_sha256"], fixture["executable_version"],
            fixture["content_sha256"], camera,
            p["logical"][0], p["logical"][1], p["physical"][0],
            p["physical"][1], p["dpi"], p["device"], p["palette"],
            p["ui"], 1 if p["advanced"] else 0]
        values.extend([p["overlay_state"], p["terminal_outcome"]])
        rows.append("\t".join(str(value) for value in values))
    return rows, cases


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("--fixtures", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--warmup", type=int, default=120)
    parser.add_argument("--samples", type=int, default=300)
    args = parser.parse_args()
    correction_gate = Path(__file__).resolve().parents[1] / \
        "docs/art/phase4-overlay-v3-correction-design-gate-v1.json"
    correction_sha = \
        "9cba251af89c6a0b6724966f983be8ad68b5c94ad0d3d902c840e2275bcf28bb"
    if not correction_gate.is_file() or sha(correction_gate) != correction_sha:
        raise SystemExit("v3 correction design gate binding drift")
    if args.warmup < 120 or args.samples < 300:
        raise SystemExit("formal preflight requires >=120 warmup and >=300 samples")
    fixtures = json.loads(args.fixtures.read_text(encoding="utf-8"))
    if fixtures.get("fixture_count") != 12:
        raise SystemExit("exact 12-fixture manifest required")
    fixture = fixtures["fixtures"][0]
    replay = Path(fixture["replay_path"])
    if sha(replay) != fixture["replay_sha256"]:
        raise SystemExit("selected fixture SHA mismatch")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    plan_rows, cases = plan_cases(fixture)
    plan = args.output_dir / "phase4-overlay-preflight-plan.tsv"
    plan.write_text("\n".join(plan_rows) + "\n", encoding="utf-8")
    results = args.output_dir / "phase4-overlay-preflight-results.jsonl"
    completed = subprocess.run([str(args.runner.resolve()), str(plan),
        str(args.output_dir), str(results), str(args.warmup),
        str(args.samples)], text=True, capture_output=True)
    print(completed.stdout, end="")
    if completed.returncode:
        print(completed.stderr, end="")
        raise SystemExit(completed.returncode)
    rows = [json.loads(line) for line in results.read_text().splitlines()]
    captures = [row for row in rows if row.get("capture_id")]
    summaries = [row for row in rows if row.get("record_type") == "summary"]
    if len(captures) != 7 or len(summaries) != 1 or any(
            row["machine_status"] != "PASS_PREFLIGHT_ONLY" for row in rows):
        raise SystemExit("runner result set is incomplete or failed")
    evidence = []
    dimensions = {case_id: profile["physical"]
                  for case_id, _camera, profile in cases}
    for row in captures:
        ppm = Path(row["ppm_path"])
        png = ppm.with_suffix(".png")
        subprocess.run(["sips", "-s", "format", "png", str(ppm),
                        "--out", str(png)], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        if png_dimensions(png) != dimensions[row["capture_id"]]:
            raise RuntimeError(f"PNG dimension mismatch: {row['capture_id']}")
        ppm.unlink()
        row["png_path"] = str(png)
        row["png_sha256"] = sha(png)
        evidence.append(row)
    output = {
        "schema": "billiardgl.phase4.overlay_actual_gpu_preflight.v2",
        "status": "MACHINE_PASS_PREFLIGHT_ONLY",
        "capture_count": 7,
        "formal_68_capture_count": 0,
        "correction_design_gate_path": str(correction_gate.resolve()),
        "correction_design_gate_sha256": correction_sha,
        "runner_path": str(args.runner.resolve()),
        "runner_sha256": sha(args.runner),
        "fixture_manifest_path": str(args.fixtures.resolve()),
        "fixture_manifest_sha256": sha(args.fixtures),
        "source_sha256": fixtures["source_sha256"],
        "executable_sha256": fixtures["executable_sha256"],
        "performance": summaries[0],
        "captures": evidence,
        "ta_status": "NOT_RUN",
        "manual_status": "NOT_RUN",
        "m3_status": "NOT_CLAIMED",
        "external_real_human_records": [],
    }
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.write_text(json.dumps(output, indent=2) + "\n",
                             encoding="utf-8")
    print("Phase4 actual-GPU preflight MACHINE PASS; TA/manual/M3 remain NOT_RUN")


if __name__ == "__main__":
    main()
