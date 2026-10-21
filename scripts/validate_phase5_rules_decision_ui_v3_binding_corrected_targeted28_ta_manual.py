#!/usr/bin/env python3
"""Fail-closed validation for the frozen Phase5 targeted28 TA manual."""

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

from scripts.generate_phase5_rules_decision_ui_v3_binding_corrected_targeted28_ta_manual import (  # noqa: E402
    ARMED_CASE_IDS,
    ARMED_FAIL_AXES,
    CANONICAL_AXES,
    CASE_EVIDENCE_AGGREGATE_SHA256,
    CASE_IDS,
    DEFAULT_OUTPUT,
    EVIDENCE_MANIFEST_PATH,
    EVIDENCE_MANIFEST_SHA256,
    EVIDENCE_ROOT,
    GATE_PATH,
    GATE_SHA256,
    MACHINE_RECEIPT_PATH,
    MACHINE_RECEIPT_SHA256,
    OUTPUT_AGGREGATE_SHA256,
    PREORDINAL_FAILURE_PATH,
    PREORDINAL_FAILURE_SHA256,
    PROFILE,
    RESULTS_JSONL_PATH,
    RESULTS_JSONL_SHA256,
    TEMPLATE_PATH,
    TEMPLATE_SHA256,
    build_manual,
)


DEFAULT_MANUAL = DEFAULT_OUTPUT


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
    require(width > 0 and height > 0 and len(pixels) == width * height * 3,
            f"{path}: invalid PPM pixel length")
    return [width, height]


def read_png_dimensions(path: Path) -> list[int]:
    with path.open("rb") as image:
        require(image.read(8) == b"\x89PNG\r\n\x1a\n",
                f"{path}: PNG signature")
        length = struct.unpack(">I", image.read(4))[0]
        require(image.read(4) == b"IHDR" and length == 13,
                f"{path}: PNG IHDR")
        payload = image.read(13)
        width, height = struct.unpack(">II", payload[:8])
    require(width > 0 and height > 0, f"{path}: PNG dimensions")
    return [width, height]


def expected_input_bindings() -> dict[str, Any]:
    return build_manual()["input_bindings"]


def validate_input_bindings(manual: dict[str, Any]) -> None:
    require(manual.get("input_bindings") == expected_input_bindings(),
            "manual exact input bindings/aggregates")
    for relative, digest, label in (
        (GATE_PATH, GATE_SHA256, "visual repair gate"),
        (TEMPLATE_PATH, TEMPLATE_SHA256, "TA coverage template"),
        (PREORDINAL_FAILURE_PATH, PREORDINAL_FAILURE_SHA256,
         "preordinal failure receipt"),
        (MACHINE_RECEIPT_PATH, MACHINE_RECEIPT_SHA256,
         "binding-corrected machine receipt"),
        (EVIDENCE_MANIFEST_PATH, EVIDENCE_MANIFEST_SHA256,
         "actual-GPU evidence manifest"),
        (RESULTS_JSONL_PATH, RESULTS_JSONL_SHA256,
         "actual-GPU results JSONL"),
    ):
        bound_file(relative, digest, label)


