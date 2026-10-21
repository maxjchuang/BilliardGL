#!/usr/bin/env python3
"""Fail-closed batch dispatcher for the Phase 4 68-capture GPU matrix.

Without ``--run`` this validates only the immutable selector matrix.  A formal
run additionally requires a bound v3 manifest and the exact 12-fixture index,
invokes the Core 4.1 runner once using its TSV batch protocol, validates the
complete JSONL result set, converts framebuffer PPMs to PNG, and emits a
machine-only evidence manifest.  It never claims TA, manual, or M3 approval.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

import generate_phase4_overlay_replay_fixtures as fixture_generator
import generate_phase4_training_replay_overlay_capture_manifest as generator

GATE_SHA256 = "55b9caa8924ef8d9f3c722e90c8189064a320c8ef53c9199c191b4a0f2722d22"
FORMAL_STATUS = "GPU_MACHINE_PASS_PENDING_TA"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def normalize(value: str, field: str) -> str:
    return generator.normalized_sha(value, field)


def validate(gate_path: Path, manifest_path: Path):
    if sha(gate_path) != GATE_SHA256:
        raise RuntimeError("design gate SHA-256 mismatch")
    gate = json.loads(gate_path.read_text(encoding="utf-8"))
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    captures = manifest.get("captures", [])
    if manifest.get("design_gate_sha256") != GATE_SHA256 or len(captures) != 68:
        raise RuntimeError("manifest gate binding or exact capture count invalid")
    if len({row.get("capture_id") for row in captures}) != 68:
        raise RuntimeError("capture IDs are not unique")
    expected_scenarios = {row["id"]: normalize(row["sha256"], row["id"])
                          for row in gate["scenario_contract"]}
    core = [row for row in captures if row["capture_id"].startswith("core__")]
    extension = [row for row in captures
                 if row["capture_id"].startswith("extension__")]
    if len(core) != 48 or len(extension) != 20:
        raise RuntimeError("48+20 selector matrix is incomplete")
    for row in captures:
        if expected_scenarios.get(row.get("scenario_id")) != normalize(
                row.get("scenario_sha256"), row.get("capture_id")):
            raise RuntimeError(f"scenario binding drift: {row.get('capture_id')}")
        if row.get("actual_gpu_required") is not True:
            raise RuntimeError(
                f"actual GPU requirement missing: {row.get('capture_id')}")
    return gate, manifest


def validate_bound(gate, manifest, manifest_path: Path,
                   fixtures_path: Path, runner_path: Path):
    if manifest.get("schema") != \
            "billiardgl.phase4.training_replay_overlay_gpu_capture_manifest.v3" or \
            manifest.get("status") != "HASH_BOUND_READY_NOT_RUN":
        raise RuntimeError("formal run requires a hash-bound v3 input manifest")
    if not runner_path.is_file() or normalize(
            manifest.get("runner_sha256"), "runner") != sha(runner_path):
        raise RuntimeError("runner binding drift")
    if Path(manifest.get("runner_path", "")).resolve() != runner_path.resolve():
        raise RuntimeError("runner path binding drift")
    fixtures, by_scenario, source_sha, executable_sha = \
        generator.validate_fixture_manifest(gate, fixtures_path)
    if fixture_generator.source_digest() != source_sha:
        raise RuntimeError("live source digest drift from fixture manifest")
    executable_path = Path(fixtures["fixtures"][0]["executable_path"])
    if not executable_path.is_file() or sha(executable_path) != executable_sha:
        raise RuntimeError("fixture executable binding drift")
    expected_top = {
        "fixture_manifest_sha256": sha(fixtures_path),
        "build_sha256": source_sha,
        "executable_sha256": executable_sha,
        "asset_manifest_sha256": normalize(
            fixtures["training_pack_sha256"], "training pack"),
        "correction_design_gate_sha256": generator.CORRECTION_GATE_SHA256,
    }
    for field, expected in expected_top.items():
        if normalize(manifest.get(field), field) != expected:
            raise RuntimeError(f"top-level {field} drift")
    if Path(manifest.get("fixture_manifest_path", "")).resolve() != \
            fixtures_path.resolve():
        raise RuntimeError("fixture manifest path drift")
    if Path(manifest.get("correction_design_gate_path", "")).resolve() != \
            generator.CORRECTION_GATE_PATH.resolve():
        raise RuntimeError("correction design gate path drift")
    png_paths = set()
    for row in manifest["captures"]:
        capture_id = row["capture_id"]
        fixture = by_scenario[row["scenario_id"]]
        expected = {
            "replay_fixture_sha256": normalize(
                fixture["replay_sha256"], capture_id),
            "build_sha256": source_sha,
            "executable_sha256": executable_sha,
            "content_sha256": normalize(fixture["content_sha256"], capture_id),
            "asset_manifest_sha256": expected_top["asset_manifest_sha256"],
            "physics_profile_sha256": normalize(
                fixture["physics_profile_sha256"], capture_id),
            "runner_sha256": sha(runner_path),
        }
        for field, value in expected.items():
            if normalize(row.get(field), f"{capture_id} {field}") != value:
                raise RuntimeError(f"{field} drift: {capture_id}")
        if Path(row.get("replay_fixture_path", "")).resolve() != Path(
                fixture["replay_path"]).resolve():
            raise RuntimeError(f"replay fixture path drift: {capture_id}")
        if row.get("executable_version") != fixture["executable_version"] or \
                row.get("physics_profile_id") != fixture["physics_profile_id"]:
            raise RuntimeError(f"fixture metadata drift: {capture_id}")
        if row.get("terminal_outcome") != generator.terminal_outcome(gate, row):
            raise RuntimeError(f"terminal outcome drift: {capture_id}")
        png = row.get("png_path")
        if not png or png in png_paths:
            raise RuntimeError(f"PNG output binding invalid: {capture_id}")
        png_paths.add(png)
    return fixtures


def plan_rows(manifest):
    rows = []
    for row in manifest["captures"]:
        physical = row["physical_viewport_px"]
        logical = row.get("logical_viewport_px") or physical
        device = float(row["device_scale"])
        dpi = 96.0 * device
        advanced = row["overlay_state"] == \
            "comparison_terminal_explicit_advanced_expanded"
        values = [row["capture_id"], row["scenario_id"],
                  normalize(row["scenario_sha256"], row["capture_id"]),
                  row["replay_fixture_path"],
                  normalize(row["replay_fixture_sha256"], row["capture_id"]),
                  row["executable_version"],
                  normalize(row["content_sha256"], row["capture_id"]),
                  row["camera_preset"], logical[0], logical[1],
                  physical[0], physical[1], dpi, device, row["palette"],
                  row["ui_scale"], 1 if advanced else 0,
                  row["overlay_state"], row["terminal_outcome"]]
        serialized = "\t".join(str(value) for value in values)
        if len(serialized.split("\t")) != 19:
            raise RuntimeError(f"runner protocol field count invalid: {row['capture_id']}")
        rows.append(serialized)
    if len(rows) != 68:
        raise RuntimeError("runner plan does not contain exactly 68 rows")
    return rows


def runner_command(runner: Path, plan: Path, output_dir: Path,
                   results: Path, warmup: int, samples: int):
    return [str(runner.resolve()), str(plan), str(output_dir), str(results),
            str(warmup), str(samples)]


def ppm_dimensions(path: Path):
    with path.open("rb") as stream:
        if stream.readline().strip() != b"P6":
            raise RuntimeError(f"not a P6 framebuffer: {path}")
        dimensions = stream.readline().split()
        if len(dimensions) != 2 or stream.readline().strip() != b"255":
            raise RuntimeError(f"invalid PPM header: {path}")
    return int(dimensions[0]), int(dimensions[1])


def png_dimensions(path: Path):
    header = path.read_bytes()[:24]
    if header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise RuntimeError(f"not a PNG: {path}")
    return struct.unpack(">II", header[16:24])


def validate_results(manifest, rows, output_dir: Path):
    captures = [row for row in rows if row.get("capture_id")]
    summaries = [row for row in rows if row.get("record_type") == "summary"]
    expected = {row["capture_id"]: row for row in manifest["captures"]}
    if len(rows) != 69 or len(captures) != 68 or len(summaries) != 1 or \
            set(row.get("capture_id") for row in captures) != set(expected):
        raise RuntimeError("formal runner result set is incomplete")
    if len({row["capture_id"] for row in captures}) != 68:
        raise RuntimeError("formal runner returned duplicate capture IDs")
    for row in captures:
        bound = expected[row["capture_id"]]
        if row.get("machine_status") != FORMAL_STATUS or \
                row.get("scenario_id") != bound["scenario_id"] or \
                normalize(row.get("scenario_sha256"), row["capture_id"]) != \
                normalize(bound["scenario_sha256"], row["capture_id"]) or \
                normalize(row.get("replay_sha256"), row["capture_id"]) != \
                normalize(bound["replay_fixture_sha256"], row["capture_id"]) or \
                row.get("overlay_state") != bound["overlay_state"] or \
                row.get("terminal_outcome") != bound["terminal_outcome"]:
            raise RuntimeError(f"formal result binding drift: {row['capture_id']}")
        integer_zero = ("safe_area_violations", "truncated_text_runs",
                        "overlap_pairs", "overlay_new_textures",
                        "overlay_texture_residency_delta_bytes",
                        "debug_node_count", "opaque_icon_count",
                        "unexpanded_icon_count", "panel_alpha_violations")
        if row.get("authority_before_equals_after") is not True or any(
                row.get(field) != 0 for field in integer_zero) or \
                row.get("bounded_inspector") is not True or \
                row.get("inspector_viewport_coverage", 1.0) > 0.46 or \
                row.get("gameplay_corridor_width_fraction", 0.0) < 0.50 or \
                row.get("non_color_topology_separation") is not True or \
                row.get("exact_physical_style_contract") is not True or \
                row.get("rails_text_contract") is not True or \
                row.get("actual_only_legend_contract") is not True or \
                row.get("advanced_text_contract") is not True or \
                row.get("inspector_content_contract") is not True or \
                row.get("protected_corridor_intersection_area_px", 1) != 0 or \
                row.get("minimum_local_contrast_ratio", 0.0) < 4.0 or \
                row.get("contrast_4_5_target_coverage", 0.0) < 0.99 or \
                row.get("predicted_actual_luma_delta_srgb8", 0.0) < \
                    (32.0 if bound["palette"] == "standard" else 36.0) or \
                row.get("contrast_failure_coordinate_count", 1) != 0 or \
                row.get("contrast_source_probe_count", 0) <= 0 or \
                row.get("contrast_visible_probe_count", 0) <= 0 or \
                row.get("contrast_visible_probe_coverage", 0.0) < 0.90 or \
                row.get("overlay_draw_calls") != 1 or \
                row.get("complete_scene_draw_calls", 101) > 100 or \
                row.get("texture_resident_bytes", 256 * 1024 * 1024 + 1) > \
                256 * 1024 * 1024 or row.get("predicted_vertices", 8193) > 8192 or \
                row.get("actual_vertices", 8193) > 8192:
            raise RuntimeError(f"formal machine gate failed: {row['capture_id']}")
        ppm = Path(row.get("ppm_path", ""))
        expected_ppm = output_dir / f"{row['capture_id']}.ppm"
        dimensions = tuple(bound["physical_viewport_px"])
        if ppm.resolve() != expected_ppm.resolve() or not ppm.is_file() or \
                ppm_dimensions(ppm) != dimensions:
            raise RuntimeError(f"framebuffer output invalid: {row['capture_id']}")
    summary = summaries[0]
    if summary.get("machine_status") != FORMAL_STATUS or \
            summary.get("capture_count") != 68 or \
            summary.get("warmup_frames", 0) < 120 or \
            summary.get("sample_frames", 0) < 300 or \
            summary.get("p95_total_frame_ms", 99) > 16.7 or \
            summary.get("p95_overlay_cpu_build_ms", 99) > 0.5 or \
            summary.get("p95_ui_plus_overlay_gpu_ms", 99) > 1.0 or \
            summary.get("long_replay_8192_fail_closed") is not True or \
            not summary.get("per_pass_cpu_p95_ms") or \
            not summary.get("per_pass_gpu_p95_ms") or \
            summary.get("ta_status") != "NOT_RUN" or \
            summary.get("manual_status") != "NOT_RUN" or \
            summary.get("m3_status") != "NOT_CLAIMED":
        raise RuntimeError("formal summary machine gate failed")
    return captures, summary


def convert_and_emit(manifest, manifest_path: Path, fixtures_path: Path,
                     runner_path: Path, captures, summary,
                     evidence_path: Path):
    by_id = {row["capture_id"]: row for row in captures}
    evidence = []
    for bound in manifest["captures"]:
        row = by_id[bound["capture_id"]]
        ppm = Path(row["ppm_path"])
        png = Path(bound["png_path"])
        png.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(["sips", "-s", "format", "png", str(ppm),
                        "--out", str(png)], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        if png_dimensions(png) != tuple(bound["physical_viewport_px"]):
            raise RuntimeError(f"PNG dimension mismatch: {bound['capture_id']}")
        ppm.unlink()
        combined = dict(bound)
        combined.update(row)
        combined["png_path"] = str(png.resolve())
        combined["png_sha256"] = sha(png)
        combined["status"] = FORMAL_STATUS
        evidence.append(combined)
    output = {
        "schema": "billiardgl.phase4.training_replay_overlay_gpu_evidence.v3",
        "status": FORMAL_STATUS,
        "capture_count": 68,
        "actual_gpu_capture_count": 68,
        "design_gate_sha256": GATE_SHA256,
        "bound_manifest_path": str(manifest_path.resolve()),
        "bound_manifest_sha256": sha(manifest_path),
        "fixture_manifest_path": str(fixtures_path.resolve()),
        "fixture_manifest_sha256": sha(fixtures_path),
        "runner_path": str(runner_path.resolve()),
        "runner_sha256": sha(runner_path),
        "build_sha256": manifest["build_sha256"],
        "executable_sha256": manifest["executable_sha256"],
        "asset_manifest_sha256": manifest["asset_manifest_sha256"],
        "performance": summary,
        "captures": evidence,
        "ta_status": "NOT_RUN",
        "manual_status": "NOT_RUN",
        "m3_status": "NOT_CLAIMED",
        "external_real_human_records": [],
    }
    evidence_path.parent.mkdir(parents=True, exist_ok=True)
    evidence_path.write_text(json.dumps(output, indent=2) + "\n",
                             encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--gate", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--fixtures", type=Path)
    parser.add_argument("--runner", type=Path)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--evidence-manifest", type=Path)
    parser.add_argument("--warmup", type=int, default=120)
    parser.add_argument("--samples", type=int, default=300)
    parser.add_argument("--run", action="store_true")
    args = parser.parse_args()
    gate, manifest = validate(args.gate, args.manifest)
    if not args.run:
        if args.fixtures or args.runner:
            if not args.fixtures or not args.runner:
                raise SystemExit("bound preflight requires --fixtures and --runner together")
            validate_bound(gate, manifest, args.manifest,
                           args.fixtures, args.runner)
            print("BOUND PREFLIGHT PASS: 68 selectors and 12 fixtures match current source/runner/executable; actual GPU evidence remains NOT_RUN")
        else:
            print("PREFLIGHT PASS: 68 selectors are exact and hash-bound; actual GPU evidence remains NOT_RUN")
        return
    required = (args.fixtures, args.runner, args.output_dir,
                args.evidence_manifest)
    if not all(required):
        raise SystemExit("formal capture refused: --fixtures, --runner, --output-dir and --evidence-manifest are required")
    if args.warmup < 120 or args.samples < 300:
        raise SystemExit("formal capture refused: >=120 warmup and >=300 samples required")
    validate_bound(gate, manifest, args.manifest, args.fixtures, args.runner)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    plan = args.output_dir / "phase4-overlay-formal-plan.tsv"
    results = args.output_dir / "phase4-overlay-formal-results.jsonl"
    plan.write_text("\n".join(plan_rows(manifest)) + "\n", encoding="utf-8")
    completed = subprocess.run(runner_command(args.runner, plan,
        args.output_dir, results, args.warmup, args.samples),
        text=True, capture_output=True)
    print(completed.stdout, end="")
    if completed.returncode:
        print(completed.stderr, end="", file=sys.stderr)
        raise SystemExit(completed.returncode)
    rows = [json.loads(line) for line in results.read_text(
        encoding="utf-8").splitlines() if line]
    captures, summary = validate_results(manifest, rows, args.output_dir)
    convert_and_emit(manifest, args.manifest, args.fixtures, args.runner,
                     captures, summary, args.evidence_manifest)
    print("Phase4 68 actual-GPU captures MACHINE PASS; TA/manual/M3 remain NOT_RUN")


if __name__ == "__main__":
    main()
