#!/usr/bin/env python3
"""Fail-closed v4 compound-visibility GPU capture orchestrator.

Without ``--run`` this performs read-only hash/schema validation.  GPU work is
possible only with ``--run`` plus a separately authored, exact, single-use root
authorization receipt.  The runner independently validates and atomically
consumes the same receipt before creating an OpenGL context.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

GATE_SHA256 = "1901a825b092c08ce79425ec936ec150fada9ff4e3b4f9817e818d481013a39f"
AUTH_SCHEMA = "billiardgl.phase4.overlay_v4_gpu_authorization_receipt.v1"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise RuntimeError(f"JSON object required: {path}")
    return value


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def validate_ready(ready_path: Path, fixture_path: Path, runner: Path,
                   billiards: Path) -> dict:
    ready = load(ready_path)
    captures = ready.get("captures", [])
    require(ready.get("correction_design_gate_sha256") == GATE_SHA256,
            "v4 correction gate binding drift")
    require(len(captures) in (11, 68) and ready.get("capture_count") ==
            len(captures), "ready manifest must contain exactly 11 or 68")
    require(len({row.get("capture_id") for row in captures}) == len(captures),
            "capture IDs must be unique")
    require(ready.get("runner_path") == str(runner.resolve()) and
            ready.get("runner_sha256") == sha(runner),
            "v4 GPU runner binding drift")
    require(ready.get("billiards_executable_path") ==
            str(billiards.resolve()) and
            ready.get("billiards_executable_sha256") == sha(billiards),
            "Billiards executable binding drift")
    require(ready.get("fixture_manifest_path") == str(fixture_path.resolve()) and
            ready.get("fixture_manifest_sha256") == sha(fixture_path),
            "v4 fixture manifest binding drift")
    fixture = load(fixture_path)
    require(fixture.get("source_sha256") == ready.get("source_sha256") and
            fixture.get("executable_sha256") == sha(billiards) and
            fixture.get("fixture_count") == 12,
            "v4 source/executable/fixture identity drift")
    for row in captures:
        require(row.get("source_sha256") == ready["source_sha256"] and
                row.get("runner_path") == str(runner.resolve()) and
                row.get("runner_sha256") == sha(runner) and
                row.get("billiards_executable_path") ==
                str(billiards.resolve()) and
                row.get("billiards_executable_sha256") == sha(billiards) and
                row.get("executable_sha256") == sha(billiards) and
                row.get("fixture_manifest_path") ==
                str(fixture_path.resolve()) and
                row.get("fixture_manifest_sha256") == sha(fixture_path) and
                row.get("correction_design_gate_sha256") == GATE_SHA256,
                f"per-capture hash binding drift: {row.get('capture_id')}")
        require(row.get("actual_gpu_capture_count") == 0 and
                row.get("machine_status") == "NOT_RUN" and
                row.get("ta_status") == "NOT_RUN" and
                row.get("gpu_capture_authorized") is False and
                row.get("formal68_authorized") is False,
                f"ready row is not pristine NOT_RUN: {row.get('capture_id')}")
    require(ready.get("actual_gpu_capture_count") == 0 and
            ready.get("gpu_capture_authorized") is False and
            ready.get("formal68_authorized") is False,
            "ready manifest authorization boundary drift")
    if len(captures) == 11:
        groups = {name: sum(row.get("group") == name for row in captures)
                  for name in ("original_v3_seven",
                               "exact_frozen_failure_case", "mirrored_case",
                               "deterministic_non_overlap_control")}
        require(list(groups.values()) == [7, 1, 1, 2],
                "targeted ready groups must be 7+1+1+2")
        require(all(value.get("overlap_fraction_of_smaller_core", 1.0) <= .10
                    and value.get("both_tracks_have_source_probes") is True
                    for value in ready.get("non_overlap_controls", [])),
                "non-overlap controls are not deterministically qualified")
    return ready


def validate_authorization(receipt_path: Path, ready_path: Path, ready: dict,
                           fixture_path: Path, runner: Path, billiards: Path,
                           output_root: Path) -> dict:
    receipt = load(receipt_path)
    expected = {
        "schema": AUTH_SCHEMA,
        "authorized": True,
        "single_use": True,
        "correction_design_gate_sha256": GATE_SHA256,
        "runner_sha256": sha(runner),
        "billiards_executable_sha256": sha(billiards),
        "source_sha256": ready["source_sha256"],
        "fixture_manifest_sha256": sha(fixture_path),
        "ready_manifest_sha256": sha(ready_path),
        "output_root": str(output_root.resolve()),
    }
    for field, value in expected.items():
        require(receipt.get(field) == value,
                f"authorization receipt {field} binding drift")
    require(isinstance(receipt.get("authorization_nonce"), str) and
            len(receipt["authorization_nonce"]) >= 16,
            "authorization nonce missing")
    require(not Path(str(receipt_path) + ".consumed").exists(),
            "authorization receipt already consumed")
    return receipt


def plan_rows(ready: dict) -> list[str]:
    rows = []
    for row in ready["captures"]:
        physical = row["physical_viewport_px"]
        logical = row.get("logical_viewport_px") or physical
        values = [row["capture_id"], row["scenario_id"],
                  row["scenario_sha256"], row["replay_fixture_path"],
                  row["replay_fixture_sha256"],
                  "phase4-overlay-gpu-fixture-v1",
                  row["scenario_sha256"], row["camera_preset"],
                  logical[0], logical[1], physical[0], physical[1],
                  96.0 * float(row["device_scale"]),
                  row["device_scale"], row["palette"], row["ui_scale"],
                  1 if row["overlay_state"] ==
                    "comparison_terminal_explicit_advanced_expanded" else 0,
                  row["overlay_state"], row["terminal_outcome"]]
        serialized = "\t".join(str(value) for value in values)
        require(len(serialized.split("\t")) == 19,
                f"runner protocol drift: {row['capture_id']}")
        rows.append(serialized)
    return rows


def validate_results(ready: dict, results_path: Path) -> tuple[list[dict], dict]:
    rows = [json.loads(line) for line in results_path.read_text(
        encoding="utf-8").splitlines() if line]
    captures = [row for row in rows if row.get("capture_id")]
    summaries = [row for row in rows if row.get("record_type") == "summary"]
    expected_ids = {row["capture_id"] for row in ready["captures"]}
    require(len(captures) == len(expected_ids) and len(summaries) == 1 and
            {row.get("capture_id") for row in captures} == expected_ids,
            "v4 result set incomplete")
    required_components = {"core", "outer_halo_edge",
                           "solid_in_predicted_dash_gap", "tick", "endpoint"}
    ready_by_id = {row["capture_id"]: row for row in ready["captures"]}
    for row in captures:
        mode = ready_by_id[row["capture_id"]]["overlay_state"]
        predicted_required = mode != "actual_replay_paused_collapsed"
        actual_required = mode != "predicted_only_active_collapsed"
        mode_components = set(required_components)
        if not predicted_required:
            mode_components.discard("solid_in_predicted_dash_gap")
        if not actual_required:
            mode_components.difference_update(
                {"solid_in_predicted_dash_gap", "tick"})
        require(str(row.get("machine_status", "")).startswith("V4_") and
                row.get("compound_visible_probe_coverage", 0) >= .90 and
                (not predicted_required or
                 row.get("predicted_compound_visible_probe_coverage", 0) >= .90) and
                (not actual_required or
                 row.get("actual_compound_visible_probe_coverage", 0) >= .90) and
                row.get("compound_double_assignment_count") == 0 and
                row.get("compound_foreign_track_evidence_count") == 0 and
                row.get("compound_unowned_evidence_count") == 0 and
                mode_components.issubset(
                    row.get("compound_component_source_counts", {})) and
                row.get("minimum_local_contrast_ratio", 0) >= 4.0 and
                row.get("contrast_4_5_target_coverage", 0) >= .99 and
                row.get("authorization_receipt_consumed") is True,
                f"v4 compound machine gate failed: {row.get('capture_id')}")
        if row["capture_id"] == \
                "core__launch_right_english_one_rail_v1__broadcast":
            require(row.get("compound_source_probe_count", 0) >= 159,
                    "frozen failure source inventory regressed below 159")
    return captures, summaries[0]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ready", type=Path, required=True)
    parser.add_argument("--fixtures", type=Path, required=True)
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("--billiards", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--warmup", type=int, default=120)
    parser.add_argument("--samples", type=int, default=300)
    parser.add_argument("--authorization-receipt", type=Path)
    parser.add_argument("--run", action="store_true")
    args = parser.parse_args()
    ready = validate_ready(args.ready.resolve(), args.fixtures.resolve(),
                           args.runner.resolve(), args.billiards.resolve())
    if not args.run:
        require(args.authorization_receipt is None,
                "authorization receipt is accepted only with --run")
        print(f"v4 ready validation PASS count={ready['capture_count']} "
              "actual_gpu=0 authorization=false; no GPU invoked")
        return
    require(args.authorization_receipt is not None,
            "--run requires an explicit root authorization receipt")
    require(args.warmup >= 120 and args.samples >= 300,
            "v4 capture requires >=120 warmup and >=300 samples")
    output = args.output_root.resolve()
    require(not output.exists(), "v4 output root must be new and immutable")
    receipt = validate_authorization(args.authorization_receipt.resolve(),
        args.ready.resolve(), ready, args.fixtures.resolve(),
        args.runner.resolve(), args.billiards.resolve(), output)
    output.mkdir(parents=True)
    plan = output / "phase4-overlay-v4-plan.tsv"
    results = output / "phase4-overlay-v4-results.jsonl"
    plan.write_text("\n".join(plan_rows(ready)) + "\n", encoding="utf-8")
    environment = os.environ.copy()
    environment["BILLIARDGL_PHASE4_V4_GPU_AUTH_RECEIPT"] = str(
        args.authorization_receipt.resolve())
    completed = subprocess.run([str(args.runner.resolve()), str(plan),
        str(output), str(results), str(args.warmup), str(args.samples)],
        env=environment, text=True)
    require(completed.returncode == 0, "v4 GPU runner failed closed")
    captures, summary = validate_results(ready, results)
    manifest = {
        "schema": "billiardgl.phase4.overlay_v4_gpu_capture_evidence.v1",
        "status": "V4_MACHINE_PASS_PENDING_INDEPENDENT_TA",
        "correction_design_gate_sha256": GATE_SHA256,
        "ready_manifest_path": str(args.ready.resolve()),
        "ready_manifest_sha256": sha(args.ready),
        "authorization_receipt_path": str(args.authorization_receipt.resolve()),
        "authorization_receipt_sha256": sha(args.authorization_receipt),
        "authorization_nonce": receipt["authorization_nonce"],
        "runner_sha256": sha(args.runner),
        "billiards_executable_sha256": sha(args.billiards),
        "source_sha256": ready["source_sha256"],
        "fixture_manifest_sha256": sha(args.fixtures),
        "actual_gpu_capture_count": len(captures),
        "captures": captures,
        "performance": summary,
        "ta_status": "NOT_RUN",
        "formal68_authorized": False,
        "m3_status": "NOT_CLAIMED",
    }
    (output / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print("v4 GPU machine capture complete; independent TA remains NOT_RUN")


if __name__ == "__main__":
    main()