def validate_machine_receipt() -> dict[str, Any]:
    receipt = load_json(ROOT / MACHINE_RECEIPT_PATH)
    require(receipt.get("schema") ==
            "phase5_rules_decision_ui_v3_binding_corrected_"
            "targeted_machine_pass_receipt_v1"
            and receipt.get("status") ==
            "TARGETED_GPU_MACHINE_28_PASS_TA_REVIEW_PENDING",
            "machine receipt schema/status")
    require(receipt.get("frozen_runtime_bindings", {}).get(
        "visual_repair_design_gate_sha256") == GATE_SHA256,
        "machine receipt visual gate binding")
    require(receipt.get("lineage", {}).get(
        "evidence_manifest_sha256") == EVIDENCE_MANIFEST_SHA256
        and receipt.get("lineage", {}).get(
            "results_jsonl_sha256") == RESULTS_JSONL_SHA256,
            "machine receipt evidence lineage")
    require(receipt.get("verified_counts") == {
        "planned_cases": 28,
        "result_rows": 28,
        "evidence_cases": 28,
        "ppm_files": 28,
        "png_files": 28,
        "output_files_total": 58,
    }, "machine receipt exact 28/28/58 counts")
    require(receipt.get("decoded_physical_dimension_counts") == {
        "1280x720": 14, "1920x1080": 7, "2560x1440": 7},
        "machine receipt physical dimension profile counts")
    stable = receipt.get("stable_aggregates", {})
    require(stable.get("case_evidence_aggregate_sha256") ==
            CASE_EVIDENCE_AGGREGATE_SHA256
            and stable.get("output_aggregate_sha256") ==
            OUTPUT_AGGREGATE_SHA256,
            "machine receipt stable aggregates")
    require(receipt.get("claims") == {
        "targeted_machine_28_pass": True,
        "actual_gpu_completed": 28,
        "technical_art_review_completed": 0,
        "product_exposed": False,
        "golden_promotion": False,
        "m2_promotion": False,
    }, "machine receipt claims boundary")
    return receipt


def validate_evidence_files(manual: dict[str, Any]) -> dict[str, Any]:
    manifest = load_json(ROOT / EVIDENCE_MANIFEST_PATH)
    require(manifest.get("schema") ==
            "phase5_rules_decision_ui_v3_targeted_actual_gpu_evidence_v1"
            and manifest.get("status") ==
            "TARGETED_GPU_MACHINE_EVIDENCE_TA_REVIEW_PENDING"
            and manifest.get("case_count") == 28,
            "evidence manifest schema/status/count")
    evidence_cases = manifest.get("cases", [])
    require([case.get("capture_id") for case in evidence_cases] == CASE_IDS,
            "evidence exact targeted28 order")
    results = [json.loads(line) for line in
               (ROOT / RESULTS_JSONL_PATH).read_text(encoding="utf-8").splitlines()]
    require(len(results) == 28
            and [row.get("capture_id") for row in results] == CASE_IDS,
            "results JSONL exact targeted28 order")
    manual_by_id = {case["case_id"]: case for case in manual["cases"]}
    aggregate = hashlib.sha256()
    for evidence, result in zip(evidence_cases, results):
        case_id = evidence["capture_id"]
        profile_id = case_id.split("--")[1]
        expected_dimensions = PROFILE[profile_id]["physical_dimensions"]
        require(evidence.get("actual_gpu_capture") is True
                and evidence.get("canonical_sha256") ==
                evidence.get("gpu_canonical_sha256")
                and result.get("actual_gpu_capture") is True
                and result.get("canonical_sha256") ==
                evidence.get("canonical_sha256")
                and evidence.get("rules_ui_after_tone_map") is True,
                f"{case_id}: actual-GPU/canonical/render binding")
        ppm_relative = Path(evidence["ppm_path"]).resolve().relative_to(
            ROOT.resolve()).as_posix()
        png_relative = Path(evidence["png_path"]).resolve().relative_to(
            ROOT.resolve()).as_posix()
        ppm_path = bound_file(ppm_relative, evidence.get("ppm_sha256"),
                              case_id + " PPM")
        png_path = bound_file(png_relative, evidence.get("png_sha256"),
                              case_id + " PNG")
        dimensions = [evidence.get("png_physical_width"),
                      evidence.get("png_physical_height")]
        require(dimensions == expected_dimensions
                and read_ppm_dimensions(ppm_path) == expected_dimensions
                and read_png_dimensions(png_path) == expected_dimensions,
                f"{case_id}: decoded original physical dimensions")
        expected_manual_evidence = {
            "ppm": {"path": ppm_relative,
                    "sha256": evidence["ppm_sha256"],
                    "physical_dimensions": expected_dimensions},
            "png": {"path": png_relative,
                    "sha256": evidence["png_sha256"],
                    "physical_dimensions": expected_dimensions},
        }
        manual_case = manual_by_id[case_id]
        require({"ppm": manual_case.get("ppm"),
                 "png": manual_case.get("png")} == expected_manual_evidence,
                f"{case_id}: manual PPM/PNG bindings")
        aggregate.update(case_id.encode("utf-8"))
        aggregate.update(b"\0")
        aggregate.update(evidence["ppm_sha256"].encode("ascii"))
        aggregate.update(b"\0")
        aggregate.update(evidence["png_sha256"].encode("ascii"))
        aggregate.update(b"\0")
        aggregate.update(
            f"{dimensions[0]}x{dimensions[1]}\n".encode("ascii"))
    require(aggregate.hexdigest() == CASE_EVIDENCE_AGGREGATE_SHA256,
            "independently recomputed case evidence aggregate")

    output_root = ROOT / EVIDENCE_ROOT
    files = sorted(path for path in output_root.rglob("*") if path.is_file())
    require(len(files) == 58, "exact 58 output files required")
    output_aggregate = hashlib.sha256()
    for path in files:
        relative = path.relative_to(output_root).as_posix()
        output_aggregate.update(relative.encode("utf-8"))
        output_aggregate.update(b"\0")
        output_aggregate.update(sha256(path).encode("ascii"))
        output_aggregate.update(b"\n")
    require(output_aggregate.hexdigest() == OUTPUT_AGGREGATE_SHA256,
            "independently recomputed exact-58 output aggregate")
    return manifest


