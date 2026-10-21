#!/usr/bin/env python3
"""Evaluate all 35 v10i candidates and emit GPU-free deterministic evidence."""

import argparse
import itertools
import json
import math
from pathlib import Path

from generate_phase2_modular_gltf_v10i import (
    ASSET_NAME, MANIFEST_NAME, DEFAULT_OUTPUT_ROOT, DEFAULT_BASE_NEUTRAL,
    DEFAULT_LOWER_SCALAR, DEFAULT_UPPER_SCALAR, DEFAULT_ROUGHNESS,
    DESIGN_GATE, EXPECTED_SHA256, _canonical_json, sha256_path,
)
from validate_phase2_modular_gltf_v10i import validate
from calibrate_phase2_pocket_v10g import (
    barycentric_quality, erode_ownership, projection_measurements,
    software_object_id_msaa,
)


ROOT=Path(__file__).resolve().parents[1]
V10H_GPU_MANIFEST=ROOT/"docs/art/phase2-pocket-v10h-gpu-preflight-v1/phase2-pocket-v10h-gpu-preflight-manifest-v1.json"
V10H_GPU_METRICS=ROOT/"docs/art/phase2-pocket-v10h-gpu-preflight-v1/pocket_gpu_metrics.json"
V10H_GPU_FREEZE=ROOT/"docs/art/phase2-pocket-v10h-gpu-source-freeze-v1/receipt.json"
V10G_GPU_MANIFEST=ROOT/"docs/art/phase2-pocket-v10g-gpu-preflight-v1/phase2-pocket-v10g-gpu-preflight-manifest-v1.json"
V10G_GPU_METRICS=ROOT/"docs/art/phase2-pocket-v10g-gpu-preflight-v1/pocket_gpu_metrics.json"
V10G_GPU_FREEZE=ROOT/"docs/art/phase2-pocket-v10g-gpu-source-freeze-v1/receipt.json"
EXPECTED_EVIDENCE={
 "v10h_gpu_manifest":"47bcf3df0c2b7344fc1cd31ad33e123e33258e0a244258f63ce2d81005138a29",
 "v10h_gpu_metrics":"954b9b6ee3ae2b520388d2b1af047d7a191a6afd2aa4e5e234827819ed3bc7fd",
 "v10h_gpu_freeze":"293ae8979508dd21061a0391f49232ad3b8f744954ac1b53a63e35ee176902e0",
 "v10g_gpu_manifest":"6dbc9c8c259add304eb5b039a5321f38e80764dfcc0eb01fd657bcff66d27a02",
 "v10g_gpu_metrics":"a700726d55080d06bdceeb10447568b64285c1080b6e0c6b715ba48b79f18061",
 "v10g_gpu_freeze":"301af66efbdb2f77ce71a666093c024dbbf58365e372f81347406f37f695eed7",
}
FU=.5199589362511269; FL=.10358596765078035; VOID_FLOOR=2.0
ANCHOR_G={"B":.055,"U":.95,"L":.30,"upper":32.294,"lower":19.4344}
ANCHOR_H={"B":.05625,"U":.99,"L":.055,"upper":28.294,"lower":6.8596}


def _x(anchor,fraction):
    return anchor["B"]*((1-fraction)*anchor["L"]+fraction*anchor["U"])


def band_models(base,lower,upper,fraction,band):
    xg,xh=_x(ANCHOR_G,fraction),_x(ANCHOR_H,fraction)
    x=base*((1-fraction)*lower+fraction*upper); t=(x-xh)/(xg-xh)
    gg,gh=ANCHOR_G[band],ANCHOR_H[band]
    linear=gh+t*(gg-gh)
    log_linear=VOID_FLOOR+math.exp(math.log(gh-VOID_FLOOR)+t*(
        math.log(gg-VOID_FLOOR)-math.log(gh-VOID_FLOOR)))
    interval=[max(0,min(linear,log_linear)-.5),max(linear,log_linear)+.5]
    return {"band_scalar":(1-fraction)*lower+fraction*upper,
            "effective_reflectance":x,"anchor_interpolation_t":t,
            "model_a_secant_linear":linear,"model_b_log_linear_above_void":log_linear,
            "model_spread":abs(linear-log_linear),"conservative_interval":interval}


