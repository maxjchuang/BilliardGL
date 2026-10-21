#!/usr/bin/env python3
"""Search all 225 v10h candidates and emit deterministic offline evidence.

No renderer or GPU process is invoked.  The CPU raster is an object-ID/MSAA
projection used only for frozen ownership, band sampling and continuity checks.
"""

import argparse
import itertools
import json
import math
from pathlib import Path

from generate_phase2_modular_gltf_v10h import (
    ASSET_NAME, MANIFEST_NAME, DEFAULT_OUTPUT_ROOT, DEFAULT_BASE_NEUTRAL,
    DEFAULT_LOWER_SCALAR, DEFAULT_UPPER_SCALAR, DEFAULT_ROUGHNESS,
    DESIGN_GATE, EXPECTED_SHA256, _canonical_json, sha256_path,
)
from validate_phase2_modular_gltf_v10h import validate
from calibrate_phase2_pocket_v10g import (
    barycentric_quality, erode_ownership, luma, predict_endpoint_rgb,
    projection_measurements, software_object_id_msaa,
)


ROOT = Path(__file__).resolve().parents[1]
V10G_GPU_MANIFEST = ROOT / "docs/art/phase2-pocket-v10g-gpu-preflight-v1/phase2-pocket-v10g-gpu-preflight-manifest-v1.json"
V10G_GPU_METRICS = ROOT / "docs/art/phase2-pocket-v10g-gpu-preflight-v1/pocket_gpu_metrics.json"
V10G_OFFLINE_REPORT = ROOT / "docs/art/phase2-pocket-v10g-offline-preflight-v2/phase2-pocket-v10g-offline-report-v2.json"
EXPECTED_EVIDENCE_SHA256 = {
    "gpu_manifest": "6dbc9c8c259add304eb5b039a5321f38e80764dfcc0eb01fd657bcff66d27a02",
    "gpu_metrics": "a700726d55080d06bdceeb10447568b64285c1080b6e0c6b715ba48b79f18061",
    "v10g_offline_report": "0830044860a94ac157c3953b995531ac695460370271d2feaaeeabbf1b3a39e1",
}

FU = 0.5199589362511269
FL = 0.10358596765078035
V10G_SU = 0.6379733085632324
V10G_SL = 0.3673308789730072
V10G_CPU_UPPER = 75.07789257620489
RATIO_UPPER = 0.4301399372288229
RATIO_LOWER = 1.2031832440037131
V10G_GPU_UPPER = 32.294
V10G_GPU_LOWER = 19.4344
RIM_LUMA = 42.941
VOID_UPPER = 3.0
RESERVE = 1.0


def cpu_upper_normalization():
    """Frozen v10g upper endpoint-to-band CPU bias, never a GPU fit."""
    raw = luma(predict_endpoint_rgb(0.055, V10G_SU, 0.84, FU))
    return V10G_CPU_UPPER / raw


