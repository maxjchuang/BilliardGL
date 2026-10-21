#!/usr/bin/env python3
import hashlib, json, pathlib, sys
ROOT = pathlib.Path(__file__).resolve().parents[1]
PATH = ROOT / "docs/rules/m4-d5-d-push-frozen-classification-policy-root-selected-v1.json"
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def req(v, m):
    if not v: raise ValueError(m)
def validate(d):
    req(d["schema"] == "m4_d5_d_push_frozen_classification_policy_root_selected_v1", "schema")
    req(d["status"] == "ROOT_SELECTED_POLICY_ONLY_IMPLEMENTATION_NOT_AUTHORIZED", "status")
    for b in d["input_bindings"].values(): req(sha(ROOT / b["path"]) == b["sha256"], "binding")
    directive = d["root_selection_directive"]
    req(hashlib.sha256(directive["semantic_text"].encode()).hexdigest() == directive["semantic_sha256"], "directive")
    bundle = d["selected_bundle"]
    req(bundle["id"] == "D5D-A_SIGNED_REFEREE_EXACT_MEASUREMENTS_STRAIGHT_ONLY" and bundle["candidate_order"] == 0 and bundle["root_selected"], "bundle")
    req(bundle["push_obviousness_authority"] == "AUTHENTICATED_REFEREE_FINAL_BOUND_TO_SAME_SHOT", "authority")
    measurement = bundle["duration_distance"]
    req(measurement["use"] == "REPORT_ONLY_CANONICAL_MEASUREMENTS" and measurement["duration_threshold_seconds"] is None and measurement["distance_threshold_cm"] is None and measurement["comparison_tolerance"] == 0.0 and not measurement["automatic_numeric_push_classification"] and measurement["complete_measurements_required_for_signed_push_consumption"], "measurement")
    frozen = bundle["frozen_cushion"]
    req(not frozen["pocket_jaw_counts"] and frozen["accepted_cushion_kind"] == "STRAIGHT_CUSHION_WITH_VELOCITY_IMPULSE_APPLIED", "cushion")
    req(frozen["prerequisite_legal_cushion_predicate"] == "POST_FIRST_LEGAL_OBJECT_CONTACT_STRAIGHT_CUSHION_VELOCITY_IMPULSE_OR_COMPLETE_OBJECT_POCKET", "prerequisite")
    req(len(frozen["unmet_prerequisite_alternatives"]) == 3, "alternatives")
    semantics = d["selection_semantics"]
    req(not semantics["measurement_alone_is_push"] and not semantics["signed_final_without_complete_same_shot_measurements_is_push"] and semantics["signed_same_shot_final_plus_complete_measurements_is_push_foul"] and not semantics["frozen_declaration_suppresses_signed_push_foul"] and not semantics["jaw_or_position_correction_counts_as_straight_cushion"] and not semantics["cue_ball_pocket_counts_as_object_pocket_alternative"] and not semantics["selection_is_implementation"] and not semantics["selection_closes_wpa_ii_12_7_15_1_15_2_or_17"] and not semantics["selection_closes_d5_or_m4"], "semantics")
    permissions = d["implementation_permissions"]
    req(permissions["independent_implementation_design_gate_may_be_prepared"] and not permissions["production_or_cmake_change_allowed_before_gate_revalidation"] and not permissions["tests_first_implementation_authorized"] and not permissions["rules_or_shotfacts_consumer_authorized"] and not permissions["visible_or_global_ui_allowed"] and not permissions["gpu_readiness_or_authorization_allowed"], "permissions")
    claims = d["claims"]
    req(claims["policy_selected"] and not claims["implementation_started"] and not claims["push_or_frozen_classification_implemented"] and not claims["audit_rows_promoted"] and not claims["d5_complete"] and not claims["m4_complete"] and not claims["production_or_cmake_modified"] and not claims["gpu_executed_or_authorized"], "claims")
def main():
    try: validate(json.loads(PATH.read_text()))
    except Exception as e: print("INVALID_M4_D5_D_ROOT_SELECTED_POLICY:", e); return 1
    print("PASS_M4_D5_D_ROOT_SELECTED_POLICY"); return 0
if __name__ == "__main__": sys.exit(main())