def candidate_metrics(base,lower,upper,roughness=.84):
    u=band_models(base,lower,upper,FU,"upper"); l=band_models(base,lower,upper,FL,"lower")
    iu,il=u["conservative_interval"],l["conservative_interval"]
    checks={
      "base_frozen_0_05625":base==.05625,
      "roughness_frozen_0_84":roughness==.84,
      "upper_on_declared_grid":any(abs(upper-v)<=1e-9 for v in [.97,.975,.98,.985,.99]),
      "lower_on_declared_grid":any(abs(lower-v)<=1e-9 for v in [.105,.1075,.11,.1125,.115,.1175,.12]),
      "endpoint_difference_0_85_to_0_885":.85-1e-9<=upper-lower<=.885+1e-9,
      "upper_interpolation_locality_t_0_to_1":0<=u["anchor_interpolation_t"]<=1,
      "lower_interpolation_locality_t_0_to_1":0<=l["anchor_interpolation_t"]<=1,
      "upper_model_spread_at_most_0_25":u["model_spread"]<=.25,
      "lower_model_spread_at_most_1_5":l["model_spread"]<=1.5,
      "hard_upper_full_interval_27_to_31":iu[0]>=27 and iu[1]<=31,
      "hard_lower_full_interval_8_to_11":il[0]>=8 and il[1]<=11,
      "hard_rim_minus_upper_high_at_least_10":42.941-iu[1]>=10,
      "hard_upper_low_minus_lower_high_at_least_12":iu[0]-il[1]>=12,
      "hard_lower_low_minus_void_upper3_at_least_5":il[0]-3>=5,
      "strict_ordering_with_rim_and_void":42.941>iu[1]>il[0]>3,
      "safety_upper_full_interval_27_25_to_30_5":iu[0]>=27.25 and iu[1]<=30.5,
      "safety_lower_full_interval_8_25_to_10_75":il[0]>=8.25 and il[1]<=10.75,
      "safety_conservative_delta_at_least_16":iu[0]-il[1]>=16,
      "safety_lower_low_minus_void_upper3_at_least_5_25":il[0]-3>=5.25,
      "anti_upper_push_base_fixed_and_upper_at_most_v10h":base==.05625 and upper<=.99,
      "anti_lower_overbright_interval_high_at_most_10_75":il[1]<=10.75,
    }
    failures=[name for name,passed in checks.items() if not passed]
    cost=(base-.05625)**2+(upper-.99)**2+(lower-.055)**2
    return {"parameters":{"base_neutral_scalar":base,"lower_endpoint_scalar":lower,
                          "upper_endpoint_scalar":upper,"roughness":roughness},
            "upper":u,"lower":l,
            "hierarchy":{"rim_minus_upper_high":42.941-iu[1],
                         "upper_low_minus_lower_high":iu[0]-il[1],
                         "lower_low_minus_void_upper3":il[0]-3},
            "selection_displacement_squared_from_v10h":cost,
            "checks":checks,"rejection_reasons":failures,
            "passes_all_hard_safety_locality_spread":not failures}


def deterministic_grid():
    candidates=[candidate_metrics(.05625,l,u) for u,l in itertools.product(
        [.97,.975,.98,.985,.99],[.105,.1075,.11,.1125,.115,.1175,.12])]
    passing=[item for item in candidates if item["passes_all_hard_safety_locality_spread"]]
    selected=min(passing,key=lambda item:(
        item["selection_displacement_squared_from_v10h"],
        item["lower"]["conservative_interval"][1],
        item["parameters"]["base_neutral_scalar"],
        item["parameters"]["lower_endpoint_scalar"],
        item["parameters"]["upper_endpoint_scalar"])) if passing else None
    return candidates,selected


