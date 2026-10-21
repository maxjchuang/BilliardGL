#!/usr/bin/env python3
"""Validate the Phase4 v12 targeted5 independent TA template or completion."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_REVIEW = ROOT / "docs/art/phase4-v12-targeted5-independent-ta-review-template-v1.json"
BINDINGS = [
    ("docs/art/phase4-v12-broadcast-projected-trajectory-visual-repair-design-gate-v1.json", "0fe6d893cd8fa1130e93c975be8e297802749b6515eb4f4e65b7d9af2006b8d2"),
    ("docs/art/phase4-final-ownership-v11-ordinal4-offline-diagnostic-v1/minimal-raster-support-connectivity-correction-design-gate-v1.json", "59bbabf437e9195392b2a41f4e48eea0513afb222706115a07e9c0965cbad059"),
    ("docs/art/phase4-v11-ordinal4-independent-ta-failure-acceptance-addendum-v1.json", "64936ca990a0077323dcc2d64e9dbdf8bf9b400da72f7a11d450beb0a89b04c4"),
    ("docs/art/phase4-final-ownership-v11-ordinal4-offline-diagnostic-v1/failure-evidence-receipt-v1.json", "07f251d466b0dcad550b4951e8242403bcaaaeb97dd800cbde99cdf23c2f3f85"),
    ("docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v11/results.jsonl.final-ownership.jsonl", "4e7e884569eec1197ef11d39d256e2e43eef11c49ee7c84ff150b93c3c6d61ef"),
    ("docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v11/phase4-final-ownership-dispatch-failure-receipt-v1.json", "282056928154ce78ad79b8cee0062d460d03f76c9d130f1c9f268f0dfa009a39"),
]
CASE_IDS = [
    "camera_player_low_1080", "camera_elevated_1080",
    "camera_overhead_1080", "camera_broadcast_1080",
    "actual_retina_ui135_cvd",
]
AXES = [
    "predicted_track_visual_identity", "actual_track_visual_identity",
    "core_inner_halo_separation", "outer_halo_background_silhouette",
    "predicted_dash_gap_rhythm", "endpoint_tick_terminal_readability",
    "coaxial_predicted_actual_disambiguation",
]
TEMPLATE_FALSE_FIELDS = [
    "future_readiness_bound", "future_machine_receipt_bound",
    "machine_five_of_five_pass", "human_review_performed",
    "targeted5_ta_pass", "targeted11_pass", "formal68_pass", "m3_pass",
    "product_exposed", "golden_promotion", "beta_pass",
    "production_modified", "gpu_authorized_or_executed",
    "authorization_created_or_issued",
]
ALWAYS_FALSE_FIELDS = [
    "targeted11_pass", "formal68_pass", "m3_pass", "product_exposed",
    "golden_promotion", "beta_pass", "production_modified",
    "gpu_authorized_or_executed", "authorization_created_or_issued",
]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_json(path: Path | str) -> dict[str, Any]:
    candidate = Path(path)
    if not candidate.is_absolute():
        candidate = ROOT / candidate
    return json.loads(candidate.read_text(encoding="utf-8"))


def resolve(path: str) -> Path:
    candidate = Path(path)
    return candidate if candidate.is_absolute() else ROOT / candidate


def canonical_row_sha(row: dict[str, Any]) -> str:
    payload = json.dumps(row, sort_keys=True, separators=(",", ":"),
                         ensure_ascii=False).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def ppm_dimensions(path: Path) -> tuple[int, int]:
    raw = path.read_bytes()
    tokens: list[bytes] = []
    index = 0
    while len(tokens) < 4:
        while raw[index:index + 1].isspace():
            index += 1
        if raw[index:index + 1] == b"#":
            while raw[index:index + 1] != b"\n":
                index += 1
            continue
        end = index
        while not raw[end:end + 1].isspace():
            end += 1
        tokens.append(raw[index:end])
        index = end
    require(tokens[0] == b"P6" and tokens[3] == b"255", "P6 RGB PPM")
    width, height = int(tokens[1]), int(tokens[2])
    index += 1
    require(len(raw[index:]) == width * height * 3, "PPM byte count")
    return width, height


def validate_bindings(review: dict[str, Any]) -> None:
    rows = review.get("frozen_bindings", [])
    require([(row.get("path"), row.get("sha256")) for row in rows] == BINDINGS,
            "exact ordered frozen bindings")
    for path, digest in BINDINGS:
        candidate = ROOT / path
        require(candidate.is_file() and sha256(candidate) == digest,
                "binding missing/drift: " + path)
    visual_gate = load_json(BINDINGS[0][0])
    require(visual_gate.get("status") ==
            "DESIGN_ONLY_NOT_IMPLEMENTED_NO_GPU_NO_AUTHORIZATION"
            and visual_gate.get("future_targeted_capture_gate", {}).get(
                "minimum_case_count") == 5,
            "bound v12 visual gate")
    algorithm_gate = load_json(BINDINGS[1][0])
    require(algorithm_gate.get("status") ==
            "DESIGN_ONLY_NOT_IMPLEMENTED_NO_CAPTURE_AUTHORIZED",
            "bound algorithm gate")


def validate_inventory(review: dict[str, Any]) -> None:
    scope = review.get("review_scope", {})
    require(scope.get("case_ids_in_order") == CASE_IDS
            and scope.get("control_case_ids_in_order") ==
                [CASE_IDS[0], CASE_IDS[1], CASE_IDS[2], CASE_IDS[4]]
            and scope.get("repair_case_id") == CASE_IDS[3]
            and scope.get("case_count") == 5
            and scope.get("canonical_axes_in_order") == AXES
            and scope.get("axes_per_case") == 7
            and scope.get("total_axis_count") == 35
            and scope.get("original_physical_resolution_required") is True
            and scope.get("machine_five_of_five_required_before_human_review") is True,
            "exact targeted5 x seven-axis inventory")
    cases = review.get("cases", [])
    require(len(cases) == 5
            and [case.get("ordinal") for case in cases] == [1, 2, 3, 4, 5]
            and [case.get("case_id") for case in cases] == CASE_IDS,
            "exact ordered five cases")
    expected = [
        ("STANDARD_CONTROL", "player_low", "standard", [1920, 1080]),
        ("STANDARD_CONTROL", "elevated", "standard", [1920, 1080]),
        ("STANDARD_CONTROL", "overhead", "standard", [1920, 1080]),
        ("PRIMARY_REPAIR", "broadcast", "standard", [1920, 1080]),
        ("CVD_CONTROL", "broadcast", "color_vision_safe", [1280, 720]),
    ]
    for case, (kind, camera, palette, logical) in zip(cases, expected):
        require(case.get("coverage_kind") == kind
                and case.get("camera_preset") == camera
                and case.get("palette") == palette
                and case.get("logical_dimensions") == logical
                and case.get("expected_physical_dimensions") == [1920, 1080]
                and list(case.get("axis_findings", {}).keys()) == AXES,
                "exact case profile and seven ordered axes")
    broadcast_contract = "\n".join(cases[3].get("required_review_contract", []))
    for needle in ("[783,413]", "[892,491]", "[940,525]", "[855,469]",
                   "[879,486]", ">=4.0", "Predicted yellow", "Endpoint"):
        require(needle in broadcast_contract,
                "broadcast closure contract missing: " + needle)
    cvd_contract = "\n".join(cases[4].get("required_review_contract", []))
    require("without hue-only inference" in cvd_contract,
            "CVD non-color control contract")


def validate_null_future_evidence(review: dict[str, Any]) -> None:
    future = review.get("future_successor_evidence", {})
    for name in ("readiness", "machine_receipt"):
        item = future.get(name, {})
        require(item.get("status") == "NOT_AVAILABLE"
                and item.get("path") is None and item.get("sha256") is None,
                "future " + name + " remains null/NOT_AVAILABLE")
    for field in ("ppm_aggregate_sha256", "telemetry_aggregate_sha256",
                  "stable_aggregate_sha256"):
        require(future.get(field) is None, "future aggregate remains null: " + field)
    for case in review["cases"]:
        evidence = case.get("future_evidence", {})
        require(all(evidence.get(field) is None for field in (
            "ppm_path", "ppm_sha256", "ppm_physical_dimensions",
            "telemetry_path", "telemetry_file_sha256", "telemetry_row_sha256")),
            "per-case future evidence remains null")
        require(all(value is None for value in
                    case.get("future_review_metrics", {}).values()),
                "per-case future metrics remain null")


def validate_template_state(review: dict[str, Any]) -> dict[str, Any]:
    require(review.get("status") ==
            "TEMPLATE_NOT_RUN_WAITING_FOR_SUCCESSOR_EVIDENCE"
            and review.get("overall_result") == "NOT_RUN",
            "template status/overall result")
    validate_null_future_evidence(review)
    for case in review["cases"]:
        require(case.get("manual_case_result") == "NOT_RUN"
                and case.get("blocking_axes") == [], "template case NOT_RUN")
        for finding in case["axis_findings"].values():
            require(finding == {"status": "PENDING", "observation": None,
                                "physical_coordinates": []},
                    "template axis exact PENDING")
    summary = review.get("summary", {})
    require(summary == {
        "case_result_counts": {"PASS": 0, "FAIL": 0, "NOT_RUN": 5},
        "axis_result_counts": {"PASS": 0, "FAIL": 0, "PENDING": 35},
        "blocking_case_ids": [],
        "machine_five_of_five_pass": False,
        "human_review_performed": False,
        "targeted5_ta_pass": False,
    }, "template summary")
    boundary = review.get("proof_boundary", {})
    require(boundary.get("template_only") is True
            and boundary.get("future_ppm_count") == 0
            and boundary.get("future_telemetry_row_count") == 0,
            "template evidence boundary")
    for field in TEMPLATE_FALSE_FIELDS:
        require(boundary.get(field) is False,
                "template proof boundary must be false: " + field)
    return {
        "status": "PASS_PHASE4_V12_TARGETED5_TA_TEMPLATE_NOT_RUN",
        "case_count": 5,
        "axis_count": 35,
        "cases_not_run": 5,
        "axes_pending": 35,
        "future_readiness_bound": False,
        "future_machine_receipt_bound": False,
        "future_ppm_count": 0,
        "future_telemetry_row_count": 0,
        "targeted5_ta_pass": False,
        "targeted11_pass": False,
        "formal68_pass": False,
        "m3_pass": False,
        "product_exposed": False,
    }


def validate_bound_file(item: dict[str, Any], label: str) -> tuple[Path, dict[str, Any]]:
    require(item.get("status") == "BOUND"
            and isinstance(item.get("path"), str)
            and isinstance(item.get("sha256"), str), label + " bound metadata")
    path = resolve(item["path"])
    require(path.is_file() and sha256(path) == item["sha256"], label + " path/SHA")
    return path, load_json(path)


def validate_completed_evidence(review: dict[str, Any]) -> None:
    future = review["future_successor_evidence"]
    readiness_path, readiness = validate_bound_file(future["readiness"], "readiness")
    require(readiness.get("schema") == future["readiness"]["required_schema"]
            and readiness.get("status") == future["readiness"]["required_status"]
            and readiness.get("case_ids_in_order") == CASE_IDS,
            "future readiness schema/status/inventory")
    _, machine = validate_bound_file(future["machine_receipt"], "machine receipt")
    required = future["machine_receipt"]
    require(machine.get("schema") == required["required_schema"]
            and machine.get("status") == required["required_status"]
            and machine.get("case_count") == 5
            and machine.get("passed_count") == 5
            and machine.get("failed_count") == 0
            and machine.get("unrun_count") == 0
            and machine.get("case_ids_in_order") == CASE_IDS
            and machine.get("readiness_sha256") == sha256(readiness_path),
            "future machine exact 5/5 pass")
    ppm_rows = []
    telemetry_rows = []
    stable_rows = []
    for case in review["cases"]:
        evidence = case["future_evidence"]
        require(all(isinstance(evidence.get(field), str) for field in (
            "ppm_path", "ppm_sha256", "telemetry_path",
            "telemetry_file_sha256", "telemetry_row_sha256")),
            "completed per-case evidence strings")
        ppm = resolve(evidence["ppm_path"])
        telemetry = resolve(evidence["telemetry_path"])
        require(ppm.is_file() and sha256(ppm) == evidence["ppm_sha256"]
                and ppm_dimensions(ppm) == (1920, 1080)
                and evidence.get("ppm_physical_dimensions") == [1920, 1080],
                "completed PPM path/SHA/dimensions")
        require(telemetry.is_file()
                and sha256(telemetry) == evidence["telemetry_file_sha256"],
                "completed telemetry file path/SHA")
        rows = [json.loads(line) for line in
                telemetry.read_text(encoding="utf-8").splitlines() if line]
        matched = [row for row in rows if row.get("capture_id") == case["case_id"]]
        require(len(matched) == 1
                and canonical_row_sha(matched[0]) == evidence["telemetry_row_sha256"]
                and matched[0].get("machine_status") == "PASS",
                "completed exact PASS telemetry row")
        dims = "1920x1080"
        ppm_rows.append(case["case_id"] + "\0" + evidence["ppm_sha256"] +
                        "\0" + dims + "\n")
        telemetry_rows.append(case["case_id"] + "\0" +
                              evidence["telemetry_row_sha256"] + "\n")
        stable_rows.append(case["case_id"] + "\0" + evidence["ppm_sha256"] +
                           "\0" + evidence["telemetry_row_sha256"] +
                           "\0" + dims + "\n")
    require(future.get("ppm_aggregate_sha256") ==
            hashlib.sha256("".join(ppm_rows).encode()).hexdigest()
            and future.get("telemetry_aggregate_sha256") ==
            hashlib.sha256("".join(telemetry_rows).encode()).hexdigest()
            and future.get("stable_aggregate_sha256") ==
            hashlib.sha256("".join(stable_rows).encode()).hexdigest(),
            "completed exact ordered aggregates")


def validate_completed_metrics(review: dict[str, Any]) -> None:
    controls = review["cases"][:3]
    floors = [(95, 89), (83, 80), (87, 84)]
    for case, (source, minimum) in zip(controls, floors):
        metrics = case["future_review_metrics"]
        svh = metrics.get("source_visible_hidden")
        require(metrics.get("machine_status") == "PASS"
                and isinstance(svh, list) and len(svh) == 3
                and svh[0] == source and svh[1] >= minimum
                and metrics.get("actual_coverage") == 1.0
                and metrics.get("no_visual_regression") is True
                and all(metrics.get(field) == 0 for field in (
                    "foreign_track_evidence_count", "double_assignment_count",
                    "unowned_evidence_count")),
                "standard control completion metrics")
    broadcast = review["cases"][3]["future_review_metrics"]
    require(broadcast.get("machine_status") == "PASS"
            and broadcast.get("source_visible_hidden") == [81, 81, 0]
            and broadcast.get("predicted_visible_hidden") == [69, 69, 0]
            and broadcast.get("actual_visible_hidden") == [12, 12, 0]
            and all(broadcast.get(field) is True for field in (
                "actual_full_path_independently_traceable",
                "actual_visible_in_predicted_gap_783_413",
                "actual_visible_in_predicted_gap_892_491",
                "actual_visible_in_predicted_gap_940_525",
                "predicted_yellow_and_open_gaps_preserved",
                "endpoint_and_ticks_preserved"))
            and broadcast.get("silhouette_contrast_855_469", 0) >= 4.0
            and broadcast.get("silhouette_contrast_879_486", 0) >= 4.0
            and all(broadcast.get(field) == 0 for field in (
                "foreign_track_evidence_count", "double_assignment_count",
                "unowned_evidence_count")),
            "broadcast exact repair completion metrics")
    cvd = review["cases"][4]["future_review_metrics"]
    require(cvd.get("machine_status") == "PASS"
            and isinstance(cvd.get("source_visible_hidden"), list)
            and all(cvd.get(field) is True for field in (
                "standard_cvd_non_color_track_distinction",
                "actual_visible_in_predicted_gaps", "predicted_gaps_preserved"))
            and all(cvd.get(field) == 0 for field in (
                "foreign_track_evidence_count", "double_assignment_count",
                "unowned_evidence_count")),
            "CVD control completion metrics")


def validate_completed_state(review: dict[str, Any]) -> dict[str, Any]:
    require(review.get("status") == "COMPLETED_INDEPENDENT_TA_REVIEW"
            and review.get("overall_result") in ("PASS", "FAIL"),
            "completed review status/result")
    validate_completed_evidence(review)
    validate_completed_metrics(review)
    case_results = []
    axis_results = []
    blocking = []
    for case in review["cases"]:
        findings = case["axis_findings"]
        results = []
        for axis in AXES:
            finding = findings[axis]
            require(finding.get("status") in ("PASS", "FAIL")
                    and isinstance(finding.get("observation"), str)
                    and finding["observation"].strip()
                    and isinstance(finding.get("physical_coordinates"), list)
                    and len(finding["physical_coordinates"]) >= 1,
                    "completed axis result/observation/coordinates")
            results.append(finding["status"])
            axis_results.append(finding["status"])
        expected_case = "FAIL" if "FAIL" in results else "PASS"
        expected_blocking = [axis for axis, result in zip(AXES, results)
                             if result == "FAIL"]
        require(case.get("manual_case_result") == expected_case
                and case.get("blocking_axes") == expected_blocking,
                "case result/blocking axes consistent")
        case_results.append(expected_case)
        if expected_case == "FAIL":
            blocking.append(case["case_id"])
    expected_overall = "FAIL" if "FAIL" in case_results else "PASS"
    require(review.get("overall_result") == expected_overall,
            "overall result consistent")
    summary = review.get("summary", {})
    require(summary.get("case_result_counts") == {
                "PASS": case_results.count("PASS"),
                "FAIL": case_results.count("FAIL"), "NOT_RUN": 0}
            and summary.get("axis_result_counts") == {
                "PASS": axis_results.count("PASS"),
                "FAIL": axis_results.count("FAIL"), "PENDING": 0}
            and summary.get("blocking_case_ids") == blocking
            and summary.get("machine_five_of_five_pass") is True
            and summary.get("human_review_performed") is True
            and summary.get("targeted5_ta_pass") is
                (expected_overall == "PASS"),
            "completed summary")
    boundary = review.get("proof_boundary", {})
    require(boundary.get("template_only") is False
            and boundary.get("future_readiness_bound") is True
            and boundary.get("future_machine_receipt_bound") is True
            and boundary.get("future_ppm_count") == 5
            and boundary.get("future_telemetry_row_count") == 5
            and boundary.get("machine_five_of_five_pass") is True
            and boundary.get("human_review_performed") is True
            and boundary.get("targeted5_ta_pass") is
                (expected_overall == "PASS"),
            "completed evidence/review boundary")
    for field in ALWAYS_FALSE_FIELDS:
        require(boundary.get(field) is False,
                "completed proof boundary must remain false: " + field)
    return {
        "status": "PASS_PHASE4_V12_TARGETED5_TA_COMPLETED_RECORD",
        "case_count": 5,
        "axis_count": 35,
        "axis_pass": axis_results.count("PASS"),
        "axis_fail": axis_results.count("FAIL"),
        "overall_result": expected_overall,
        "targeted5_ta_pass": expected_overall == "PASS",
        "targeted11_pass": False,
        "formal68_pass": False,
        "m3_pass": False,
        "product_exposed": False,
    }


def validate(path: Path | str = DEFAULT_REVIEW) -> dict[str, Any]:
    review = load_json(path)
    require(review.get("schema") ==
            "billiardgl.phase4.v12.targeted5_independent_ta_review.v1"
            and review.get("schema_version") == 1,
            "review schema/version")
    validate_bindings(review)
    validate_inventory(review)
    if review.get("status") == "TEMPLATE_NOT_RUN_WAITING_FOR_SUCCESSOR_EVIDENCE":
        return validate_template_state(review)
    return validate_completed_state(review)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("path", nargs="?", default=str(DEFAULT_REVIEW))
    args = parser.parse_args()
    print(json.dumps(validate(args.path), sort_keys=True))


if __name__ == "__main__":
    main()