def candidate_metrics(base, lower, upper, roughness=0.84):
    su = (1.0 - FU) * lower + FU * upper
    sl = (1.0 - FL) * lower + FL * upper
    cpu_upper = cpu_upper_normalization() * \
        luma(predict_endpoint_rgb(base, su, roughness, FU))
    # The lower band uses the direct PBR proxy; unlike the upper projected band,
    # no endpoint-to-band screen normalization is applied.
    cpu_lower = luma(predict_endpoint_rgb(base, sl, roughness, FL))
    model_a_upper = RATIO_UPPER * cpu_upper
    model_a_lower = RATIO_LOWER * cpu_lower
    qu = (base * su) / (0.055 * V10G_SU)
    ql = (base * sl) / (0.055 * V10G_SL)
    model_b_upper = 2.0 + (V10G_GPU_UPPER - 2.0) * qu
    model_b_lower = 2.0 + (V10G_GPU_LOWER - 2.0) * ql
    spread_upper = abs(model_a_upper - model_b_upper)
    spread_lower = abs(model_a_lower - model_b_lower)
    interval_upper = [max(0.0, min(model_a_upper, model_b_upper) - RESERVE),
                      max(model_a_upper, model_b_upper) + RESERVE]
    interval_lower = [max(0.0, min(model_a_lower, model_b_lower) - RESERVE),
                      max(model_a_lower, model_b_lower) + RESERVE]
    delta_lower = interval_upper[0] - interval_lower[1]
    checks = {
        "base_on_declared_grid": any(abs(base-v) <= 1e-9 for v in
                                     [0.0525, 0.05375, 0.055, 0.05625, 0.0575]),
        "upper_on_declared_grid": any(abs(upper-v) <= 1e-9 for v in
                                      [0.98, 0.985, 0.99, 0.995, 1.0]),
        "lower_on_declared_grid": any(abs(lower-(0.04+0.005*i)) <= 1e-9
                                      for i in range(9)),
        "roughness_frozen_0_84": roughness == 0.84,
        "endpoint_difference_0_90_to_0_96": 0.90-1e-9 <= upper-lower <= 0.96+1e-9,
        "cpu_upper_in_declared_target": 65.09509482051362 <= cpu_upper <= 68.58233204304113,
        "cpu_lower_in_declared_target": 7.563270221183297 <= cpu_lower <= 8.145060238197397,
        "cpu_delta_at_least_56_95003458231623": cpu_upper-cpu_lower >= 56.95003458231623,
        "upper_model_spread_at_most_0_5": spread_upper <= 0.5,
        "lower_model_spread_at_most_0_5": spread_lower <= 0.5,
        "hard_upper_interval_lower_at_least_27": interval_upper[0] >= 27.0,
        "hard_upper_interval_upper_at_most_31": interval_upper[1] <= 31.0,
        "hard_lower_interval_lower_at_least_8": interval_lower[0] >= 8.0,
        "hard_lower_interval_upper_at_most_11": interval_lower[1] <= 11.0,
        "hard_conservative_delta_at_least_12": delta_lower >= 12.0,
        "rim_minus_upper_bound_at_least_10": RIM_LUMA-interval_upper[1] >= 10.0,
        "lower_bound_minus_void_upper_at_least_5": interval_lower[0]-VOID_UPPER >= 5.0,
        "strict_ordering": RIM_LUMA > interval_upper[1] > interval_lower[0] > VOID_UPPER,
        "recommended_upper_lower_bound_at_least_27_25": interval_upper[0] >= 27.25,
        "recommended_upper_upper_bound_at_most_30_5": interval_upper[1] <= 30.5,
        "recommended_lower_lower_bound_at_least_8_25": interval_lower[0] >= 8.25,
        "recommended_lower_upper_bound_at_most_10_5": interval_lower[1] <= 10.5,
        "recommended_conservative_delta_at_least_16": delta_lower >= 16.0,
        "anti_lower_push_model_a_at_most_9_8": model_a_lower <= 9.8,
        "anti_lower_push_model_b_at_most_9_8": model_b_lower <= 9.8,
        "anti_lower_push_cpu_within_declared_max": cpu_lower <= 8.145060238197397,
    }
    failures = [name for name, passed in checks.items() if not passed]
    displacement = (base-0.055)**2 + (upper-0.95)**2 + (lower-0.30)**2
    return {
        "parameters": {"base_neutral_scalar": base,
                       "lower_endpoint_scalar": lower,
                       "upper_endpoint_scalar": upper, "roughness": roughness},
        "band_scalars": {"upper": su, "lower": sl},
        "cpu_luma": {"upper": cpu_upper, "lower": cpu_lower,
                     "upper_minus_lower": cpu_upper-cpu_lower},
        "model_a_gpu_luma": {"upper": model_a_upper, "lower": model_a_lower},
        "model_b_gpu_luma": {"upper": model_b_upper, "lower": model_b_lower},
        "model_spread": {"upper": spread_upper, "lower": spread_lower},
        "conservative_intervals": {"upper": interval_upper, "lower": interval_lower,
                                   "upper_minus_lower_lower_bound": delta_lower},
        "selection_displacement_squared_from_v10g": displacement,
        "checks": checks, "rejection_reasons": failures,
        "passes_all_hard_and_recommended": not failures,
    }


def deterministic_grid():
    bases = [round(0.0525 + 0.00125*i, 8) for i in range(5)]
    uppers = [round(0.98 + 0.005*i, 8) for i in range(5)]
    lowers = [round(0.04 + 0.005*i, 8) for i in range(9)]
    candidates = [candidate_metrics(b, l, u) for b, u, l in
                  itertools.product(bases, uppers, lowers)]
    passing = [item for item in candidates if item["passes_all_hard_and_recommended"]]
    selected = min(passing, key=lambda item: (
        item["selection_displacement_squared_from_v10g"],
        item["conservative_intervals"]["lower"][1],
        item["parameters"]["base_neutral_scalar"],
        item["parameters"]["lower_endpoint_scalar"],
        item["parameters"]["upper_endpoint_scalar"],
    )) if passing else None
    return candidates, selected