def validate_manual_conclusion(manual: dict[str, Any]) -> None:
    require(manual.get("schema") ==
            "phase5_rules_decision_ui_v3_binding_corrected_targeted28_"
            "independent_ta_manual_review_v1"
            and manual.get("schema_version") == 1
            and manual.get("status") ==
            "MANUAL_VISUAL_REVIEW_FAIL_BLOCKING"
            and manual.get("overall_result") == "FAIL",
            "manual schema/status/result")
    method = manual.get("review_method", {})
    require(method.get("all_images_opened_individually_at_original_resolution")
            is True and method.get("case_ids_in_order") == CASE_IDS
            and method.get("axes_in_order") == CANONICAL_AXES
            and method.get("case_count") == 28
            and method.get("axes_per_case") == 10
            and method.get("axis_review_count") == 280
            and method.get("partial_or_inherited_manual_pass_allowed") is False,
            "manual original-resolution 28x10 method")
    cases = manual.get("cases", [])
    require(len(cases) == 28
            and [case.get("case_id") for case in cases] == CASE_IDS,
            "manual exact case order")
    axis_pass = axis_fail = 0
    failed_cases: list[str] = []
    blockers: list[str] = []
    for ordinal, case in enumerate(cases, start=1):
        case_id = case["case_id"]
        expected_failed = ARMED_FAIL_AXES if case_id in ARMED_CASE_IDS else []
        require(case.get("ordinal") == ordinal
                and case.get("reviewed_original_resolution") is True
                and case.get("manual_case_result") ==
                ("FAIL" if expected_failed else "PASS")
                and case.get("blocking_axes") == expected_failed,
                f"{case_id}: exact manual case/blocker result")
        findings = case.get("axis_findings", {})
        require(list(findings) == CANONICAL_AXES,
                f"{case_id}: canonical axis order")
        for axis, finding in findings.items():
            expected_status = "FAIL" if axis in expected_failed else "PASS"
            require(finding.get("status") == expected_status
                    and finding.get("blocking") == (expected_status == "FAIL")
                    and isinstance(finding.get("observation"), str)
                    and len(finding["observation"].strip()) >= 40,
                    f"{case_id}/{axis}: exact status/blocking/observation")
            if expected_status == "PASS":
                axis_pass += 1
            else:
                axis_fail += 1
                blockers.append(f"{case_id}:{axis}")
        if expected_failed:
            failed_cases.append(case_id)
            shape_text = " ".join(
                findings[axis]["observation"] for axis in expected_failed)
            for token in ("bare checkmark", "bare right arrow",
                          "focus_chevron", "selected_check_diamond",
                          "confirm_double_frame_plus_enter_arrow"):
                require(token in shape_text,
                        f"{case_id}: missing exact shape finding {token}")
    require(axis_pass == 272 and axis_fail == 8
            and manual.get("review_counts") == {
                "cases_total": 28,
                "cases_reviewed": 28,
                "cases_pass": 24,
                "cases_fail": 4,
                "cases_pending": 0,
                "axes_total": 280,
                "axes_pass": 272,
                "axes_fail": 8,
                "axes_pending": 0,
            }
            and manual.get("failed_case_ids") == failed_cases
            and manual.get("blocking_axis_occurrences") == blockers,
            "manual exact 24/4 and 272/8 counts/inventory")
    defects = manual.get("blocking_defects", [])
    require(len(defects) == 1
            and defects[0].get("defect_id") ==
            "D06_ARMED_FOCUS_CONFIRM_EXACT_SHAPES_ABSENT"
            and defects[0].get("affected_case_ids") == ARMED_CASE_IDS
            and defects[0].get("affected_case_count") == 4
            and defects[0].get("failed_axes") == ARMED_FAIL_AXES
            and defects[0].get("severity") == "BLOCKING",
            "manual exact blocking defect")


