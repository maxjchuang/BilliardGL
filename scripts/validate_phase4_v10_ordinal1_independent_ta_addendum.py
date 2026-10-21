#!/usr/bin/env python3
"""Validate the Phase4 v10 ordinal1 independent TA failure addendum."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ADDENDUM = ROOT / (
    "docs/art/phase4-v10-ordinal1-independent-ta-"
    "failure-acceptance-addendum-v1.json"
)
BINDINGS = [
    ("docs/art/phase4-training-replay-overlay-v4-final-ownership-"
     "targeted11-output-v10/camera_player_low_1080.ppm",
     "82d6f9e5aba1821b3b27c00f2b76977fff035cf8541c768bd5361c1ee4ee744a"),
    ("docs/art/phase4-training-replay-overlay-v4-final-ownership-"
     "targeted11-output-v10/results.jsonl.final-ownership.jsonl",
     "d8e06f0aa0e77d0e677af72ce2250a563d2595b70b790b05ad5728c9cc59c94d"),
    ("docs/art/phase4-training-replay-overlay-v4-final-ownership-"
     "targeted11-output-v10/phase4-final-ownership-dispatch-"
     "failure-receipt-v1.json",
     "5b5923a8efbce6f293af4d2fd71ce2d4c016d2faaabc5dbd4d19600ea10772ae"),
    ("docs/art/phase4-training-replay-overlay-v4-final-ownership-"
     "targeted11-ready-v10/targeted11-readiness-v10.json",
     "878d69d0833b1a8becd6322fbb29e3cdb20f57723a8d1d3ff43c985c10750f37"),
    ("docs/art/phase4-final-ownership-v9-ordinal1-offline-diagnostic-v1/"
     "minimal-compositing-style-correction-design-gate-v1.json",
     "e80baf05f0240872ba24b9ca9a6c7daa0d5a51496dde99902014d8ea3391a4f6"),
    ("docs/art/phase4-final-ownership-v9-ordinal1-offline-diagnostic-v1/"
     "failure-evidence-receipt-v1.json",
     "6bfcaf2c5ac5602deb557ccca176ebfd478c4183cf9bad6a054b6642f13a39eb"),
    ("docs/art/phase4-training-replay-overlay-v4-final-ownership-"
     "targeted11-output-v9/camera_player_low_1080.ppm",
     "90db92505afaf6d6c2d74193dee646afb73515a42acb3d25fa32b8034d84686c"),
    ("docs/art/phase4-training-replay-overlay-v4-final-ownership-"
     "targeted11-output-v9/results.jsonl.final-ownership.jsonl",
     "3f40581fc4f66861a3a5f0953d6350619ca3195ad0c560f1202c5458dd66f62a"),
    ("docs/art/phase4-training-replay-overlay-v4-final-ownership-"
     "targeted11-output-v8/camera_player_low_1080.ppm",
     "f3626f44ead6cfe4729e6ff3b76168747154b2452d0ca2afd70e1520b0e09744"),
    ("docs/art/phase4-training-replay-overlay-v4-final-ownership-"
     "targeted11-output-v8/results.jsonl.final-ownership.jsonl",
     "48d34fb11a342ffb7ff00f0729396ad48544bcfec4db43470bedd3d92148bd99"),
]
AXES = [
    "predicted_track_visual_identity",
    "actual_track_visual_identity",
    "core_inner_halo_separation",
    "outer_halo_background_silhouette",
    "predicted_dash_gap_rhythm",
    "endpoint_tick_terminal_readability",
    "coaxial_predicted_actual_disambiguation",
]
RESULTS = ["PASS", "PASS", "PASS", "FAIL", "PASS", "PASS", "PASS"]
EXPECTED_ROI = {
    8: {"white_pixels": 838, "triple_color_rows": 106,
        "yellow_dominant_rows": 414},
    9: {"white_pixels": 2317, "triple_color_rows": 430,
        "yellow_dominant_rows": 326},
    10: {"white_pixels": 1132, "triple_color_rows": 249,
         "yellow_dominant_rows": 378},
}
EXPECTED_MACHINE = {
    8: ("PASS", 92, 19, 73),
    9: ("FAILED", 66, 14, 52),
    10: ("FAILED", 89, 19, 70),
}


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


def telemetry(version: int) -> dict[str, Any]:
    path = ROOT / (
        "docs/art/phase4-training-replay-overlay-v4-final-ownership-"
        f"targeted11-output-v{version}/results.jsonl.final-ownership.jsonl")
    rows = [json.loads(line) for line in
            path.read_text(encoding="utf-8").splitlines() if line]
    ordinal_one = [row for row in rows
                   if row.get("plan_ordinal_one_based") == 1
                   and row.get("capture_id") == "camera_player_low_1080"]
    require(len(ordinal_one) == 1,
            f"v{version} exact ordinal1 telemetry row")
    return ordinal_one[0]


def read_ppm(path: Path) -> tuple[int, int, bytes]:
    raw = path.read_bytes()
    index = 0
    tokens: list[bytes] = []
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
    require(tokens[0] == b"P6" and tokens[3] == b"255",
            "PPM must be binary RGB max255")
    index += 1
    width, height = int(tokens[1]), int(tokens[2])
    pixels = raw[index:]
    require(len(pixels) == width * height * 3, "PPM byte count")
    return width, height, pixels


def roi_metrics(path: Path) -> dict[str, int]:
    width, height, pixels = read_ppm(path)
    require((width, height) == (1920, 1080), "ordinal1 PPM 1920x1080")
    yellow = (255, 253, 112)
    white = (255, 255, 255)
    black = (0, 0, 0)
    white_pixels = 0
    triple_rows = 0
    yellow_dominant_rows = 0
    for y in range(406, 950):
        counts = {yellow: 0, white: 0, black: 0}
        for x in range(945, 976):
            offset = (y * width + x) * 3
            color = tuple(pixels[offset:offset + 3])
            if color in counts:
                counts[color] += 1
        white_pixels += counts[white]
        if counts[yellow] and counts[white] and counts[black]:
            triple_rows += 1
        if counts[yellow] > counts[white]:
            yellow_dominant_rows += 1
    return {"white_pixels": white_pixels,
            "triple_color_rows": triple_rows,
            "yellow_dominant_rows": yellow_dominant_rows}


def validate_bindings(addendum: dict[str, Any]) -> None:
    rows = addendum.get("read_only_bindings", [])
    require([(row.get("path"), row.get("sha256")) for row in rows] == BINDINGS,
            "addendum exact read-only bindings/order")
    for path, digest in BINDINGS:
        candidate = ROOT / path
        require(candidate.is_file() and sha256(candidate) == digest,
                "binding missing/drift: " + path)
    for index in (0, 6, 8):
        require(rows[index].get("physical_dimensions") == [1920, 1080],
                "bound PPM dimensions")
        width, height, _ = read_ppm(ROOT / rows[index]["path"])
        require([width, height] == rows[index]["physical_dimensions"],
                "decoded PPM dimensions")
    gate = load_json(BINDINGS[4][0])
    require(gate.get("status") ==
            "DESIGN_ONLY_NO_IMPLEMENTATION_NO_CAPTURE_AUTHORIZATION"
            and gate.get("immutable_invariants", {}).get(
                "minimum_edge_contrast_ratio") == 4.0
            and gate.get("capture_authorized") is False
            and gate.get("technical_art_review_authorized") is False,
            "bound v9 compositing gate threshold/no-auth boundary")


def validate_authoritative_v10(addendum: dict[str, Any]) -> None:
    row = telemetry(10)
    require(row.get("machine_status") == "FAILED"
            and row.get("machine_error_code") ==
                "phase4_overlay_machine_gate_failed"
            and row.get("source_probe_count") == 95
            and row.get("visible_probe_count") == 89
            and row.get("hidden_probe_count") == 6
            and row.get("component_hidden_reason_counts") == {
                "outer_halo_edge|final_edge_contrast_below_4": 6}
            and row.get("actual_source_visible_hidden_and_coverage") == {
                "source": 19, "visible": 19, "hidden": 0, "coverage": 1}
            and row.get("predicted_source_visible_hidden_and_coverage") == {
                "source": 76, "visible": 70, "hidden": 6,
                "coverage": 0.9210526315789473}
            and row.get("foreign_track_evidence_count") == 0
            and row.get("double_assignment_count") == 0
            and row.get("unowned_evidence_count") == 0,
            "authoritative v10 failed telemetry facts")
    frozen = addendum.get("authoritative_v10_result", {})
    require(frozen.get("machine_status") == "FAILED"
            and frozen.get("visible_probe_count") == 89
            and frozen.get("hidden_probe_count") == 6
            and frozen.get("component_hidden_reason_counts") == {
                "outer_halo_edge|final_edge_contrast_below_4": 6}
            and frozen.get("actual_source_visible_hidden_coverage") == {
                "source": 19, "visible": 19, "hidden": 0, "coverage": 1.0}
            and frozen.get("predicted_source_visible_hidden_coverage") == {
                "source": 76, "visible": 70, "hidden": 6,
                "coverage": 0.9210526315789473}
            and frozen.get("foreign_track_evidence_count") == 0
            and frozen.get("double_assignment_count") == 0
            and frozen.get("unowned_evidence_count") == 0
            and frozen.get("dispatch_status") ==
                "CAPTURE_FAILED_TELEMETRY_PRESERVED"
            and frozen.get("dispatch_completed_count") == 0
            and frozen.get("dispatch_failed_count") == 1
            and frozen.get("dispatch_unrun_count") == 10
            and frozen.get("technical_art_review_authorized") is False,
            "addendum exact authoritative v10 failure")
    receipt = load_json(BINDINGS[2][0])
    require(receipt.get("status") == "CAPTURE_FAILED_TELEMETRY_PRESERVED"
            and receipt.get("telemetry_sha256") == BINDINGS[1][1]
            and receipt.get("completed_count") == 0
            and receipt.get("failed_count") == 1
            and receipt.get("unrun_count") == 10
            and receipt.get("technical_art_review_authorized") is False,
            "v10 dispatch failure receipt")
    failures = addendum.get("failed_local_contrast_evidence", [])
    require(len(failures) == 6
            and [row["surface"] for row in failures] == [
                "deep_dark", "deep_dark", "red_ball", "pale_ball",
                "near_white_cue_ball", "near_white_cue_ball"]
            and all(row.get("track") == "predicted"
                    and row.get("component") == "outer_halo_edge"
                    and row.get("contrast_ratio", 4.0) < 4.0
                    for row in failures),
            "six exact local-contrast blockers")


def validate_comparison(addendum: dict[str, Any]) -> None:
    comparison = addendum.get("v8_v9_v10_comparison", {})
    for version in (8, 9, 10):
        ppm_path = ROOT / BINDINGS[{8: 8, 9: 6, 10: 0}[version]][0]
        require(roi_metrics(ppm_path) == EXPECTED_ROI[version],
                f"v{version} frozen ROI metrics")
        row = telemetry(version)
        machine, visible, actual, predicted = EXPECTED_MACHINE[version]
        require(row.get("machine_status") == machine
                and row.get("visible_probe_count") == visible
                and row.get("actual_source_visible_hidden_and_coverage", {}).get(
                    "visible") == actual
                and row.get("predicted_source_visible_hidden_and_coverage", {}).get(
                    "visible") == predicted,
                f"v{version} authoritative comparison telemetry")
        frozen = comparison.get(f"v{version}", {})
        require(frozen.get("machine_status") == machine
                and frozen.get("visible_over_source") == [visible, 95]
                and frozen.get("white_pixels") ==
                    EXPECTED_ROI[version]["white_pixels"]
                and frozen.get("triple_color_rows") ==
                    EXPECTED_ROI[version]["triple_color_rows"]
                and frozen.get("yellow_dominant_rows") ==
                    EXPECTED_ROI[version]["yellow_dominant_rows"]
                and frozen.get("actual_visible_over_source") == [actual, 19]
                and frozen.get("predicted_visible_over_source") ==
                    [predicted, 76], f"v{version} addendum comparison values")
    interpretation = " ".join(comparison.get("interpretation", []))
    for term in ("continuous broad", "1185", "181", "52",
                 "Some three-color cross-sections remain",
                 "authoritatively FAILED"):
        require(term in interpretation, "comparison interpretation: " + term)


def validate_visual_decisions(addendum: dict[str, Any]) -> None:
    review = addendum.get("review_method", {})
    require(review.get("original_resolution_reviewed") is True
            and review.get("physical_dimensions") == [1920, 1080]
            and review.get("source_format") == "P6 RGB PPM"
            and review.get("resampling_used_for_decision") is False
            and review.get("comparison_roi_xyxy_inclusive") ==
                [945, 406, 975, 949], "original-resolution review method")
    axes = addendum.get("canonical_axis_decisions", [])
    require([row.get("axis_id") for row in axes] == AXES
            and [row.get("result") for row in axes] == RESULTS
            and all(row.get("finding") for row in axes),
            "seven canonical visual decisions with one blocker")
    focused = addendum.get("focused_visual_decisions", {})
    require(focused == {
        "adaptive_narrow_rim_removed_continuous_three_line_artifact": "PASS",
        "primary_track_color_dominance": "PASS",
        "local_contrast_and_readability": "FAIL",
        "cross_track_behavior": "PASS",
        "formal_ordinal1_ta_result": "FAIL",
    }, "focused partial repair/local contrast failure")
    acceptance = addendum.get("acceptance", {})
    require(acceptance.get("ordinal1_visual_style_acceptance") is False
            and acceptance.get("blocking_axis") ==
                "outer_halo_background_silhouette"
            and "six final_edge_contrast_below_4" in
                acceptance.get("blocking_reason", "")
            and acceptance.get("targeted11_capture_acceptance") is False
            and acceptance.get("targeted11_technical_art_acceptance") is False
            and "does not create readiness or authorization" in
                acceptance.get("reentry_requirement", ""),
            "fail-closed ordinal1/capture/TA acceptance")


def validate_boundary(addendum: dict[str, Any]) -> None:
    boundary = addendum.get("proof_boundary", {})
    require(boundary.get("informational_original_resolution_review_performed")
            is True and boundary.get("machine_pass_inferred") is False,
            "informational review/no inferred machine pass")
    for field in (
            "capture_acceptance", "technical_art_review_authorized",
            "technical_art_acceptance", "production_source_modified",
            "gpu_authorized_or_executed_by_this_addendum", "threshold_changed",
            "formal68_authorized", "product_exposed", "golden_promotion",
            "m3_promotion", "beta_pass"):
        require(boundary.get(field) is False,
                "proof boundary must remain false: " + field)


def validate(path: Path = DEFAULT_ADDENDUM) -> dict[str, Any]:
    addendum = load_json(path.resolve(strict=True))
    require(addendum.get("schema") ==
            "phase4_v10_ordinal1_independent_ta_"
            "failure_acceptance_addendum_v1"
            and addendum.get("schema_version") == 1
            and addendum.get("status") ==
            "ORDINAL1_VISUAL_STYLE_PARTIAL_REPAIR_LOCAL_CONTRAST_FAIL_"
            "CAPTURE_AND_TA_ACCEPTANCE_FALSE", "addendum identity/status")
    validate_bindings(addendum)
    validate_authoritative_v10(addendum)
    validate_comparison(addendum)
    validate_visual_decisions(addendum)
    validate_boundary(addendum)
    return {
        "status": "PASS_PHASE4_V10_ORDINAL1_TA_FAILURE_ADDENDUM",
        "original_resolution_reviewed": True,
        "visual_axes_pass": 6,
        "visual_axes_fail": 1,
        "machine_status": "FAILED",
        "hidden_outer_halo_probes": 6,
        "capture_acceptance": False,
        "technical_art_acceptance": False,
        "production_modified": False,
        "gpu_executed_by_addendum": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--addendum", type=Path, default=DEFAULT_ADDENDUM)
    arguments = parser.parse_args()
    print(json.dumps(validate(arguments.addendum), sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
