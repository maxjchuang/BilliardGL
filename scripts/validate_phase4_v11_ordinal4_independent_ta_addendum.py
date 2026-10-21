#!/usr/bin/env python3
"""Validate the frozen Phase4 v11 ordinal4 independent TA addendum."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ADDENDUM = ROOT / (
    "docs/art/phase4-v11-ordinal4-independent-ta-"
    "failure-acceptance-addendum-v1.json"
)
BINDINGS = [
    ("docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v11/targeted11-readiness-v11.json", "0e2e112f889aa5fc6fd5713a2d454945b1fe69678f9fd34bb873338ce3bbbc06"),
    ("docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v11/root-one-shot-gpu-authorization-v11-run1.json", "bc8e0ec237db21dec3ae71c150f92a1f29d8c7953872a273cecbdd86aa08e8d7"),
    ("docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v11/root-one-shot-gpu-authorization-v11-run1.json.consumed", "04a15b76a677f60e1721b19cf0564338f87c9147c8976de12905dddce9d052d0"),
    ("docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v11/phase4-final-ownership-dispatch-failure-receipt-v1.json", "282056928154ce78ad79b8cee0062d460d03f76c9d130f1c9f268f0dfa009a39"),
    ("docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v11/results.jsonl", "0fd67fe71d6115e7fe7bebe874ec7f4280a6a48dacd4432e2a15e1fe1ec57e69"),
    ("docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v11/results.jsonl.final-ownership.jsonl", "4e7e884569eec1197ef11d39d256e2e43eef11c49ee7c84ff150b93c3c6d61ef"),
    ("docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v11/camera_player_low_1080.ppm", "82d6f9e5aba1821b3b27c00f2b76977fff035cf8541c768bd5361c1ee4ee744a"),
    ("docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v11/camera_elevated_1080.ppm", "e6eca69d865f87f8b0d49960567520d81dcd2b5c8f83d217e0cde8c919e2fa46"),
    ("docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v11/camera_overhead_1080.ppm", "a03a84d5de933106980406d9ddcfdeb2cc2e8a36984eab39db1cfaff79fbba8e"),
    ("docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v11/camera_broadcast_1080.ppm", "0b6c87cb415a2e80ad1f6e061a972be57c683b8104e28d0065ba52b6a0045878"),
]
CAPTURE_IDS = [
    "camera_player_low_1080",
    "camera_elevated_1080",
    "camera_overhead_1080",
    "camera_broadcast_1080",
]
MACHINE_STATUSES = ["PASS", "PASS", "PASS", "FAILED"]
SOURCE_VISIBLE_HIDDEN = [[95, 89, 6], [83, 80, 3], [87, 84, 3], [81, 68, 13]]
ACTUAL = [[19, 19, 0, 1.0], [12, 12, 0, 1.0], [15, 15, 0, 1.0], [12, 6, 6, 0.5]]
PREDICTED = [
    [76, 70, 6, 0.9210526315789473],
    [71, 68, 3, 0.9577464788732394],
    [72, 69, 3, 0.9583333333333334],
    [69, 62, 7, 0.8985507246376812],
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
AXIS_RESULTS = ["PASS", "FAIL", "FAIL", "FAIL", "FAIL", "PASS", "FAIL"]
FALSE_BOUNDARY_FIELDS = [
    "machine_pass_inferred", "targeted11_machine_pass", "capture_acceptance",
    "technical_art_review_authorized", "technical_art_acceptance",
    "formal68_acceptance", "production_source_modified",
    "gpu_authorized_or_executed_by_this_addendum",
    "capture_retry_authorized", "readiness_or_authorization_created",
    "threshold_changed", "product_exposed", "golden_promotion",
    "m2_promotion", "beta_pass",
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


def load_telemetry() -> list[dict[str, Any]]:
    path = ROOT / BINDINGS[5][0]
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line]


def ppm_dimensions(path: Path) -> tuple[int, int]:
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
    require(tokens[0] == b"P6" and tokens[3] == b"255", "PPM format/max value")
    width, height = int(tokens[1]), int(tokens[2])
    index += 1
    require(len(raw[index:]) == width * height * 3, "PPM RGB byte count")
    return width, height


def validate_bindings(addendum: dict[str, Any]) -> None:
    rows = addendum.get("read_only_bindings", [])
    require([(row.get("path"), row.get("sha256")) for row in rows] == BINDINGS,
            "exact ordered read-only bindings")
    for path, digest in BINDINGS:
        candidate = ROOT / path
        require(candidate.is_file() and sha256(candidate) == digest,
                "binding missing/drift: " + path)
    for row in rows[6:10]:
        require(row.get("physical_dimensions") == [1920, 1080],
                "bound PPM physical dimensions")
        require(ppm_dimensions(ROOT / row["path"]) == (1920, 1080),
                "decoded PPM physical dimensions")


def validate_dispatch(addendum: dict[str, Any]) -> None:
    receipt = load_json(BINDINGS[3][0])
    expected = addendum.get("authoritative_dispatch_result", {})
    require(receipt.get("status") == "CAPTURE_FAILED_TELEMETRY_PRESERVED"
            and receipt.get("completed_count") == 3
            and receipt.get("failed_count") == 1
            and receipt.get("unrun_count") == 7
            and receipt.get("failed_capture_id") == "camera_broadcast_1080"
            and receipt.get("authorization_consumed_once") is True
            and receipt.get("technical_art_review_authorized") is False
            and receipt.get("formal68_authorized") is False,
            "frozen dispatch failure facts")
    require(expected.get("status") == receipt["status"]
            and expected.get("completed_count") == 3
            and expected.get("failed_count") == 1
            and expected.get("unrun_count") == 7
            and expected.get("failed_capture_id") == "camera_broadcast_1080"
            and expected.get("technical_art_review_authorized") is False
            and expected.get("formal68_authorized") is False,
            "addendum dispatch facts")
    results = [json.loads(line) for line in
               (ROOT / BINDINGS[4][0]).read_text(encoding="utf-8").splitlines()
               if line]
    require([row.get("capture_id") for row in results] == CAPTURE_IDS[:3]
            and all(row.get("machine_status") ==
                    "V4_PASS_PREFLIGHT_ONLY_PENDING_TA" for row in results),
            "results prefix exact first three only")


def validate_telemetry(addendum: dict[str, Any]) -> None:
    rows = load_telemetry()
    require(len(rows) == 4
            and [row.get("plan_ordinal_one_based") for row in rows] == [1, 2, 3, 4]
            and [row.get("capture_id") for row in rows] == CAPTURE_IDS
            and [row.get("machine_status") for row in rows] == MACHINE_STATUSES,
            "exact telemetry ordinal/status inventory")
    comparisons = addendum.get("ordinal_comparison", [])
    require(len(comparisons) == 4, "four ordinal comparisons")
    for index, (row, comparison) in enumerate(zip(rows, comparisons)):
        require([row.get("source_probe_count"), row.get("visible_probe_count"),
                 row.get("hidden_probe_count")] == SOURCE_VISIBLE_HIDDEN[index],
                "telemetry source/visible/hidden")
        actual = row.get("actual_source_visible_hidden_and_coverage", {})
        predicted = row.get("predicted_source_visible_hidden_and_coverage", {})
        require([actual.get("source"), actual.get("visible"), actual.get("hidden"),
                 actual.get("coverage")] == ACTUAL[index], "actual coverage facts")
        require([predicted.get("source"), predicted.get("visible"),
                 predicted.get("hidden"), predicted.get("coverage")] == PREDICTED[index],
                "predicted coverage facts")
        require(comparison.get("ordinal") == index + 1
                and comparison.get("capture_id") == CAPTURE_IDS[index]
                and comparison.get("machine_status") == MACHINE_STATUSES[index]
                and comparison.get("source_visible_hidden") == SOURCE_VISIBLE_HIDDEN[index]
                and comparison.get("actual_source_visible_hidden_coverage") == ACTUAL[index]
                and comparison.get("predicted_source_visible_hidden_coverage") == PREDICTED[index],
                "addendum ordinal comparison exact facts")
    require(rows[3].get("failed_machine_predicate_names") == ["ownership_ok"]
            and comparisons[3].get("failed_machine_predicates") == ["ownership_ok"],
            "ordinal4 exact failed predicate")


def validate_broadcast_failure(addendum: dict[str, Any]) -> None:
    row = load_telemetry()[3]
    evidence = addendum.get("broadcast_failure_evidence", {})
    require(evidence.get("component_source_counts") == row.get("component_source_counts")
            and evidence.get("component_visible_counts") == row.get("component_visible_counts")
            and evidence.get("component_hidden_reason_counts") ==
                row.get("component_hidden_reason_counts"),
            "broadcast component counts/reasons")
    candidates = {item["source_probe_id"]: item
                  for item in row["ownership_candidate_records"]}
    hidden_actual = []
    local_contrast = []
    for hidden in row["hidden_probe_id_track_role_reason_coordinates"]:
        candidate = candidates[hidden["source_probe_id"]]
        if hidden["track"] == "actual":
            hidden_actual.append({
                "component_role": hidden["component_role"],
                "coordinate": [hidden["x_physical_px"], hidden["y_physical_px"]],
                "resolved_pixel_equivalent_area":
                    candidate["resolved_pixel_equivalent_area"],
            })
        if hidden["track"] == "predicted" and hidden["reason"] == \
                "final_edge_contrast_below_4":
            selected = [boundary for boundary in
                        row["component_aware_boundary_records"]
                        if boundary.get("source_probe_id") == hidden["source_probe_id"]
                        and boundary.get("selected")]
            require(len(selected) == 1, "one selected contrast boundary")
            boundary = selected[0]
            local_contrast.append({
                "component_role": hidden["component_role"],
                "coordinate": [hidden["x_physical_px"], hidden["y_physical_px"]],
                "contrast_ratio": boundary["edge_contrast_ratio"],
                "owner_rgb": boundary["owner_final_rgb_srgb8"],
                "adjacent_rgb": boundary["adjacent_final_rgb_srgb8"],
            })
    require(evidence.get("actual_unique_area_failures") == hidden_actual,
            "all six actual unique-area failures")
    require(evidence.get("predicted_local_contrast_failures") == local_contrast
            and len(local_contrast) == 2
            and all(item["contrast_ratio"] < 4.0 for item in local_contrast),
            "two exact predicted local-contrast failures")
    anchors = evidence.get("actual_surviving_anchor_facts", {})
    require(anchors.get("actual_visible_over_source") == [6, 12]
            and anchors.get("actual_hidden_core_count") == 3
            and anchors.get("actual_hidden_solid_in_predicted_dash_gap_count") == 3
            and anchors.get("actual_visible_tick_count") == 2
            and anchors.get("actual_visible_endpoint_count") == 1,
            "actual unique-area/anchor facts")
    ownership = evidence.get("ownership_integrity_controls", {})
    require(all(row.get(field) == 0 and ownership.get(field) == 0 for field in
                ("foreign_track_evidence_count", "double_assignment_count",
                 "unowned_evidence_count")),
            "cross-track ownership integrity controls")


def validate_visual_decisions(addendum: dict[str, Any]) -> None:
    method = addendum.get("review_method", {})
    require(method.get("original_resolution_reviewed") is True
            and method.get("image_count_reviewed") == 4
            and method.get("resampling_used_for_decision") is False
            and method.get("broadcast_trajectory_roi_xyxy_inclusive") ==
                [680, 340, 1000, 570],
            "original-resolution review method")
    axes = addendum.get("canonical_axis_decisions", [])
    require([item.get("axis_id") for item in axes] == AXES
            and [item.get("result") for item in axes] == AXIS_RESULTS
            and all(item.get("finding") for item in axes),
            "exact seven visual decisions")
    focused = addendum.get("focused_visual_decisions", {})
    require(focused == {
        "broadcast_predicted_primary_color_dominance": "PASS",
        "broadcast_actual_solid_core_unique_area": "FAIL",
        "broadcast_actual_in_predicted_dash_gap_readability": "FAIL",
        "broadcast_endpoint_and_tick_readability": "PASS",
        "broadcast_local_silhouette_contrast": "FAIL",
        "broadcast_same_axis_independent_traceability": "FAIL",
        "broadcast_cross_track_ownership_integrity": "PASS_CONTROL_ONLY",
        "ordinal4_independent_visual_result": "FAIL",
    }, "focused broadcast decisions")
    acceptance = addendum.get("acceptance", {})
    require(acceptance.get("ordinal4_visual_acceptance") is False
            and acceptance.get("blocking_axes") ==
                [axis for axis, result in zip(AXES, AXIS_RESULTS)
                 if result == "FAIL"]
            and acceptance.get("targeted11_machine_pass") is False
            and acceptance.get("targeted11_capture_acceptance") is False
            and acceptance.get("targeted11_technical_art_acceptance") is False
            and acceptance.get("formal68_acceptance") is False,
            "fail-closed acceptance")
    constraints = addendum.get("future_visual_constraints_only", [])
    require(len(constraints) == 6
            and any("does not prescribe implementation" in item for item in constraints),
            "visual constraints only/no implementation design")


def validate_boundary(addendum: dict[str, Any]) -> None:
    boundary = addendum.get("proof_boundary", {})
    require(boundary.get("informational_original_resolution_review_performed") is True,
            "informational review performed")
    for field in FALSE_BOUNDARY_FIELDS:
        require(boundary.get(field) is False, "proof boundary must be false: " + field)


def validate(path: Path | str = DEFAULT_ADDENDUM) -> dict[str, Any]:
    addendum = load_json(path)
    require(addendum.get("schema") ==
            "phase4_v11_ordinal4_independent_ta_failure_acceptance_addendum_v1"
            and addendum.get("schema_version") == 1
            and addendum.get("status") ==
            "BROADCAST_PROJECTED_TRAJECTORY_FAIL_CAPTURE_TA_FORMAL_FALSE",
            "addendum schema/status")
    validate_bindings(addendum)
    validate_dispatch(addendum)
    validate_telemetry(addendum)
    validate_broadcast_failure(addendum)
    validate_visual_decisions(addendum)
    validate_boundary(addendum)
    return {
        "status": "PASS_PHASE4_V11_ORDINAL4_TA_FAILURE_ADDENDUM",
        "reviewed_ppm_count": 4,
        "control_machine_pass_count": 3,
        "broadcast_machine_status": "FAILED",
        "broadcast_actual_visible_over_source": [6, 12],
        "visual_axes_pass": AXIS_RESULTS.count("PASS"),
        "visual_axes_fail": AXIS_RESULTS.count("FAIL"),
        "targeted11_machine_pass": False,
        "capture_acceptance": False,
        "technical_art_acceptance": False,
        "formal68_acceptance": False,
        "production_modified": False,
        "gpu_executed_by_addendum": False,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("path", nargs="?", default=str(DEFAULT_ADDENDUM))
    args = parser.parse_args()
    print(json.dumps(validate(args.path), sort_keys=True))


if __name__ == "__main__":
    main()
