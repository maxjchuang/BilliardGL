#!/usr/bin/env python3
"""Fail-closed validation for the Phase4 v12 broadcast visual design gate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GATE = ROOT / (
    "docs/art/phase4-v12-broadcast-projected-trajectory-"
    "visual-repair-design-gate-v1.json"
)
BINDINGS = [
    ("docs/art/phase4-final-ownership-v11-ordinal4-offline-diagnostic-v1/failure-evidence-receipt-v1.json", "07f251d466b0dcad550b4951e8242403bcaaaeb97dd800cbde99cdf23c2f3f85"),
    ("docs/art/phase4-final-ownership-v11-ordinal4-offline-diagnostic-v1/minimal-raster-support-connectivity-correction-design-gate-v1.json", "59bbabf437e9195392b2a41f4e48eea0513afb222706115a07e9c0965cbad059"),
    ("docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v11/phase4-final-ownership-dispatch-failure-receipt-v1.json", "282056928154ce78ad79b8cee0062d460d03f76c9d130f1c9f268f0dfa009a39"),
    ("docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v11/results.jsonl", "0fd67fe71d6115e7fe7bebe874ec7f4280a6a48dacd4432e2a15e1fe1ec57e69"),
    ("docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v11/results.jsonl.final-ownership.jsonl", "4e7e884569eec1197ef11d39d256e2e43eef11c49ee7c84ff150b93c3c6d61ef"),
    ("docs/art/phase4-v11-ordinal4-independent-ta-failure-acceptance-addendum-v1.json", "64936ca990a0077323dcc2d64e9dbdf8bf9b400da72f7a11d450beb0a89b04c4"),
    ("docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v11/camera_player_low_1080.ppm", "82d6f9e5aba1821b3b27c00f2b76977fff035cf8541c768bd5361c1ee4ee744a"),
    ("docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v11/camera_elevated_1080.ppm", "e6eca69d865f87f8b0d49960567520d81dcd2b5c8f83d217e0cde8c919e2fa46"),
    ("docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v11/camera_overhead_1080.ppm", "a03a84d5de933106980406d9ddcfdeb2cc2e8a36984eab39db1cfaff79fbba8e"),
    ("docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v11/camera_broadcast_1080.ppm", "0b6c87cb415a2e80ad1f6e061a972be57c683b8104e28d0065ba52b6a0045878"),
]
ALLOWED_PRODUCTION_FILES = [
    "src/Billiards/training/trajectory_overlay_renderer.cpp",
    "src/Billiards/training/trajectory_overlay_renderer.h",
]
OUTCOMES = [
    "actual_full_path_independent_traceability",
    "actual_visible_in_every_predicted_dash_gap",
    "predicted_yellow_dominance_and_gap_rhythm_preserved",
    "endpoint_ticks_and_terminals_preserved",
    "outer_silhouette_contrast_repaired",
    "same_axis_repaired_cross_track_integrity_preserved",
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
CASES = [
    "camera_player_low_1080", "camera_elevated_1080",
    "camera_overhead_1080", "camera_broadcast_1080",
    "actual_retina_ui135_cvd",
]
FALSE_BOUNDARY_FIELDS = [
    "production_modified_by_this_gate", "algorithm_implemented_by_this_gate",
    "visual_repair_implemented", "cpu_static_preflight_run",
    "readiness_created", "authorization_created_or_issued",
    "gpu_authorized_or_executed", "future_targeted_machine_pass",
    "future_targeted_ta_pass", "targeted11_pass", "formal68_pass",
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
    return int(tokens[1]), int(tokens[2])


def validate_bindings(gate: dict[str, Any]) -> None:
    rows = gate.get("frozen_bindings", [])
    require([(row.get("path"), row.get("sha256")) for row in rows] == BINDINGS,
            "exact ordered frozen bindings")
    for path, digest in BINDINGS:
        candidate = ROOT / path
        require(candidate.is_file() and sha256(candidate) == digest,
                "binding missing/drift: " + path)
    for row in rows[6:10]:
        require(row.get("physical_dimensions") == [1920, 1080]
                and ppm_dimensions(ROOT / row["path"]) == (1920, 1080),
                "bound original PPM dimensions")
    algorithm = load_json(BINDINGS[1][0])
    require(algorithm.get("schema") ==
            "billiardgl.phase4.final_ownership_v11_raster_support_connectivity_correction_gate.v1"
            and algorithm.get("status") ==
            "DESIGN_ONLY_NOT_IMPLEMENTED_NO_CAPTURE_AUTHORIZED"
            and algorithm.get("gpu_cocoa_opengl_authorized") is False
            and algorithm.get("capture_retry_authorized") is False,
            "coexisting algorithm gate remains design-only/no auth")
    addendum = load_json(BINDINGS[5][0])
    require(addendum.get("status") ==
            "BROADCAST_PROJECTED_TRAJECTORY_FAIL_CAPTURE_TA_FORMAL_FALSE"
            and addendum.get("acceptance", {}).get("ordinal4_visual_acceptance") is False,
            "bound independent TA failure addendum")


def validate_failure_facts(gate: dict[str, Any]) -> None:
    facts = gate.get("bound_failure_facts", {})
    require(facts == {
        "broadcast_machine_status": "FAILED",
        "failed_predicate": "ownership_ok",
        "broadcast_source_visible_hidden": [81, 68, 13],
        "broadcast_predicted_source_visible_hidden": [69, 62, 7],
        "broadcast_actual_source_visible_hidden": [12, 6, 6],
        "actual_hidden_core_count": 3,
        "actual_hidden_solid_in_predicted_dash_gap_count": 3,
        "predicted_outer_silhouette_contrast_failures":
            [3.9028463506845466, 3.9970607697890235],
        "actual_visible_endpoint_count": 1,
        "actual_visible_tick_count": 2,
        "foreign_track_evidence_count": 0,
        "double_assignment_count": 0,
        "unowned_evidence_count": 0,
        "independent_ta_axis_result": [2, 5],
        "targeted11_machine_pass": False,
        "targeted11_ta_pass": False,
        "formal68_pass": False,
    }, "exact v11 visual failure facts")
    telemetry = [json.loads(line) for line in
                 (ROOT / BINDINGS[4][0]).read_text(encoding="utf-8").splitlines()
                 if line]
    require(len(telemetry) == 4 and telemetry[3].get("capture_id") ==
            "camera_broadcast_1080" and telemetry[3].get("machine_status") ==
            "FAILED" and telemetry[3].get("failed_machine_predicate_names") ==
            ["ownership_ok"], "authoritative ordinal4 telemetry remains failed")


def validate_scope(gate: dict[str, Any]) -> None:
    coexist = gate.get("coexistence_with_algorithm_gate", {})
    require(coexist.get("algorithm_gate_sha256") == BINDINGS[1][1]
            and coexist.get("algorithm_gate_remains_independently_required") is True
            and coexist.get("algorithm_patch_must_remain_visual_byte_equivalent") is True
            and coexist.get("visual_patch_must_not_modify_ownership_connectivity_or_reason_code_logic") is True
            and coexist.get("separate_change_sets_and_separate_validator_receipts_required") is True
            and coexist.get("combined_successor_source_binding_must_bind_both_implemented_change_sets") is True
            and coexist.get("algorithm_counterfactual_is_not_visual_acceptance") is True
            and coexist.get("visual_gate_does_not_amend_algorithm_gate_forbidden_changes") is True
            and len(coexist.get("sequencing", [])) == 3,
            "algorithm/visual gate coexistence")
    outcomes = gate.get("required_visual_outcomes", [])
    require([item.get("outcome_id") for item in outcomes] == OUTCOMES
            and all(item.get("requirement") for item in outcomes),
            "six exact visual outcomes")
    allowed = gate.get("allowed_production_files", [])
    require([item.get("path") for item in allowed] == ALLOWED_PRODUCTION_FILES
            and all(item.get("allowed_symbols_or_regions") for item in allowed),
            "exact production allowlist")
    visual_changes = gate.get("allowed_renderer_visual_changes", [])
    require(len(visual_changes) == 6
            and any("color-vision-safe" in item for item in visual_changes)
            and any("connected final-LDR" in item for item in visual_changes),
            "bounded renderer visual changes")
    forbidden = gate.get("forbidden_changes", [])
    joined = "\n".join(forbidden)
    require(len(forbidden) == 8
            and "No coverage, local-contrast" in joined
            and "centerline offset" in joined
            and "z-order" in joined
            and "camera pose" in joined
            and "No product exposure" in joined,
            "threshold/offset/z-order/camera/layout and exposure forbidden")


def validate_preflight(gate: dict[str, Any]) -> None:
    preflight = gate.get("required_cpu_and_static_preflight", {})
    checks = preflight.get("required_checks", [])
    require(preflight.get("status") == "NOT_RUN" and len(checks) == 10,
            "ten NOT_RUN CPU/static checks")
    joined = "\n".join(checks)
    for needle in ("both standard and color-vision-safe palettes", "81/81",
                   "[783,413]", "[892,491]", "[940,525]", "[855,469]",
                   "[879,486]", "contrast >=4.0", "no generated readiness"):
        require(needle in joined, "missing CPU/static requirement: " + needle)
    require("does not authorize GPU capture" in preflight.get("implementation_exit", ""),
            "CPU/static exit remains no GPU authorization")


def validate_future_acceptance(gate: dict[str, Any]) -> None:
    future = gate.get("future_targeted_capture_gate", {})
    cases = future.get("exact_minimum_cases", [])
    require(future.get("status") == "NOT_AUTHORIZED_NOT_RUN"
            and future.get("minimum_case_count") == 5
            and [item.get("capture_id") for item in cases] == CASES
            and all(item.get("physical_dimensions") == [1920, 1080]
                    for item in cases)
            and future.get("machine_pass_does_not_equal_ta_pass") is True
            and future.get("targeted5_pass_does_not_equal_targeted11_or_formal68") is True,
            "future exact targeted5/no overclaim")
    machine = "\n".join(future.get("machine_acceptance", []))
    require("81/81/0" in machine and "69/69/0" in machine
            and "12/12/0" in machine and "contrast >=4.0" in machine
            and "single-use authorization" in machine,
            "future strict machine acceptance")
    ta = gate.get("future_independent_ta_acceptance", {})
    require(ta.get("status") == "NOT_RUN"
            and ta.get("machine_five_of_five_pass_required_first") is True
            and ta.get("canonical_axes") == AXES
            and ta.get("image_axis_count") == 35
            and ta.get("required_result") == "35_PASS_0_FAIL_0_NOT_RUN"
            and len(ta.get("broadcast_specific_requirements", [])) == 5
            and "may not inherit v11 conclusions" in ta.get("control_requirement", "")
            and "narrower than targeted11" in
                ta.get("targeted5_visual_repair_acceptance", ""),
            "future independent TA acceptance")


def validate_boundary(gate: dict[str, Any]) -> None:
    boundary = gate.get("proof_boundary", {})
    require(boundary.get("design_gate_only") is True, "design gate only")
    for field in FALSE_BOUNDARY_FIELDS:
        require(boundary.get(field) is False, "proof boundary must be false: " + field)


def validate(path: Path | str = DEFAULT_GATE) -> dict[str, Any]:
    gate = load_json(path)
    require(gate.get("schema") ==
            "billiardgl.phase4.v12.broadcast_projected_trajectory_visual_repair_design_gate.v1"
            and gate.get("schema_version") == 1
            and gate.get("status") ==
            "DESIGN_ONLY_NOT_IMPLEMENTED_NO_GPU_NO_AUTHORIZATION",
            "gate schema/status")
    validate_bindings(gate)
    validate_failure_facts(gate)
    validate_scope(gate)
    validate_preflight(gate)
    validate_future_acceptance(gate)
    validate_boundary(gate)
    return {
        "status": "PASS_PHASE4_V12_BROADCAST_VISUAL_REPAIR_DESIGN_GATE",
        "frozen_binding_count": len(BINDINGS),
        "visual_outcome_count": len(OUTCOMES),
        "allowed_production_file_count": len(ALLOWED_PRODUCTION_FILES),
        "cpu_static_preflight": "NOT_RUN",
        "future_targeted_case_count": len(CASES),
        "future_ta_axis_count": len(CASES) * len(AXES),
        "production_modified": False,
        "gpu_authorized": False,
        "authorization_created": False,
        "targeted11_pass": False,
        "formal68_pass": False,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("path", nargs="?", default=str(DEFAULT_GATE))
    args = parser.parse_args()
    print(json.dumps(validate(args.path), sort_keys=True))


if __name__ == "__main__":
    main()
