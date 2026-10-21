#!/usr/bin/env python3
"""Fail-closed validator for the Phase4 v13 predicted dash-cap design gate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GATE = ROOT / (
    "docs/art/phase4-v13-broadcast-predicted-dash-cap-release-"
    "design-gate-v1.json"
)
BINDINGS = [
    ("docs/art/phase4-v12-broadcast-projected-trajectory-visual-repair-design-gate-v1.json", "0fe6d893cd8fa1130e93c975be8e297802749b6515eb4f4e65b7d9af2006b8d2"),
    ("docs/art/phase4-v12-visual-local-alternatives-exhaustion-v1/exhaustion-evidence-receipt-v1.json", "62b9120252f9a224e965cb3815ebbcce4a4854019e75a5e4b0d80b7f0b563f97"),
    ("docs/art/phase4-v11-raster-support-connectivity-implementation-v1/algorithm-implementation-receipt-v1.json", "abc181f48adb8e94db31965c2d0111c7641c0ac20242dd4a93189a237c8d01f2"),
    ("docs/art/phase4-final-ownership-v9-ordinal1-offline-diagnostic-v1/minimal-compositing-style-correction-design-gate-v1.json", "e80baf05f0240872ba24b9ca9a6c7daa0d5a51496dde99902014d8ea3391a4f6"),
    ("docs/art/phase4-final-ownership-v11-ordinal4-offline-diagnostic-v1/failure-evidence-receipt-v1.json", "07f251d466b0dcad550b4951e8242403bcaaaeb97dd800cbde99cdf23c2f3f85"),
    ("docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v11/results.jsonl.final-ownership.jsonl", "4e7e884569eec1197ef11d39d256e2e43eef11c49ee7c84ff150b93c3c6d61ef"),
]
ALLOWED_PRODUCTION_FILES = [
    "src/Billiards/training/trajectory_overlay_renderer.cpp",
    "src/Billiards/training/trajectory_overlay_renderer.h",
]
REPAIR_IDS = [
    "d6d850a1a10c7c7ce403e6de4afd4330e4c6e89b9100c72f4c23fbab1f6f978b",
    "1890fd0652c8f74a8b03a35a6cee1ad2cab43062987cab73efb1f6b455da468a",
    "1bf60bcd841a0d754c82876b55a4d3d6c29ebad582e069d4f4a608a4778c33a9",
]
CONTROL_ROWS = {
    "camera_player_low_1080": (95, 89, "d533924980efc85aeb94e4f1d31831d93e2bfce744be3b66481d08c56d10289a", "4bfe20cedf200e827dc68aca9a7e67ca473e05d1d3d6f2357e320c2009ac4fe0"),
    "camera_elevated_1080": (83, 80, "a3314c4ad04a72bae1718b290129604067954c16685d1d7573726caeb6d5b3f9", "7e426c475bb80b5a8e10f55edf22ea724dc109ed625001c349d7f082e37f54b3"),
    "camera_overhead_1080": (87, 84, "7b3a0a611d4f47c00bbc77c42244dd3c5615e5d2c3ab8a0074c6791d50b677f0", "ffdfb8ce651c2eef0edf1b2d049ce54a4c32f199ea04febc0af4ba44179c2e29"),
}
FALSE_FIELDS = [
    "production_modified_by_this_gate", "cap_repair_implemented",
    "cpu_static_preflight_run", "readiness_created",
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
            "exact ordered frozen bindings")
    for path, digest in BINDINGS:
        candidate = ROOT / path
        require(candidate.is_file() and sha256(candidate) == digest,
                "binding missing/drift: " + path)

    exhaustion = load_json(BINDINGS[1][0])
    require(exhaustion.get("status") ==
            "FAIL_CLOSED_LOCAL_ALTERNATIVES_EXHAUSTED"
            and exhaustion.get("root_cause_boundary", {}).get("classification") ==
            "tangent_gap_last_draw_competition_not_normal_width_or_internal_join_crack"
            and exhaustion.get("authorization", {}).get("gpu_capture_authorized") is False,
            "bound exhaustion receipt/result")
    restored = exhaustion.get("bindings", {})
    for key, digest in (
        ("restored_renderer_cpp", "0c108c506641ab4828290a1a4c7b6b5ebbd51ab4fc7c12845bde0f5deb18a840"),
        ("restored_renderer_h", "9805e6a5f56c17d7645200576505425cddf0d642970e5dc5cec186e003bd564e"),
        ("cpu_test_source", "cb1578c37f9b228e521a6d5450f6f290bf55dde374e8cafd3586874059e5029d"),
    ):
        require(restored.get(key, {}).get("sha256") == digest,
                "exhaustion restored hash: " + key)

    algorithm = load_json(BINDINGS[2][0])
    replay = algorithm.get("validation", {}).get("frozen_v11_analytic_replay", {})
    require(algorithm.get("status") ==
            "PASS_ALGORITHM_SOURCE_STABLE_WAIT_VISUAL_GATE"
            and replay.get("source_visible_hidden") == [81, 78, 3]
            and replay.get("predicted_source_visible_hidden") == [69, 67, 2]
            and replay.get("actual_source_visible_hidden") == [12, 11, 1]
            and replay.get("analytic_only_not_machine_pass") is True,
            "algorithm receipt exact 78+3 analytic boundary")


def validate_exhaustion_and_repairs(gate: dict[str, Any]) -> None:
    facts = gate.get("bound_exhaustion_facts", {})
    require(facts.get("blocking_coordinate") == [940, 525]
            and facts.get("standard_actual_core_pixels_and_largest8") == [1, 1]
            and facts.get("cvd_actual_core_pixels_and_largest8") == [1, 1]
            and facts.get("exhausted_actual_internal_overlap_px") == [0.75, 1.0, 1.25, 1.5]
            and facts.get("exhausted_actual_route_body_width_px") == [5.0, 5.5, 6.0]
            and facts.get("restored_actual_width_px") == 4.5
            and facts.get("restored_actual_internal_overlap_px") == 0.75
            and facts.get("actual_halo_pixels_may_not_substitute_for_actual_core") is True
            and facts.get("current_machine_pass") is False
            and facts.get("current_ta_pass") is False,
            "exact exhausted failure facts")
    repairs = gate.get("exact_repair_ids", {})
    require(repairs.get("algorithm_replay_inherited_visible_count") == 78
            and repairs.get("predicted_contrast_repair_probe_ids") == REPAIR_IDS[:2]
            and repairs.get("actual_connectivity_repair_probe_id") == REPAIR_IDS[2]
            and repairs.get("required_union_count") == 81
            and repairs.get("required_predicted_visible_count") == 69
            and repairs.get("required_actual_visible_count") == 12,
            "exact 78 plus three repair IDs")


def validate_cap_contract(gate: dict[str, Any]) -> None:
    sweep = gate.get("primary_equal_cap_contraction_sweep", {})
    baseline = sweep.get("baseline", {})
    candidates = sweep.get("ordered_candidates", [])
    expected = [(1.25, 3.25, 3.25, 2.0), (1.0, 3.0, 3.0, 2.0)]
    actual = [(row.get("predicted_core_cap_extension_px"),
               row.get("predicted_inner_cap_extension_px"),
               row.get("predicted_outer_cap_extension_px"),
               row.get("core_to_inner_annulus_px")) for row in candidates]
    require((baseline.get("predicted_core_cap_extension_px"),
             baseline.get("predicted_inner_cap_extension_px"),
             baseline.get("predicted_outer_cap_extension_px"),
             baseline.get("core_to_inner_annulus_px")) == (1.5, 3.5, 3.5, 2.0)
            and actual == expected
            and sweep.get("outer_rim_side_only") is True
            and sweep.get("candidate_must_be_minimal") is True,
            "exact equal cap contraction sweep")
    pattern = sweep.get("unchanged_center_pattern", {})
    require(pattern == {"dash_on_px": 18.0, "dash_off_px": 12.0,
                        "period_px": 30.0,
                        "dash_phase_and_center_positions_unchanged": True},
            "unchanged dash center rhythm")
    fallback = gate.get("fallback_equivalent_cap_policy", {})
    require(fallback.get("status") ==
            "DISALLOWED_UNLESS_BOTH_EQUAL_CONTRACTION_CANDIDATES_FAIL"
            and fallback.get("must_be_cap_only") is True
            and fallback.get("must_not_change_draw_order") is True
            and fallback.get("must_not_use_color_only_detection") is True
            and fallback.get("requires_renderer_tagged_prior_actual_core_coverage") is True
            and fallback.get("cannot_be_combined_with_equal_cap_contraction") is True
            and fallback.get("requires_separate_cpu_receipt_and_root_approval") is True,
            "strict mutually exclusive fallback")


def validate_scope(gate: dict[str, Any]) -> None:
    allowed = gate.get("allowed_production_files", [])
    require([row.get("path") for row in allowed] == ALLOWED_PRODUCTION_FILES
            and all(row.get("allowed_symbols_or_regions") for row in allowed),
            "exact renderer-only production allowlist")
    forbidden = gate.get("forbidden_changes", [])
    joined = "\n".join(forbidden)
    require(len(forbidden) == 8
            and "acceptance-threshold relaxation" in joined
            and "dash on/off length" in joined
            and "exhausted v12 actual-only" in joined
            and "z-order" in joined
            and "source probe ID/count/order/coordinates" in joined
            and "2px predicted core-to-inner annulus" in joined
            and "two repaired silhouette probes" in joined
            and "No product exposure" in joined,
            "forbidden thresholds/rhythm/actual-retry/z-order/identity/exposure")


def validate_controls() -> None:
    rows = [json.loads(line) for line in
            (ROOT / BINDINGS[5][0]).read_text(encoding="utf-8").splitlines()
            if line.strip()]
    indexed = {row["capture_id"]: row for row in rows}
    for capture_id, (source, visible, source_digest, visible_digest) in CONTROL_ROWS.items():
        row = indexed[capture_id]
        require(row.get("source_probe_count") == source
                and row.get("visible_probe_count") == visible
                and row.get("ordered_source_probe_digest") == source_digest
                and row.get("ordered_visible_probe_digest") == visible_digest
                and row.get("actual_source_visible_hidden_and_coverage", {}).get("coverage") == 1,
                "frozen v11 control row: " + capture_id)


def validate_preflight(gate: dict[str, Any]) -> None:
    preflight = gate.get("required_cpu_and_static_preflight", {})
    checks = preflight.get("required_checks", [])
    joined = "\n".join(checks)
    require(preflight.get("status") == "NOT_RUN" and len(checks) == 11,
            "eleven NOT_RUN checks")
    for needle in ("1.5 baseline, 1.25 candidate, then 1.0 candidate",
                   "innerCap-coreCap is exactly 2px", "[783,413]",
                   "[892,491]", "[940,525]", "exact ordered 78",
                   "[855,469]", "[879,486]", "player 95 source/at least 89",
                   "v9 compositing", "<=8192", "No readiness"):
        require(needle in joined, "missing preflight requirement: " + needle)
    require("does not authorize GPU capture" in preflight.get("implementation_exit", ""),
            "implementation exit no GPU")


def validate_boundary(gate: dict[str, Any]) -> None:
    future = gate.get("future_targeted_acceptance", {})
    require(future.get("status") == "NOT_AUTHORIZED_NOT_RUN"
            and len(future.get("minimum_cases", [])) == 5
            and future.get("targeted5_is_not_targeted11_or_formal68") is True
            and future.get("product_or_beta_acceptance") is False,
            "future targeted boundary")
    boundary = gate.get("proof_boundary", {})
    require(boundary.get("design_gate_only") is True, "design only")
    for field in FALSE_FIELDS:
        require(boundary.get(field) is False, "boundary false: " + field)


def validate(path: Path | str = DEFAULT_GATE) -> dict[str, Any]:
    gate = load_json(path)
    require(gate.get("schema") ==
            "billiardgl.phase4.v13.broadcast_predicted_dash_cap_release_design_gate.v1"
            and gate.get("schema_version") == 1
            and gate.get("status") ==
            "DESIGN_ONLY_NOT_IMPLEMENTED_NO_GPU_NO_AUTHORIZATION",
            "schema/status")
    validate_bindings(gate)
    validate_exhaustion_and_repairs(gate)
    validate_cap_contract(gate)
    validate_scope(gate)
    validate_controls()
    validate_preflight(gate)
    validate_boundary(gate)
    return {
        "status": "PASS_PHASE4_V13_PREDICTED_DASH_CAP_RELEASE_DESIGN_GATE",
        "binding_count": len(BINDINGS),
        "candidate_count": 2,
        "repair_id_count": 3,
        "control_count": len(CONTROL_ROWS),
        "cpu_static_preflight": "NOT_RUN",
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
