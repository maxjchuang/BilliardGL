#!/usr/bin/env python3
"""Freeze and diagnose the consumed Phase4 v9 ordinal-1 failure offline only."""

from __future__ import annotations

from collections import Counter
import hashlib
import json
import re
from pathlib import Path
import stat
import statistics


ROOT = Path(__file__).resolve().parents[1]
READY = ROOT / ("docs/art/phase4-training-replay-overlay-v4-final-ownership-"
                "targeted11-ready-v9")
OUTPUT = ROOT / ("docs/art/phase4-training-replay-overlay-v4-final-ownership-"
                 "targeted11-output-v9")
OUT = ROOT / "docs/art/phase4-final-ownership-v9-ordinal1-offline-diagnostic-v1"
AUTH = READY / "root-one-shot-gpu-authorization-v9-run1.json"
CONSUMED = Path(str(AUTH) + ".consumed")
READINESS = READY / "targeted11-readiness-v9.json"
SOURCE_BINDINGS = READY / "source-bindings-v1.json"
PLAN = OUTPUT / "phase4-final-ownership-targeted11-plan.tsv"
RESULTS = OUTPUT / "results.jsonl"
TELEMETRY = OUTPUT / "results.jsonl.final-ownership.jsonl"
PPM = OUTPUT / "camera_player_low_1080.ppm"
FAILURE = OUTPUT / "phase4-final-ownership-dispatch-failure-receipt-v1.json"
V8_TELEMETRY = ROOT / ("docs/art/phase4-training-replay-overlay-v4-final-"
                       "ownership-targeted11-output-v8/"
                       "results.jsonl.final-ownership.jsonl")
V8_VISUAL_GATE = ROOT / ("docs/art/phase4-final-ownership-v8-ordinal2-"
                         "offline-diagnostic-v1/"
                         "minimal-visual-style-correction-design-gate-v1.json")
RENDERER = ROOT / "src/Billiards/training/trajectory_overlay_renderer.cpp"
RENDERER_HEADER = ROOT / "src/Billiards/training/trajectory_overlay_renderer.h"

EXPECTED = {
    AUTH: "67d06a6f7a8cd25fb38367beb0e8b975eeaf8dd6905dc250ec5ebb511b2d08c0",
    CONSUMED: "2b20f90a76309fa425264580582f020b94120f0cb102af0949f18516e983e66e",
    READINESS: "c3057b9147b232935c29a0a1f046511e53b8e5c97b426822c9c0325d45f196ab",
    SOURCE_BINDINGS: "8718bf638c96be25cc0a54aa204086436352fd7af6ce9b9a7cae6bd41a5ddb12",
    PLAN: "3b969b34e52fe2e01e31e34b148c4ce7c211682db65b3c609a43274366c8717d",
    RESULTS: "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    TELEMETRY: "3f40581fc4f66861a3a5f0953d6350619ca3195ad0c560f1202c5458dd66f62a",
    PPM: "90db92505afaf6d6c2d74193dee646afb73515a42acb3d25fa32b8034d84686c",
    FAILURE: "597bbf92be859cf0d4e37cbf677140bdafa286cde59412077a75416fcf035f0a",
    V8_TELEMETRY: "48d34fb11a342ffb7ff00f0729396ad48544bcfec4db43470bedd3d92148bd99",
    V8_VISUAL_GATE: "0fe967c8ca0e855c6cb0cd29f745aefd7dfcf8a9ae387eb1522d254753196526",
    RENDERER: "3e652402ab8cd5603defb0c54b9ff0951b7d5a292bcc81f9b4429f3877e0aa25",
    RENDERER_HEADER: "3918037812b177d4713bb4f99ee6ae96d598e023ff8e2f40afceea749d81cec1",
}
FROZEN_0444 = (AUTH, CONSUMED, PLAN, RESULTS, TELEMETRY, PPM, FAILURE)


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


def primitive_stem(value: str) -> str:
    return re.sub(r"_(core|halo)$", "", value)