def require_bound_inputs():
    observed = {"design_gate": sha256_path(DESIGN_GATE),
                "baseline_gltf": sha256_path(DEFAULT_OUTPUT_ROOT.parent / "phase2-v10g" / ASSET_NAME),
                "baseline_manifest": sha256_path(DEFAULT_OUTPUT_ROOT.parent / "phase2-v10g" / MANIFEST_NAME),
                "gpu_manifest": sha256_path(V10G_GPU_MANIFEST),
                "gpu_metrics": sha256_path(V10G_GPU_METRICS),
                "v10g_offline_report": sha256_path(V10G_OFFLINE_REPORT)}
    expected = dict(EXPECTED_SHA256)
    expected.update(EXPECTED_EVIDENCE_SHA256)
    if any(observed[key] != expected[key] for key in observed):
        raise ValueError("a frozen v10h calibration input SHA changed")
    return observed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--asset-root", type=Path, default=DEFAULT_OUTPUT_ROOT)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    inputs = require_bound_inputs()
    asset_root = args.asset_root.resolve()
    document = json.loads((asset_root / ASSET_NAME).read_text(encoding="utf-8"))
    manifest = json.loads((asset_root / MANIFEST_NAME).read_text(encoding="utf-8"))
    errors, decoded_diff = validate(document, manifest, asset_root)
    if errors:
        raise ValueError("v10h validator failed: " + "; ".join(errors))
    candidates, selected = deterministic_grid()
    if selected is None:
        raise ValueError("complete v10h grid has no passing candidate")
    committed = manifest["v10h_offline_contract"]["parameters"]
    if committed != selected["parameters"]:
        raise ValueError("committed asset is not the deterministic selected candidate")

    ownership_raw, shade, raster = software_object_id_msaa(document, manifest)
    ownership, eroded_pixels = erode_ownership(ownership_raw)
    selected_flat = dict(selected["parameters"])
    image = projection_measurements(ownership_raw, ownership, shade, selected_flat)
    barycentric = barycentric_quality(document, selected_flat)
    prior_gpu = json.loads(V10G_GPU_MANIFEST.read_text(encoding="utf-8"))
    inherited_names = [
        "connected_stage_height_at_least_72px", "drop_bottom_mouth_ratio_0_55_to_0_85",
        "drop_has_five_distinct_widths", "gold_bar_absent", "luma_rim_40_to_52",
        "luma_void_1_to_3_p95_at_most_4", "ordered_stage_columns_11_of_11",
        "p95_frame_under_16_667ms", "physics_marker_binding_frozen",
        "pocket_ui_and_full_frame_delta_zero", "rim_geometry_11_to_13_all_10_to_14px",
        "rim_sidewall_drop_position_matched_0_25mm", "rim_width_frozen",
        "rounded_arc_segments_by_lod", "sidewall_bands_each_128px_15percent_and_9_of_11",
        "sidewall_drop_frozen", "sidewall_geometry_median_18_to_24_p10_16_min_14px",
        "viewport_1920x1080", "visual_marker_z_mapping_exact", "void_back_exact_2_20d",
        "void_front_exact_1_00d", "void_is_not_rectangular_slot", "void_span_exact_1_20d",
        "walnut_area_inside_envelope_at_most_10_percent", "walnut_does_not_enter_cavity",
        "walnut_intrusion_max_8px_median_0px", "warmup_120_and_samples_300",
    ]
    inherited = {name: prior_gpu["machine_checks"].get(name) is True for name in inherited_names}
    band = image["band_statistical_checks"]
    fragment = image["fragment_continuity"]
    gates = {
        "input_design_gate_sha_exact": inputs["design_gate"] == EXPECTED_SHA256["design_gate"],
        "input_v10g_gltf_sha_exact": inputs["baseline_gltf"] == EXPECTED_SHA256["baseline_gltf"],
        "input_v10g_manifest_sha_exact": inputs["baseline_manifest"] == EXPECTED_SHA256["baseline_manifest"],
        "input_v10g_gpu_manifest_sha_exact": inputs["gpu_manifest"] == EXPECTED_EVIDENCE_SHA256["gpu_manifest"],
        "input_v10g_gpu_metrics_sha_exact": inputs["gpu_metrics"] == EXPECTED_EVIDENCE_SHA256["gpu_metrics"],
        "input_v10g_offline_report_sha_exact": inputs["v10g_offline_report"] == EXPECTED_EVIDENCE_SHA256["v10g_offline_report"],
        "complete_candidate_count_225": len(candidates) == 225,
        "candidate_keys_unique_225": len({tuple(item["parameters"].values()) for item in candidates}) == 225,
        "every_candidate_has_two_models_intervals_checks_reasons": all(
            all(key in item for key in ("model_a_gpu_luma", "model_b_gpu_luma",
                                        "conservative_intervals", "checks", "rejection_reasons"))
            for item in candidates),
        "deterministic_selected_candidate_exists": selected is not None,
        "committed_parameters_equal_selection": committed == selected["parameters"],
        "selected_passes_every_candidate_gate": all(selected["checks"].values()),
        "validator_pass": not errors,
        "only_sidewall_COLOR_0_buffer_bytes_changed": decoded_diff["checks"]["only_sidewall_COLOR_0_buffer_bytes_changed"],
        "decoded_non_COLOR_accessors_exact": decoded_diff["decoded_non_COLOR_accessors_exact"],
        "scene_restore_exact": decoded_diff["scene_json_exact_after_permitted_restore"],
        "shared_image_bindings_exact": decoded_diff["shared_image_bindings_exact"],
        "raster_uses_2x2_msaa": raster.get("samples_per_pixel") == 4,
        "raster_roi_frozen": image["roi_px"] == [537, 756, 1113, 1015],
        "upper_band_at_least_128_pixels": image["bands_geometric_labels"]["upper"]["sample_count"] >= 128,
        "lower_band_at_least_128_pixels": image["bands_geometric_labels"]["lower"]["sample_count"] >= 128,
        "both_bands_at_least_15_percent": band["each_at_least_15_percent"],
        "band_columns_at_least_9_of_11": band["central_column_coverage_at_least_9_of_11"],
        "fragment_columns_all_pass": fragment["passing"] == fragment["sampled"],
        "ordered_stage_columns_all_pass": image["ordered_stage_columns"]["passing"] == image["ordered_stage_columns"]["sampled"],
        "barycentric_all_600_triangles": barycentric["checks"]["all_triangles_tested"],
        "barycentric_no_convex_violation": barycentric["checks"]["no_convex_hull_violation"],
        "barycentric_shared_edge_exact": barycentric["checks"]["shared_edge_mismatch_at_most_1e_6"],
        "barycentric_seams_exact": barycentric["checks"]["geometric_seam_mismatch_at_most_1e_6"],
        "barycentric_no_positive_derivative": barycentric["checks"]["no_positive_derivative"],
        "visual_review_status_not_run": manifest["v10h_offline_contract"]["visual_review_status"] == "NOT_RUN",
        "actual_gpu_used_false": manifest["v10h_offline_contract"]["actual_gpu_used"] is False,
        "gpu_capture_authorized_false": manifest["v10h_offline_contract"]["gpu_capture_authorized"] is False,
        "ta_review_authorized_false": manifest["v10h_offline_contract"]["ta_review_authorized"] is False,
        "formal_six_scene_authorized_false": manifest["v10h_offline_contract"]["formal_six_scene_evidence_authorized"] is False,
        "golden_promotion_authorized_false": manifest["v10h_offline_contract"]["golden_promotion_authorized"] is False,
    }
    gates.update({"inherited_v10g_" + name: value for name, value in inherited.items()})
    gates.update({"selected_" + name: value for name, value in selected["checks"].items()})
    failures = [name for name, passed in gates.items() if not passed]
    status = "OFFLINE_PREFLIGHT_PASS_GPU_NOT_AUTHORIZED" if not failures else \
        "OFFLINE_PREFLIGHT_FAIL_GPU_NOT_AUTHORIZED"
    report = {
        "schema": "phase2_pocket_v10h_offline_report_v1",
        "status": status,
        "evidence_scope": "phase2_pocket_v10h_offline_only",
        "visual_review_status": "NOT_RUN", "actual_gpu_used": False,
        "GPU_CAPTURE_AUTHORIZED": False, "ta_review_authorized": False,
        "formal_six_scene_evidence_authorized": False,
        "formal_pass_claimed": False, "golden_promotion_claimed": False,
        "frozen_input_sha256": inputs,
        "cpu_model": {"type": "offline_linear_srgb_pbr_proxy_with_aces_and_band_sampling",
                      "upper_normalization": cpu_upper_normalization(),
                      "lower_normalization": 1.0,
                      "actual_gpu_used": False},
        "search": {"candidate_count": len(candidates),
                   "passing_candidate_count": sum(i["passes_all_hard_and_recommended"] for i in candidates),
                   "selection_rule": "min displacement, lower interval upper, base, lower, upper",
                   "candidates": candidates},
        "selected_candidate": selected,
        "decoded_accessor_diff": decoded_diff,
        "cpu_object_id_msaa_raster": raster,
        "projection_measurements": image,
        "eroded_object_id_pixel_count": eroded_pixels,
        "barycentric_interpolation_quality": barycentric,
        "inherited_frozen_v10g_checks": inherited,
        "machine_checks": gates, "machine_check_count": len(gates),
        "machine_failures": failures,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(_canonical_json(report))
    if failures:
        raise SystemExit("v10h offline preflight failed: " + "; ".join(failures))
    print("v10h offline preflight PASS (%d/%d gates, GPU=false)" %
          (sum(gates.values()), len(gates)))


if __name__ == "__main__":
    main()
