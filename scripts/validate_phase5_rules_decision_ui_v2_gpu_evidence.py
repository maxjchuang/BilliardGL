#!/usr/bin/env python3
"""Validate the frozen Phase-5 Rules Decision UI v2 actual-GPU evidence."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_RECEIPT = (ROOT /
    "docs/evidence/phase5/rules-decision-ui-v2-gpu-machine-pass-receipt-v1.json")
DEFAULT_FINAL_MANIFEST = (ROOT / "docs/evidence/phase5/actual-gpu-v2-evidence/"
                          "rules-decision-ui-actual-gpu-evidence-v2.json")
DEFAULT_MANUAL_REVIEW = (ROOT / "docs/evidence/phase5/"
                         "rules-decision-ui-v2-independent-manual-review-v1.json")
EXPECTED_STATUS = (
    "ACTUAL_GPU_MACHINE_PASS_60_OF_60_PENDING_INDEPENDENT_MANUAL_REVIEW")
EXPECTED_PASSES = [
    "shadow_depth", "hdr_opaque", "overlay", "aces_tone_map",
    "product_ui", "product_present",
]


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def bound_path(value: str) -> Path:
    path = Path(value)
    return path if path.is_absolute() else ROOT / path


def parse_ppm(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    position = 0

    def token() -> bytes:
        nonlocal position
        while position < len(data):
            if data[position] == 35:
                newline = data.find(b"\n", position)
                require(newline >= 0, f"unterminated PPM comment: {path}")
                position = newline + 1
            elif data[position] in b" \t\r\n":
                position += 1
            else:
                break
        end = position
        while end < len(data) and data[end] not in b" \t\r\n":
            end += 1
        value = data[position:end]
        position = end
        return value

    require(token() == b"P6", f"PPM magic mismatch: {path}")
    width, height, maximum = int(token()), int(token()), int(token())
    require(maximum == 255, f"PPM maximum mismatch: {path}")
    while position < len(data) and data[position] in b" \t\r\n":
        position += 1
    require(len(data) - position == width * height * 3,
            f"PPM payload length mismatch: {path}")
    return width, height


def load_inputs(receipt_path: Path) -> tuple[dict[str, Any], list[list[str]],
                                                list[dict[str, Any]]]:
    receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    frozen = receipt["frozen_bindings"]
    plan_path = bound_path("docs/evidence/phase5/rules-decision-ui-v2-ready/"
                           "rules-decision-ui-gpu-plan-v2.tsv")
    results_path = bound_path(frozen["gpu_results_jsonl_path"])
    plan = [line.split("\t") for line in
            plan_path.read_text(encoding="utf-8").splitlines() if line]
    rows = [json.loads(line) for line in
            results_path.read_text(encoding="utf-8").splitlines() if line]
    return receipt, plan, rows


def validate_structures(receipt: dict[str, Any], plan: list[list[str]],
                        rows: list[dict[str, Any]]) -> None:
    require(receipt.get("schema") ==
            "phase5_rules_decision_ui_v2_gpu_machine_pass_receipt_v1",
            "receipt schema mismatch")
    require(receipt.get("status") == EXPECTED_STATUS, "receipt status mismatch")
    require(receipt.get("counts") == {
        "actual_gpu_captures_completed": 60,
        "actual_gpu_captures_failed": 0,
        "independent_manual_reviews_completed": 0,
        "planned_cases": 60,
        "unrun_cases": 0,
    }, "receipt counts mismatch")
    require(all(value is False for value in receipt.get("unclaimed", {}).values())
            and len(receipt.get("unclaimed", {})) == 6,
            "premature promotion claim")
    require(len(plan) == 60 and all(len(row) == 12 for row in plan),
            "plan must contain 60 rows of 12 fields")
    require(len({row[0] for row in plan}) == 60, "plan capture IDs are not unique")
    require(len(rows) == 60, "GPU result row count mismatch")
    require(len({row.get("capture_id") for row in rows}) == 60,
            "GPU result capture IDs are not unique")
    require(len({row.get("ppm_path") for row in rows}) == 60,
            "GPU result PPM paths are not unique")
    require(len({row.get("ppm_sha256") for row in rows}) == 60,
            "GPU result PPM hashes are not unique")
    for ordinal, (selector, row) in enumerate(zip(plan, rows), 1):
        capture_id, state = selector[0], selector[1]
        require(row.get("capture_id") == capture_id and row.get("state") == state,
                f"plan/result order mismatch at ordinal {ordinal}")
        require(row.get("actual_gpu_capture") is True,
                f"actual GPU flag missing: {capture_id}")
        require(row.get("rules_ui_after_tone_map") is True and
                row.get("render_passes") == EXPECTED_PASSES,
                f"render pass order mismatch: {capture_id}")
        require(row.get("gpu_draw_calls") == 1,
                f"GPU draw count mismatch: {capture_id}")
        canonical = row.get("canonical_sha256")
        require(isinstance(canonical, str) and len(canonical) == 64 and
                canonical == row.get("gpu_canonical_sha256"),
                f"CPU/GPU canonical mismatch: {capture_id}")
        expected_path = ("docs/evidence/phase5/actual-gpu-v2/" +
                         capture_id + ".ppm")
        require(row.get("ppm_path") == expected_path,
                f"PPM path mismatch: {capture_id}")


def validate_files(receipt_path: Path, receipt: dict[str, Any],
                   plan: list[list[str]], rows: list[dict[str, Any]]) -> None:
    frozen = receipt["frozen_bindings"]
    checks = {
        "gpu_plan_sha256": bound_path(
            "docs/evidence/phase5/rules-decision-ui-v2-ready/"
            "rules-decision-ui-gpu-plan-v2.tsv"),
        "gpu_results_jsonl_sha256": bound_path(frozen["gpu_results_jsonl_path"]),
        "ready_manifest_sha256": bound_path(
            "docs/evidence/phase5/rules-decision-ui-v2-ready/"
            "rules-decision-ui-gpu-capture-ready-v2.json"),
        "source_bindings_sha256": bound_path(
            "docs/evidence/phase5/rules-decision-ui-v2-ready/"
            "rules-decision-ui-source-bindings-v2.json"),
        "runner_source_sha256": bound_path(
            "spikes/render_core_profile/"
            "phase5_rules_decision_ui_gpu_capture_runner.mm"),
    }
    for field, path in checks.items():
        require(path.is_file() and sha(path) == frozen[field],
                f"frozen binding mismatch: {field}")
    authorization = receipt["authorization"]
    auth_path = bound_path(authorization["authorization_path"])
    marker_path = bound_path(authorization["consumed_marker_path"])
    require(auth_path.is_file() and sha(auth_path) ==
            authorization["authorization_sha256"], "authorization hash mismatch")
    require(marker_path.is_file() and sha(marker_path) ==
            authorization["consumed_marker_sha256"], "consumed marker mismatch")
    auth = json.loads(auth_path.read_text(encoding="utf-8"))
    require(marker_path.read_text(encoding="utf-8") ==
            auth["authorization_id"] + "\n", "consumed marker payload mismatch")

    aggregate = hashlib.sha256()
    total_bytes = 0
    for selector, row in zip(plan, rows):
        path = bound_path(row["ppm_path"])
        require(path.is_file(), f"PPM missing: {row['capture_id']}")
        digest = sha(path)
        require(digest == row["ppm_sha256"],
                f"PPM hash mismatch: {row['capture_id']}")
        width, height = parse_ppm(path)
        require((width, height) == (int(selector[7]), int(selector[8])),
                f"PPM dimension mismatch: {row['capture_id']}")
        aggregate.update(row["ppm_path"].encode("utf-8"))
        aggregate.update(b"\0")
        aggregate.update(digest.encode("ascii"))
        aggregate.update(b"\n")
        total_bytes += path.stat().st_size
    require(aggregate.hexdigest() == frozen["ppm_aggregate_sha256"],
            "PPM aggregate mismatch")
    require(total_bytes == frozen["ppm_total_bytes"], "PPM byte total mismatch")
    require(frozen["ppm_file_count"] == 60, "PPM file count mismatch")
    require(sha(receipt_path) ==
            "3b9a807ce6c906dc2f3d29247edf15f747740c52d955a74ff50fef70d7da3cc0",
            "committed receipt hash mismatch")


def validate_final_manifest(manifest_path: Path, receipt_path: Path,
                            receipt: dict[str, Any], plan: list[list[str]],
                            rows: list[dict[str, Any]]) -> None:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    require(manifest.get("schema") ==
            "phase5_rules_decision_ui_actual_gpu_evidence_v2" and
            manifest.get("status") ==
            "ACTUAL_GPU_MACHINE_PASS_60_OF_60_MANUAL_REVIEW_PENDING" and
            manifest.get("case_count") == 60,
            "final PNG evidence header mismatch")
    claims = manifest.get("claims", {})
    require(claims == {
        "actual_gpu_capture_completed": 60,
        "external_beta_qa_completed": 0,
        "machine_validation_completed": 60,
        "manual_review_completed": 0,
        "product_exposed": False,
        "profile_complete": False,
        "screen_reader_session_completed": False,
        "true_input_session_completed": False,
    }, "final PNG evidence claims mismatch")
    require(manifest.get("conversion") == {
        "destination_format": "PNG RGB8 lossless",
        "gpu_rerun": False,
        "pixel_transform": "none",
        "source_format": "P6 PPM RGB8",
    }, "final PNG conversion boundary mismatch")
    frozen = manifest["frozen_bindings"]
    require(frozen["machine_pass_receipt_sha256"] == sha(receipt_path) and
            frozen["machine_pass_receipt_path"] ==
            str(receipt_path.relative_to(ROOT)),
            "final PNG receipt binding mismatch")
    for field in ("authorization_sha256", "consumed_marker_sha256",
                  "ready_manifest_sha256", "gpu_plan_sha256",
                  "gpu_results_jsonl_sha256", "source_bindings_sha256",
                  "source_aggregate_sha256", "runner_source_sha256",
                  "runner_binary_sha256", "billiards_binary_sha256",
                  "ppm_aggregate_sha256"):
        source = receipt["authorization"] if field in {
            "authorization_sha256", "consumed_marker_sha256"} else \
            receipt["frozen_bindings"]
        require(frozen[field] == source[field],
                f"final PNG inherited binding mismatch: {field}")
    cases = manifest.get("cases", [])
    require(len(cases) == 60 and
            len({case.get("png_path") for case in cases}) == 60 and
            len({case.get("png_sha256") for case in cases}) == 60,
            "final PNG case uniqueness mismatch")
    aggregate = hashlib.sha256()
    for selector, row, case in zip(plan, rows, cases):
        capture_id = selector[0]
        require(case.get("capture_id") == capture_id and
                case.get("ppm_path") == row["ppm_path"] and
                case.get("ppm_sha256") == row["ppm_sha256"] and
                case.get("canonical_sha256") == row["canonical_sha256"] and
                case.get("gpu_canonical_sha256") == row["gpu_canonical_sha256"] and
                case.get("actual_gpu_capture") is True and
                case.get("rules_ui_after_tone_map") is True,
                f"final PNG case binding mismatch: {capture_id}")
        path = bound_path(case["png_path"])
        require(path.is_file() and sha(path) == case["png_sha256"],
                f"final PNG hash mismatch: {capture_id}")
        header = path.read_bytes()[:24]
        require(header[:16] == b"\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR" and
                len(header) == 24,
                f"final PNG header mismatch: {capture_id}")
        width, height = struct.unpack(">II", header[16:24])
        require([width, height] == case.get("physical_dimensions") ==
                [int(selector[7]), int(selector[8])],
                f"final PNG dimension mismatch: {capture_id}")
        aggregate.update(case["png_path"].encode("utf-8"))
        aggregate.update(b"\0")
        aggregate.update(case["png_sha256"].encode("ascii"))
        aggregate.update(b"\n")
    require(aggregate.hexdigest() == frozen["png_aggregate_sha256"],
            "final PNG aggregate mismatch")


def validate_manual_review(manual_path: Path, manifest_path: Path) -> None:
    manual = json.loads(manual_path.read_text(encoding="utf-8"))
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    require(manual.get("schema") ==
            "phase5_rules_decision_ui_v2_independent_manual_visual_review_v1" and
            manual.get("status") == "MANUAL_VISUAL_REVIEW_FAIL_BLOCKING" and
            manual.get("overall_result") == "FAIL",
            "manual review header mismatch")
    expected_counts = {
        "axes_fail": 32, "axes_na": 96, "axes_pass": 472,
        "axes_pending": 0, "axes_total": 600, "cases_fail": 20,
        "cases_pass": 40, "cases_pending": 0, "cases_reviewed": 60,
        "cases_total": 60,
    }
    require(manual.get("review_counts") == expected_counts,
            "manual review declared counts mismatch")
    boundary = manual.get("evidence_boundary", {})
    require(boundary.get("manual_visual_result") == "FAIL" and
            boundary.get("product_exposed") is False and
            boundary.get("beta_pass") is False and
            boundary.get("profile_complete") is False and
            boundary.get("required_real_keyboard_mouse_sessions") == "NOT_RUN" and
            boundary.get("required_screen_reader_sessions") == "NOT_RUN" and
            boundary.get("required_external_human_cvd_localization_qa") == "NOT_RUN",
            "manual review external/promotion boundary mismatch")
    official = manual["input_bindings"]["official_png_evidence"]
    require(official["sha256"] == sha(manifest_path) and
            official["png_aggregate_sha256"] ==
            manifest["frozen_bindings"]["png_aggregate_sha256"] and
            official["ppm_png_pixel_exact_verified"] == "60_OF_60",
            "manual review final PNG binding mismatch")
    cases = manual.get("cases", [])
    manifest_cases = manifest["cases"]
    require(len(cases) == len(manifest_cases) == 60,
            "manual review case count mismatch")
    axis_counts: dict[str, int] = {}
    case_counts: dict[str, int] = {}
    failed_ids = []
    for case, evidence in zip(cases, manifest_cases):
        capture_id = evidence["capture_id"]
        require(case.get("case_id") == capture_id and
                case.get("reviewed_original_resolution") is True and
                case.get("actual_gpu_capture") is True and
                case.get("official_png_path") == evidence["png_path"] and
                case.get("official_png_sha256") == evidence["png_sha256"] and
                case.get("ppm_path") == evidence["ppm_path"] and
                case.get("ppm_sha256") == evidence["ppm_sha256"] and
                case.get("physical_dimensions") == evidence["physical_dimensions"],
                f"manual case artifact binding mismatch: {capture_id}")
        findings = case.get("axis_findings", {})
        require(len(findings) == 10, f"manual axis count mismatch: {capture_id}")
        for finding in findings.values():
            status = finding.get("status")
            require(status in {"PASS", "FAIL", "N/A"},
                    f"manual axis status invalid: {capture_id}")
            require(bool(finding.get("evidence")),
                    f"manual axis evidence missing: {capture_id}")
            require(finding.get("blocking") is (status == "FAIL"),
                    f"manual blocking flag mismatch: {capture_id}")
            axis_counts[status] = axis_counts.get(status, 0) + 1
        case_status = case.get("manual_case_status")
        require(case_status in {"PASS", "FAIL"},
                f"manual case status invalid: {capture_id}")
        require((case_status == "FAIL") == bool(case.get("blocking_defect_ids")),
                f"manual case/defect mismatch: {capture_id}")
        case_counts[case_status] = case_counts.get(case_status, 0) + 1
        if case_status == "FAIL":
            failed_ids.append(capture_id)
    require(axis_counts == {"PASS": 472, "FAIL": 32, "N/A": 96} and
            case_counts == {"PASS": 40, "FAIL": 20},
            "manual review derived counts mismatch")
    require(failed_ids == manual.get("failed_case_ids"),
            "manual review failed ID ordering mismatch")
    require(sha(manual_path) ==
            "40ab295412081bba3ecb87351dabebc76a85d522c75b4bbddd9c1ed90f6540c0",
            "committed manual review hash mismatch")


def validate(receipt_path: Path) -> None:
    receipt, plan, rows = load_inputs(receipt_path)
    validate_structures(receipt, plan, rows)
    validate_files(receipt_path, receipt, plan, rows)
    require(DEFAULT_FINAL_MANIFEST.is_file(), "final PNG evidence missing")
    validate_final_manifest(DEFAULT_FINAL_MANIFEST, receipt_path, receipt,
                            plan, rows)
    require(DEFAULT_MANUAL_REVIEW.is_file(), "independent manual review missing")
    validate_manual_review(DEFAULT_MANUAL_REVIEW, DEFAULT_FINAL_MANIFEST)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("receipt", type=Path, nargs="?", default=DEFAULT_RECEIPT)
    args = parser.parse_args()
    validate(args.receipt.resolve())
    print("Phase5 Rules Decision UI v2 actual-GPU evidence: PASS 60/60; "
          "manual/product/Beta remain unclaimed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
