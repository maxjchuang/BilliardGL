#!/usr/bin/env python3
"""Validate Phase5 Rules UI v5 exact16 independent-TA template/review."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts.generate_phase5_rules_decision_ui_v5_exact16_ta_review_template import (  # noqa: E402
    CANONICAL_AXES,
    CASE_IDS,
    CONTROL_CASE_IDS,
    DEFAULT_OUTPUT,
    PREORDINAL_FAILURE_PATH,
    PREORDINAL_FAILURE_SHA256,
    PRIOR_BLOCKING_AXES,
    PRODUCER_GATE_PATH,
    PRODUCER_GATE_SHA256,
    PROFILE_DIMENSIONS,
    REPAIR_CASE_IDS,
    REPAIR_REQUIRED_AXES,
    REPAIR_VISUAL_CONTRACT,
    STABLE_AGGREGATE_ALGORITHM,
    V4_MANUAL_PATH,
    V4_MANUAL_SHA256,
    V5_GATE_PATH,
    V5_GATE_SHA256,
    build_template,
    control_axes,
    control_visual_contract,
    profile,
)


DEFAULT_REVIEW = DEFAULT_OUTPUT
READINESS_SCHEMA = "phase5_rules_decision_ui_v5_exact16_readiness_v2"
READINESS_STATUS = (
    "V5_PIXEL_VISIBLE_EXACT16_CORRECTED_PRODUCER_NOT_RUN_NO_AUTH")
MACHINE_SCHEMA = (
    "phase5_rules_decision_ui_v5_exact16_machine_pass_receipt_v1")
MACHINE_STATUS = "TARGETED16_MACHINE_PASS"
PNG_MANIFEST_SCHEMA = (
    "phase5_rules_decision_ui_v5_exact16_stable_png_manifest_v1")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(value, dict), f"{path}: JSON object required")
    return value


def canonical_hash(value: Any) -> str:
    payload = json.dumps(value, ensure_ascii=False, sort_keys=True,
                         separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def bound_file(relative: Any, digest: Any, label: str) -> Path:
    require(isinstance(relative, str) and relative
            and not Path(relative).is_absolute(),
            label + ": repository-relative path required")
    require(isinstance(digest, str) and len(digest) == 64
            and all(character in "0123456789abcdef" for character in digest),
            label + ": lowercase SHA-256 required")
    try:
        path = (ROOT / relative).resolve(strict=True)
        path.relative_to(ROOT.resolve())
    except (OSError, ValueError) as error:
        raise RuntimeError(label + ": missing/outside repository") from error
    require(path.is_file() and sha256(path) == digest,
            label + ": byte binding mismatch")
    return path


def read_ppm_dimensions(path: Path) -> list[int]:
    with path.open("rb") as image:
        require(image.readline().strip() == b"P6", f"{path}: P6 required")
        line = image.readline()
        while line.startswith(b"#"):
            line = image.readline()
        width, height = map(int, line.split())
        require(image.readline().strip() == b"255", f"{path}: RGB8 required")
        pixels = image.read()
    require(len(pixels) == width * height * 3, f"{path}: PPM byte count")
    return [width, height]


def read_png_dimensions(path: Path) -> list[int]:
    with path.open("rb") as image:
        require(image.read(8) == b"\x89PNG\r\n\x1a\n",
                f"{path}: PNG signature")
        require(struct.unpack(">I", image.read(4))[0] == 13
                and image.read(4) == b"IHDR", f"{path}: PNG IHDR")
        width, height = struct.unpack(">II", image.read(8))
    return [width, height]


def validate_frozen_bindings(review: dict[str, Any]) -> None:
    expected = {
        "v4_independent_ta_manual": {
            "path": V4_MANUAL_PATH, "sha256": V4_MANUAL_SHA256},
        "v5_pixel_visible_design_gate": {
            "path": V5_GATE_PATH, "sha256": V5_GATE_SHA256},
        "v5_preordinal_failure_receipt": {
            "path": PREORDINAL_FAILURE_PATH,
            "sha256": PREORDINAL_FAILURE_SHA256},
        "v5_producer_contract_correction_gate": {
            "path": PRODUCER_GATE_PATH, "sha256": PRODUCER_GATE_SHA256},
    }
    require(review.get("frozen_bindings") == expected,
            "exact four frozen bindings required")
    manual = load_json(bound_file(
        V4_MANUAL_PATH, V4_MANUAL_SHA256, "v4 manual"))
    require(manual.get("overall_result") == "FAIL"
            and manual.get("failed_case_ids") == REPAIR_CASE_IDS
            and manual.get("review_counts", {}).get("cases_expected") == 16
            and manual.get("review_counts", {}).get("axes_expected") == 160
            and manual.get("review_counts", {}).get("axes_fail") == 8,
            "v4 manual exact 4-case/8-axis failure")
    gate = load_json(bound_file(V5_GATE_PATH, V5_GATE_SHA256, "v5 gate"))
    targeted = gate.get("future_targeted_actual_gpu_gate", {})
    ta = gate.get("future_technical_art_gate", {})
    require(gate.get("schema") ==
            "phase5_rules_decision_ui_v5_pixel_visible_shape_"
            "repair_design_gate_v1"
            and targeted.get("repair_case_ids") == REPAIR_CASE_IDS
            and targeted.get("regression_control_case_ids") == CONTROL_CASE_IDS
            and targeted.get("minimum_case_count") == 16
            and ta.get("canonical_axes") == CANONICAL_AXES
            and ta.get("image_count") == 16
            and ta.get("axis_review_count") == 160
            and gate.get("evidence_boundary", {}).get(
                "gpu_execution_authorized") is False,
            "v5 gate exact16/160-axis no-GPU contract")
    failure = load_json(bound_file(
        PREORDINAL_FAILURE_PATH, PREORDINAL_FAILURE_SHA256,
        "v5 preordinal failure"))
    attempt = failure.get("attempt_scope", {})
    claims = failure.get("claims", {})
    require(failure.get("schema") ==
            "phase5_rules_decision_ui_v5_exact16_"
            "preordinal_contract_failure_receipt_v1"
            and failure.get("status") ==
            "FROZEN_PREORDINAL_PRODUCER_CONTRACT_FAILURE_NO_GPU_NO_REUSE"
            and attempt.get("case_count") == 16
            and attempt.get("completed_count") == 0
            and attempt.get("unrun_count") == 16
            and attempt.get("cocoa_started") is False
            and attempt.get("opengl_context_created") is False
            and attempt.get("gpu_work_submitted") is False
            and attempt.get("ppm_count") == 0
            and attempt.get("png_count") == 0
            and claims.get("targeted_machine16_pass") is False
            and claims.get("technical_art_review") is False
            and claims.get("product_exposed") is False,
            "frozen preordinal zero-image/no-pass boundary")
    producer = load_json(bound_file(
        PRODUCER_GATE_PATH, PRODUCER_GATE_SHA256, "producer gate"))
    contract = producer.get("required_corrected_contract", {})
    acceptance = producer.get("acceptance", {})
    require(producer.get("schema") ==
            "phase5_rules_decision_ui_v5_exact16_"
            "producer_contract_correction_gate_v1"
            and producer.get("status") ==
            "APPROVED_MINIMAL_CAPTURE_ONLY_CONTRACT_CORRECTION_NO_GPU_NO_AUTH"
            and producer.get("failure_receipt", {}).get("sha256") ==
                PREORDINAL_FAILURE_SHA256
            and contract.get("manifest_gate_sha256") == V5_GATE_SHA256
            and contract.get("readiness_schema") == READINESS_SCHEMA
            and contract.get("case_count") == 16
            and contract.get("case_ids_and_order_unchanged") is True
            and acceptance.get("gpu_executed") is False,
            "producer correction gate exact successor/no-GPU boundary")


def validate_common_structure(review: dict[str, Any]) -> None:
    require(review.get("schema") ==
            "phase5_rules_decision_ui_v5_exact16_"
            "independent_ta_review_v1"
            and review.get("schema_version") == 1,
            "review schema/version")
    require(review.get("review_scope") == {
        "case_ids_in_order": CASE_IDS,
        "repair_case_ids_in_order": REPAIR_CASE_IDS,
        "regression_control_case_ids_in_order": CONTROL_CASE_IDS,
        "case_count": 16, "repair_case_count": 4,
        "regression_control_case_count": 12,
        "canonical_axes_in_order": CANONICAL_AXES,
        "axes_per_case": 10, "total_axis_count": 160,
        "prior_blocking_axis_occurrence_count": 8,
        "original_physical_resolution_required": True,
    }, "exact 4+12/16x10 scope")
    require(review.get("allowed_statuses") == {
        "template_case": ["NOT_RUN"], "template_axis": ["PENDING"],
        "completed_case": ["PASS", "FAIL"],
        "completed_axis": ["PASS", "FAIL"],
    }, "status vocabulary")
    preconditions = review.get("completion_preconditions", [])
    require(len(preconditions) == 7
            and any("16/16 targeted machine PASS" in row for row in preconditions)
            and any("PPM+PNG" in row for row in preconditions)
            and any("leading chevron" in row for row in preconditions)
            and any("12 controls" in row and "placement" in row
                    for row in preconditions)
            and any("all 160 axes atomically" in row for row in preconditions),
            "fail-closed completion preconditions")
    cases = review.get("cases", [])
    require(len(cases) == 16
            and [case.get("case_id") for case in cases] == CASE_IDS,
            "exact case inventory/order")
    for ordinal, case in enumerate(cases, start=1):
        case_id = case["case_id"]
        repair = case_id in REPAIR_CASE_IDS
        require(case.get("ordinal") == ordinal
                and case.get("coverage_kind") ==
                    ("REPAIR" if repair else "REGRESSION_CONTROL")
                and case.get("profile_id") == profile(case_id)
                and case.get("expected_physical_dimensions") ==
                    PROFILE_DIMENSIONS[profile(case_id)]
                and case.get("prior_blocking_axes") ==
                    (PRIOR_BLOCKING_AXES if repair else [])
                and case.get("required_review_axes") ==
                    (REPAIR_REQUIRED_AXES if repair else control_axes(case_id))
                and case.get("required_visual_contract") ==
                    (list(REPAIR_VISUAL_CONTRACT.values()) if repair else
                     control_visual_contract(case_id)),
                case_id + ": repair/control contract")
        findings = case.get("axis_findings", {})
        require(list(findings) == CANONICAL_AXES,
                case_id + ": canonical ten-axis order")
        for axis in case["prior_blocking_axes"] + case["required_review_axes"]:
            require(axis in findings, case_id + ": required axis missing")


def validate_template_mode(review: dict[str, Any]) -> dict[str, Any]:
    require(review == build_template(),
            "NOT_RUN template must remain byte-semantically pristine")
    return {
        "status": "PASS_V5_EXACT16_TA_TEMPLATE_NOT_RUN",
        "mode": "TEMPLATE", "case_count": 16,
        "repair_case_count": 4, "control_case_count": 12,
        "axis_count": 160, "cases_not_run": 16, "axes_pending": 160,
        "readiness_bound": False, "machine_receipt_bound": False,
        "ppm_bound_count": 0, "png_bound_count": 0,
        "human_ta_review_performed": False,
        "targeted16_ta_pass": False,
    }


def validate_completed_mode(review: dict[str, Any]) -> dict[str, Any]:
    require(review.get("status") == "COMPLETED_INDEPENDENT_TA_REVIEW"
            and review.get("overall_result") in ("PASS", "FAIL"),
            "completed review identity/result")
    future = review.get("future_successor_evidence", {})
    readiness_binding = future.get("readiness", {})
    machine_binding = future.get("machine_receipt", {})
    manifest_binding = future.get("stable_png_manifest", {})
    require(readiness_binding.get("status") == "BOUND"
            and machine_binding.get("status") == "BOUND"
            and manifest_binding.get("status") == "BOUND",
            "completed review needs readiness/machine/PNG manifest")
    readiness_path = bound_file(readiness_binding.get("path"),
                                readiness_binding.get("sha256"), "readiness")
    machine_path = bound_file(machine_binding.get("path"),
                              machine_binding.get("sha256"), "machine receipt")
    manifest_path = bound_file(manifest_binding.get("path"),
                               manifest_binding.get("sha256"), "PNG manifest")
    readiness = load_json(readiness_path)
    require(readiness.get("schema") == READINESS_SCHEMA
            and readiness.get("status") == READINESS_STATUS
            and readiness.get("case_count") == 16
            and readiness.get("case_ids_in_order") == CASE_IDS
            and [row.get("status") for row in
                 readiness.get("case_results", [])] == ["NOT_RUN"] * 16,
            "successor readiness identity/16 NOT_RUN")
    machine = load_json(machine_path)
    require(machine.get("schema") == MACHINE_SCHEMA
            and machine.get("status") == MACHINE_STATUS
            and machine.get("readiness_sha256") ==
                readiness_binding.get("sha256")
            and machine.get("stable_png_manifest_sha256") ==
                manifest_binding.get("sha256")
            and machine.get("case_count") == 16
            and machine.get("machine_pass_count") == 16
            and machine.get("failure_count") == 0
            and machine.get("unrun_count") == 0,
            "future exact 16/16 machine receipt")
    manifest = load_json(manifest_path)
    require(manifest.get("schema") == PNG_MANIFEST_SCHEMA
            and manifest.get("case_count") == 16
            and [row.get("case_id") for row in manifest.get("cases", [])] ==
                CASE_IDS, "future stable PNG manifest inventory")
    machine_cases = machine.get("cases", [])
    require(len(machine_cases) == 16
            and [row.get("case_id") for row in machine_cases] == CASE_IDS,
            "future machine case inventory")
    ppm_entries, png_entries, stable_entries = [], [], []
    case_results, failed_cases = [], []
    axes_pass = axes_fail = prior_pass = prior_fail = 0
    for ordinal, (case, evidence) in enumerate(
            zip(review["cases"], machine_cases), start=1):
        case_id = case["case_id"]
        require(evidence.get("ordinal") == ordinal
                and evidence.get("machine_result") == "PASS",
                case_id + ": future machine PASS row")
        future_case = case.get("future_evidence", {})
        require(future_case == evidence.get("evidence"),
                case_id + ": review/machine evidence equality")
        dimensions = PROFILE_DIMENSIONS[profile(case_id)]
        ppm_path = bound_file(future_case.get("ppm_path"),
                              future_case.get("ppm_sha256"), case_id + " PPM")
        png_path = bound_file(future_case.get("png_path"),
                              future_case.get("png_sha256"), case_id + " PNG")
        require(future_case.get("ppm_physical_dimensions") == dimensions
                and future_case.get("png_physical_dimensions") == dimensions
                and read_ppm_dimensions(ppm_path) == dimensions
                and read_png_dimensions(png_path) == dimensions,
                case_id + ": physical dimensions")
        common = {"ordinal": ordinal, "case_id": case_id}
        ppm = {"path": future_case["ppm_path"],
               "sha256": future_case["ppm_sha256"],
               "physical_dimensions": dimensions}
        png = {"path": future_case["png_path"],
               "sha256": future_case["png_sha256"],
               "physical_dimensions": dimensions}
        ppm_entries.append({**common, **ppm})
        png_entries.append({**common, **png})
        stable_entries.append({**common, "ppm": ppm, "png": png})
        findings = case["axis_findings"]
        require(all(row.get("status") in ("PASS", "FAIL")
                    and isinstance(row.get("observation"), str)
                    and row.get("observation") for row in findings.values()),
                case_id + ": all ten completed findings/observations")
        failed_axes = [axis for axis, row in findings.items()
                       if row["status"] == "FAIL"]
        require(case.get("blocking_axes") == failed_axes
                and case.get("manual_case_result") ==
                    ("FAIL" if failed_axes else "PASS"),
                case_id + ": result/blocking-axis consistency")
        axes_pass += 10 - len(failed_axes)
        axes_fail += len(failed_axes)
        for axis in case.get("prior_blocking_axes", []):
            if findings[axis]["status"] == "PASS":
                prior_pass += 1
            else:
                prior_fail += 1
        case_results.append(case["manual_case_result"])
        if failed_axes:
            failed_cases.append(case_id)
    require(future.get("aggregate_algorithm") == STABLE_AGGREGATE_ALGORITHM
            and future.get("ppm_aggregate_sha256") ==
                canonical_hash(ppm_entries)
            and future.get("png_aggregate_sha256") ==
                canonical_hash(png_entries)
            and future.get("stable_aggregate_sha256") ==
                canonical_hash(stable_entries),
            "future ordered PPM/PNG/stable aggregates")
    overall = "FAIL" if failed_cases else "PASS"
    require(review.get("overall_result") == overall
            and review.get("failed_case_ids") == failed_cases,
            "completed overall/failed cases")
    counts = review.get("review_counts", {})
    require(counts == {
        "cases_total": 16, "repair_cases_total": 4,
        "regression_control_cases_total": 12,
        "cases_not_run": 0, "cases_pass": case_results.count("PASS"),
        "cases_fail": case_results.count("FAIL"), "axes_total": 160,
        "axes_pending": 0, "axes_pass": axes_pass, "axes_fail": axes_fail,
        "prior_blocking_axis_occurrences_total": 8,
        "prior_blocking_axis_occurrences_pass": prior_pass,
        "prior_blocking_axis_occurrences_fail": prior_fail,
    }, "completed counts")
    boundary = review.get("evidence_boundary", {})
    require(boundary.get("template_only") is False
            and boundary.get("successor_readiness_bound") is True
            and boundary.get("machine_receipt_bound") is True
            and boundary.get("stable_png_manifest_bound") is True
            and boundary.get("ppm_bound_count") == 16
            and boundary.get("png_bound_count") == 16
            and boundary.get("stable_aggregate_bound") is True
            and boundary.get("human_ta_review_performed") is True
            and boundary.get("targeted16_ta_pass") is (overall == "PASS"),
            "completed evidence boundary")
    for field in (
            "authorization_created_by_template", "gpu_executed_by_template",
            "production_modified", "product_exposed", "golden_promotion",
            "m2_promotion", "beta_pass", "real_input_pass",
            "screen_reader_pass"):
        require(boundary.get(field) is False,
                "completed external boundary: " + field)
    return {"status": "PASS_V5_EXACT16_COMPLETED_TA_REVIEW_VALIDATION",
            "mode": "COMPLETED", "overall_result": overall,
            "case_count": 16, "axis_count": 160,
            "targeted16_ta_pass": overall == "PASS"}


def validate(path: Path = DEFAULT_REVIEW) -> dict[str, Any]:
    review = load_json(path.resolve(strict=True))
    validate_frozen_bindings(review)
    validate_common_structure(review)
    if (review.get("status") ==
            "TEMPLATE_NOT_RUN_WAITING_FOR_SUCCESSOR_EVIDENCE"):
        return validate_template_mode(review)
    return validate_completed_mode(review)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--review", type=Path, default=DEFAULT_REVIEW)
    arguments = parser.parse_args()
    print(json.dumps(validate(arguments.review), sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
