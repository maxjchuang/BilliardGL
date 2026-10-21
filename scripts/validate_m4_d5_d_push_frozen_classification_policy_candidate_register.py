#!/usr/bin/env python3
import hashlib, json, pathlib, sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
PATH = ROOT / "docs/rules/m4-d5-d-push-frozen-classification-policy-candidate-register-v1.json"

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def req(value, message):
    if not value: raise ValueError(message)

def validate(data):
    req(data["schema"] == "m4_d5_d_push_frozen_classification_policy_candidate_register_v1", "schema")
    req(data["status"] == "FINAL_DESIGN_ONLY_ALL_BUNDLES_UNSELECTED_ROOT_SELECTION_REQUIRED", "status")
    for binding in data["authority_bindings"].values():
        req(sha(ROOT / binding["path"]) == binding["sha256"], "binding " + binding["path"])
    bundles = data["bundles"]
    selection = data["selection_contract"]
    req(len(bundles) == selection["bundle_count"] == 3, "bundle count")
    req([b["order"] for b in bundles] == [0, 1, 2], "order")
    req([b["id"] for b in bundles] == selection["stable_order"], "stable ids")
    req(len(set(selection["stable_order"])) == 3 and selection["mutually_exclusive"], "unique mutual exclusion")
    req(all(b["selection_status"] == "UNSELECTED" for b in bundles) and selection["all_unselected"], "unselected")
    req(sum(bool(b["recommended"]) for b in bundles) == 1 and next(b["id"] for b in bundles if b["recommended"]) == selection["recommended_id"], "recommended")
    recommended = bundles[0]
    policy = recommended["duration_distance_policy"]
    req(policy["duration_threshold_seconds"] is None and policy["distance_threshold_cm"] is None and policy["comparison_tolerance"] == 0.0 and not policy["automatic_push_classification"], "recommended numeric boundary")
    req(recommended["push_obviousness_authority"] == "AUTHENTICATED_REFEREE_FINAL_BOUND_TO_SAME_SHOT", "referee authority")
    frozen = recommended["frozen_cushion_policy"]
    req(not frozen["pocket_jaw_counts"] and frozen["accepted_cushion_kind"] == "STRAIGHT_CUSHION_WITH_VELOCITY_IMPULSE_APPLIED", "straight only")
    req("POST_FIRST_LEGAL_OBJECT_CONTACT" in frozen["prerequisite_legal_cushion_predicate"], "prerequisite sample")
    machine = bundles[1]["duration_distance_policy"]
    req(machine["duration_threshold_ratio"] == 2.0 and machine["distance_threshold_ratio"] == 2.0 and machine["calibration_manifest_required_before_implementation"], "machine alternative")
    req(bundles[2]["frozen_cushion_policy"]["pocket_jaw_counts"], "jaw alternative")
    req(selection["root_selection_required_before_implementation_gate"] and selection["selection_does_not_close_any_row_or_m4"], "selection boundary")
    req(data["future_implementation_allowlist"]["dormant_until_root_selection_and_implementation_gate"], "dormant allowlist")
    req(len(data["future_fixtures"]["positive"]) == 8 and len(data["future_fixtures"]["negative"]) == 15, "fixtures")
    claims = data["claims"]
    req(claims["candidate_register_complete"] and not claims["root_policy_selected"] and not claims["implementation_authorized"] and not claims["production_or_cmake_modified"] and not claims["audit_rows_promoted"] and not claims["d5_complete"] and not claims["gpu_or_readiness_authorized"] and not claims["m4_complete"], "claims")

def main():
    try: validate(json.loads(PATH.read_text()))
    except Exception as exc:
        print("INVALID_M4_D5_D_POLICY_CANDIDATE_REGISTER:", exc); return 1
    print("PASS_M4_D5_D_POLICY_CANDIDATE_REGISTER"); return 0

if __name__ == "__main__": sys.exit(main())
