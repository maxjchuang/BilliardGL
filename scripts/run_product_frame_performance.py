#!/usr/bin/env python3
"""Run the visible shipping product loop and retain formal frame-timing evidence.

This runner validates collection completeness and provenance. It deliberately
does not decide whether the captured machine meets the 16.7 ms reference
budget; review of an uncontaminated run remains a separate acceptance step.
"""

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import tempfile


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def validate_percentiles(value: object, expected_samples: int) -> None:
    require(isinstance(value, dict), "percentile series must be an object")
    require(value.get("sample_count") == expected_samples,
            "percentile series has an incomplete sample count")
    ordered = []
    for key in ("p50_ms", "p95_ms", "p99_ms"):
        sample = value.get(key)
        require(type(sample) in (int, float) and math.isfinite(sample) and sample >= 0,
                f"invalid {key}")
        ordered.append(float(sample))
    require(ordered == sorted(ordered), "percentiles are not monotonic")


def validate_report(report: object) -> None:
    require(isinstance(report, dict), "performance report must be an object")
    require(report.get("schema") == "billiardgl_product_frame_performance_v1",
            "unexpected performance schema")
    require(report.get("measurement_scope") ==
            "visible_product_event_loop_through_swap_completion",
            "performance report has the wrong measurement scope")
    require(report.get("budget_reference_series") ==
            "swap_to_swap_cadence" and
            report.get("loop_through_swap_includes_post_swap_logging") is False and
            report.get("swap_to_swap_cadence_includes_previous_post_swap_logging") is True,
            "performance report does not distinguish loop and cadence scope")
    require(report.get("budget_verdict") == "not_evaluated",
            "collector must not certify its own budget result")
    require(report.get("warmup_frames") == 120 and
            report.get("sample_frames") == 300,
            "formal warmup/sample contract mismatch")
    require(report.get("drawable_width_px") == 1920 and
            report.get("drawable_height_px") == 1080 and
            report.get("render_quality") == "medium" and
            report.get("requested_msaa_samples") == 4 and
            report.get("actual_render_quality") == "medium" and
            report.get("actual_msaa_samples") == 4,
            "formal drawable/quality contract mismatch")
    for key in ("includes_product_ui", "includes_simulation",
                "includes_display_swap", "includes_shipping_playtest_logging"):
        require(report.get(key) is True, f"missing timing scope flag: {key}")
    require(report.get("measures_scanout") is False and
            report.get("forces_diagnostic_gl_finish") is False,
            "collector misstates scanout or diagnostic synchronization")
    full = report.get("full_frame_percentiles")
    require(isinstance(full, dict), "full frame percentiles are missing")
    for key in ("loop_through_swap", "swap_to_swap_cadence", "swap",
                "physics", "render_submission"):
        validate_percentiles(full.get(key), 300)
    gpu = report.get("gpu_pass_percentiles")
    require(isinstance(gpu, dict) and gpu,
            "no asynchronous GPU pass timings were retained")
    expected_gpu_passes = {
        "shadow_depth", "hdr_opaque", "scene_bloom", "aces_tone_map",
        "overlay", "product_ui", "product_present",
    }
    require(expected_gpu_passes.issubset(gpu),
            "one or more production GPU passes are missing")
    positive_gpu_passes = 0
    for value in gpu.values():
        require(isinstance(value, dict) and
                type(value.get("completed_sample_frames")) is int and
                type(value.get("zero_elapsed_sample_frames")) is int,
                "GPU pass availability accounting is missing")
        completed = value["completed_sample_frames"]
        zero = value["zero_elapsed_sample_frames"]
        require(0 <= zero <= completed <= 300,
                "invalid GPU timer-query completion accounting")
        if value.get("available") is True:
            count = value.get("sample_count")
            require(type(count) is int and 0 < count <= 300 and
                    count + zero == completed,
                    "invalid positive GPU pass sample count")
            validate_percentiles(value, count)
            positive_gpu_passes += 1
        else:
            require(value.get("available") is False and
                    value.get("sample_count") == 0 and
                    value.get("unavailable_reason") in (
                        "completed_queries_returned_zero_elapsed",
                        "no_completed_timer_query") and
                    zero == completed,
                    "unmeasured GPU pass is not explicitly unavailable")
    require(positive_gpu_passes > 0,
            "all GPU pass timings are unavailable")
    require(type(report.get("physics_steps")) is int and
            report["physics_steps"] >= 30,
            "sample window did not execute real fixed simulation steps")
    require(type(report.get("moving_sample_frames")) is int and
            report["moving_sample_frames"] >= 30,
            "sample window did not overlap the native moving-ball shot")
    first_moving = report.get("first_moving_sample_frame")
    last_moving = report.get("last_moving_sample_frame")
    require(type(first_moving) is int and type(last_moving) is int and
            1 <= first_moving <= last_moving <= 300,
            "moving-ball sample distribution is invalid")
    require(report.get("native_shot_requested_power") == 200 and
            report.get("native_shot_dispatch_presented_frame") == 120,
            "native max-power shot was not dispatched after warmup")
    for key in ("native_shot_dispatch_tick", "first_sample_tick", "last_sample_tick"):
        value = report.get(key)
        require(isinstance(value, str) and value.isdigit(),
                f"missing canonical tick field: {key}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    binary = args.binary.resolve()
    output = args.output.resolve()
    require(binary.is_file(), "Billiards binary does not exist")
    binary_sha256_before = sha256(binary)
    output.mkdir(parents=True, exist_ok=True)
    run_directory = Path(tempfile.mkdtemp(prefix="run-", dir=output))
    report_path = run_directory / "product-frame-performance.json"
    settings_path = run_directory / "product-settings.json"
    log_root = run_directory / "playtest-logs"
    command = [str(binary), "--product-mode", "--windowed",
               "--width", "1920", "--height", "1080",
               "--settings-path", str(settings_path),
               "--product-frame-performance", str(report_path)]
    environment = os.environ.copy()
    environment["BILLIARDGL_PLAYTEST_LOG_DIR"] = str(log_root)
    completed = subprocess.run(command, cwd=binary.parent, env=environment,
                               text=True, capture_output=True, timeout=120)
    binary_sha256_after = sha256(binary)
    (run_directory / "stdout.txt").write_text(completed.stdout)
    (run_directory / "stderr.txt").write_text(completed.stderr)
    require(completed.returncode == 0,
            f"product performance process exited {completed.returncode}")
    require(binary_sha256_after == binary_sha256_before,
            "Billiards binary changed during performance capture")
    require(report_path.is_file(), "product performance report is missing")
    report = json.loads(report_path.read_text())
    validate_report(report)
    log_directory = Path(report["playtest_log_directory"])
    require(log_directory.is_dir(), "shipping playtest log directory is missing")
    log_files = sorted(log_directory.glob("events-*.jsonl"))
    require(bool(log_files), "shipping playtest log contains no event segments")
    receipt = {
        "schema": "billiardgl_product_frame_performance_receipt_v1",
        "command": command,
        "binary": str(binary),
        "binary_sha256": binary_sha256_after,
        "binary_sha256_before": binary_sha256_before,
        "binary_sha256_after": binary_sha256_after,
        "report": str(report_path),
        "report_sha256": sha256(report_path),
        "playtest_segments": [
            {"path": str(path), "sha256": sha256(path)} for path in log_files
        ],
        "budget_verdict": "not_evaluated",
    }
    receipt_path = run_directory / "receipt.json"
    receipt_path.write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps({"report": str(report_path),
                      "receipt": str(receipt_path),
                      "binary_sha256": receipt["binary_sha256"]}))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError, TypeError, RuntimeError,
            subprocess.SubprocessError) as error:
        raise SystemExit(f"product frame performance rejected: {error}")
