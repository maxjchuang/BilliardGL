#!/usr/bin/env python3
"""Validate Phase-3 GPU captures and their independent TA review, fail closed."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from generate_phase3_gpu_capture_templates import (  # noqa: E402
    ASSET_BINDINGS,
    MANUAL_AXES,
    PERFORMANCE_THRESHOLDS,
    build_capture_template,
    build_manual_template,
    expected_capture_entries,
)


SHA_PATTERN = re.compile(r"sha256:[0-9a-f]{64}")
SELECTOR_KEYS = (
    "surface", "state", "screen", "variant", "camera_preset", "viewport",
    "palette", "ui_scale", "expected_physical_px", "safe_area_physical_px",
    "minimum_text_physical_px", "minimum_expanded_icons", "minimum_focus",
    "minimum_selected", "minimum_error", "minimum_disabled",
)
OPTIONAL_SELECTOR_KEYS = ("scenario", "setting_category", "scroll_state", "focus_state")


class EvidenceError(Exception):
    pass


def _sha256(data):
    return "sha256:" + hashlib.sha256(data).hexdigest()


def _sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return "sha256:" + digest.hexdigest()


def _image_dimensions(path):
    data = path.read_bytes()
    if data.startswith(b"\x89PNG\r\n\x1a\n") and len(data) >= 24 and data[12:16] == b"IHDR":
        return struct.unpack(">II", data[16:24])
    if data.startswith(b"P6"):
        tokens = []
        for line in data.splitlines():
            line = line.split(b"#", 1)[0]
            tokens.extend(line.split())
            if len(tokens) >= 4:
                break
        if len(tokens) >= 4 and tokens[0] == b"P6":
            return int(tokens[1]), int(tokens[2])
    raise EvidenceError(str(path) + ": capture must be PNG or binary PPM")


def validate_pending_capture_template(value):
    return [] if value == build_capture_template() else [
        "pending capture matrix differs from the generated fail-closed 594-case template"]


def validate_pending_manual_template(value):
    return [] if value == build_manual_template() else [
        "pending manual review differs from the generated fail-closed 594-case template"]


def _non_pending_runtime(attestation):
    expected = {"git_revision", "executable_sha256", "renderer", "gpu_vendor",
                "gpu_device", "driver", "operating_system", "capture_utc"}
    return set(attestation) == expected and all(
        isinstance(value, str) and value and value != "PENDING"
        for value in attestation.values()) and \
        SHA_PATTERN.fullmatch(attestation["executable_sha256"]) is not None and \
        attestation["renderer"] == "modern_opengl_core"


def _count_gate(actual, minimum):
    return actual >= minimum if minimum > 0 else actual == 0


def validate_capture_manifest(value, manifest_path=None, repository_root=ROOT,
                              verify_files=True):
    errors = []
    repository_root = Path(repository_root)
    manifest_path = Path(manifest_path) if manifest_path else repository_root / "capture.json"
    if value.get("schema") != "phase3_gpu_capture_manifest_v1" or \
            value.get("status") != "PASS" or \
            value.get("evidence_class") != "actual_gpu_framebuffer_capture_required":
        errors.append("actual capture manifest identity/status/evidence class is invalid")
    if value.get("asset_bindings") != ASSET_BINDINGS:
        errors.append("HUD/Shell asset hash bindings differ from the frozen contract")
    elif verify_files:
        for name, binding in ASSET_BINDINGS.items():
            path = repository_root / binding["path"]
            if not path.is_file() or _sha256_file(path) != binding["sha256"]:
                errors.append(name + " frozen asset SHA does not match repository bytes")
    if not _non_pending_runtime(value.get("runtime_attestation", {})):
        errors.append("runtime/GPU/executable attestation is incomplete")
    expected_template = build_capture_template()
    if value.get("matrix_contract") != expected_template["matrix_contract"]:
        errors.append("capture matrix factors/counts differ from the approved contract")
    if value.get("machine_thresholds") != PERFORMANCE_THRESHOLDS:
        errors.append("existing machine/performance thresholds changed")
    boundary = value.get("evidence_boundary", {})
    if boundary != expected_template["evidence_boundary"]:
        errors.append("capture evidence boundary differs; flat proof/desktop capture remains forbidden")

    expected_entries = {entry["capture_id"]: entry for entry in expected_capture_entries()}
    captures = value.get("captures")
    if not isinstance(captures, list):
        errors.append("captures must be an array")
        captures = []
    actual_entries = {entry.get("capture_id"): entry for entry in captures if isinstance(entry, dict)}
    if len(captures) != len(actual_entries) or set(actual_entries) != set(expected_entries):
        errors.append("capture IDs must exactly cover all 594 matrix cases")
    allowed_methods = set(boundary.get("allowed_capture_methods", []))
    forbidden_names = set(boundary.get("forbidden_proof_files", []))
    for capture_id, expected in expected_entries.items():
        entry = actual_entries.get(capture_id)
        if entry is None:
            continue
        for key in SELECTOR_KEYS + OPTIONAL_SELECTOR_KEYS:
            if entry.get(key) != expected.get(key):
                errors.append(capture_id + " selector/threshold differs: " + key)
        artifact = entry.get("artifact", {})
        artifact_path = artifact.get("path")
        artifact_hash = artifact.get("sha256")
        if artifact.get("actual_gpu_capture") is not True or \
                artifact.get("capture_method") not in allowed_methods or \
                not isinstance(artifact_path, str) or artifact_path == "PENDING" or \
                Path(artifact_path).name in forbidden_names or \
                Path(artifact_path).suffix.lower() not in {".png", ".ppm"} or \
                not isinstance(artifact_hash, str) or SHA_PATTERN.fullmatch(artifact_hash) is None:
            errors.append(capture_id + " is not bound to an allowed actual GPU capture")
        elif verify_files:
            path = Path(artifact_path)
            if not path.is_absolute():
                path = manifest_path.parent / path
            if not path.is_file():
                errors.append(capture_id + " capture file is missing")
            else:
                if _sha256_file(path) != artifact_hash:
                    errors.append(capture_id + " screenshot SHA-256 differs")
                try:
                    if list(_image_dimensions(path)) != expected["expected_physical_px"]:
                        errors.append(capture_id + " screenshot dimensions differ")
                except EvidenceError as error:
                    errors.append(str(error))
        if entry.get("machine_status") != "PASS":
            errors.append(capture_id + " machine status is not PASS")
        metrics = entry.get("machine_metrics", {})
        exact_zero = ("safe_area_violations", "truncated_text_runs", "overlap_pairs",
                      "opaque_icon_count", "unexpanded_icon_count", "debug_node_count",
                      "glyph_rasterizations_during_frame")
        for metric in exact_zero:
            if metrics.get(metric) != 0:
                errors.append(capture_id + " machine metric must be zero: " + metric)
        minimum_text = metrics.get("minimum_rendered_text_px")
        if not isinstance(minimum_text, (int, float)) or \
                minimum_text + 1e-6 < expected["minimum_text_physical_px"]:
            errors.append(capture_id + " minimum rendered text is below contract")
        if not isinstance(metrics.get("expanded_icon_count"), int) or \
                metrics.get("expanded_icon_count", -1) < expected["minimum_expanded_icons"]:
            errors.append(capture_id + " expanded icon coverage is incomplete")
        for metric, expected_key in (("focus_indicator_count", "minimum_focus"),
                                     ("selected_indicator_count", "minimum_selected"),
                                     ("error_indicator_count", "minimum_error"),
                                     ("disabled_shape_count", "minimum_disabled")):
            actual = metrics.get(metric)
            if not isinstance(actual, int) or not _count_gate(actual, expected[expected_key]):
                errors.append(capture_id + " state indicator coverage differs: " + metric)
        performance = entry.get("performance", {})
        if performance.get("measurement_method") not in {
                "gpu_timer_query_plus_cpu_frame", "gpu_frame_capture"}:
            errors.append(capture_id + " performance measurement method is invalid")
        gates = (
            ("warmup_frames", ">=", PERFORMANCE_THRESHOLDS["warmup_frames_min"]),
            ("sample_frames", ">=", PERFORMANCE_THRESHOLDS["sample_frames_min"]),
            ("p95_total_frame_ms", "<=", PERFORMANCE_THRESHOLDS["p95_total_frame_ms_max"]),
            ("draw_calls_complete_scene", "<=", PERFORMANCE_THRESHOLDS["draw_calls_complete_scene_max"]),
            ("texture_resident_mib", "<=", PERFORMANCE_THRESHOLDS["texture_resident_mib_max"]),
        )
        for metric, comparison, threshold in gates:
            actual = performance.get(metric)
            passed = isinstance(actual, (int, float)) and (
                actual >= threshold if comparison == ">=" else actual <= threshold)
            if not passed:
                errors.append(capture_id + " performance gate failed: " + metric)
    return errors


def validate_manual_review(manual, capture, capture_manifest_path=None,
                           repository_root=ROOT, verify_files=True):
    errors = validate_capture_manifest(capture, capture_manifest_path,
                                       repository_root, verify_files)
    if errors:
        return ["manual review prohibited until all 594 machine captures PASS"] + errors
    if manual.get("schema") != "phase3_gpu_manual_review_v1" or \
            manual.get("status") not in {"PASS", "FAIL"} or \
            manual.get("reviewer_role") != "independent_technical_artist":
        errors.append("manual review identity/status/reviewer is invalid")
    path = Path(capture_manifest_path) if capture_manifest_path else None
    capture_bytes = (json.dumps(capture, indent=2, sort_keys=True) + "\n").encode() \
        if path is None else path.read_bytes()
    binding = manual.get("capture_binding", {})
    if binding.get("manifest_sha256") != _sha256(capture_bytes) or \
            binding.get("machine_status_required") != "PASS_ALL_594" or \
            not isinstance(binding.get("manifest_path"), str) or \
            binding.get("manifest_path") == "PENDING":
        errors.append("manual review is not SHA-bound to the passing capture manifest")
    boundary = manual.get("evidence_boundary", {})
    if boundary.get("flat_proof_is_capture") is not False or \
            boundary.get("flat_proof_is_user_study") is not False:
        errors.append("manual review cannot treat flat proof as capture/user study")
    if manual.get("review_axes") != list(MANUAL_AXES):
        errors.append("manual review axes differ")
    expected_hashes = {entry["capture_id"]: entry["artifact"]["sha256"]
                       for entry in capture["captures"]}
    reviews = manual.get("capture_reviews")
    if not isinstance(reviews, list):
        reviews = []
        errors.append("manual capture_reviews must be an array")
    by_id = {entry.get("capture_id"): entry for entry in reviews if isinstance(entry, dict)}
    if len(reviews) != len(by_id) or set(by_id) != set(expected_hashes):
        errors.append("manual review must exactly cover all 594 capture IDs")
    any_fail = False
    for capture_id, screenshot_hash in expected_hashes.items():
        review = by_id.get(capture_id)
        if review is None:
            continue
        if review.get("screenshot_sha256") != screenshot_hash:
            errors.append(capture_id + " manual screenshot hash differs")
        axes = review.get("axes", {})
        if set(axes) != set(MANUAL_AXES):
            errors.append(capture_id + " manual axis coverage differs")
            continue
        surface = capture_id.split("__", 1)[0]
        for axis, result in axes.items():
            status = result.get("status")
            evidence = result.get("evidence")
            if status not in {"PASS", "FAIL", "N/A"} or \
                    not isinstance(evidence, str) or not evidence.strip() or "PENDING" in evidence:
                errors.append(capture_id + " manual result is incomplete: " + axis)
            if surface == "hud" and axis == "focus_selected_error_disabled_clarity" and status != "N/A":
                errors.append(capture_id + " HUD focus/selection manual axis must be N/A")
            if surface == "shell" and axis == "camera_preset_consistency" and status != "N/A":
                errors.append(capture_id + " Shell camera manual axis must be N/A")
            any_fail = any_fail or status == "FAIL"
    if manual.get("status") == "PASS" and any_fail:
        errors.append("manual summary cannot PASS while an axis fails")
    if manual.get("status") == "FAIL" and not any_fail:
        errors.append("manual summary cannot FAIL without a failing axis")
    policy = manual.get("template_policy", {})
    if policy.get("golden_creation_authorized") is not False:
        errors.append("manual review template does not authorize golden creation")
    return errors


def manual_acceptance_errors(manual):
    """Return acceptance failures after a manual artifact is structurally valid."""
    return [] if manual.get("status") == "PASS" else [
        "independent manual review outcome is FAIL"]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("capture_manifest", type=Path)
    parser.add_argument("--manual-review", type=Path)
    parser.add_argument("--pending-template", action="store_true")
    parser.add_argument(
        "--allow-failed-manual", action="store_true",
        help="validate and preserve a structurally valid failed manual artifact without passing acceptance")
    args = parser.parse_args()
    try:
        capture = json.loads(args.capture_manifest.read_text())
    except (OSError, json.JSONDecodeError) as error:
        print("Phase3 GPU evidence validation failed: " + str(error), file=sys.stderr)
        return 1
    if args.pending_template:
        errors = validate_pending_capture_template(capture)
        if args.manual_review:
            try:
                manual = json.loads(args.manual_review.read_text())
            except (OSError, json.JSONDecodeError) as error:
                print("Phase3 GPU evidence validation failed: " + str(error), file=sys.stderr)
                return 1
            errors += validate_pending_manual_template(manual)
        if errors:
            for error in errors:
                print("Phase3 GPU evidence validation failed: " + error, file=sys.stderr)
            return 1
        print("Phase3 GPU PENDING templates valid: captures=594 manual_reviews=594 acceptance=NOT_RUN")
        return 0
    if args.manual_review:
        manual = json.loads(args.manual_review.read_text())
        errors = validate_manual_review(manual, capture, args.capture_manifest)
    else:
        errors = validate_capture_manifest(capture, args.capture_manifest)
    if errors:
        for error in errors:
            print("Phase3 GPU evidence validation failed: " + error, file=sys.stderr)
        return 1
    if args.manual_review:
        acceptance_errors = manual_acceptance_errors(manual)
        if acceptance_errors:
            print("Phase3 GPU evidence VALID: captures=594 machine=594 manual=594 "
                  "manual_outcome=FAIL acceptance=FAIL")
            if args.allow_failed_manual:
                return 0
            for error in acceptance_errors:
                print("Phase3 GPU acceptance failed: " + error, file=sys.stderr)
            return 1
    print("Phase3 GPU evidence PASS: captures=594 machine=594 manual=%s" %
          ("594 manual_outcome=PASS" if args.manual_review else "NOT_PROVIDED"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
