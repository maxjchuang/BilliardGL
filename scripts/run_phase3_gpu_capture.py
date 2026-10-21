#!/usr/bin/env python3
"""Run the real Core 4.1 Phase-3 capture matrix without claiming manual review."""

from __future__ import annotations

import argparse
import copy
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import platform
import struct
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
TEMPLATE = ROOT / "docs/art/phase3-gpu-capture-matrix-v1.pending.json"
APPROVED_TEMPLATE_SHA256 = (
    "e4f4d3357e84ecdece03a3f271a24ced6985b06a80cd0d8560ee25781182a86c"
)
FORMAL_WARMUP = 120
FORMAL_SAMPLES = 300
MICRO_WARMUP = 8
MICRO_SAMPLES = 30
ZERO_METRICS = (
    "safe_area_violations", "truncated_text_runs", "overlap_pairs",
    "opaque_icon_count", "unexpanded_icon_count", "debug_node_count",
    "glyph_rasterizations_during_frame",
)
MICRO_IDS = (
    "hud__help__1280x720__broadcast__standard__ui085",
    "hud__aiming__1920x1080__broadcast__color_vision_safe__ui135",
    "hud__foul__retina_1280x720__broadcast__standard__ui100",
    "shell__main_menu_training_unavailable__1280x720__standard__ui085",
    "shell__settings_audio__1920x1080__color_vision_safe__ui135",
    "shell__help_back_focused__retina_1280x720__standard__ui100",
)
REPLAY_PREFLIGHT_IDS = (
    "shell__replay_browser_invalid_path_error__1280x720__standard__ui085",
    "shell__replay_browser_invalid_path_error__1920x1080__color_vision_safe__ui100",
    "shell__replay_browser_invalid_path_error__retina_1280x720__standard__ui135",
)
AUDIO_ROW_PREFLIGHT_IDS = (
    "shell__settings_audio__1280x720__standard__ui135",
    "shell__settings_audio__1280x720__color_vision_safe__ui135",
    "shell__settings_audio__retina_1280x720__standard__ui135",
    "shell__settings_audio__retina_1280x720__color_vision_safe__ui135",
    "shell__settings_input__1280x720__standard__ui135",
    "shell__settings_input__retina_1280x720__standard__ui135",
)
TRAINING_SESSION_PREFLIGHT_IDS = (
    "shell__training_session_active__1280x720__standard__ui135",
    "shell__training_session_active__1280x720__color_vision_safe__ui135",
    "shell__training_session_active__retina_1280x720__standard__ui135",
    "shell__training_session_active__retina_1280x720__color_vision_safe__ui135",
    "shell__settings_audio__1280x720__standard__ui135",
    "shell__settings_audio__1280x720__color_vision_safe__ui135",
    "shell__settings_audio__retina_1280x720__standard__ui135",
    "shell__settings_audio__retina_1280x720__color_vision_safe__ui135",
)


def _path_for_report(path: Path) -> str:
    """Keep repository artifacts portable and external artifacts inspectable."""
    resolved = path.resolve()
    try:
        return str(resolved.relative_to(ROOT))
    except ValueError:
        return str(resolved)


def _asset_binding_from_rows(rows: list[dict]) -> dict:
    if not rows or not isinstance(rows[0].get("asset_binding"), dict):
        raise RuntimeError("runner result is missing asset binding")
    binding = rows[0]["asset_binding"]
    required = {
        "asset_root", "gltf_path", "gltf_sha256",
        "manifest_path", "manifest_sha256",
    }
    if set(binding) != required:
        raise RuntimeError("runner asset binding fields differ")
    if any(row.get("asset_binding") != binding for row in rows[1:]):
        raise RuntimeError("runner asset binding changed during capture")
    return binding


def _hud_failing_axis_ids(entries: list[dict]) -> tuple[str, ...]:
    return tuple(entry["capture_id"] for entry in entries
        if entry["surface"] == "hud" and entry["camera_preset"] == "broadcast"
        and entry["ui_scale"] in {0.85, 1.35})


