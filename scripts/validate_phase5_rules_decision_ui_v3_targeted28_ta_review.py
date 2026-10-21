#!/usr/bin/env python3
"""Validate the Phase5 Rules UI v3 targeted28 independent-TA review.

Template mode is exact all-NOT_RUN/all-PENDING and contains no future machine,
readiness, capture, or aggregate binding. Completed mode fails closed until an
exact 28/28 machine receipt, 28 PPMs, 28 PNGs, physical dimensions, and three
ordered aggregate hashes are independently verified. This tool is CPU-only and
read-only; it never authorizes or performs capture.
"""

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

from scripts.generate_phase5_rules_decision_ui_v3_targeted28_ta_review_template import (  # noqa: E402
    CANONICAL_AXES,
    CASE_IDS,
    CONTROL_CASE_IDS,
    CONTROL_REGRESSION_AXES,
    DEFAULT_OUTPUT,
    FAILURE_RECEIPT_PATH,
    FAILURE_RECEIPT_SHA256,
    PROFILE_PHYSICAL_DIMENSIONS,
    REPAIR_BLOCKING_AXES,
    REPAIR_CASE_IDS,
    STABLE_AGGREGATE_ALGORITHM,
    VISUAL_REPAIR_GATE_PATH,
    VISUAL_REPAIR_GATE_SHA256,
    build_template,
    profile_from_case_id,
)


DEFAULT_REVIEW = DEFAULT_OUTPUT
EXPECTED_MACHINE_SCHEMA = (
    "phase5_rules_decision_ui_v3_targeted28_machine_pass_receipt_v1")
EXPECTED_MACHINE_STATUS = "TARGETED28_MACHINE_PASS"
EXPECTED_FAILURE_SCHEMA = (
    "phase5_rules_decision_ui_v3_targeted_preordinal_failure_receipt_v1")
EXPECTED_FAILURE_STATUS = (
    "TARGETED28_PREORDINAL_BINDING_FAILURE_FROZEN_NO_RETRY")


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
    require(isinstance(value, dict), f"{path}: object required")
    return value


