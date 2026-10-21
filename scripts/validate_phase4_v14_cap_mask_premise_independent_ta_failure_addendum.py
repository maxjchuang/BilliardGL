#!/usr/bin/env python3
"""Validate the independent v14 premise-failure addendum and v15 diagnostic gate."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from scripts import validate_phase4_v14_cap_mask_premise_falsification as receipt_validator

DEFAULT = ROOT / "docs/art/phase4-v14-cap-mask-premise-independent-ta-failure-addendum-v1.json"

BINDINGS = {
    "docs/art/phase4-v14-cap-mask-premise-falsification-v1/premise-falsification-receipt-v1.json":
        "a7ee84eae3622b4e9dc472ed1dccdd8c2d6604d0218501e9cafc019473baa76e",
    "docs/art/phase4-v14-broadcast-predicted-cap-prior-actual-core-mask-design-gate-v1.json":
        "4e7d2419c9ae78b0e573b20482cbca8d91d2de084277ed40910862786cc91be4",
    "docs/art/phase4-v13-broadcast-predicted-dash-cap-release-design-gate-v1.json":
        "da26e980cf5fb365c37473d74025e03122ed99d9c7328482ce325a9cbf3e097d",
    "docs/art/phase4-v13-primary-equal-cap-contraction-exhaustion-v1/primary-exhaustion-receipt-v1.json":
        "6055242d80be491c1cda05f5283cf92aa301a26e33c5f0b436b4ce5c04bf811d",
    "docs/art/phase4-v12-visual-local-alternatives-exhaustion-v1/exhaustion-evidence-receipt-v1.json":
        "62b9120252f9a224e965cb3815ebbcce4a4854019e75a5e4b0d80b7f0b563f97",
    "docs/art/phase4-v11-raster-support-connectivity-implementation-v1/algorithm-implementation-receipt-v1.json":
        "abc181f48adb8e94db31965c2d0111c7641c0ac20242dd4a93189a237c8d01f2",
    "src/Billiards/training/trajectory_overlay_renderer.cpp":
        "0c108c506641ab4828290a1a4c7b6b5ebbd51ab4fc7c12845bde0f5deb18a840",
    "src/Billiards/training/trajectory_overlay_renderer.h":
        "9805e6a5f56c17d7645200576505425cddf0d642970e5dc5cec186e003bd564e",
    "src/Billiards/render/modern/modern_renderer.cpp":
        "b00cc6b370122275a42f106967f935dbb34bd5d3f81f2a750ca9ba6e503af191",
    "src/Billiards/render/modern/modern_renderer.h":
        "936de5b77c9e3485f2e51f10e9de26c59c8ec94674865e0b50ae125725183c9d",
}
REPAIR_IDS = {
    "predicted_contrast": [
        "d6d850a1a10c7c7ce403e6de4afd4330e4c6e89b9100c72f4c23fbab1f6f978b",
        "1890fd0652c8f74a8b03a35a6cee1ad2cab43062987cab73efb1f6b455da468a",
    ],
    "actual_connectivity":
        "1bf60bcd841a0d754c82876b55a4d3d6c29ebad582e069d4f4a608a4778c33a9",
}
FRAGMENT_CLASSES = [
    "core_base_body", "core_start_cap_extension", "core_end_cap_extension",
    "inner_central_body", "inner_lateral_annulus",
    "inner_start_cap_annulus", "inner_end_cap_annulus",
    "outer_side_band", "endpoint_or_tick_or_marker",
]
INTERVAL_CLASSES = [
    "dash_on_interior", "dash_gap_intrusion", "route_endpoint_envelope",
    "cross_segment_or_cross_source_overlap",
]
ALLOWED_DIAGNOSTIC_PATHS = [
    "src/Billiards/training/trajectory_overlay_renderer.cpp",
    "src/Billiards/training/trajectory_overlay_renderer.h",
    "tests/phase4_v15_broadcast_overwrite_provenance_cpu_tests.cpp",
    "CMakeLists.txt",
    "scripts/validate_phase4_v15_broadcast_overwrite_provenance_diagnostic.py",
    "tests/test_phase4_v15_broadcast_overwrite_provenance_diagnostic.py",
]


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate(path: Path = DEFAULT) -> list[str]:
    errors: list[str] = []
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        return [f"addendum_invalid:{error}"]

    def require(condition: bool, message: str) -> None:
        if not condition:
            errors.append(message)

    require(value.get("schema") ==
            "billiardgl.phase4.v14.cap_mask_premise_independent_ta_failure_addendum.v1",
            "schema_invalid")
    require(value.get("status") ==
            "FAIL_ADDENDUM_CPU_ONLY_SUCCESSOR_DIAGNOSTIC_REQUIRED_NO_GPU_NO_AUTH",
            "status_invalid")

    observed: dict[str, str] = {}
    for item in value.get("frozen_bindings", []):
        observed[item.get("path")] = item.get("sha256")
    for item in value.get("restored_production_bindings", {}).values():
        observed[item.get("path")] = item.get("sha256")
    require(observed == BINDINGS, "binding_inventory_or_sha_invalid")
    for relative, digest in BINDINGS.items():
        target = ROOT / relative
        require(target.is_file(), f"binding_missing:{relative}")
        if target.is_file():
            require(sha(target) == digest, f"binding_drift:{relative}")
    require(not receipt_validator.validate(ROOT / next(iter(BINDINGS))),
            "upstream_premise_receipt_validator_failed")

    finding = value.get("independent_failure_finding", {})
    require(finding.get("v14_gate_result") ==
            "FAIL_PREMISE_FALSIFIED_DO_NOT_RESUME_IMPLEMENTATION",
            "v14_failure_result_invalid")
    require(finding.get("blocking_coordinate") == [940, 525] and
            finding.get("neighborhood_radius_px") == 8 and
            finding.get("palettes") == ["standard", "color_vision_safe"],
            "failure_fixture_invalid")
    require(finding.get("blocking_axes") == [
        "actual_track_visual_identity",
        "coaxial_predicted_actual_disambiguation",
        "route_body_vs_tick_semantic_provenance",
    ], "blocking_axes_invalid")
    require(finding.get("unchanged_failed_result_per_palette") == {
        "actual_route_body_final_pixels": 0,
        "legacy_helper_actual_core_pixels": 1,
        "legacy_helper_actual_core_largest8": 1,
        "required_actual_route_body_largest8": 2,
    }, "failed_result_metrics_invalid")

    cause = value.get("pixel_and_control_flow_root_cause", {})
    require(cause.get("prior_actual_route_body_mask_pixels") == 88 and
            cause.get("tagged_predicted_inner_cap_discards_executed") == 59 and
            cause.get("final_prior_mask_pixels_owned_by_predicted_core") == 81 and
            cause.get("final_prior_mask_pixels_owned_by_untagged_predicted_inner_body") == 7 and
            cause.get("final_prior_mask_pixels_owned_by_tagged_predicted_inner_cap") == 0,
            "root_cause_owner_counts_invalid")
    require(cause.get("owner_partition_identity") == "81 + 7 + 0 = 88",
            "owner_partition_identity_invalid")
    require("actual tick core" in cause.get("helper_misclassification", ""),
            "tick_misclassification_not_preserved")

    disposition = value.get("v14_disposition", {})
    require(disposition == {
        "cap_only_strategy_can_meet_required_result": False,
        "partial_implementation_reverted": True,
        "four_production_files_restored": True,
        "v14_readiness_allowed": False,
        "v14_gpu_allowed": False,
        "v14_machine_or_ta_claim_allowed": False,
        "threshold_or_helper_relaxation_allowed": False,
    }, "v14_disposition_invalid")

    successor = value.get("successor_design_decision", {})
    require(successor.get("decision") ==
            "CONDITIONALLY_FEASIBLE_ONLY_AFTER_V15_DIAGNOSTIC_PROVENANCE_GATE" and
            successor.get("visual_repair_authorized_now") is False and
            successor.get("mandatory_next_stage") ==
            "CPU_STATIC_DIAGNOSTIC_ONLY_NO_VISUAL_CHANGE" and
            successor.get("successor_implementation_gate_required_after_diagnostic") is True and
            successor.get("automatic_scope_expansion_from_v14") is False,
            "successor_decision_invalid")

    gate = value.get("v15_diagnostic_only_gate", {})
    exact = gate.get("exact_case", {})
    require(gate.get("status") == "DESIGN_ONLY_NOT_IMPLEMENTED_NOT_RUN",
            "diagnostic_gate_status_invalid")
    require(exact == {
        "capture_id": "camera_broadcast_1080",
        "palettes": ["standard", "color_vision_safe"],
        "physical_dimensions": [1920, 1080],
        "blocking_gap_coordinates": [[783, 413], [892, 491], [940, 525]],
        "neighborhood_radius_px": 8,
        "ordered_source_count": 81,
        "ordered_source_digest":
            "33511aa46717a208f3db43acd278416b8fe8393e2e003484da8f44dabffe5645",
    }, "diagnostic_exact_case_invalid")
    require(gate.get("exact_repair_probe_ids") == REPAIR_IDS,
            "exact_repair_probe_ids_invalid")
    require(gate.get("required_predicted_fragment_classes") == FRAGMENT_CLASSES,
            "fragment_classes_invalid")
    require(gate.get("required_semantic_interval_classes") == INTERVAL_CLASSES,
            "semantic_interval_classes_invalid")
    require(len(gate.get("required_per_sample_fields", [])) == 13 and
            len(set(gate.get("required_per_sample_fields", []))) == 13,
            "per_sample_fields_invalid")
    aggregates = gate.get("required_aggregates", [])
    require(len(aggregates) == 6 and
            any("sum exactly to 81" in item for item in aggregates) and
            any("sum exactly to seven" in item for item in aggregates) and
            any("exact three repair probe IDs" in item for item in aggregates),
            "diagnostic_aggregates_invalid")
    allowed = gate.get("allowed_future_diagnostic_files", [])
    require([item.get("path") for item in allowed] == ALLOWED_DIAGNOSTIC_PATHS,
            "diagnostic_allowlist_invalid")
    require(gate.get("modern_renderer_edit_allowed") is False and
            gate.get("visual_or_raster_behavior_change_allowed") is False and
            gate.get("diagnostic_execution_authorized_by_this_addendum") is False,
            "diagnostic_no_visual_or_authorization_boundary_invalid")

    eligibility = value.get("future_narrow_candidate_eligibility", {})
    require(eligibility.get("status") == "UNPROVEN_NOT_AUTHORIZED",
            "candidate_status_invalid")
    protected = eligibility.get("always_protected_never_suppress", [])
    for required in ("core_base_body", "inner_lateral_annulus",
                     "any fragment classified dash_on_interior"):
        require(required in protected, f"protected_class_missing:{required}")
    require("INFEASIBLE_UNDER_FROZEN_SEMANTICS" in
            eligibility.get("fail_closed_outcome", ""),
            "fail_closed_infeasible_outcome_missing")

    forbidden = value.get("forbidden", [])
    require(len(forbidden) == 7, "forbidden_inventory_invalid")
    joined = "\n".join(forbidden)
    for token in ("Discard all predicted core", "core_base_body",
                  "inner_lateral_annulus", "actual tick pixel",
                  "Edit modern_renderer", "Run GPU"):
        require(token in joined, f"forbidden_token_missing:{token}")
    proof = value.get("proof_boundary", {})
    require(len(proof) == 15 and all(item is False for item in proof.values()),
            "proof_boundary_must_be_all_false")
    return errors


def main() -> int:
    path = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT
    errors = validate(path)
    if errors:
        for error in errors:
            print("FAIL: " + error, file=sys.stderr)
        return 1
    print("PASS_PHASE4_V14_CAP_MASK_PREMISE_INDEPENDENT_TA_FAILURE_ADDENDUM")
    print("v14=FAIL premise=FALSIFIED owner_partition=81+7+0=88")
    print("successor=V15_DIAGNOSTIC_ONLY visual_change=false gpu=false auth=false")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
