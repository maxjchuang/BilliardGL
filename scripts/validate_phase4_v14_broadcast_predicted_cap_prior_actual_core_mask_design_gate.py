#!/usr/bin/env python3
"""Fail-closed validator for the Phase4 v14 midpoint actual-core mask gate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GATE = ROOT / (
    "docs/art/phase4-v14-broadcast-predicted-cap-prior-actual-core-mask-"
    "design-gate-v1.json"
)
BINDINGS = [
    ("docs/art/phase4-v13-broadcast-predicted-dash-cap-release-design-gate-v1.json", "da26e980cf5fb365c37473d74025e03122ed99d9c7328482ce325a9cbf3e097d"),
    ("docs/art/phase4-v13-primary-equal-cap-contraction-exhaustion-v1/primary-exhaustion-receipt-v1.json", "6055242d80be491c1cda05f5283cf92aa301a26e33c5f0b436b4ce5c04bf811d"),
    ("docs/art/phase4-v11-raster-support-connectivity-implementation-v1/algorithm-implementation-receipt-v1.json", "abc181f48adb8e94db31965c2d0111c7641c0ac20242dd4a93189a237c8d01f2"),
    ("docs/art/phase4-final-ownership-v9-ordinal1-offline-diagnostic-v1/minimal-compositing-style-correction-design-gate-v1.json", "e80baf05f0240872ba24b9ca9a6c7daa0d5a51496dde99902014d8ea3391a4f6"),
    ("src/Billiards/training/trajectory_overlay_renderer.cpp", "0c108c506641ab4828290a1a4c7b6b5ebbd51ab4fc7c12845bde0f5deb18a840"),
    ("src/Billiards/training/trajectory_overlay_renderer.h", "9805e6a5f56c17d7645200576505425cddf0d642970e5dc5cec186e003bd564e"),
    ("src/Billiards/render/modern/modern_renderer.cpp", "b00cc6b370122275a42f106967f935dbb34bd5d3f81f2a750ca9ba6e503af191"),
    ("src/Billiards/render/modern/modern_renderer.h", "936de5b77c9e3485f2e51f10e9de26c59c8ec94674865e0b50ae125725183c9d"),
]
ALLOWED_PRODUCTION_FILES = [path for path, _ in BINDINGS[4:8]]
FIXTURE_IDS = [
    "tag_scope_exact", "geometry_union_identity", "actual_core_positive",
    "actual_halo_negative", "scene_color_negative", "predicted_core_negative",
    "lateral_annulus_negative", "no_actual_negative",
    "msaa_any_sample_positive", "stale_or_invalid_mask_fail_closed",
    "three_gap_and_cvd_positive", "controls_and_exact_ids",
    "silhouette_and_order_regression", "resource_lifecycle_and_budget",
]
FALSE_FIELDS = [
    "production_modified_by_this_gate",
    "renderer_only_geometry_fallback_approved", "modern_mask_implemented",
    "cpu_static_fixtures_run", "readiness_created",
    "authorization_created_or_issued", "gpu_authorized_or_executed",
    "machine_pass", "ta_pass", "targeted5_pass", "targeted11_pass",
    "formal68_pass", "threshold_changed", "product_exposed",
    "golden_promotion", "m2_promotion", "beta_pass",
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


def validate_bindings(gate: dict[str, Any]) -> None:
    rows = gate.get("frozen_bindings", [])
    require([(row.get("path"), row.get("sha256")) for row in rows] == BINDINGS,
            "exact ordered bindings")
    for path, digest in BINDINGS:
        candidate = ROOT / path
        require(candidate.is_file() and sha256(candidate) == digest,
                "binding missing/drift: " + path)
    primary = load_json(BINDINGS[1][0])
    require(primary.get("status") ==
            "PRIMARY_EXHAUSTED_FAIL_CLOSED_FALLBACK_NOT_IMPLEMENTED"
            and [row.get("core_cap_px") for row in
                 primary.get("ordered_primary_sweep", [])] == [1.5, 1.25, 1.0]
            and all(row.get("result") ==
                    "FAIL_ACTUAL_CORE_1_PIXEL_AT_940_525" for row in
                    primary.get("ordered_primary_sweep", []))
            and primary.get("fallback_boundary", {}).get("fallback_implemented") is False
            and primary.get("authorization", {}).get("gpu_capture_authorized") is False,
            "v13 primary exact exhaustion/no fallback")
    local_binding = primary.get("bindings", {}).get("v12_exhaustion_receipt", {})
    require(local_binding == {
        "path": "docs/art/phase4-v12-visual-local-alternatives-exhaustion-v1/exhaustion-evidence-receipt-v1.json",
        "sha256": "62b9120252f9a224e965cb3815ebbcce4a4854019e75a5e4b0d80b7f0b563f97",
    }, "primary receipt transitively binds v12 local exhaustion")
    local_path = ROOT / local_binding["path"]
    require(local_path.is_file() and sha256(local_path) == local_binding["sha256"],
            "transitive v12 local exhaustion receipt drift")
    local = load_json(local_path)
    require(local.get("status") == "FAIL_CLOSED_LOCAL_ALTERNATIVES_EXHAUSTED"
            and local.get("root_cause_boundary", {}).get("classification") ==
            "tangent_gap_last_draw_competition_not_normal_width_or_internal_join_crack",
            "v12 local exhaustion")
    algorithm = load_json(BINDINGS[2][0])
    require(algorithm.get("status") ==
            "PASS_ALGORITHM_SOURCE_STABLE_WAIT_VISUAL_GATE"
            and algorithm.get("post_algorithm_source_aggregate_sha256") ==
            "ea72f28dbcb9755275f1b12763e1fd8f9a41ac90806564d283083c26859686d8",
            "algorithm source stable")


def validate_feasibility(gate: dict[str, Any]) -> None:
    result = gate.get("feasibility_conclusion", {})
    require(result == {
        "trajectory_renderer_two_file_cpu_geometry_only":
            "NOT_FEASIBLE_FAIL_CLOSED",
        "reason": result.get("reason"),
        "current_v13_allowlist_sufficient": False,
        "modern_renderer_midpoint_actual_core_mask_required": True,
        "default_scope_expansion_permitted": False,
        "new_gate_required_and_this_file_is_that_design_only_gate": True,
    } and "MSAA/sample-resolved prior owner" in result.get("reason", "")
      and "actual halo" in result.get("reason", ""),
      "renderer-only infeasible/new gate required")
    failure = gate.get("bound_failure", {})
    require(failure.get("coordinate") == [940, 525]
            and failure.get("standard_actual_core_pixels_and_largest8") == [1, 1]
            and failure.get("cvd_actual_core_pixels_and_largest8") == [1, 1]
            and failure.get("required_actual_core_minimum_area") == 1.5
            and failure.get("required_actual_core_largest8") == 2
            and failure.get("primary_cap_candidates_exhausted") == [1.5, 1.25, 1.0]
            and failure.get("actual_only_local_candidates_exhausted") is True
            and failure.get("fallback_implemented") is False
            and failure.get("machine_pass") is False
            and failure.get("ta_pass") is False,
            "exact blocked failure")


def validate_implementation_contract(gate: dict[str, Any]) -> None:
    contract = gate.get("minimum_implementable_contract", {})
    tagging = "\n".join(contract.get("trajectory_package_tagging", []))
    pipeline = "\n".join(contract.get("actual_core_mask_pipeline", []))
    behavior = "\n".join(contract.get("resulting_behavior", []))
    require(len(contract.get("trajectory_package_tagging", [])) == 4
            and "overlayMaskRole enum" in tagging
            and "ActualRouteBodyCore" in tagging
            and "PredictedInnerCap" in tagging
            and "2px start/end cap-annulus" in tagging
            and "semantic batch/token/source identity" in tagging,
            "exact package tagging contract")
    for needle in ("GL_R8UI or GL_R32UI", "blending disabled",
                   "explicit integer any-sample/max resolve",
                   "never rely on normalized-color averaging",
                   "discard a fragment before both color and ownership-token writes",
                   "exactly two trajectory color draws"):
        require(needle in pipeline, "missing mask pipeline: " + needle)
    require("Actual core under a predicted inner-halo cap fragment" in behavior
            and "Predicted core still replaces actual core" in behavior
            and "Actual halo/rim" in behavior
            and "two repaired predicted silhouette probes" in behavior,
            "bounded resulting behavior")


def validate_scope(gate: dict[str, Any]) -> None:
    allowed = gate.get("allowed_production_files", [])
    require([row.get("path") for row in allowed] == ALLOWED_PRODUCTION_FILES
            and all(row.get("allowed_symbols_or_regions") for row in allowed),
            "exact four-file allowlist")
    forbidden = gate.get("forbidden_changes", [])
    joined = "\n".join(forbidden)
    require(len(forbidden) == 9
            and "four allowed production files" in joined
            and "HDR/RGB/color-equality" in joined
            and "predicted core discard" in joined
            and "dash on/off/period/phase/center" in joined
            and "exhausted v12/v13" in joined
            and "extra trajectory color draw" in joined
            and "source ID/count/order/coordinate" in joined
            and "stale mask reuse" in joined
            and "No product exposure flag" in joined,
            "strict mask/scope/order/identity/exposure prohibitions")


def validate_fixtures(gate: dict[str, Any]) -> None:
    group = gate.get("required_cpu_and_static_negative_fixtures", {})
    fixtures = group.get("fixtures", [])
    require(group.get("status") == "NOT_RUN"
            and [row.get("id") for row in fixtures] == FIXTURE_IDS
            and all(row.get("requirement") for row in fixtures),
            "exact fourteen NOT_RUN fixtures")
    joined = "\n".join(row["requirement"] for row in fixtures)
    for needle in ("ActualRouteBodyCore", "PredictedInnerCap", "<=8192",
                   "actual halo", "arbitrary scene RGB", "predicted yellow",
                   "lateral 2px annulus", "no unconditional cap hole",
                   "1x and 4x", "instrumentation on/off", "wrong-format",
                   "[783,413]", "[892,491]", "[940,525]",
                   "78 inherited visible IDs", "[855,469]", "[879,486]",
                   "exactly two trajectory color draws", "context loss"):
        require(needle in joined, "missing negative fixture: " + needle)
    require("does not authorize GPU capture" in group.get("implementation_exit", ""),
            "fixture exit remains no GPU")


def validate_boundary(gate: dict[str, Any]) -> None:
    future = gate.get("future_targeted_boundary", {})
    require(future.get("status") == "NOT_AUTHORIZED_NOT_RUN"
            and len(future.get("minimum_cases", [])) == 5
            and future.get("requires_separate_implementation_receipt") is True
            and future.get("requires_separate_readiness_and_single_use_authorization") is True
            and future.get("machine_five_of_five_before_ta") is True
            and future.get("ta_axes") == 35
            and future.get("targeted5_is_not_targeted11_or_formal68") is True
            and future.get("product_or_beta_acceptance") is False,
            "future target boundary")
    boundary = gate.get("proof_boundary", {})
    require(boundary.get("design_gate_only") is True, "design only")
    for field in FALSE_FIELDS:
        require(boundary.get(field) is False, "proof boundary false: " + field)


def validate(path: Path | str = DEFAULT_GATE) -> dict[str, Any]:
    gate = load_json(path)
    require(gate.get("schema") ==
            "billiardgl.phase4.v14.broadcast_predicted_cap_prior_actual_core_mask_design_gate.v1"
            and gate.get("schema_version") == 1
            and gate.get("status") ==
            "DESIGN_ONLY_NOT_IMPLEMENTED_NO_GPU_NO_AUTHORIZATION",
            "schema/status")
    validate_bindings(gate)
    validate_feasibility(gate)
    validate_implementation_contract(gate)
    validate_scope(gate)
    validate_fixtures(gate)
    validate_boundary(gate)
    return {
        "status": "PASS_PHASE4_V14_PRIOR_ACTUAL_CORE_MASK_DESIGN_GATE",
        "binding_count": len(BINDINGS),
        "allowed_production_file_count": len(ALLOWED_PRODUCTION_FILES),
        "negative_fixture_count": len(FIXTURE_IDS),
        "renderer_only_feasible": False,
        "modern_mask_required": True,
        "cpu_static_fixtures": "NOT_RUN",
        "production_modified": False,
        "gpu_authorized": False,
        "authorization_created": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--gate", type=Path, default=DEFAULT_GATE)
    args = parser.parse_args()
    try:
        print(json.dumps(validate(args.gate), sort_keys=True))
        return 0
    except (OSError, ValueError, RuntimeError, KeyError) as failure:
        print(json.dumps({"status": "FAIL", "error": str(failure)}, sort_keys=True))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
