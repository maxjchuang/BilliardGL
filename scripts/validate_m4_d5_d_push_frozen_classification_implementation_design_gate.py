#!/usr/bin/env python3
import hashlib,json,pathlib,sys
ROOT=pathlib.Path(__file__).resolve().parents[1];PATH=ROOT/"docs/rules/m4-d5-d-push-frozen-classification-implementation-design-gate-v1.json"
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def req(v,m):
 if not v:raise ValueError(m)
def validate(d):
 req(d["schema"]=="m4_d5_d_push_frozen_classification_implementation_design_gate_v1","schema")
 req(d["status"]=="FINAL_DESIGN_ONLY_ROOT_REVALIDATION_REQUIRED_NO_PRODUCTION_CHANGE","status")
 for b in d["authority_bindings"].values():req(sha(ROOT/b["path"])==b["sha256"],"authority")
 p=d["selected_policy_constants"];req(p["push_obviousness_authority"]=="AUTHENTICATED_REFEREE_FINAL_BOUND_TO_SAME_SHOT" and p["duration_threshold_seconds"] is None and p["distance_threshold_cm"] is None and p["comparison_tolerance"]==0.0 and p["measurements_report_only"] and not p["pocket_jaw_counts"] and p["accepted_cushion_kind"]=="STRAIGHT_CUSHION_WITH_VELOCITY_IMPULSE_APPLIED","policy")
 m=d["measurement_extension_contract"];req(m["existing_duration_available"] and not m["existing_distance_available"] and m["minimum_new_interval_field"]=="relative_tip_ball_path_length_cm" and m["duration_or_distance_never_compared_to_a_numeric_threshold"],"measurement")
 push=d["push_classifier_contract"];req(push["signed_same_shot_complete_result"]=="PUSH_FOUL" and push["complete_measurement_without_signed_final"]=="NO_PUSH_CLASSIFICATION" and push["signed_final_without_complete_measurement"]=="INCOMPLETE_NO_CLASSIFICATION" and not push["numeric_machine_push_branch"],"push")
 frozen=d["frozen_cushion_classifier_contract"];req(frozen["prerequisite_sample_boundary"].startswith("strictly after") and len(frozen["prerequisite_satisfied_by"])==2 and len(frozen["prerequisite_excludes"])==5 and not frozen["jaw_counts"],"frozen")
 req(len(d["required_fixtures"]["positive"])==12 and len(d["required_fixtures"]["negative"])==20,"fixtures")
 allow=d["implementation_allowlist"];req(len(allow["production_files"])==13 and len(allow["test_files"])==7 and allow["build_files"]==["CMakeLists.txt"] and len(allow["explicitly_forbidden_files"])==10,"allowlist")
 forbidden=set(allow["explicitly_forbidden_files"]);req(not forbidden.intersection(allow["production_files"]),"overlap")
 for b in d["live_source_bindings"]:req(sha(ROOT/b["path"])==b["sha256"],"live "+b["path"])
 c=d["completion_boundary"];req(not c["implementation_authorized_by_this_gate"] and c["root_revalidation_required"] and not c["d5_d_implemented"] and not c["d5_complete"] and not c["wpa_rows_promoted"] and not c["visible_or_global_ui_modified"] and not c["gpu_readiness_or_authorization"] and not c["production_or_cmake_modified"] and not c["m4_complete"],"completion")
def main():
 try:validate(json.loads(PATH.read_text()))
 except Exception as e:print("INVALID_M4_D5_D_IMPLEMENTATION_GATE:",e);return 1
 print("PASS_M4_D5_D_IMPLEMENTATION_GATE");return 0
if __name__=="__main__":sys.exit(main())