def validate_boundary(manual: dict[str, Any]) -> None:
    require(manual.get("evidence_boundary") == {
        "actual_gpu_machine_input": "28_OF_28_PASS_INPUT_ONLY",
        "manual_original_resolution_cases_reviewed": "28_OF_28",
        "manual_axes_reviewed": "280_OF_280",
        "manual_visual_result": "FAIL",
        "machine_pass_inherited_as_manual_pass": False,
        "scoped_visual_repair_gate_pass": False,
        "product_exposed": False,
        "product_pass": False,
        "golden_promotion": False,
        "m2_promotion": False,
        "beta_pass": False,
        "required_real_keyboard_pointer_sessions": "NOT_RUN",
        "required_screen_reader_sessions": "NOT_RUN",
        "required_external_human_cvd_localization_qa": "NOT_RUN",
        "gpu_executed_by_this_review": False,
        "production_modified_by_this_review": False,
    }, "manual fail-closed evidence/exposure boundary")


def validate(path: Path = DEFAULT_MANUAL) -> dict[str, Any]:
    path = path.resolve(strict=True)
    manual = load_json(path)
    validate_input_bindings(manual)
    validate_machine_receipt()
    validate_evidence_files(manual)
    validate_manual_conclusion(manual)
    validate_boundary(manual)
    require(manual == build_manual(),
            "manual must exactly match deterministic reviewed conclusion")
    return {
        "status": "PASS_VALIDATED_MANUAL_VISUAL_FAIL_BLOCKING",
        "manual_result": "FAIL",
        "manual_sha256": sha256(path),
        "machine_receipt_sha256": MACHINE_RECEIPT_SHA256,
        "evidence_manifest_sha256": EVIDENCE_MANIFEST_SHA256,
        "case_evidence_aggregate_sha256":
            CASE_EVIDENCE_AGGREGATE_SHA256,
        "output_aggregate_sha256": OUTPUT_AGGREGATE_SHA256,
        "cases_reviewed": 28,
        "cases_pass": 24,
        "cases_fail": 4,
        "axes_reviewed": 280,
        "axes_pass": 272,
        "axes_fail": 8,
        "blocking_axes": ARMED_FAIL_AXES,
        "scoped_visual_repair_gate_pass": False,
        "product_exposed": False,
        "golden_promotion": False,
        "m2_promotion": False,
        "beta_pass": False,
        "real_input_sessions": "NOT_RUN",
        "screen_reader_sessions": "NOT_RUN",
        "gpu_executed_by_review": False,
        "production_modified_by_review": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manual", type=Path, default=DEFAULT_MANUAL)
    arguments = parser.parse_args()
    try:
        print(json.dumps(validate(arguments.manual), indent=2, sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