def _ta_repair_preflight_ids(entries: list[dict]) -> tuple[str, ...]:
    """Minimal production-route visual set for the five TA FAIL roots."""
    wanted: list[tuple[str, str, str, str, str, str, float]] = []
    # A: shared shell header typography implementation.
    for viewport, palette in (("1280x720", "standard"),
                              ("1280x720", "color_vision_safe"),
                              ("retina_1280x720", "standard")):
        wanted.append(("shell", "", "settings_input", "", viewport,
                       palette, 1.35))
    # B: broadcast composition.  The 1920 sample is the venue-context check.
    for viewport, palette in (("1280x720", "standard"),
                              ("1280x720", "color_vision_safe"),
                              ("1920x1080", "standard")):
        wanted.append(("hud", "default", "", "broadcast", viewport,
                       palette, 1.35))
    # C/D: the geometry fix is camera independent, but retain every camera
    # that was in the failed review at the worst 1280 accessibility scale.
    for state in ("post_shot", "foul"):
        for camera in ("player_low", "elevated", "overhead"):
            for palette in ("standard", "color_vision_safe"):
                wanted.append(("hud", state, "", camera, "1280x720",
                               palette, 1.35))
        wanted.append(("hud", state, "", "overhead",
                       "retina_1280x720", "standard", 1.35))
    # E: both affected overhead states, plus a Retina worst case.
    for state in ("default", "aiming"):
        for palette in ("standard", "color_vision_safe"):
            wanted.append(("hud", state, "", "overhead", "1280x720",
                           palette, 1.35))
    wanted.append(("hud", "aiming", "", "overhead",
                   "retina_1280x720", "standard", 1.35))

    selected: list[str] = []
    for surface, state, scenario, camera, viewport, palette, ui_scale in wanted:
        candidates = [entry for entry in entries
            if entry["surface"] == surface and
            (not state or entry.get("state") == state) and
            (not scenario or entry.get("scenario") == scenario) and
            (not camera or entry.get("camera_preset") == camera) and
            entry["viewport"] == viewport and entry["palette"] == palette and
            entry["ui_scale"] == ui_scale]
        candidates = [entry for entry in candidates
            if entry["capture_id"] not in selected]
        if not candidates:
            raise RuntimeError("TA repair preflight selector is incomplete")
        selected.append(candidates[0]["capture_id"])
    return tuple(selected)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return "sha256:" + digest.hexdigest()


def _png_dimensions(path: Path) -> tuple[int, int]:
    with path.open("rb") as source:
        header = source.read(24)
    if header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise RuntimeError(f"not a PNG: {path}")
    return struct.unpack(">II", header[16:24])


def _load_template() -> dict:
    actual = hashlib.sha256(TEMPLATE.read_bytes()).hexdigest()
    if actual != APPROVED_TEMPLATE_SHA256:
        raise RuntimeError(
            f"approved Phase3 pending contract SHA differs: {actual}"
        )
    return json.loads(TEMPLATE.read_text())


def _plan_line(entry: dict, contract: dict) -> str:
    viewport = contract["matrix_contract"]["viewports"][entry["viewport"]]
    values = (
        entry["capture_id"], entry["surface"], entry.get("state") or "",
        entry.get("scenario") or "", entry.get("camera_preset") or "",
        entry["viewport"], entry["palette"], entry["ui_scale"],
        viewport["logical_px"][0], viewport["logical_px"][1],
        viewport["physical_px"][0], viewport["physical_px"][1],
        viewport["dpi"],
        entry["minimum_text_physical_px"],
    )
    return "\t".join(str(value) for value in values)