def main() -> None:
    for path, expected in EXPECTED.items():
        require(path.is_file() and sha(path) == expected,
                f"bound input hash drift:{path}")
    for path in FROZEN_0444:
        require(mode(path) == "0444", f"frozen mode is not 0444:{path}")
    require(mode(OUTPUT) == "0555", "frozen output directory mode is not 0555")

    auth = json.loads(AUTH.read_text())
    nonce = CONSUMED.read_text().strip()
    readiness = json.loads(READINESS.read_text())
    failure = json.loads(FAILURE.read_text())
    rows = [json.loads(line) for line in TELEMETRY.read_text().splitlines()
            if line.strip()]
    v8_rows = [json.loads(line) for line in V8_TELEMETRY.read_text().splitlines()
               if line.strip()]
    require(auth.get("authorized") is True and auth.get("single_use") is True and
            nonce == auth.get("authorization_nonce"),
            "consumed authorization semantics invalid")
    require(readiness.get("schema") ==
            "billiardgl.phase4.final_ownership_targeted11_ready.v9",
            "v9 readiness semantics invalid")
    require(failure.get("status") == "CAPTURE_FAILED_TELEMETRY_PRESERVED" and
            failure.get("authorization_consumed_once") is True and
            failure.get("authorization_nonce") == nonce and
            [failure.get(key) for key in
             ("completed_count", "failed_count", "unrun_count")] == [0, 1, 10] and
            failure.get("failed_capture_id") == "camera_player_low_1080" and
            failure.get("validation_errors") == [] and
            failure.get("results_sha256") == EXPECTED[RESULTS] and
            failure.get("telemetry_sha256") == EXPECTED[TELEMETRY] and
            failure.get("ready_manifest_sha256") == EXPECTED[READINESS],
            "failure receipt semantics invalid")
    require(RESULTS.stat().st_size == 0 and len(rows) == 1,
            "frozen result/telemetry inventory invalid")
    row = rows[0]
    v8 = v8_rows[0]
    require(row.get("plan_ordinal_one_based") == 1 and
            row.get("capture_id") == "camera_player_low_1080" and
            row.get("machine_status") == "FAILED" and
            row.get("machine_error_code") == "compound_visibility_below_contract" and
            row.get("authoritative_visibility_gate") ==
            "corrected_integrated_final_semantic_ownership" and
            row.get("integrated_final_ownership") is True and
            row.get("legacy_ppm_diagnostic_only") is True,
            "v9 ordinal1 authority semantics invalid")
    require([row[key] for key in ("source_probe_count", "visible_probe_count",
                                  "hidden_probe_count")] == [95, 66, 29] and
            row["aggregate_visible_probe_coverage"] == 66 / 95 and
            [row[key] for key in ("legacy_ppm_visible_probe_count",
                                  "legacy_ppm_hidden_probe_count")] == [47, 48],
            "v9 corrected/legacy counts invalid")
    require(v8.get("machine_status") == "PASS" and
            [v8[key] for key in ("source_probe_count", "visible_probe_count",
                                 "hidden_probe_count")] == [95, 92, 3] and
            v8["ordered_source_probe_ids"] == row["ordered_source_probe_ids"] and
            v8["ordered_source_probe_digest"] == row["ordered_source_probe_digest"] ==
            "d533924980efc85aeb94e4f1d31831d93e2bfce744be3b66481d08c56d10289a",
            "v8 comparison/source inventory invalid")

    candidates = row["ownership_candidate_records"]
    boundaries = row["component_aware_boundary_records"]
    arbitration = row["exclusive_arbitration_records"]
    hidden = row["hidden_probe_id_track_role_reason_coordinates"]
    selected = [value for value in boundaries if value["selected"]]
    require(len(candidates) == len(selected) == 95 and len(hidden) == 29 and
            len(arbitration) == 6552 and
            len({value["source_probe_id"] for value in selected}) == 95 and
            all(value["eligible_for_role"] for value in selected) and
            all(value["eligibility_or_rejection_reason"] in {
                "eligible", "final_edge_contrast_below_4"}
                for value in candidates) and
            [row[key] for key in ("foreign_track_evidence_count",
                                  "unowned_evidence_count",
                                  "double_assignment_count")] == [0, 0, 0],
            "ownership/association inventory invalid")

    candidate_by_id = {value["source_probe_id"]: value for value in candidates}
    selected_by_id = {value["source_probe_id"]: value for value in selected}
    hidden_details = []
    for value in hidden:
        source_id = value["source_probe_id"]
        candidate = candidate_by_id[source_id]
        boundary = selected_by_id[source_id]
        hidden_details.append({
            **value,
            "owner_track": candidate["owner_track"],
            "owner_component": candidate["owner_component"],
            "owner_primitive_id": candidate["owner_primitive_id"],
            "resolved_sample_count": candidate["resolved_sample_count"],
            "resolved_pixel_equivalent_area":
                candidate["resolved_pixel_equivalent_area"],
            "selected_association_mode": boundary["association_mode"],
            "selected_boundary_class": boundary["boundary_class"],
            "selected_owner_primitive_id": boundary["owner_primitive_id"],
            "selected_adjacent_primitive_id": boundary["adjacent_primitive_id"],
            "selected_owner_rgb_srgb8": boundary["owner_final_rgb_srgb8"],
            "selected_adjacent_rgb_srgb8": boundary["adjacent_final_rgb_srgb8"],
            "selected_edge_contrast_ratio": boundary["edge_contrast_ratio"],
            "selected_boundary_area_owner_source_probe_id":
                boundary["boundary_area_owner_source_probe_id"],
            "selected_associated_owned_island_id":
                boundary["associated_owned_island_id"],
            "same_primitive_stem": primitive_stem(boundary["owner_primitive_id"]) ==
                primitive_stem(boundary["adjacent_primitive_id"]),
        })

    inner = [value for value in hidden_details if
             value["selected_boundary_class"] ==
             "same_track_compound_inner_edge"]
    outer = [value for value in hidden_details if
             value["selected_boundary_class"] == "compound_outer_silhouette"]
    require(len(inner) == 14 and len(outer) == 15 and
            sum(value["track"] == "predicted" and
                value["component_role"] == "core" and
                value["selected_owner_rgb_srgb8"] == [255, 253, 112] and
                value["selected_adjacent_rgb_srgb8"] == [255, 255, 255]
                for value in inner) == 12 and
            sum(value["track"] == "actual" and
                value["component_role"] == "tick" and
                value["selected_owner_rgb_srgb8"] == [2, 21, 31] and
                value["selected_adjacent_rgb_srgb8"] == [0, 0, 0]
                for value in inner) == 2 and
            sum(value["same_primitive_stem"] for value in inner) == 4,
            "inner-layer overwrite evidence invalid")
    require(sum(value["track"] == "predicted" for value in outer) == 12 and
            sum(value["track"] == "actual" for value in outer) == 3 and
            all(value["reason"] == "final_edge_contrast_below_4"
                for value in hidden_details),
            "fixed outer-rim failure inventory invalid")

    v8_candidates = {value["source_probe_id"]: value for value in
                     v8["ownership_candidate_records"]}
    v8_selected = {value["source_probe_id"]: value for value in
                   v8["component_aware_boundary_records"] if value["selected"]}
    area_deltas = [candidate_by_id[source_id]["resolved_pixel_equivalent_area"] -
                   v8_candidates[source_id]["resolved_pixel_equivalent_area"]
                   for source_id in v8_candidates]
    comparison = {
        "source_probe_ids_and_ordered_digest_equal": True,
        "v8_authoritative_visible_hidden": [92, 3],
        "v9_authoritative_visible_hidden": [66, 29],
        "visible_delta": -26,
        "candidate_area": {
            "v8_min_median_max": [
                min(value["resolved_pixel_equivalent_area"]
                    for value in v8_candidates.values()),
                statistics.median(value["resolved_pixel_equivalent_area"]
                                  for value in v8_candidates.values()),
                max(value["resolved_pixel_equivalent_area"]
                    for value in v8_candidates.values())],
            "v9_min_median_max": [
                min(value["resolved_pixel_equivalent_area"]
                    for value in candidates),
                statistics.median(value["resolved_pixel_equivalent_area"]
                                  for value in candidates),
                max(value["resolved_pixel_equivalent_area"]
                    for value in candidates)],
            "delta_increased_equal_decreased": [
                sum(value > 0 for value in area_deltas),
                sum(value == 0 for value in area_deltas),
                sum(value < 0 for value in area_deltas)],
            "v9_below_area_floor_count": sum(
                value["resolved_pixel_equivalent_area"] < 1.5
                for value in candidates),
            "v9_hidden_min_area": min(
                value["resolved_pixel_equivalent_area"]
                for value in hidden_details),
        },
        "selected_association_mode_changed_count": sum(
            v8_selected[source_id]["association_mode"] !=
            selected_by_id[source_id]["association_mode"]
            for source_id in v8_selected),
        "selected_boundary_class_changed_count": sum(
            v8_selected[source_id]["boundary_class"] !=
            selected_by_id[source_id]["boundary_class"]
            for source_id in v8_selected),
        "selected_owner_track_or_component_changed_count": sum(
            (v8_selected[source_id]["owner_track"],
             v8_selected[source_id]["owner_component"]) !=
            (selected_by_id[source_id]["owner_track"],
             selected_by_id[source_id]["owner_component"])
            for source_id in v8_selected),
    }
    require(comparison["candidate_area"]["delta_increased_equal_decreased"] ==
            [77, 8, 10] and
            comparison["candidate_area"]["v9_below_area_floor_count"] == 0 and
            comparison["candidate_area"]["v9_hidden_min_area"] == 12 and
            comparison["selected_association_mode_changed_count"] == 9 and
            comparison["selected_boundary_class_changed_count"] == 0 and
            comparison["selected_owner_track_or_component_changed_count"] == 0,
            "v8/v9 counterfactual comparison invalid")

    renderer = RENDERER.read_text()
    require("appendLayeredHaloSegment" in renderer and
            renderer[renderer.index("void appendLayeredHaloSegment"):
                     renderer.index("void appendExtendedThickSegment")].count(
                         "appendHaloSegment(vertices") == 2 and
            "innerHaloWidth + outerRimWidth, outerRim" in renderer and
            "innerHaloWidth, innerHalo" in renderer and
            "style.predictedCoreCapExtensionPx," in renderer,
            "production visual-layer source anchor invalid")

    frozen = {str(path.relative_to(ROOT)): {"sha256": EXPECTED[path],
              "mode": mode(path)} for path in FROZEN_0444}
    receipt = {
        "schema": "billiardgl.phase4.final_ownership_v9_ordinal1_offline_diagnostic.v1",
        "status": "COMPOSITING_STYLE_LAYER_ALIAS_AND_FIXED_RIM_CONTRAST_DEFECT_ISOLATED_ALGORITHM_CLEAN_NO_CAPTURE_AUTHORIZED",
        "gpu_cocoa_opengl_invoked": False,
        "capture_retry_authorized": False,
        "new_readiness_or_authorization_created": False,
        "frozen_evidence": frozen,
        "frozen_output_directory_mode": mode(OUTPUT),
        "dispatch_outcome": {
            "status": failure["status"], "completed": 0, "failed": 1,
            "unrun": 10, "failed_capture_id": "camera_player_low_1080",
            "authorization_consumed_once": True, "validation_errors": []},
        "authority_separation": {
            "authoritative_gate":
                "corrected_integrated_final_semantic_ownership",
            "corrected_integrated": {"source": 95, "visible": 66,
                "hidden": 29, "coverage": 66 / 95, "machine_pass": False},
            "legacy_ppm_diagnostic_only": {"source": 95, "visible": 47,
                "hidden": 48, "coverage": 47 / 95,
                "authoritative": False}},
        "track_counts": {"predicted":
            row["predicted_source_visible_hidden_and_coverage"], "actual":
            row["actual_source_visible_hidden_and_coverage"]},
        "component_counts": {"source": row["component_source_counts"],
            "visible": row["component_visible_counts"],
            "hidden_reason": row["component_hidden_reason_counts"]},
        "ownership_and_association": {
            "candidate_count": 95, "selected_boundary_count": 95,
            "arbitration_record_count": len(arbitration),
            "contested_arbitration_record_count": sum(
                len(value["claiming_source_probe_ids"]) > 1
                for value in arbitration),
            "winner_source_probe_count": len({
                value["winner_source_probe_id"] for value in arbitration}),
            "foreign_unowned_double_counts": [0, 0, 0],
            "candidate_reason_counts": counter(
                value["eligibility_or_rejection_reason"]
                for value in candidates),
            "association_mode_counts": counter(
                value["association_mode"] for value in selected),
            "boundary_class_counts": counter(
                value["boundary_class"] for value in selected),
        },
        "contrast_failure_partition": {
            "total": 29, "same_track_inner_edge": 14,
            "compound_outer_silhouette": 15,
            "predicted_core_adjacent_white_outer_rim": 12,
            "actual_tick_adjacent_black_outer_rim": 2,
            "same_primitive_stem_inner_failures": 4,
            "cross_primitive_stem_inner_failures": 10,
            "predicted_white_rim_vs_pale_background": 12,
            "actual_black_rim_vs_dark_or_teal_background": 3,
            "area_or_association_failure_count": 0,
        },
        "hidden_probe_details": hidden_details,
        "v8_to_v9_comparison": comparison,
        "static_layering_evidence": {
            "renderer_sha256": EXPECTED[RENDERER],
            "renderer_header_sha256": EXPECTED[RENDERER_HEADER],
            "outer_and_inner_share_one_semantic_halo_component": True,
            "per_primitive_geometry_is_full_outer_fill_then_full_inner_fill": True,
            "outer_fill_can_overwrite_prior_inner_fill_at_overlap": True,
            "predicted_core_cap_extension_px_at_1080": 1.5,
            "predicted_inner_halo_extension_px_at_1080": 2.0,
            "same_primitive_remaining_cap_annulus_px": 0.5,
        },
        "root_cause": {
            "isolated": True,
            "primary": "VISUAL_COMPOSITING_LAYER_ORDER_AND_BACKGROUND_ADAPTIVITY_DEFECT",
            "not_ownership_or_association_algorithm": True,
            "cap_expansion_area_result":
                "AREA_RECOVERED_BUT_FOUR_SAME_PRIMITIVE_CORE_INNER_EDGES_LOST_CONTRAST",
            "three_layers_same_semantic_result":
                "SEMANTIC_IDENTITY_STABLE_BUT_COLOR_LAYERS_ALIAS_IN_BOUNDARY_TELEMETRY_AND_FULL_OUTER_FILLS_OVERWRITE_INNER_COLOR",
            "direct_gate_reason": {"final_edge_contrast_below_4": 29},
        },
        "root_original_resolution_observation_crosscheck": {
            "observation_is_not_machine_authority": True,
            "observation": "The white outer rim is clearly present along the predicted route, but produces a high-frequency white/black/yellow triple-line treatment and becomes crowded around balls and the cue ball.",
            "telemetry_crosscheck": {
                "yellow_to_white_selected_inner_edge_count": 12,
                "yellow_to_white_contrast_ratio": 1.0749340470546902,
                "white_to_pale_outer_silhouette_failure_count": 12,
                "white_to_pale_contrast_range": [1.0834217233705468,
                    1.9309778443054044],
            },
            "product_clarity_conclusion": "Do not widen the three-band treatment; retain a dominant track stroke and use only a narrow, locally necessary contrast boundary.",
        },
    }
    OUT.mkdir(parents=True, exist_ok=False)
    evidence_path = OUT / "failure-evidence-receipt-v1.json"
    write_json(evidence_path, receipt)
    gate = {
        "schema": "billiardgl.phase4.final_ownership_v9_compositing_style_correction_design_gate.v1",
        "status": "DESIGN_ONLY_NO_IMPLEMENTATION_NO_CAPTURE_AUTHORIZATION",
        "bound_failure_evidence_receipt_sha256": sha(evidence_path),
        "bound_v9_telemetry_sha256": EXPECTED[TELEMETRY],
        "bound_v9_dispatch_failure_receipt_sha256": EXPECTED[FAILURE],
        "bound_v9_authorization_sha256": EXPECTED[AUTH],
        "bound_v9_consumed_sha256": EXPECTED[CONSUMED],
        "bound_v8_visual_gate_sha256": EXPECTED[V8_VISUAL_GATE],
        "capture_authorized": False,
        "formal68_authorized": False,
        "technical_art_review_authorized": False,
        "allowed_next_implementation_scope": [
            "Add visual-sublayer provenance (outer_rim, inner_halo, core) beneath the existing immutable track/component/primitive source identity so core/tick/endpoint inner-edge pairing can require inner_halo and outer-silhouette pairing can require outer_rim; source probe IDs and ownership components must not change.",
            "Replace full-width outer-rim fills with a non-overwriting narrow band and globally order each track as all outer bands, then all inner halos, then all cores while reusing the exact existing semantic primitive identity.",
            "Keep the predicted cap-area recovery only with a post-rounding inner-halo cap annulus of at least 2 physical pixels at 1920x1080.",
            "Choose the outermost black/white contrast polarity from a deterministic local pre-overlay backdrop measurement proven equivalent to final resolve/blend/tone-map output; a fixed track-wide polarity is forbidden.",
        ],
        "required_cpu_static_negative_fixtures": [
            "Cross-primitive overlap: a later outer band must never replace an earlier inner halo inside the inner-halo footprint.",
            "Same-primitive cap: predicted core-to-inner-halo contrast remains >=4 with the retained unique-area cap expansion.",
            "Background polarity: dark [9,9,11], teal [25,117,127], pale [223,221,212], and near-white [246,246,242] each select a >=4 outer silhouette polarity.",
            "Reject a fixed predicted-white/actual-black outer polarity.",
            "Reject semantic source ID/digest, owner track/component, boundary association/class, threshold, denominator, probe, route, offset, z-order, camera, layout, asset, scene, or UI drift.",
            "Product clarity: reject a continuous high-frequency white/black/yellow or black/white/dark three-line treatment; the track color must remain dominant and the adaptive rim must remain a narrow conditional aid.",
            "Frozen v9 replay remains 66/29 FAILED and may not be promoted to a pass.",
        ],
        "exit_condition": "CPU overlap/cap/background fixtures and frozen v9 replay must pass before a fresh NOT_RUN readiness may be prepared; capture still requires a separate one-shot authorization.",
        "immutable_invariants": {
            "minimum_visible_coverage": 0.90,
            "minimum_edge_contrast_ratio": 4.0,
            "minimum_owned_area_pixel_equivalents": 1.5,
            "ordinal1_source_probe_count": 95,
            "ordinal1_ordered_source_probe_digest":
                "d533924980efc85aeb94e4f1d31831d93e2bfce744be3b66481d08c56d10289a",
            "probe_ids_denominator_route_offsets_z_order": "UNCHANGED",
            "ownership_arbitration_association_and_hidden_reason_algorithm":
                "UNCHANGED",
            "camera_layout_assets_scene_ui": "UNCHANGED",
        },
        "forbidden": [
            "No retry, GPU, Cocoa, OpenGL, formal68, or TA execution under this gate.",
            "No rewrite of v9 authorization, consumed marker, output, results, telemetry, PPM, plan, or failure receipt.",
            "No threshold or visibility-classification relaxation.",
            "No further outer-envelope widening and no fixed track-wide rim polarity.",
        ],
    }
    write_json(OUT / "minimal-compositing-style-correction-design-gate-v1.json",
               gate)
    print(OUT)


if __name__ == "__main__":
    main()