def canonical_hash(value: Any) -> str:
    payload = json.dumps(
        value, ensure_ascii=False, sort_keys=True,
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
        raise RuntimeError(label + ": bound file missing/outside repository") \
            from error
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
    require(width > 0 and height > 0 and len(pixels) == width * height * 3,
            f"{path}: invalid PPM dimensions/pixel length")
    return [width, height]


def read_png_dimensions(path: Path) -> list[int]:
    with path.open("rb") as image:
        signature = image.read(8)
        length_bytes = image.read(4)
        chunk_type = image.read(4)
        require(signature == b"\x89PNG\r\n\x1a\n"
                and len(length_bytes) == 4 and chunk_type == b"IHDR",
                f"{path}: PNG signature/IHDR required")
        length = struct.unpack(">I", length_bytes)[0]
        require(length == 13, f"{path}: PNG IHDR length")
        ihdr = image.read(13)
        require(len(ihdr) == 13, f"{path}: truncated PNG IHDR")
        width, height = struct.unpack(">II", ihdr[:8])
    require(width > 0 and height > 0, f"{path}: PNG dimensions")
    return [width, height]


def validate_frozen_bindings(review: dict[str, Any]) -> tuple[dict[str, Any], dict[str, Any]]:
    expected = {
        "visual_repair_design_gate": {
            "path": VISUAL_REPAIR_GATE_PATH,
            "sha256": VISUAL_REPAIR_GATE_SHA256,
        },
        "targeted_preordinal_failure_receipt": {
            "path": FAILURE_RECEIPT_PATH,
            "sha256": FAILURE_RECEIPT_SHA256,
        },
    }
    require(review.get("frozen_bindings") == expected,
            "exact two frozen bindings required")
    gate_path = bound_file(
        VISUAL_REPAIR_GATE_PATH, VISUAL_REPAIR_GATE_SHA256,
        "visual repair gate")
    failure_path = bound_file(
        FAILURE_RECEIPT_PATH, FAILURE_RECEIPT_SHA256,
        "preordinal failure receipt")
    gate = load_json(gate_path)
    failure = load_json(failure_path)
    require(gate.get("schema") ==
            "phase5_rules_decision_ui_v3_visual_repair_design_gate_v1"
            and gate.get("status") ==
            "APPROVED_FOR_SCOPED_VISUAL_REPAIR_IMPLEMENTATION_ONLY",
            "visual repair gate identity/status")
    targeted = gate.get("targeted_actual_gpu_gate", {})
    ta_gate = gate.get("technical_art_re_review_gate", {})
    require(targeted.get("repair_case_ids") == REPAIR_CASE_IDS
            and targeted.get("regression_control_case_ids") ==
            CONTROL_CASE_IDS
            and targeted.get("repair_case_count") == 20
            and targeted.get("regression_control_count") == 8
            and targeted.get("minimum_case_count") == 28,
            "visual repair gate targeted28 inventory")
    require(ta_gate.get("axes_per_image") == CANONICAL_AXES
            and ta_gate.get("image_count") == 28
            and ta_gate.get("axis_review_count") == 280,
            "visual repair gate canonical 280-axis contract")
    failed_by_axis = gate.get("bound_v2_failure_set", {}).get(
        "failed_axis_case_ids", {})
    derived: dict[str, list[str]] = {case_id: [] for case_id in REPAIR_CASE_IDS}
    for axis in CANONICAL_AXES:
        for case_id in failed_by_axis.get(axis, []):
            require(case_id in derived,
                    "gate prior failure outside repair inventory")
            derived[case_id].append(axis)
    require(derived == REPAIR_BLOCKING_AXES
            and sum(len(axes) for axes in derived.values()) == 32,
            "gate exact 20-case/32-axis repair mapping")
    boundary = gate.get("evidence_boundary", {})
    require(boundary.get("gpu_execution_authorized") is False
            and boundary.get("gpu_capture_performed_by_this_gate") is False
            and boundary.get("product_exposure_authorized") is False
            and gate.get("product_exposed") is False
            and gate.get("beta_pass") is False,
            "visual repair gate exposure boundary")

    require(failure.get("schema") == EXPECTED_FAILURE_SCHEMA
            and failure.get("status") == EXPECTED_FAILURE_STATUS
            and failure.get("counts") == {
                "planned_cases": 28,
                "completed_cases": 0,
                "failed_capture_cases": 0,
                "unrun_cases": 28,
            }, "preordinal failure identity/counts")
    failure_detail = failure.get("failure", {})
    output = failure.get("output_audit", {})
    failure_boundary = failure.get("evidence_boundary", {})
    require(failure_detail.get("failed_before_capture_ordinal") == 0
            and failure_detail.get("output_created") is False
            and failure_detail.get("opengl_context_created") is False
            and output.get("exists") is False
            and output.get("ppm_files") == 0
            and output.get("png_files") == 0
            and failure.get("frozen_bindings", {}).get(
                "visual_repair_design_gate_sha256") ==
            VISUAL_REPAIR_GATE_SHA256
            and failure_boundary.get("targeted_machine_pass") is False
            and failure_boundary.get("rerun_authorized") is False
            and failure_boundary.get("product_exposed") is False,
            "preordinal failure must remain zero-capture/non-authorizing")
    return gate, failure


def validate_common_structure(review: dict[str, Any]) -> None:
    require(review.get("schema") ==
            "phase5_rules_decision_ui_v3_targeted28_"
            "independent_ta_review_v1"
            and review.get("template_version") == 1,
            "review schema/version")
    require(review.get("review_scope") == {
        "kind": "phase5_rules_ui_v3_targeted28_independent_ta",
        "case_ids_in_order": CASE_IDS,
        "repair_case_ids_in_order": REPAIR_CASE_IDS,
        "regression_control_case_ids_in_order": CONTROL_CASE_IDS,
        "case_count": 28,
        "repair_case_count": 20,
        "regression_control_case_count": 8,
        "canonical_axes_in_order": CANONICAL_AXES,
        "axis_count_per_case": 10,
        "total_axis_count": 280,
        "prior_failed_axis_occurrence_count": 32,
        "original_physical_resolution_required": True,
    }, "review exact targeted28/280-axis scope")
    require(review.get("allowed_statuses") == {
        "template_case": ["NOT_RUN"],
        "template_axis": ["PENDING"],
        "completed_case": ["PASS", "FAIL"],
        "completed_axis": ["PASS", "FAIL"],
    }, "allowed status vocabulary")
    preconditions = review.get("completion_preconditions", [])
    require(isinstance(preconditions, list) and len(preconditions) == 8
            and any("28/28 machine PASS" in item for item in preconditions)
            and any("PPM and PNG" in item for item in preconditions)
            and any("stable aggregate" in item for item in preconditions)
            and any("20 repair" in item for item in preconditions)
            and any("eight controls" in item for item in preconditions)
            and any("all 280 axes" in item for item in preconditions)
            and any("Partial or mixed" in item for item in preconditions),
            "completion preconditions")
    cases = review.get("cases", [])
    require(isinstance(cases, list) and len(cases) == 28
            and [case.get("case_id") for case in cases] == CASE_IDS,
            "exact targeted28 case order")
    for ordinal, case in enumerate(cases, start=1):
        case_id = case["case_id"]
        profile = profile_from_case_id(case_id)
        is_repair = case_id in REPAIR_BLOCKING_AXES
        require(case.get("ordinal") == ordinal
                and case.get("coverage_kind") ==
                ("REPAIR" if is_repair else "REGRESSION_CONTROL")
                and case.get("profile_id") == profile
                and case.get("expected_physical_dimensions") ==
                PROFILE_PHYSICAL_DIMENSIONS[profile]
                and case.get("prior_blocking_axes") ==
                (REPAIR_BLOCKING_AXES[case_id] if is_repair else [])
                and case.get("required_regression_axes") ==
                ([] if is_repair else CONTROL_REGRESSION_AXES[case_id]),
                f"{case_id}: coverage/profile/repair-regression contract")
        findings = case.get("axis_findings", {})
        require(isinstance(findings, dict)
                and list(findings) == CANONICAL_AXES,
                f"{case_id}: canonical ten-axis order")
        for axis in (case.get("prior_blocking_axes", []) +
                     case.get("required_regression_axes", [])):
            require(axis in findings,
                    f"{case_id}: required repair/regression axis absent")


def validate_template_mode(review: dict[str, Any]) -> dict[str, Any]:
    require(review == build_template(),
            "NOT_RUN template must be byte-semantically pristine")
    return {
        "status": "PASS_TARGETED28_TA_TEMPLATE_NOT_RUN",
        "mode": "TEMPLATE",
        "case_count": 28,
        "repair_case_count": 20,
        "regression_control_case_count": 8,
        "axis_count": 280,
        "cases_not_run": 28,
        "axes_pending": 280,
        "machine_receipt_bound": False,
        "ppm_bound_count": 0,
        "png_bound_count": 0,
        "stable_aggregate_bound": False,
        "human_ta_review_performed": False,
        "targeted28_ta_pass": False,
    }


def aggregate_payloads(machine_cases: list[dict[str, Any]]) -> tuple[list[dict[str, Any]], list[dict[str, Any]], list[dict[str, Any]]]:
    ppm_entries: list[dict[str, Any]] = []
    png_entries: list[dict[str, Any]] = []
    stable_entries: list[dict[str, Any]] = []
    for item in machine_cases:
        common = {"ordinal": item["ordinal"], "case_id": item["case_id"]}
        ppm = item["ppm"]
        png = item["png"]
        ppm_entry = {**common, **ppm}
        png_entry = {**common, **png}
        ppm_entries.append(ppm_entry)
        png_entries.append(png_entry)
        stable_entries.append({**common, "ppm": ppm, "png": png})
    return ppm_entries, png_entries, stable_entries


def validate_machine_and_images(review: dict[str, Any]) -> dict[str, Any]:
    binding = review.get("future_machine_pass_binding", {})
    require(binding.get("status") == "BOUND_TARGETED28_MACHINE_PASS"
            and binding.get("required_schema") == EXPECTED_MACHINE_SCHEMA
            and binding.get("required_status") == EXPECTED_MACHINE_STATUS
            and binding.get("required_case_count") == 28
            and binding.get("required_machine_pass_case_count") == 28
            and binding.get("required_failed_machine_check_count") == 0
            and binding.get("required_ppm_count") == 28
            and binding.get("required_png_count") == 28
            and binding.get("required_aggregate_algorithm") ==
            STABLE_AGGREGATE_ALGORITHM,
            "completed review machine binding contract")
    receipt_path = bound_file(binding.get("receipt_path"),
                              binding.get("receipt_sha256"),
                              "targeted28 machine receipt")
    receipt = load_json(receipt_path)
    require(receipt.get("schema") == EXPECTED_MACHINE_SCHEMA
            and receipt.get("status") == EXPECTED_MACHINE_STATUS
            and receipt.get("visual_repair_design_gate_sha256") ==
            VISUAL_REPAIR_GATE_SHA256
            and receipt.get("targeted_preordinal_failure_receipt_sha256") ==
            FAILURE_RECEIPT_SHA256
            and receipt.get("counts") == {
                "planned_cases": 28,
                "actual_gpu_captures": 28,
                "machine_passed_cases": 28,
                "failed_machine_checks": 0,
                "ppm_files": 28,
                "png_files": 28,
            }
            and receipt.get("case_ids_in_order") == CASE_IDS,
            "targeted28 machine receipt identity/counts/order")
    machine_cases = receipt.get("cases_in_order", [])
    require(isinstance(machine_cases, list) and len(machine_cases) == 28
            and [item.get("case_id") for item in machine_cases] == CASE_IDS,
            "machine receipt exact cases_in_order")
    review_cases = {case["case_id"]: case for case in review["cases"]}
    for ordinal, machine in enumerate(machine_cases, start=1):
        case_id = CASE_IDS[ordinal - 1]
        expected_dimensions = PROFILE_PHYSICAL_DIMENSIONS[
            profile_from_case_id(case_id)]
        require(machine.get("ordinal") == ordinal
                and machine.get("machine_result") == "PASS",
                f"{case_id}: machine PASS/ordinal")
        ppm = machine.get("ppm", {})
        png = machine.get("png", {})
        require(ppm.get("physical_dimensions") == expected_dimensions
                and png.get("physical_dimensions") == expected_dimensions,
                f"{case_id}: receipt physical dimensions")
        ppm_path = bound_file(ppm.get("path"), ppm.get("sha256"),
                              f"{case_id} PPM")
        png_path = bound_file(png.get("path"), png.get("sha256"),
                              f"{case_id} PNG")
        require(read_ppm_dimensions(ppm_path) == expected_dimensions,
                f"{case_id}: decoded PPM physical dimensions")
        require(read_png_dimensions(png_path) == expected_dimensions,
                f"{case_id}: decoded PNG physical dimensions")
        require(review_cases[case_id].get("future_evidence") == {
            "ppm": ppm,
            "png": png,
        }, f"{case_id}: review evidence must equal machine receipt")
    ppm_entries, png_entries, stable_entries = aggregate_payloads(machine_cases)
    expected_aggregates = {
        "algorithm": STABLE_AGGREGATE_ALGORITHM,
        "ppm_sha256": canonical_hash(ppm_entries),
        "png_sha256": canonical_hash(png_entries),
        "stable_sha256": canonical_hash(stable_entries),
    }
    require(receipt.get("aggregates") == expected_aggregates,
            "machine receipt aggregate hashes")
    require(binding.get("ppm_aggregate_sha256") ==
            expected_aggregates["ppm_sha256"]
            and binding.get("png_aggregate_sha256") ==
            expected_aggregates["png_sha256"]
            and binding.get("stable_aggregate_sha256") ==
            expected_aggregates["stable_sha256"],
            "review aggregate bindings must equal recomputed receipt")
    return receipt


def validate_completed_mode(review: dict[str, Any]) -> dict[str, Any]:
    receipt = validate_machine_and_images(review)
    overall = review.get("overall_result")
    require(overall in ("PASS", "FAIL")
            and review.get("status") ==
            ("TARGETED28_TA_REVIEW_PASS" if overall == "PASS"
             else "TARGETED28_TA_REVIEW_FAIL"),
            "completed review status/result")
    case_pass = case_fail = axis_pass = axis_fail = 0
    prior_pass = prior_fail = 0
    failed_cases: list[str] = []
    blockers: list[str] = []
    for case in review["cases"]:
        case_id = case["case_id"]
        result = case.get("manual_case_result")
        require(result in ("PASS", "FAIL"),
                f"{case_id}: completed case PASS/FAIL only")
        failed_axes: list[str] = []
        for axis, finding in case["axis_findings"].items():
            status = finding.get("status")
            require(status in ("PASS", "FAIL"),
                    f"{case_id}/{axis}: completed axis PASS/FAIL only")
            require(isinstance(finding.get("observation"), str)
                    and len(finding["observation"].strip()) >= 20,
                    f"{case_id}/{axis}: substantive observation required")
            if status == "PASS":
                axis_pass += 1
            else:
                axis_fail += 1
                failed_axes.append(axis)
            if axis in case["prior_blocking_axes"]:
                if status == "PASS":
                    prior_pass += 1
                else:
                    prior_fail += 1
        require(case.get("blocking_axes") == failed_axes
                and (result == "FAIL") == bool(failed_axes),
                f"{case_id}: exact blocker/case consistency")
        if result == "PASS":
            case_pass += 1
        else:
            case_fail += 1
            failed_cases.append(case_id)
            blockers.extend(f"{case_id}:{axis}" for axis in failed_axes)
    require(prior_pass + prior_fail == 32,
            "all 32 prior blocking axes must be reviewed")
    require((overall == "PASS") ==
            (case_fail == 0 and axis_fail == 0 and prior_fail == 0),
            "completed overall/case/axis/prior-repair consistency")
    require(review.get("review_counts") == {
        "cases_total": 28,
        "repair_cases_total": 20,
        "regression_control_cases_total": 8,
        "cases_not_run": 0,
        "cases_pass": case_pass,
        "cases_fail": case_fail,
        "axes_total": 280,
        "axes_pending": 0,
        "axes_pass": axis_pass,
        "axes_fail": axis_fail,
        "prior_blocking_axis_occurrences_total": 32,
        "prior_blocking_axis_occurrences_pass": prior_pass,
        "prior_blocking_axis_occurrences_fail": prior_fail,
    } and review.get("failed_case_ids") == failed_cases
       and review.get("blocking_axis_occurrences") == blockers,
       "completed review counts/failure inventory")
    boundary = review.get("evidence_boundary", {})
    require(boundary == {
        "template_only": False,
        "machine_receipt_bound": True,
        "capture_ppm_bound_count": 28,
        "capture_png_bound_count": 28,
        "stable_aggregate_bound": True,
        "human_ta_review_performed": True,
        "targeted28_ta_pass": overall == "PASS",
        "scoped_visual_repair_gate_pass": False,
        "authorization_prepared_by_template_work": False,
        "authorization_issued_by_template_work": False,
        "gpu_executed_by_template_work": False,
        "production_modified": False,
        "m2_promotion": False,
        "golden_promotion": False,
        "product_exposed": False,
        "product_pass": False,
        "beta_pass": False,
        "true_keyboard_or_pointer_session_pass": False,
        "screen_reader_session_pass": False,
    }, "completed review evidence/exposure boundary")
    return {
        "status": "PASS_COMPLETED_TARGETED28_TA_REVIEW_STRUCTURE",
        "mode": "COMPLETED",
        "overall_result": overall,
        "case_count": 28,
        "axis_count": 280,
        "prior_blocking_axes_reviewed": 32,
        "machine_receipt_sha256":
            review["future_machine_pass_binding"]["receipt_sha256"],
        "machine_pass_case_count":
            receipt["counts"]["machine_passed_cases"],
        "ppm_bound_count": 28,
        "png_bound_count": 28,
        "stable_aggregate_bound": True,
        "targeted28_ta_pass": overall == "PASS",
        "scoped_visual_repair_gate_pass": False,
    }


def validate(path: Path = DEFAULT_REVIEW) -> dict[str, Any]:
    path = path.resolve(strict=True)
    review = load_json(path)
    validate_frozen_bindings(review)
    validate_common_structure(review)
    if review.get("status") == (
            "TEMPLATE_NOT_RUN_WAITING_FOR_TARGETED28_MACHINE_PASS"):
        result = validate_template_mode(review)
    else:
        result = validate_completed_mode(review)
    result["review_sha256"] = sha256(path)
    result["visual_repair_gate_sha256"] = VISUAL_REPAIR_GATE_SHA256
    result["preordinal_failure_receipt_sha256"] = FAILURE_RECEIPT_SHA256
    result["m2_promotion"] = False
    result["golden_promotion"] = False
    result["product_exposed"] = False
    result["product_pass"] = False
    result["beta_pass"] = False
    result["true_keyboard_or_pointer_session_pass"] = False
    result["screen_reader_session_pass"] = False
    result["gpu_executed"] = False
    result["production_modified"] = False
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--review", type=Path, default=DEFAULT_REVIEW)
    arguments = parser.parse_args()
    try:
        print(json.dumps(validate(arguments.review), indent=2, sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