def _formal_order(entries: list[dict]) -> list[dict]:
    batches: dict[tuple[str, str], list[dict]] = {}
    for entry in entries:
        batches.setdefault((entry["surface"], entry["viewport"]), []).append(entry)
    ordered: list[dict] = []
    for surface in ("hud", "shell"):
        for viewport in ("1280x720", "1920x1080", "retina_1280x720"):
            group = batches[(surface, viewport)]
            if surface == "hud":
                preferred = next(entry for entry in group
                    if entry["state"] == "help" and
                    entry["camera_preset"] == "broadcast" and
                    entry["palette"] == "color_vision_safe" and
                    entry["ui_scale"] == 1.35)
            else:
                preferred = next(entry for entry in group
                    if entry["scenario"] == "settings_audio" and
                    entry["palette"] == "color_vision_safe" and
                    entry["ui_scale"] == 1.35)
            ordered.append(preferred)
            ordered.extend(entry for entry in group if entry is not preferred)
    if len(ordered) != 594 or len({entry["capture_id"] for entry in ordered}) != 594:
        raise RuntimeError("formal plan does not exactly cover 594 unique captures")
    return ordered


def _convert_capture(ppm: Path, png: Path, expected: tuple[int, int]) -> None:
    if not ppm.is_file():
        raise RuntimeError(f"runner announced missing PPM: {ppm}")
    png.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        ["sips", "-s", "format", "png", str(ppm), "--out", str(png)],
        check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True,
    )
    if _png_dimensions(png) != expected:
        png.unlink(missing_ok=True)
        raise RuntimeError(f"converted PNG dimensions differ: {png}")
    ppm.unlink()


def _run_streaming(runner: Path, plan: Path, capture_dir: Path,
                   results_path: Path, entries: list[dict], warmup: int,
                   samples: int) -> list[dict]:
    expected = {
        entry["capture_id"]: tuple(entry["expected_physical_px"])
        for entry in entries
    }
    process = subprocess.Popen(
        [str(runner), str(plan), str(capture_dir), str(results_path),
         str(warmup), str(samples)],
        cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, bufsize=1,
    )
    converted: set[str] = set()
    assert process.stdout is not None
    for line in process.stdout:
        print(line, end="", flush=True)
        if not line.startswith("phase3-micro-capture "):
            continue
        capture_id = line.rstrip().rsplit(" ", 1)[-1]
        if capture_id not in expected:
            process.kill()
            raise RuntimeError(f"runner announced unknown capture: {capture_id}")
        _convert_capture(
            capture_dir / f"{capture_id}.ppm",
            capture_dir / f"{capture_id}.png",
            expected[capture_id],
        )
        converted.add(capture_id)
    return_code = process.wait()
    if return_code != 0:
        raise RuntimeError(f"Phase3 GPU runner exited {return_code}")
    if converted != set(expected):
        raise RuntimeError("streaming conversion did not cover the exact plan")
    rows = [json.loads(line) for line in results_path.read_text().splitlines()
            if line.strip()]
    if len(rows) != len(entries) or {row["capture_id"] for row in rows} != set(expected):
        raise RuntimeError("runner JSONL does not exactly cover the plan")
    return rows


def _machine_ok(expected: dict, row: dict, formal: bool) -> tuple[bool, list[str]]:
    errors: list[str] = []
    metrics = row["machine_metrics"]
    for name in ZERO_METRICS:
        if metrics.get(name) != 0:
            errors.append(f"{name}={metrics.get(name)}")
    if metrics.get("minimum_rendered_text_px", -1) + 1e-6 < expected["minimum_text_physical_px"]:
        errors.append("minimum_rendered_text_px")
    if metrics.get("expanded_icon_count", -1) < expected["minimum_expanded_icons"]:
        errors.append("expanded_icon_count")
    for metric, threshold in (
        ("focus_indicator_count", expected["minimum_focus"]),
        ("selected_indicator_count", expected["minimum_selected"]),
        ("error_indicator_count", expected["minimum_error"]),
        ("disabled_shape_count", expected["minimum_disabled"]),
    ):
        actual = metrics.get(metric, -1)
        if not (actual >= threshold if threshold > 0 else actual == 0):
            errors.append(metric)
    performance = row["performance"]
    if performance.get("p95_total_frame_ms", 1e9) > 16.7:
        errors.append("p95_total_frame_ms")
    if performance.get("draw_calls_complete_scene", 1e9) > 100:
        errors.append("draw_calls_complete_scene")
    if performance.get("texture_resident_mib", 1e9) > 256:
        errors.append("texture_resident_mib")
    if formal and performance.get("warmup_frames", 0) < FORMAL_WARMUP:
        errors.append("warmup_frames")
    if formal and performance.get("sample_frames", 0) < FORMAL_SAMPLES:
        errors.append("sample_frames")
    return not errors, errors