def require_inputs():
    paths={"design_gate":DESIGN_GATE,
           "baseline_gltf":DEFAULT_OUTPUT_ROOT.parent/"phase2-v10h"/ASSET_NAME,
           "baseline_manifest":DEFAULT_OUTPUT_ROOT.parent/"phase2-v10h"/MANIFEST_NAME,
           "v10h_gpu_manifest":V10H_GPU_MANIFEST,"v10h_gpu_metrics":V10H_GPU_METRICS,
           "v10h_gpu_freeze":V10H_GPU_FREEZE,"v10g_gpu_manifest":V10G_GPU_MANIFEST,
           "v10g_gpu_metrics":V10G_GPU_METRICS,"v10g_gpu_freeze":V10G_GPU_FREEZE}
    observed={name:sha256_path(path) for name,path in paths.items()}
    expected={"design_gate":EXPECTED_SHA256["design_gate"],
              "baseline_gltf":EXPECTED_SHA256["baseline_gltf"],
              "baseline_manifest":EXPECTED_SHA256["baseline_manifest"],**EXPECTED_EVIDENCE}
    if any(observed[name]!=expected[name] for name in observed):
        raise ValueError("a frozen v10i calibration input SHA changed")
    vh=json.loads(V10H_GPU_MANIFEST.read_text())
    if vh.get("status")!="REJECTED_MACHINE_NO_RERUN" or vh.get("machine_failures") != [
            "luma_sidewall_lower_8_to_11","lower_minus_void_at_least_5"]:
        raise ValueError("v10h immutable failure disposition changed")
    return observed


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--asset-root",type=Path,default=DEFAULT_OUTPUT_ROOT)
    parser.add_argument("--output",type=Path,required=True); args=parser.parse_args()
    inputs=require_inputs(); asset_root=args.asset_root.resolve()
    document=json.loads((asset_root/ASSET_NAME).read_text()); manifest=json.loads((asset_root/MANIFEST_NAME).read_text())
    errors,diff=validate(document,manifest,asset_root)
    if errors: raise ValueError("v10i validator failed: "+"; ".join(errors))
    candidates,selected=deterministic_grid()
    if selected is None: raise ValueError("v10i grid has no passing candidate")
    committed=manifest["v10i_offline_contract"]["parameters"]
    if committed!=selected["parameters"]: raise ValueError("committed candidate is not deterministic selection")
    ownership_raw,shade,raster=software_object_id_msaa(document,manifest)
    ownership,eroded=erode_ownership(ownership_raw)
    projection=projection_measurements(ownership_raw,ownership,shade,dict(selected["parameters"]))
    barycentric=barycentric_quality(document,dict(selected["parameters"]))
    vh=json.loads(V10H_GPU_MANIFEST.read_text())
    inherited_names=[
      "connected_stage_height_at_least_72px","drop_bottom_mouth_ratio_0_55_to_0_85",
      "drop_has_five_distinct_widths","gold_bar_absent","luma_rim_40_to_52",
      "luma_sidewall_upper_27_to_31","luma_void_1_to_3_p95_at_most_4",
      "ordered_luma_chain","ordered_stage_columns_11_of_11","p95_frame_under_16_667ms",
      "physics_marker_binding_frozen","pocket_ui_and_full_frame_delta_zero",
      "rim_width_frozen","rim_sidewall_drop_position_matched_0_25mm",
      "rounded_arc_segments_by_lod","sidewall_bands_each_128px_15percent_and_9_of_11",
      "sidewall_drop_frozen","viewport_1920x1080","visual_marker_z_mapping_exact",
      "void_back_exact_2_20d","void_front_exact_1_00d","void_is_not_rectangular_slot",
      "void_span_exact_1_20d","walnut_area_inside_envelope_at_most_10_percent",
      "walnut_does_not_enter_cavity","warmup_120_and_samples_300"]
    inherited={name:vh["machine_checks"].get(name) is True for name in inherited_names}
    band=projection["band_statistical_checks"]; fragment=projection["fragment_continuity"]
    gates={
      "input_gate_sha_exact":inputs["design_gate"]==EXPECTED_SHA256["design_gate"],
      "input_v10h_gltf_sha_exact":inputs["baseline_gltf"]==EXPECTED_SHA256["baseline_gltf"],
      "input_v10h_manifest_sha_exact":inputs["baseline_manifest"]==EXPECTED_SHA256["baseline_manifest"],
      **{"input_"+name+"_sha_exact":inputs[name]==EXPECTED_EVIDENCE[name] for name in EXPECTED_EVIDENCE},
      "complete_candidate_count_35":len(candidates)==35,
      "candidate_keys_unique_35":len({tuple(item["parameters"].values()) for item in candidates})==35,
      "passing_candidate_count_5":sum(item["passes_all_hard_safety_locality_spread"] for item in candidates)==5,
      "every_candidate_has_models_intervals_checks_reasons":all(
        all(key in item for key in ("upper","lower","checks","rejection_reasons")) for item in candidates),
      "selected_candidate_exists":selected is not None,
      "committed_parameters_equal_selection":committed==selected["parameters"],
      "selected_every_gate_pass":all(selected["checks"].values()),
      "validator_pass":not errors,
      "raw_diff_only_sidewall_COLOR_0":diff["checks"]["only_sidewall_COLOR_0_raw_bytes_changed"],
      "decoded_non_COLOR_exact":diff["decoded_non_COLOR_accessors_exact"],
      "scene_restore_exact":diff["scene_json_exact_after_permitted_restore"],
      "shared_image_bindings_exact":diff["shared_image_bindings_exact"],
      "material_numeric_factors_frozen":diff["checks"]["material_numeric_factors_frozen"],
      "cpu_raster_2x2_msaa":raster.get("samples_per_pixel")==4,
      "cpu_raster_roi_frozen":projection["roi_px"]==[537,756,1113,1015],
      "band_samples_each_at_least_128":band["each_at_least_128_pixels"],
      "band_coverage_each_at_least_15_percent":band["each_at_least_15_percent"],
      "band_columns_at_least_9_of_11":band["central_column_coverage_at_least_9_of_11"],
      "fragment_columns_all_pass":fragment["passing"]==fragment["sampled"],
      "ordered_stage_columns_all_pass":projection["ordered_stage_columns"]["passing"]==projection["ordered_stage_columns"]["sampled"],
      "barycentric_600_triangles":barycentric["checks"]["all_triangles_tested"],
      "barycentric_no_convex_violation":barycentric["checks"]["no_convex_hull_violation"],
      "barycentric_shared_edges_exact":barycentric["checks"]["shared_edge_mismatch_at_most_1e_6"],
      "barycentric_seams_exact":barycentric["checks"]["geometric_seam_mismatch_at_most_1e_6"],
      "barycentric_no_positive_derivative":barycentric["checks"]["no_positive_derivative"],
      "actual_gpu_used_false":manifest["v10i_offline_contract"]["actual_gpu_used"] is False,
      "gpu_capture_authorized_false":manifest["v10i_offline_contract"]["gpu_capture_authorized"] is False,
      "ta_review_authorized_false":manifest["v10i_offline_contract"]["ta_review_authorized"] is False,
      "formal_six_scene_authorized_false":manifest["v10i_offline_contract"]["formal_six_scene_evidence_authorized"] is False,
      "golden_authorized_false":manifest["v10i_offline_contract"]["golden_promotion_authorized"] is False}
    gates.update({"inherited_v10h_"+name:value for name,value in inherited.items()})
    gates.update({"selected_"+name:value for name,value in selected["checks"].items()})
    failures=[name for name,passed in gates.items() if not passed]
    status="OFFLINE_PREFLIGHT_PASS_GPU_NOT_AUTHORIZED" if not failures else "OFFLINE_PREFLIGHT_FAIL_GPU_NOT_AUTHORIZED"
    report={"schema":"phase2_pocket_v10i_offline_report_v1","status":status,
      "evidence_scope":"phase2_pocket_v10i_offline_only","visual_review_status":"NOT_RUN",
      "actual_gpu_used":False,"GPU_CAPTURE_AUTHORIZED":False,"ta_review_authorized":False,
      "formal_six_scene_evidence_authorized":False,"formal_pass_claimed":False,
      "golden_promotion_claimed":False,"v10h_rerun_authorized":False,
      "frozen_input_sha256":inputs,
      "model_contract":{"model_a":"band-specific two-actual-anchor secant",
                        "model_b":"band-specific log-linear above common void floor 2",
                        "interval":"full model hull plus/minus 0.5 encoded luma",
                        "actual_gpu_used":False},
      "search":{"candidate_count":len(candidates),"passing_candidate_count":sum(i["passes_all_hard_safety_locality_spread"] for i in candidates),
                "selection_rule":"min displacement, lower interval high, base, lower, upper",
                "candidates":candidates},
      "selected_candidate":selected,"decoded_raw_diff":diff,
      "cpu_object_id_msaa_raster":raster,"projection_measurements":projection,
      "eroded_object_id_pixel_count":eroded,"barycentric_interpolation_quality":barycentric,
      "inherited_frozen_v10h_passed_checks":inherited,
      "machine_checks":gates,"machine_check_count":len(gates),"machine_failures":failures}
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(_canonical_json(report))
    if failures: raise SystemExit("v10i offline preflight failed: "+"; ".join(failures))
    print("v10i offline preflight PASS (%d/%d gates, GPU=false)"%(sum(gates.values()),len(gates)))


if __name__=="__main__":main()
