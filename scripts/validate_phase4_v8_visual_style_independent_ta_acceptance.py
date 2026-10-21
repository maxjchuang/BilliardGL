#!/usr/bin/env python3
"""Validate the Phase4 v8 visual-style TA addendum and NOT_RUN review template."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ADDENDUM = ROOT / "docs/art/phase4-v8-visual-style-independent-ta-acceptance-addendum-v1.json"
TEMPLATE = ROOT / "docs/art/phase4-v8-visual-style-independent-ta-review-template-v1.json"
ADDENDUM_SHA256 = "8c04a126ad8ed04ca869cbb576291eb30b45fdcf4dc763bcc05ebaf8adc321f0"
TEMPLATE_SHA256 = "99b708bfbbb792c3c5197b543aedf3c7e5a516e05413065d97c6e6f88fef0132"

AXES = [
    "predicted_track_visual_identity", "actual_track_visual_identity",
    "core_inner_halo_separation", "outer_halo_background_silhouette",
    "predicted_dash_gap_rhythm", "endpoint_tick_terminal_readability",
    "coaxial_predicted_actual_disambiguation",
]
ROLES = ["predicted", "actual", "core", "halo", "dash", "endpoint"]
CONTEXTS = ["deep_dark_surround", "teal_table_cloth", "red_ball_adjacency",
            "near_white_ball_surface", "coaxial_track_overlap"]
O2_AXES = [
    "O2_PREDICTED_OUTER_SILHOUETTE_MULTIBACKGROUND",
    "O2_PREDICTED_CORE_HALO_COAXIAL_INTEGRITY",
    "O2_ACTUAL_NEAR_WHITE_DASH_GAP_SEPARATION",
    "O2_COAXIAL_TRACK_DISAMBIGUATION",
    "O2_ENDPOINT_TICK_TERMINAL_READABILITY",
]
CASES = [
    (1, "camera_player_low_1080", [1920, 1080], "ordinal1_regression_control"),
    (2, "camera_elevated_1080", [1920, 1080], "ordinal2_primary_blocker"),
    (3, "camera_overhead_1080", [1920, 1080], "camera_regression_control"),
    (4, "camera_broadcast_1080", [1920, 1080], "camera_regression_control"),
    (5, "comparison_720_ui135_cvd", [1280, 720], "small_cvd_stress_control"),
    (6, "actual_retina_ui135_cvd", [1920, 1080], "retina_cvd_actual_stress_control"),
    (7, "advanced_1080_ui100_standard", [1920, 1080], "advanced_overlay_control"),
    (8, "core__launch_right_english_one_rail_v1__broadcast", [1920, 1080], "core_fixture_control"),
    (9, "core__launch_left_english_one_rail_v1__broadcast", [1920, 1080], "core_fixture_control"),
    (10, "core__launch_corner_pocket_angle_v1__broadcast", [1920, 1080], "core_fixture_control"),
    (11, "core__launch_side_pocket_angle_v1__overhead", [1920, 1080], "core_fixture_control"),
]
FROZEN = {
    "v8_visual_style_correction_design_gate": (
        "docs/art/phase4-final-ownership-v8-ordinal2-offline-diagnostic-v1/minimal-visual-style-correction-design-gate-v1.json",
        "0fe967c8ca0e855c6cb0cd29f745aefd7dfcf8a9ae387eb1522d254753196526"),
    "v8_ordinal2_failure_evidence_receipt": (
        "docs/art/phase4-final-ownership-v8-ordinal2-offline-diagnostic-v1/failure-evidence-receipt-v1.json",
        "7994824c30ef85e41bb4a80d0372e8fee8b011b832507ac18c09f09e7adb07b7"),
    "v8_failure_dispatch_receipt": (
        "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v8/phase4-final-ownership-dispatch-failure-receipt-v1.json",
        "1b4cac8f1df51e8b0d04a3e06770cddbf815b8a64377326f424c58e3901cfd89"),
    "v8_machine_results_prefix": (
        "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v8/results.jsonl",
        "bbe1082a330f111421e8767f88c20ac9b6dda3ba945a746b6723a199c4fd84e1"),
    "v8_integrated_final_ownership_telemetry": (
        "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v8/results.jsonl.final-ownership.jsonl",
        "48d34fb11a342ffb7ff00f0729396ad48544bcfec4db43470bedd3d92148bd99"),
    "v8_exact_targeted11_plan": (
        "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v8/phase4-final-ownership-targeted11-plan.tsv",
        "3b969b34e52fe2e01e31e34b148c4ce7c211682db65b3c609a43274366c8717d"),
    "frozen_ordinal1_player_low_ppm": (
        "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v8/camera_player_low_1080.ppm",
        "f3626f44ead6cfe4729e6ff3b76168747154b2452d0ca2afd70e1520b0e09744"),
    "frozen_ordinal2_elevated_ppm": (
        "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v8/camera_elevated_1080.ppm",
        "1761d04326782eebaef2f92043d422d32f1140297bc64515a8a114d637c51da7"),
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(value, dict), "JSON object required: " + str(path))
    return value


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def ppm_dimensions(path: Path) -> list[int]:
    with path.open("rb") as stream:
        tokens = []
        while len(tokens) < 4:
            line = stream.readline()
            require(line, "truncated PPM header")
            line = line.split(b"#", 1)[0]
            tokens.extend(line.split())
    require(tokens[0] == b"P6" and tokens[3] == b"255", "unsupported PPM header")
    return [int(tokens[1]), int(tokens[2])]


def validate_addendum(value: dict, enforce_live_bindings: bool = True) -> None:
    require(value.get("schema") ==
            "phase4_v8_visual_style_independent_ta_acceptance_addendum_v1" and
            value.get("schema_version") == 1 and value.get("status") ==
            "TA_ACCEPTANCE_PROTOCOL_APPROVED_FUTURE_TARGETED11_NOT_RUN",
            "addendum identity/status mismatch")
    bindings = value.get("frozen_bindings", [])
    require(len(bindings) == 8, "frozen binding count mismatch")
    actual = {row.get("role"): (row.get("path"), row.get("sha256"))
              for row in bindings}
    require(actual == FROZEN, "frozen binding path/SHA mismatch")
    dimensions = {row["role"]: row.get("physical_dimensions") for row in bindings}
    require(dimensions["frozen_ordinal1_player_low_ppm"] == [1920, 1080] and
            dimensions["frozen_ordinal2_elevated_ppm"] == [1920, 1080],
            "frozen PPM dimension binding mismatch")
    if enforce_live_bindings:
        for role, (relative, digest) in FROZEN.items():
            path = ROOT / relative
            require(path.is_file() and sha(path) == digest,
                    "live frozen binding drift: " + role)
        require(ppm_dimensions(ROOT / FROZEN["frozen_ordinal1_player_low_ppm"][0]) ==
                [1920, 1080] and
                ppm_dimensions(ROOT / FROZEN["frozen_ordinal2_elevated_ppm"][0]) ==
                [1920, 1080], "live frozen PPM dimensions drift")
        telemetry_path = ROOT / FROZEN["v8_integrated_final_ownership_telemetry"][0]
        with telemetry_path.open(encoding="utf-8") as stream:
            rows = [json.loads(next(stream)), json.loads(next(stream))]
        require(rows[0].get("capture_id") == "camera_player_low_1080" and
                rows[0].get("machine_status") == "PASS" and
                [rows[0].get("visible_probe_count"), rows[0].get("source_probe_count")] ==
                [92, 95] and
                rows[1].get("capture_id") == "camera_elevated_1080" and
                rows[1].get("machine_status") == "FAILED" and
                [rows[1].get("visible_probe_count"), rows[1].get("source_probe_count")] ==
                [72, 83], "frozen ordinal telemetry identity/result mismatch")

    review = value.get("independent_frozen_evidence_review", {})
    o1, o2 = review.get("ordinal1", {}), review.get("ordinal2", {})
    require(o1.get("visible_source_hidden") == [92, 95, 3] and
            o1.get("ta_interpretation") ==
            "PASS_CONTROL_ONLY_NOT_INHERITABLE_TO_FUTURE_CAPTURE" and
            o2.get("visible_source_hidden") == [72, 83, 11] and
            o2.get("ta_interpretation") ==
            "FAIL_DIAGNOSTIC_ONLY_BLOCKS_FUTURE_ACCEPTANCE_UNTIL_RECAPTURE",
            "independent frozen review result mismatch")
    require([item.get("axis_id") for item in value.get("canonical_ta_axes", [])] == AXES,
            "canonical TA axes mismatch")
    stress = value.get("stress_context_contract", [])
    require([item.get("context_id") for item in stress] == CONTEXTS and
            all(item.get("required_roles") == ROLES for item in stress),
            "stress context/role contract mismatch")
    require([item.get("axis_id") for item in value.get("ordinal2_blocking_axes", [])] ==
            O2_AXES, "ordinal2 blocking axis inventory mismatch")
    o1_gate = value.get("ordinal1_regression_gate", {})
    require(o1_gate.get("capture_id") == "camera_player_low_1080" and
            o1_gate.get("source_probe_count_immutable") == 95 and
            o1_gate.get("future_machine_result_required") == "PASS" and
            o1_gate.get("future_manual_axes_required") == 7 and
            o1_gate.get("future_manual_failures_allowed") == 0 and
            o1_gate.get("future_review_must_not_inherit_frozen_pass") is True,
            "ordinal1 regression contract mismatch")
    future = value.get("future_targeted11_machine_prerequisite", {})
    require(future.get("status") == "NOT_AVAILABLE_NOT_BOUND" and
            future.get("future_receipt_path") is None and
            future.get("future_receipt_sha256") is None and
            future.get("required_schema") ==
            "billiardgl.phase4.final_ownership_visual_style_targeted11_machine_pass.v1" and
            future.get("required_case_count") == future.get("required_passed_count") == 11 and
            future.get("required_failed_count") == 0 and
            future.get("machine_pass_does_not_equal_ta_pass") is True,
            "future machine prerequisite must remain absent and exact")
    cases = value.get("targeted11_case_contract", [])
    require([(row.get("ordinal"), row.get("capture_id"),
              row.get("physical_dimensions"), row.get("role")) for row in cases] == CASES,
            "targeted11 case contract mismatch")
    protocol = value.get("future_independent_ta_protocol", {})
    require(protocol.get("status_values") == ["PASS", "FAIL", "NOT_RUN"] and
            [protocol.get(key) for key in (
                "image_count", "canonical_axis_count", "image_axis_result_count",
                "stress_context_count", "roles_per_stress_context",
                "stress_cell_count", "ordinal2_blocking_axis_count")] ==
            [11, 7, 77, 5, 6, 30, 5], "future TA protocol counts mismatch")
    forbidden = " ".join(value.get("forbidden_workarounds", []))
    require(all(term in forbidden for term in
                ("threshold", "offset", "z-order", "camera", "layout")),
            "forbidden workaround boundary incomplete")
    boundary = value.get("proof_boundary", {})
    require(boundary and all(item is False for item in boundary.values()),
            "addendum cannot claim execution or promotion")


def validate_template(value: dict) -> dict:
    require(value.get("schema") == "phase4_v8_visual_style_independent_ta_review_v1" and
            value.get("schema_version") == 1, "review schema mismatch")
    binding = value.get("acceptance_addendum_binding", {})
    require(binding == {
        "path": "docs/art/phase4-v8-visual-style-independent-ta-acceptance-addendum-v1.json",
        "sha256": ADDENDUM_SHA256, "binding_state": "FROZEN"},
        "review addendum binding mismatch")
    require(value.get("canonical_axis_ids") == AXES, "template axis inventory mismatch")
    if value.get("status") == "TEMPLATE_NOT_RUN_WAITING_FOR_FUTURE_TARGETED11_MACHINE_PASS":
        return validate_not_run(value)
    return validate_completed(value)


def validate_not_run(value: dict) -> dict:
    require(value.get("overall_result") == "NOT_RUN", "template overall must be NOT_RUN")
    machine = value.get("future_targeted11_machine_binding", {})
    require(machine == {"path": None, "sha256": None,
        "status": "NOT_AVAILABLE_NOT_BOUND", "machine_result": "NOT_RUN",
        "case_count": 0, "passed_count": 0, "failed_count": 0,
        "ppm_telemetry_manifest_path": None, "ppm_telemetry_manifest_sha256": None,
        "stable_evidence_aggregate_sha256": None},
        "unexecuted template cannot bind future machine evidence")
    reviewer = value.get("reviewer", {})
    require(reviewer.get("status") == "NOT_RUN" and
            all(reviewer.get(key) is None for key in (
                "pseudonymous_reviewer_id", "independent_from_implementation",
                "review_started_at_utc", "review_completed_at_utc",
                "display_and_viewer_configuration_evidence_sha256")),
            "unexecuted reviewer fields must remain empty")
    cases = value.get("case_reviews", [])
    require(len(cases) == 11, "template case count mismatch")
    for row, expected in zip(cases, CASES):
        ordinal, capture_id, dimensions, role = expected
        require((row.get("ordinal"), row.get("capture_id"),
                 row.get("expected_physical_dimensions"), row.get("role")) == expected,
                "template case identity mismatch: " + capture_id)
        require(all(row.get(key) is None for key in (
                    "ppm_path", "ppm_sha256", "decoded_physical_dimensions",
                    "telemetry_path", "telemetry_sha256")) and
                row.get("opened_individually_at_original_resolution") is False and
                row.get("axis_statuses") == {axis: "NOT_RUN" for axis in AXES} and
                row.get("axis_observations") == {} and row.get("blocking_axes") == [] and
                row.get("case_result") == "NOT_RUN",
                "unexecuted case contains evidence/result: " + capture_id)
    stress = value.get("stress_context_results", [])
    require([row.get("context_id") for row in stress] == CONTEXTS and
            all(row.get("role_statuses") == {role: "NOT_RUN" for role in ROLES} and
                row.get("cell_evidence") == {} for row in stress),
            "unexecuted stress matrix mismatch")
    o1 = value.get("ordinal1_regression_result", {})
    require(o1 == {"capture_id": "camera_player_low_1080", "status": "NOT_RUN",
                   "machine_pass_confirmed": False, "seven_axes_passed": 0,
                   "observation": None}, "ordinal1 template must remain NOT_RUN")
    o2 = value.get("ordinal2_blocking_axis_results", [])
    require([row.get("axis_id") for row in o2] == O2_AXES and
            all(row.get("status") == "NOT_RUN" and row.get("observation") is None and
                row.get("evidence") == [] for row in o2),
            "ordinal2 template must remain NOT_RUN")
    require(value.get("counts") == {
        "images_expected": 11, "images_reviewed": 0,
        "image_axes_expected": 77, "image_axes_pass": 0, "image_axes_fail": 0,
        "image_axes_not_run": 77, "stress_cells_expected": 30,
        "stress_cells_pass": 0, "stress_cells_fail": 0, "stress_cells_not_run": 30,
        "ordinal2_blocking_axes_expected": 5, "ordinal2_blocking_axes_pass": 0,
        "ordinal2_blocking_axes_fail": 0, "ordinal2_blocking_axes_not_run": 5},
        "NOT_RUN counts mismatch")
    manifest = value.get("review_evidence_manifest", {})
    require(manifest == {"path": None, "sha256": None,
                         "stable_aggregate_sha256": None, "status": "NOT_AVAILABLE"},
            "unexecuted template cannot bind review evidence")
    claims = value.get("claims", {})
    require(claims and all(item is False for item in claims.values()),
            "unexecuted template cannot claim pass/execution/promotion")
    return {"status": "PASS_PHASE4_V8_TA_TEMPLATE_NOT_RUN",
            "images": 11, "image_axes_not_run": 77,
            "stress_cells_not_run": 30, "ordinal2_axes_not_run": 5,
            "future_machine_bound": False, "future_ta_pass": False}


def validate_completed(value: dict) -> dict:
    require(value.get("status") == "COMPLETED" and
            value.get("overall_result") in {"PASS", "FAIL"},
            "non-template review must be COMPLETED PASS/FAIL")
    machine = value.get("future_targeted11_machine_binding", {})
    for key in ("path", "sha256", "ppm_telemetry_manifest_path",
                "ppm_telemetry_manifest_sha256", "stable_evidence_aggregate_sha256"):
        require(isinstance(machine.get(key), str) and machine[key],
                "completed review missing machine binding: " + key)
    receipt_path = Path(machine["path"])
    manifest_path = Path(machine["ppm_telemetry_manifest_path"])
    require(receipt_path.is_file() and sha(receipt_path) == machine["sha256"] and
            manifest_path.is_file() and sha(manifest_path) ==
            machine["ppm_telemetry_manifest_sha256"],
            "future machine receipt/manifest binding invalid")
    receipt, manifest = load(receipt_path), load(manifest_path)
    case_ids = [row[1] for row in CASES]
    require(receipt.get("schema") ==
            "billiardgl.phase4.final_ownership_visual_style_targeted11_machine_pass.v1" and
            receipt.get("status") == "PASS" and
            [receipt.get("case_count"), receipt.get("passed_count"),
             receipt.get("failed_count")] == [11, 11, 0] and
            receipt.get("case_ids_in_order") == case_ids and
            machine.get("machine_result") == "PASS" and
            [machine.get("case_count"), machine.get("passed_count"),
             machine.get("failed_count")] == [11, 11, 0],
            "future targeted11 machine is not exact 11/11 PASS")
    require(manifest.get("schema") ==
            "billiardgl.phase4.final_ownership_visual_style_targeted11_evidence_manifest.v1" and
            manifest.get("case_count") == 11 and
            manifest.get("stable_evidence_aggregate_sha256") ==
            machine["stable_evidence_aggregate_sha256"],
            "future evidence manifest identity/aggregate mismatch")
    evidence_rows = manifest.get("cases", [])
    require(len(evidence_rows) == 11, "future evidence case count mismatch")
    digest = hashlib.sha256()
    for evidence, expected in zip(evidence_rows, CASES):
        _, capture_id, dimensions, _ = expected
        require(evidence.get("capture_id") == capture_id and
                evidence.get("physical_dimensions") == dimensions,
                "future evidence identity/dimensions mismatch: " + capture_id)
        ppm, telemetry = Path(evidence.get("ppm_path", "")), Path(evidence.get("telemetry_path", ""))
        require(ppm.is_file() and telemetry.is_file() and
                sha(ppm) == evidence.get("ppm_sha256") and
                sha(telemetry) == evidence.get("telemetry_sha256") and
                ppm_dimensions(ppm) == dimensions,
                "future PPM/telemetry binding invalid: " + capture_id)
        digest.update((capture_id + "\0" + evidence["ppm_sha256"] + "\0" +
                       evidence["telemetry_sha256"] + "\n").encode())
    require(digest.hexdigest() == machine["stable_evidence_aggregate_sha256"],
            "future stable evidence aggregate mismatch")
    reviewer = value.get("reviewer", {})
    require(reviewer.get("status") == "COMPLETED" and
            reviewer.get("independent_from_implementation") is True and
            all(isinstance(reviewer.get(key), str) and reviewer[key] for key in (
                "pseudonymous_reviewer_id", "review_started_at_utc",
                "review_completed_at_utc", "display_and_viewer_configuration_evidence_sha256")),
            "completed independent reviewer identity/evidence missing")
    axis_pass = axis_fail = 0
    cases = value.get("case_reviews", [])
    require(len(cases) == 11, "completed review case count mismatch")
    for row, evidence, expected in zip(cases, evidence_rows, CASES):
        _, capture_id, dimensions, role = expected
        require((row.get("ordinal"), row.get("capture_id"),
                 row.get("expected_physical_dimensions"), row.get("role")) == expected and
                row.get("ppm_path") == evidence["ppm_path"] and
                row.get("ppm_sha256") == evidence["ppm_sha256"] and
                row.get("decoded_physical_dimensions") == dimensions and
                row.get("telemetry_path") == evidence["telemetry_path"] and
                row.get("telemetry_sha256") == evidence["telemetry_sha256"] and
                row.get("opened_individually_at_original_resolution") is True,
                "completed original-resolution case binding invalid: " + capture_id)
        statuses = row.get("axis_statuses", {})
        require(set(statuses) == set(AXES) and
                all(status in {"PASS", "FAIL"} for status in statuses.values()) and
                set(row.get("axis_observations", {})) == set(AXES) and
                all(isinstance(text, str) and text for text in
                    row["axis_observations"].values()),
                "completed axis results/observations incomplete: " + capture_id)
        failed = [axis for axis in AXES if statuses[axis] == "FAIL"]
        require(row.get("blocking_axes") == failed and
                row.get("case_result") == ("FAIL" if failed else "PASS"),
                "completed case result derivation mismatch: " + capture_id)
        axis_fail += len(failed); axis_pass += 7 - len(failed)
    stress_pass = stress_fail = 0
    stress = value.get("stress_context_results", [])
    require([row.get("context_id") for row in stress] == CONTEXTS,
            "completed stress context order mismatch")
    for row in stress:
        statuses, evidence = row.get("role_statuses", {}), row.get("cell_evidence", {})
        require(set(statuses) == set(ROLES) and set(evidence) == set(ROLES) and
                all(status in {"PASS", "FAIL"} for status in statuses.values()),
                "completed stress role cells incomplete")
        for role in ROLES:
            cell = evidence[role]
            require(cell.get("capture_id") in case_ids and
                    isinstance(cell.get("physical_coordinate"), list) and
                    len(cell["physical_coordinate"]) == 2 and
                    all(isinstance(item, int) and item >= 0 for item in
                        cell["physical_coordinate"]) and
                    isinstance(cell.get("observation"), str) and cell["observation"],
                    "stress cell evidence incomplete")
            stress_fail += statuses[role] == "FAIL"
            stress_pass += statuses[role] == "PASS"
    o1 = value.get("ordinal1_regression_result", {})
    o1_case = cases[0]
    require(o1.get("status") in {"PASS", "FAIL"} and
            o1.get("machine_pass_confirmed") is True and
            o1.get("seven_axes_passed") == sum(
                status == "PASS" for status in o1_case["axis_statuses"].values()) and
            isinstance(o1.get("observation"), str) and o1["observation"],
            "ordinal1 regression result incomplete")
    o2 = value.get("ordinal2_blocking_axis_results", [])
    require([row.get("axis_id") for row in o2] == O2_AXES and
            all(row.get("status") in {"PASS", "FAIL"} and
                isinstance(row.get("observation"), str) and row["observation"] and
                isinstance(row.get("evidence"), list) and row["evidence"]
                for row in o2), "ordinal2 blocking axes incomplete")
    o2_fail = sum(row["status"] == "FAIL" for row in o2)
    counts = value.get("counts", {})
    require(counts == {"images_expected": 11, "images_reviewed": 11,
        "image_axes_expected": 77, "image_axes_pass": axis_pass,
        "image_axes_fail": axis_fail, "image_axes_not_run": 0,
        "stress_cells_expected": 30, "stress_cells_pass": stress_pass,
        "stress_cells_fail": stress_fail, "stress_cells_not_run": 0,
        "ordinal2_blocking_axes_expected": 5,
        "ordinal2_blocking_axes_pass": 5 - o2_fail,
        "ordinal2_blocking_axes_fail": o2_fail,
        "ordinal2_blocking_axes_not_run": 0}, "completed count derivation mismatch")
    passed = axis_fail == stress_fail == o2_fail == 0 and o1.get("status") == "PASS"
    require(value.get("overall_result") == ("PASS" if passed else "FAIL"),
            "completed overall result derivation mismatch")
    claims = value.get("claims", {})
    require(claims.get("future_targeted11_machine_pass") is True and
            claims.get("future_ta_pass") is passed and
            claims.get("ordinal1_regression_pass") is (o1.get("status") == "PASS") and
            claims.get("ordinal2_blockers_closed") is (o2_fail == 0) and
            claims.get("gpu_executed") is True and
            all(claims.get(field) is False for field in (
                "production_modified", "authorization_created_or_issued",
                "product_exposed", "golden_promotion", "m2_promotion", "beta_pass")),
            "completed claim boundary mismatch")
    return {"status": "PASS_VALIDATED_COMPLETED_TA_REVIEW" if passed else
            "PASS_VALIDATED_COMPLETED_TA_FAILURE", "overall": value["overall_result"],
            "images": 11, "image_axis_failures": axis_fail,
            "stress_cell_failures": stress_fail, "ordinal2_axis_failures": o2_fail}


def validate(addendum_path: Path = ADDENDUM, review_path: Path = TEMPLATE,
             enforce_frozen_sha: bool = True,
             enforce_live_bindings: bool = True) -> dict:
    addendum_path, review_path = addendum_path.resolve(strict=True), review_path.resolve(strict=True)
    if enforce_frozen_sha:
        require(addendum_path == ADDENDUM.resolve() and sha(addendum_path) == ADDENDUM_SHA256,
                "frozen addendum SHA mismatch")
        if review_path == TEMPLATE.resolve():
            require(sha(review_path) == TEMPLATE_SHA256, "frozen template SHA mismatch")
    addendum, review = load(addendum_path), load(review_path)
    validate_addendum(addendum, enforce_live_bindings=enforce_live_bindings)
    result = validate_template(review)
    return {**result, "addendum_sha256": sha(addendum_path),
            "review_sha256": sha(review_path)}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--addendum", type=Path, default=ADDENDUM)
    parser.add_argument("--review", type=Path, default=TEMPLATE)
    args = parser.parse_args()
    try:
        print(json.dumps(validate(args.addendum, args.review,
                                  enforce_frozen_sha=(args.addendum.resolve() == ADDENDUM.resolve())),
                         sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError,
            StopIteration) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
