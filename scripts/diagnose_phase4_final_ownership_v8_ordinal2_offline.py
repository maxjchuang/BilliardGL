#!/usr/bin/env python3
"""Diagnose the consumed Phase4 v8 ordinal-2 failure from frozen files only.

This tool never launches Billiards, a capture runner, Cocoa, or OpenGL.  It
creates an evidence receipt and a design-only correction gate from the already
consumed, read-only v8 artifacts.
"""

from __future__ import annotations

from collections import Counter, defaultdict
import hashlib
import json
import math
from pathlib import Path
import stat
import statistics


ROOT = Path(__file__).resolve().parents[1]
READY = ROOT / (
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-"
    "targeted11-ready-v8")
OUTPUT = ROOT / (
    "docs/art/phase4-training-replay-overlay-v4-final-ownership-"
    "targeted11-output-v8")
OUT = ROOT / "docs/art/phase4-final-ownership-v8-ordinal2-offline-diagnostic-v1"

AUTH = READY / "root-one-shot-gpu-authorization-v8-run1.json"
CONSUMED = Path(str(AUTH) + ".consumed")
READINESS = READY / "targeted11-readiness-v8.json"
SOURCE_BINDINGS = READY / "source-bindings-v1.json"
PLAN = OUTPUT / "phase4-final-ownership-targeted11-plan.tsv"
RESULTS = OUTPUT / "results.jsonl"
TELEMETRY = OUTPUT / "results.jsonl.final-ownership.jsonl"
ORDINAL1_PPM = OUTPUT / "camera_player_low_1080.ppm"
ORDINAL2_PPM = OUTPUT / "camera_elevated_1080.ppm"
FAILURE = OUTPUT / "phase4-final-ownership-dispatch-failure-receipt-v1.json"
ALGORITHM_GATE = ROOT / (
    "docs/art/phase4-final-ownership-v7-ordinal1-offline-diagnostic-v1/"
    "minimal-owned-island-correction-design-gate-v1.json")

EXPECTED_SHA256 = {
    AUTH: "58e6e901dc1ef435cd3f0aa072267cece4aa278c6d9fa111bc38cd9bd623b1a4",
    CONSUMED: "74b135e0f7054827cb7d81bbf117fe9fa096bdfe3fee9ea9d0e7ced73046a23e",
    READINESS: "9a9d44ff3cd388e85a1e4f603b74e89d3d934f4f132ec25191dbcbc0eb5e44af",
    SOURCE_BINDINGS: "331ab39540ab49e8f44a5a8dc98a2d8e39dfdaa9af474a7446cdf3f5eeb5df3f",
    PLAN: "3b969b34e52fe2e01e31e34b148c4ce7c211682db65b3c609a43274366c8717d",
    RESULTS: "bbe1082a330f111421e8767f88c20ac9b6dda3ba945a746b6723a199c4fd84e1",
    TELEMETRY: "48d34fb11a342ffb7ff00f0729396ad48544bcfec4db43470bedd3d92148bd99",
    ORDINAL1_PPM: "f3626f44ead6cfe4729e6ff3b76168747154b2452d0ca2afd70e1520b0e09744",
    ORDINAL2_PPM: "1761d04326782eebaef2f92043d422d32f1140297bc64515a8a114d637c51da7",
    FAILURE: "1b4cac8f1df51e8b0d04a3e06770cddbf815b8a64377326f424c58e3901cfd89",
    ALGORITHM_GATE: "9f6c5febd88a06b22a8d05bf621f1310ddc8c854950dc02990cd857b21002cf2",
}

FROZEN_0444 = (
    AUTH, CONSUMED, PLAN, RESULTS, TELEMETRY, ORDINAL1_PPM, ORDINAL2_PPM,
    FAILURE,
)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def mode(path: Path) -> str:
    return f"{stat.S_IMODE(path.stat().st_mode):04o}"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def write_json(path: Path, value: dict) -> None:
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def counter(values) -> dict[str, int]:
    return dict(sorted(Counter(values).items()))


def contrast_summary(rows: list[dict]) -> dict:
    ratios = [row["edge_contrast_ratio"] for row in rows]
    return {
        "count": len(rows),
        "below_4_count": sum(value < 4.0 for value in ratios),
        "minimum": min(ratios),
        "median": statistics.median(ratios),
        "maximum": max(ratios),
    }