def _sample_report(contract: dict, entries: list[dict], rows: list[dict],
                   capture_dir: Path, output_dir: Path, runner: Path,
                   schema: str, filename: str, note: str) -> Path:
    expected = {entry["capture_id"]: entry for entry in entries}
    captures = []
    all_ok = True
    for row in rows:
        entry = expected[row["capture_id"]]
        ok, errors = _machine_ok(entry, row, formal=False)
        all_ok = all_ok and ok
        png = capture_dir / f"{row['capture_id']}.png"
        captures.append({
            "capture_id": row["capture_id"],
            "surface": entry["surface"],
            "state": entry.get("state"),
            "scenario": entry.get("scenario"),
            "camera_preset": entry.get("camera_preset"),
            "viewport": entry["viewport"],
            "expected_physical_px": entry["expected_physical_px"],
            "palette": entry["palette"],
            "ui_scale": entry["ui_scale"],
            "actual_gpu_capture": True,
            "capture_method": "gpu_framebuffer_readback",
            "path": str(png.relative_to(output_dir)),
            "sha256": _sha256(png),
            "machine_checks_satisfied": ok,
            "machine_check_errors": errors,
            "machine_metrics": row["machine_metrics"],
            "performance_observation": row["performance"],
            "text_run_diagnostics": row.get("text_run_diagnostics", []),
        })
    report = {
        "schema": schema,
        "status": "NON_FORMAL",
        "formal_pass_claimed": False,
        "approved_contract_sha256": "sha256:" + APPROVED_TEMPLATE_SHA256,
        "capture_count": len(captures),
        "all_sampled_machine_checks_satisfied": all_ok,
        "visual_review_status": "PENDING_TA",
        "asset_binding": _asset_binding_from_rows(rows),
        "production_route_binding": {
            "executable": _path_for_report(runner),
            "executable_sha256": _sha256(runner),
            "git_revision": subprocess.check_output(
                ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
            "source_sha256": {
                str(path.relative_to(ROOT)): _sha256(path) for path in (
                    ROOT / "src/Billiards/render/ui/product_shell_layout.cpp",
                    ROOT / "src/Billiards/render/ui/product_shell_draw_list.cpp",
                    ROOT / "src/Billiards/product_hud.cpp",
                    ROOT / "src/Billiards/camera/camera_state_machine.cpp",
                    ROOT / "src/Billiards/product_shell_controller.cpp",
                )
            },
        },
        "note": note,
        "captures": captures,
    }
    path = output_dir / filename
    path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    if not all_ok:
        raise RuntimeError(f"sample machine gate failed; inspect {path}")
    return path


def _formal_manifest(contract: dict, entries: list[dict], rows: list[dict],
                     runner: Path, capture_dir: Path, output_dir: Path) -> Path:
    manifest = copy.deepcopy(contract)
    expected = {entry["capture_id"]: entry for entry in entries}
    row_by_id = {row["capture_id"]: row for row in rows}
    all_ok = True
    failures: dict[str, list[str]] = {}
    for entry in manifest["captures"]:
        row = row_by_id[entry["capture_id"]]
        ok, errors = _machine_ok(expected[entry["capture_id"]], row, formal=True)
        all_ok = all_ok and ok
        if errors:
            failures[entry["capture_id"]] = errors
        png = capture_dir / f"{entry['capture_id']}.png"
        entry["artifact"] = {
            "actual_gpu_capture": True,
            "capture_method": "gpu_framebuffer_readback",
            "path": str(png.relative_to(output_dir)),
            "sha256": _sha256(png),
        }
        entry["machine_status"] = "PASS" if ok else "FAIL"
        entry["machine_metrics"] = row["machine_metrics"]
        entry["performance"] = row["performance"]
    first = rows[0]
    manifest["runtime_attestation"] = {
        "git_revision": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "executable_sha256": _sha256(runner),
        "renderer": "modern_opengl_core",
        "gpu_vendor": first["gl_vendor"],
        "gpu_device": first["gl_device"],
        "driver": first["gl_version"],
        "operating_system": platform.platform(),
        "capture_utc": datetime.now(timezone.utc).isoformat(),
    }
    manifest["formal_pass_claimed"] = False
    manifest["visual_review_status"] = "PENDING_TA"
    manifest["approved_contract_sha256"] = \
        "sha256:" + APPROVED_TEMPLATE_SHA256
    manifest["production_source_binding"] = {
        str(source.relative_to(ROOT)): _sha256(source) for source in (
            ROOT / "spikes/render_core_profile/phase3_gpu_capture_runner.mm",
            ROOT / "src/Billiards/render/ui/product_shell_layout.cpp",
            ROOT / "src/Billiards/render/ui/product_shell_draw_list.cpp",
            ROOT / "src/Billiards/product_hud.cpp",
            ROOT / "src/Billiards/camera/camera_state_machine.cpp",
            ROOT / "src/Billiards/product_shell_controller.cpp",
            ROOT / "scripts/run_phase3_gpu_capture.py",
            ROOT / "scripts/validate_phase3_gpu_capture_evidence.py",
            TEMPLATE,
        )
    }
    manifest["status"] = "PASS" if all_ok else "FAIL"
    if failures:
        manifest["machine_failures"] = failures
    path = output_dir / "phase3-gpu-capture-manifest-v1.json"
    path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    validation = subprocess.run(
        [sys.executable, str(ROOT / "scripts/validate_phase3_gpu_capture_evidence.py"),
         str(path)], cwd=ROOT, text=True, capture_output=True,
    )
    print(validation.stdout, end="")
    print(validation.stderr, end="", file=sys.stderr)
    if validation.returncode != 0:
        raise RuntimeError(f"official Phase3 validator failed; inspect {path}")
    # formal_pass_claimed is promoted only after the official machine
    # validator accepted the exact 594 actual-GPU manifest above.  Manual
    # review remains an independent, explicitly pending gate.
    manifest["formal_pass_claimed"] = True
    path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    validation = subprocess.run(
        [sys.executable, str(ROOT / "scripts/validate_phase3_gpu_capture_evidence.py"),
         str(path)], cwd=ROOT, text=True, capture_output=True,
    )
    print(validation.stdout, end="")
    print(validation.stderr, end="", file=sys.stderr)
    if validation.returncode != 0:
        raise RuntimeError(f"official Phase3 post-promotion validation failed; inspect {path}")
    return path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=("micro", "replay-preflight",
                        "audio-row-preflight",
                        "training-session-preflight",
                        "hud-failing-axis-preflight", "ta-repair-preflight",
                        "formal"), required=True)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build-product-session")
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()
    contract = _load_template()
    all_entries = contract["captures"]
    if args.mode in {"micro", "replay-preflight", "audio-row-preflight",
                     "training-session-preflight",
                     "hud-failing-axis-preflight",
                     "ta-repair-preflight"}:
        selected = {entry["capture_id"]: entry for entry in all_entries}
        capture_ids = MICRO_IDS if args.mode == "micro" else \
            REPLAY_PREFLIGHT_IDS if args.mode == "replay-preflight" else \
            AUDIO_ROW_PREFLIGHT_IDS if args.mode == "audio-row-preflight" else \
            TRAINING_SESSION_PREFLIGHT_IDS \
            if args.mode == "training-session-preflight" else \
            _hud_failing_axis_ids(all_entries) \
            if args.mode == "hud-failing-axis-preflight" else \
            _ta_repair_preflight_ids(all_entries)
        entries = [selected[capture_id] for capture_id in capture_ids]
        default_name = "phase3-gpu-capture-micro-v1" if args.mode == "micro" \
            else "phase3-gpu-capture-replay-preflight-v1" \
            if args.mode == "replay-preflight" \
            else "phase3-gpu-capture-audio-row-preflight-v1" \
            if args.mode == "audio-row-preflight" \
            else "phase3-gpu-capture-training-session-preflight-v1" \
            if args.mode == "training-session-preflight" \
            else "phase3-gpu-capture-hud-failing-axis-preflight-v1" \
            if args.mode == "hud-failing-axis-preflight" else \
            "phase3-gpu-capture-ta-repair-preflight-v1"
        output_dir = args.output_dir or ROOT / "tmp" / default_name
        warmup, samples = MICRO_WARMUP, MICRO_SAMPLES
    else:
        entries = _formal_order(all_entries)
        output_dir = args.output_dir or ROOT / "docs/art/phase3-gpu-capture-evidence-v1"
        warmup, samples = FORMAL_WARMUP, FORMAL_SAMPLES
    output_dir = output_dir.resolve()
    capture_dir = output_dir / "captures"
    output_dir.mkdir(parents=True, exist_ok=True)
    capture_dir.mkdir(parents=True, exist_ok=True)
    for stale in capture_dir.glob("*.ppm"):
        stale.unlink()
    plan = output_dir / f"{args.mode}-plan.tsv"
    plan.write_text("\n".join(_plan_line(entry, contract) for entry in entries) + "\n")
    results = output_dir / f"{args.mode}-results.jsonl"
    runner = (args.build_dir / "BilliardsPhase3GpuCaptureRunner").resolve()
    if not runner.is_file():
        raise RuntimeError(f"capture runner is missing: {runner}")
    rows = _run_streaming(runner, plan, capture_dir, results, entries,
                          warmup, samples)
    if args.mode == "micro":
        report = _sample_report(contract, entries, rows, capture_dir, output_dir, runner,
            "phase3_gpu_capture_micro_v1", "phase3-gpu-capture-micro-v1.json",
            "Six-case preflight only; never substitutes for the 594-case validator.")
    elif args.mode == "replay-preflight":
        report = _sample_report(contract, entries, rows, capture_dir, output_dir, runner,
            "phase3_gpu_capture_selector_preflight_v1",
            "phase3-gpu-capture-replay-preflight-v1.json",
            "Replay public-state selector preflight only; never substitutes for the 594-case validator.")
    elif args.mode == "audio-row-preflight":
        report = _sample_report(contract, entries, rows, capture_dir, output_dir, runner,
            "phase3_gpu_capture_audio_row_preflight_v1",
            "phase3-gpu-capture-audio-row-preflight-v1.json",
            "Exact four Audio 720p/ui135 TA failures plus two settings_input controls. Machine checks are not TA approval and the immutable v2 evidence is not overwritten.")
    elif args.mode == "training-session-preflight":
        report = _sample_report(contract, entries, rows, capture_dir, output_dir, runner,
            "phase3_gpu_capture_training_session_preflight_v1",
            "phase3-gpu-capture-training-session-preflight-v1.json",
            "Exact four TrainingSession 720p/ui135 machine failures plus the four previously repaired Audio axes as regression controls. Machine checks are not TA approval; formal v3 remains immutable machine FAIL.")
    elif args.mode == "hud-failing-axis-preflight":
        report = _sample_report(contract, entries, rows, capture_dir, output_dir, runner,
            "phase3_gpu_capture_selector_preflight_v1",
            "phase3-gpu-capture-hud-failing-axis-preflight-v1.json",
            "Exact 5-state x 3-viewport x 2-palette x 2-failing-scale HUD preflight; never substitutes for the 594-case validator.")
    elif args.mode == "ta-repair-preflight":
        report = _sample_report(contract, entries, rows, capture_dir, output_dir, runner,
            "phase3_gpu_capture_ta_repair_preflight_v1",
            "phase3-gpu-capture-ta-repair-preflight-v1.json",
            "Targeted A-E repair evidence only. Machine checks are not visual approval; TA review remains required. The prior formal 594-frame evidence is immutable.")
    else:
        report = _formal_manifest(contract, entries, rows, runner,
                                  capture_dir, output_dir)
    if list(capture_dir.glob("*.ppm")):
        raise RuntimeError("raw PPM cleanup incomplete")
    print(f"Phase3 {args.mode} evidence: {report}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Phase3 capture failed: {error}", file=sys.stderr)
        raise SystemExit(1)