def bounding_box(rows: list[dict]) -> dict:
    xs = [row["physical_pixel"][0] for row in rows]
    ys = [row["physical_pixel"][1] for row in rows]
    return {"x_min": min(xs), "x_max": max(xs),
            "y_min": min(ys), "y_max": max(ys)}


def nearest_counts(source: list[dict], target: list[dict]) -> dict:
    distances = [min(math.dist(row["physical_pixel"], other["physical_pixel"])
                     for other in target) for row in source]
    return {
        "source_count": len(source),
        "minimum_px": min(distances),
        "median_px": statistics.median(distances),
        "maximum_px": max(distances),
        "within_2px_count": sum(value <= 2.0 for value in distances),
        "within_4px_count": sum(value <= 4.0 for value in distances),
        "within_8px_count": sum(value <= 8.0 for value in distances),
        "within_12px_count": sum(value <= 12.0 for value in distances),
    }


def main() -> None:
    for path, expected in EXPECTED_SHA256.items():
        require(path.is_file(), f"frozen input missing:{path}")
        require(sha(path) == expected, f"frozen input hash drift:{path}")
    for path in FROZEN_0444:
        require(mode(path) == "0444", f"frozen input mode is not 0444:{path}")

    auth = json.loads(AUTH.read_text())
    consumed_nonce = CONSUMED.read_text().strip()
    readiness = json.loads(READINESS.read_text())
    failure = json.loads(FAILURE.read_text())
    result_rows = [json.loads(line) for line in RESULTS.read_text().splitlines()
                   if line.strip()]
    telemetry_rows = [json.loads(line) for line in
                      TELEMETRY.read_text().splitlines() if line.strip()]

    require(auth["authorized"] is True and auth["single_use"] is True,
            "authorization semantics invalid")
    require(consumed_nonce == auth["authorization_nonce"],
            "consumed nonce mismatch")
    require(failure["authorization_consumed_once"] is True and
            failure["authorization_nonce"] == consumed_nonce and
            failure["status"] == "CAPTURE_FAILED_TELEMETRY_PRESERVED" and
            failure["validation_errors"] == [] and
            [failure[key] for key in ("completed_count", "failed_count",
                                      "unrun_count")] == [1, 1, 9] and
            failure["failed_capture_id"] == "camera_elevated_1080",
            "failure receipt semantics invalid")
    require(failure["results_sha256"] == EXPECTED_SHA256[RESULTS] and
            failure["telemetry_sha256"] == EXPECTED_SHA256[TELEMETRY] and
            failure["ready_manifest_sha256"] == EXPECTED_SHA256[READINESS],
            "failure receipt bindings invalid")
    require(readiness["schema"] ==
            "billiardgl.phase4.final_ownership_targeted11_ready.v8" and
            readiness["capture_count"] == 11,
            "readiness semantics invalid")
    require(len(result_rows) == 1 and
            result_rows[0]["capture_id"] == "camera_player_low_1080" and
            result_rows[0]["machine_status"] == "V4_PASS_PREFLIGHT_ONLY_PENDING_TA",
            "completed result row inventory invalid")
    require(len(telemetry_rows) == 2, "telemetry row inventory invalid")

    ordinal1, ordinal2 = telemetry_rows
    require(ordinal1["plan_ordinal_one_based"] == 1 and
            ordinal1["capture_id"] == "camera_player_low_1080" and
            ordinal1["machine_status"] == "PASS" and
            [ordinal1[key] for key in ("source_probe_count",
                                       "visible_probe_count",
                                       "hidden_probe_count")] == [95, 92, 3],
            "ordinal1 pass telemetry invalid")
    require(ordinal2["plan_ordinal_one_based"] == 2 and
            ordinal2["capture_id"] == "camera_elevated_1080" and
            ordinal2["machine_status"] == "FAILED" and
            ordinal2["machine_error_code"] ==
            "compound_visibility_below_contract",
            "ordinal2 identity/status invalid")
    require(ordinal2["authoritative_visibility_gate"] ==
            "corrected_integrated_final_semantic_ownership" and
            ordinal2["integrated_final_ownership"] is True and
            ordinal2["legacy_ppm_diagnostic_only"] is True and
            ordinal2["legacy_integrated_decision_discrepancy"] is False,
            "authoritative/legacy semantics invalid")
    require([ordinal2[key] for key in ("source_probe_count",
                                       "visible_probe_count",
                                       "hidden_probe_count")] == [83, 72, 11] and
            ordinal2["aggregate_visible_probe_coverage"] == 72 / 83 and
            ordinal2["aggregate_visible_probe_coverage"] < 0.9,
            "corrected aggregate invalid")
    require([ordinal2[key] for key in ("legacy_ppm_visible_probe_count",
                                       "legacy_ppm_hidden_probe_count")] ==
            [40, 43] and
            ordinal2["legacy_ppm_visible_probe_coverage"] == 40 / 83,
            "legacy PPM diagnostic invalid")

    predicted = ordinal2["predicted_source_visible_hidden_and_coverage"]
    actual = ordinal2["actual_source_visible_hidden_and_coverage"]
    require([predicted[key] for key in ("source", "visible", "hidden")] ==
            [71, 61, 10] and predicted["coverage"] == 61 / 71 and
            [actual[key] for key in ("source", "visible", "hidden")] ==
            [12, 11, 1] and actual["coverage"] == 11 / 12,
            "track counts invalid")

    candidates = ordinal2["ownership_candidate_records"]
    boundaries = ordinal2["component_aware_boundary_records"]
    arbitration = ordinal2["exclusive_arbitration_records"]
    hidden = ordinal2["hidden_probe_id_track_role_reason_coordinates"]
    selected = [row for row in boundaries if row["selected"]]
    require(len(candidates) == 83 and len(hidden) == 11 and
            len(selected) == 83 and
            len({row["source_probe_id"] for row in candidates}) == 83 and
            len({row["source_probe_id"] for row in selected}) == 83 and
            all(row["eligible_for_role"] for row in selected),
            "candidate/selected boundary inventory invalid")
    require(ordinal2["foreign_track_evidence_count"] == 0 and
            ordinal2["unowned_evidence_count"] == 0 and
            ordinal2["double_assignment_count"] == 0,
            "semantic ownership contamination detected")
    require(not any(row["eligibility_or_rejection_reason"].startswith("no_")
                    for row in candidates),
            "association/ownership failure unexpectedly present")

    candidate_by_id = {row["source_probe_id"]: row for row in candidates}
    selected_by_id = {row["source_probe_id"]: row for row in selected}
    hidden_details = []
    for row in hidden:
        source_id = row["source_probe_id"]
        candidate = candidate_by_id[source_id]
        boundary = selected_by_id[source_id]
        hidden_details.append({
            **row,
            "owner_track": candidate["owner_track"],
            "owner_component": candidate["owner_component"],
            "owner_primitive_id": candidate["owner_primitive_id"],
            "resolved_pixel_equivalent_area":
                candidate["resolved_pixel_equivalent_area"],
            "resolved_sample_count": candidate["resolved_sample_count"],
            "selected_association_mode": boundary["association_mode"],
            "selected_associated_owned_island_id":
                boundary["associated_owned_island_id"],
            "selected_boundary_area_owner_source_probe_id":
                boundary["boundary_area_owner_source_probe_id"],
            "selected_boundary_class": boundary["boundary_class"],
            "selected_owner_rgb_srgb8": boundary["owner_final_rgb_srgb8"],
            "selected_adjacent_rgb_srgb8":
                boundary["adjacent_final_rgb_srgb8"],
            "selected_edge_contrast_ratio": boundary["edge_contrast_ratio"],
        })

    groups: dict[tuple[str, str, str], list[dict]] = defaultdict(list)
    for row in selected:
        groups[(row["owner_track"], row["owner_component"],
                row["boundary_class"])].append(row)
    selected_contrast = {
        "record_count": len(selected),
        "unique_source_probe_count": len({row["source_probe_id"]
                                           for row in selected}),
        "selected_below_4_count": sum(row["edge_contrast_ratio"] < 4.0
                                       for row in selected),
        "serialized_contrast_failure_count": sum(
            row["reason"] == "final_edge_contrast_below_4" for row in hidden),
        "by_track_component_boundary_class": {
            "|".join(key): contrast_summary(rows)
            for key, rows in sorted(groups.items())
        },
        "below_4_records": [{
            "source_probe_id": row["source_probe_id"],
            "owner_track": row["owner_track"],
            "owner_component": row["owner_component"],
            "boundary_class": row["boundary_class"],
            "association_mode": row["association_mode"],
            "owner_rgb_srgb8": row["owner_final_rgb_srgb8"],
            "adjacent_rgb_srgb8": row["adjacent_final_rgb_srgb8"],
            "edge_contrast_ratio": row["edge_contrast_ratio"],
            "serialized_hidden_reason": next(
                item["reason"] for item in hidden
                if item["source_probe_id"] == row["source_probe_id"]),
        } for row in sorted(
            (item for item in selected if item["edge_contrast_ratio"] < 4.0),
            key=lambda item: (item["edge_contrast_ratio"],
                              item["source_probe_id"]))],
    }

    predicted_candidates = [row for row in candidates
                            if row["owner_track"] == "predicted"]
    actual_candidates = [row for row in candidates
                         if row["owner_track"] == "actual"]
    coaxis = {
        "evidence_kind": "TELEMETRY_PHYSICAL_PIXEL_PROXIMITY_NOT_SCREENSHOT_INFERENCE",
        "predicted_candidate_bounds": bounding_box(predicted_candidates),
        "actual_candidate_bounds": bounding_box(actual_candidates),
        "predicted_to_actual_nearest": nearest_counts(
            predicted_candidates, actual_candidates),
        "actual_to_predicted_nearest": nearest_counts(
            actual_candidates, predicted_candidates),
        "interpretation": (
            "The elevated case projects both tracks into the same narrow x band; "
            "all 12 actual candidates lie within 12 physical pixels of a "
            "predicted candidate. This corroborates a co-axial visual stress "
            "scene while semantic ownership remains exclusive and uncontaminated."
        ),
    }

    frozen = {
        str(path.relative_to(ROOT)): {"sha256": EXPECTED_SHA256[path],
                                      "mode": mode(path)}
        for path in FROZEN_0444
    }
    receipt = {
        "schema":
            "billiardgl.phase4.final_ownership_v8_ordinal2_offline_diagnostic.v1",
        "status":
            "VISUAL_STYLE_CONTRAST_DEFECT_ISOLATED_ALGORITHM_CLEAN_NO_CAPTURE_AUTHORIZED",
        "gpu_cocoa_opengl_invoked": False,
        "capture_retry_authorized": False,
        "new_readiness_or_authorization_created": False,
        "frozen_evidence": frozen,
        "non_frozen_binding_sha256": {
            str(READINESS.relative_to(ROOT)): EXPECTED_SHA256[READINESS],
            str(SOURCE_BINDINGS.relative_to(ROOT)):
                EXPECTED_SHA256[SOURCE_BINDINGS],
            str(ALGORITHM_GATE.relative_to(ROOT)):
                EXPECTED_SHA256[ALGORITHM_GATE],
        },
        "dispatch_outcome": {
            "status": failure["status"],
            "completed": 1,
            "failed": 1,
            "unrun": 9,
            "failed_capture_id": "camera_elevated_1080",
            "validation_errors": [],
            "authorization_consumed_once": True,
        },
        "ordinal1_completed_control": {
            "capture_id": ordinal1["capture_id"],
            "source": 95,
            "visible": 92,
            "hidden": 3,
            "coverage": 92 / 95,
            "machine_status": "PASS",
        },
        "ordinal2_authority_separation": {
            "authoritative_gate":
                "corrected_integrated_final_semantic_ownership",
            "corrected_integrated": {
                "source": 83, "visible": 72, "hidden": 11,
                "coverage": 72 / 83, "machine_pass": False,
            },
            "legacy_ppm_diagnostic_only": {
                "source": 83, "visible": 40, "hidden": 43,
                "coverage": 40 / 83, "authoritative": False,
            },
        },
        "ordinal2_track_counts": {
            "predicted": predicted,
            "actual": actual,
        },
        "ordinal2_component_counts": {
            "source": ordinal2["component_source_counts"],
            "visible": ordinal2["component_visible_counts"],
            "hidden_reason": ordinal2["component_hidden_reason_counts"],
        },
        "ownership_and_arbitration": {
            "candidate_count": len(candidates),
            "arbitration_record_count": len(arbitration),
            "contested_arbitration_record_count": sum(
                len(row["claiming_source_probe_ids"]) > 1
                for row in arbitration),
            "winner_source_probe_count": len({
                row["winner_source_probe_id"] for row in arbitration}),
            "foreign_track_evidence_count": 0,
            "unowned_evidence_count": 0,
            "double_assignment_count": 0,
            "association_or_owner_failure_count": 0,
        },
        "selected_boundary_association": {
            "selected_count": len(selected),
            "selected_unique_source_probe_count": len({
                row["source_probe_id"] for row in selected}),
            "selected_all_role_eligible": True,
            "association_mode_counts": counter(
                row["association_mode"] for row in selected),
            "boundary_class_counts": counter(
                row["boundary_class"] for row in selected),
        },
        "selected_boundary_final_ldr_contrast": selected_contrast,
        "hidden_probe_details": hidden_details,
        "screen_space_coaxial_stress": coaxis,
        "root_cause": {
            "isolated": True,
            "primary_classification":
                "PRODUCT_VISUAL_STYLE_FINAL_LDR_SILHOUETTE_CONTRAST_FAILURE",
            "scene_semantics_classification":
                "ELEVATED_COAXIAL_TRACK_PROJECTION_IS_A_VALID_STRESSOR_NOT_AN_ALGORITHM_FAILURE",
            "algorithm_classification":
                "NO_ASSOCIATION_OR_OWNERSHIP_DEFECT_DETECTED_IN_FROZEN_PAYLOAD",
            "direct_gate_reasons": {
                "final_edge_contrast_below_4": 9,
                "insufficient_owned_pixel_area": 2,
            },
            "evidence_statement": (
                "Every source probe has one selected role-eligible boundary; "
                "semantic ownership has zero foreign, unowned, or double "
                "assignment evidence. Nine serialized failures are measured "
                "final-LDR outer-silhouette contrast failures. Two small-area "
                "failures remain a projection/unique-area limitation and do "
                "not prevent a contrast-only repair from exceeding 0.90 in "
                "this analytic counterfactual."
            ),
        },
        "analytic_visual_only_counterfactual_non_final": {
            "label": "ANALYTIC_ONLY_NOT_CAPTURE_EVIDENCE_NOT_A_MACHINE_PASS",
            "assumption":
                "Only the nine serialized contrast failures are recovered; the two area failures remain hidden.",
            "source": 83,
            "visible": 81,
            "hidden": 2,
            "coverage": 81 / 83,
            "predicted_visible": 69,
            "predicted_hidden": 2,
            "actual_visible": 12,
            "actual_hidden": 0,
            "authoritative": False,
        },
    }

    OUT.mkdir(parents=True, exist_ok=False)
    receipt_path = OUT / "failure-evidence-receipt-v1.json"
    write_json(receipt_path, receipt)

    gate = {
        "schema":
            "billiardgl.phase4.final_ownership_v8_visual_style_correction_design_gate.v1",
        "status": "DESIGN_ONLY_NO_IMPLEMENTATION_NO_CAPTURE_AUTHORIZATION",
        "bound_failure_evidence_receipt_sha256": sha(receipt_path),
        "bound_v8_telemetry_sha256": EXPECTED_SHA256[TELEMETRY],
        "bound_v8_ordinal2_ppm_sha256": EXPECTED_SHA256[ORDINAL2_PPM],
        "bound_v8_dispatch_failure_receipt_sha256": EXPECTED_SHA256[FAILURE],
        "bound_v8_authorization_sha256": EXPECTED_SHA256[AUTH],
        "bound_prior_algorithm_gate_sha256": EXPECTED_SHA256[ALGORITHM_GATE],
        "evidence_basis": {
            "authoritative_corrected_visible_hidden": [72, 11],
            "authoritative_corrected_denominator": 83,
            "authoritative_corrected_coverage": 72 / 83,
            "serialized_contrast_failures": 9,
            "serialized_owned_area_failures": 2,
            "association_or_owner_failures": 0,
            "foreign_unowned_double_counts": [0, 0, 0],
        },
        "allowed_next_implementation_scope": [
            "Product overlay visual styling only: track color, halo/stroke layering, or a final-LDR-aware outer silhouette contrast rim.",
            "A predicted-track solution may add a visual outer contrast rim only if the existing core-versus-inner-halo contrast remains at least 4.0.",
            "An actual-track solution may adjust its outer halo/stroke only if track identity and semantic ownership remain unchanged.",
        ],
        "required_visual_contract": {
            "predicted_dark_background_rgb_srgb8": [15, 15, 18],
            "predicted_red_background_rgb_srgb8": [209, 31, 24],
            "predicted_teal_background_rgb_srgb8": [[25, 117, 129],
                                                     [25, 118, 129]],
            "actual_pale_background_rgb_srgb8": [231, 230, 223],
            "minimum_outer_silhouette_contrast_ratio": 4.0,
            "minimum_core_inner_halo_contrast_ratio": 4.0,
            "contrast_must_be_measured_after_resolve_blend_tone_map_and_later_ui_clip": True,
        },
        "immutable_invariants": {
            "ordinal1_source_probe_count": 95,
            "ordinal2_source_probe_count": 83,
            "ordinal2_ordered_source_probe_digest":
                ordinal2["ordered_source_probe_digest"],
            "ordinal2_source_probe_ids": ordinal2["ordered_source_probe_ids"],
            "minimum_visible_coverage": 0.9,
            "minimum_edge_contrast_ratio": 4.0,
            "canonical_plan_sha256": EXPECTED_SHA256[PLAN],
            "targeted_case_count": 11,
            "camera_preset": "elevated",
            "capture_id": "camera_elevated_1080",
            "semantic_area_ownership_and_exclusive_arbitration": "UNCHANGED",
            "exact_primitive_and_owned_island_boundary_association": "UNCHANGED",
            "probe_ids_denominator_route_offsets_z_order": "UNCHANGED",
            "layout_assets_scene_geometry_and_ui": "UNCHANGED",
        },
        "required_cpu_static_fixtures_before_any_new_readiness": [
            "Positive: predicted outer silhouette reaches >=4.0 against each frozen dark, red, and teal adjacent RGB while core-inner-halo remains >=4.0.",
            "Positive: actual outer silhouette reaches >=4.0 against the frozen pale adjacent RGB while core-inner-halo remains >=4.0.",
            "Negative: reject a single-color halo change that repairs the scene silhouette but drops core-inner-halo contrast below 4.0.",
            "Negative: reject any style path that changes track/component semantic IDs, ownership candidates, exclusive arbitration, or association provenance.",
            "Negative: reject threshold, source-probe inventory/digest, denominator, route, offset, z-order, camera, layout, asset, scene, or UI drift.",
            "Frozen v8 replay: preserve 72/11 as failure evidence; the 81/2 counterfactual must never be serialized as a machine pass.",
            "Static graph/color check: prove the style is applied in production compositing before integrated final semantic ownership is evaluated.",
        ],
        "forbidden": [
            "No boundary-association, ownership, arbitration, hidden-reason, or visibility-classification algorithm changes.",
            "No threshold, probe, denominator, route, offset, z-order, camera, layout, asset, scene, or UI changes.",
            "No promotion of legacy PPM 40/43 to authoritative evidence.",
            "No rewrite of v8 authorization, consumed marker, output, results, telemetry, PPMs, or failure receipt.",
            "No GPU, Cocoa, or OpenGL execution under this design gate.",
        ],
        "implementation_authorized": False,
        "capture_authorized": False,
        "technical_art_review_authorized": False,
        "formal68_authorized": False,
        "exit_condition": (
            "A later explicitly authorized implementation must first pass all "
            "CPU/static positive and negative fixtures, frozen v8 read-only "
            "replay, and source/binary binding checks. A fresh targeted11 "
            "readiness may then be prepared as NOT_RUN only; capture still "
            "requires a separate one-shot authorization."
        ),
    }
    write_json(OUT / "minimal-visual-style-correction-design-gate-v1.json",
               gate)

    print(json.dumps({
        "schema":
            "billiardgl.phase4.final_ownership_v8_offline_diagnostic_generation.v1",
        "status": receipt["status"],
        "gpu_cocoa_opengl_invoked": False,
        "capture_retry_authorized": False,
        "artifact_dir": str(OUT),
    }, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
