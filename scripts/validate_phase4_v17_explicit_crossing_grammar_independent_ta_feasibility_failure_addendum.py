#!/usr/bin/env python3
"""Validate the independent Phase4 V17 strict-grammar failure addendum."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import stat
import sys

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/art/phase4-v17-explicit-crossing-grammar-independent-ta-feasibility-failure-addendum-v1.json"
SCHEMA = "billiardgl.phase4.v17.explicit_crossing_grammar_independent_ta_feasibility_failure_addendum.v1"
STATUS = "INDEPENDENT_FAIL_STRICT_GRAMMAR_INFEASIBLE_CPU_ONLY_NO_PRODUCTION_NO_GPU_NO_AUTH"
BINDINGS = {
    "docs/art/phase4-v17-explicit-crossing-grammar-feasibility-diagnostic-v2/feasibility-receipt-v2.json": "03caa540de8d6f297f30e97ef3f8e69d22a8400521577aac470894131778594a",
    "docs/art/phase4-v17-explicit-crossing-grammar-feasibility-diagnostic-v2/phase4_v17_crossing_feasibility_cpu_diagnostic.cpp": "f672feddebf477ae1c4822b787a0c977b7f0f62228d55429f0bb391ca9998cf5",
    "docs/art/phase4-v17-explicit-crossing-grammar-feasibility-diagnostic-v2/intersection-ledger-v2.json": "70dddabe3f760da1e80aa1779b7010441e5cbc66018ba02d6df9b390f60a1503",
    "docs/art/phase4-v17-explicit-crossing-grammar-feasibility-diagnostic-v2/raster-ledger-v2.json": "18258250388c015969ddf0c0de54d0fec67310a781efe94a789f871d3c54ee8a",
    "scripts/validate_phase4_v17_explicit_non_occluding_crossing_grammar_feasibility_v2.py": "c6a1b9e5ca41dd632709ae7d9094574d6669c7ffd61e416dd882ff2045c4600e",
    "tests/test_phase4_v17_explicit_non_occluding_crossing_grammar_feasibility_v2.py": "231153cdbd5819344f0344d7b861899c9b120c3574a3836ab34253907a85dd89",
    "docs/art/phase4-v17-semantic-batch-provenance-cpu-feasibility-correction-gate-v1.json": "601a9322d5452178499e0922e8b423ae403d89eb7e71fd3f70198e85b1855856",
    "docs/art/phase4-v17-explicit-non-occluding-crossing-grammar-product-decision-gate-v1.json": "c08b7f2c15fa9f53c0ecc76c7c44f07265500b520dfa377097568296d56d8329",
    "docs/art/phase4-v16-local-cross-track-actual-continuity-independent-ta-implementation-failure-addendum-v1.json": "6ccbb8fc7a149c146b755709a41558bd98a2592cf353f7a639660f1c910ce8ef",
    "docs/art/phase4-v15-independent-ta-diagnostic-acceptance-decision-gate-v1.json": "2628104e6d219a98fb38fa599058ec3dd22f11c658d7fe3aa1ffc82850bcf23b",
    "docs/art/phase4-v15-broadcast-overwrite-provenance-diagnostic-v1/provenance-ledger-v1.json": "bb2c3ecafb01d42cfddb8eec89a22619bf646ddb64773846747c5c76d2f5538b",
}
BASELINES = {
    "src/Billiards/training/trajectory_overlay_renderer.cpp": "0c108c506641ab4828290a1a4c7b6b5ebbd51ab4fc7c12845bde0f5deb18a840",
    "src/Billiards/training/trajectory_overlay_renderer.h": "9805e6a5f56c17d7645200576505425cddf0d642970e5dc5cec186e003bd564e",
    "src/Billiards/render/modern/modern_renderer.cpp": "b00cc6b370122275a42f106967f935dbb34bd5d3f81f2a750ca9ba6e503af191",
    "src/Billiards/render/modern/modern_renderer.h": "936de5b77c9e3485f2e51f10e9de26c59c8ec94674865e0b50ae125725183c9d",
    "CMakeLists.txt": "f41563c35224dd809f64acedba6455355e70082697c41305c2d1b05426f71d41",
}
REPAIRS = [
    "d6d850a1a10c7c7ce403e6de4afd4330e4c6e89b9100c72f4c23fbab1f6f978b",
    "1890fd0652c8f74a8b03a35a6cee1ad2cab43062987cab73efb1f6b455da468a",
    "1bf60bcd841a0d754c82876b55a4d3d6c29ebad582e069d4f4a608a4778c33a9",
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

    require(value.get("schema") == SCHEMA and value.get("status") == STATUS, "schema_or_status_invalid")
    observed = {item.get("path"): item.get("sha256") for item in value.get("frozen_bindings", [])}
    require(observed == BINDINGS, "binding_inventory_or_sha_invalid")
    for relative, digest in BINDINGS.items():
        target = ROOT / relative
        require(target.is_file() and sha(target) == digest, f"binding_missing_or_drift:{relative}")
    receipt = ROOT / "docs/art/phase4-v17-explicit-crossing-grammar-feasibility-diagnostic-v2/feasibility-receipt-v2.json"
    require(stat.S_IMODE(receipt.stat().st_mode) == 0o444, "final_receipt_not_read_only_0444")

    restored = {item.get("path"): item.get("sha256") for item in value.get("restored_baselines", {}).values()}
    require(restored == BASELINES, "baseline_inventory_or_sha_invalid")
    for relative, digest in BASELINES.items():
        target = ROOT / relative
        require(target.is_file() and sha(target) == digest, f"baseline_missing_or_drift:{relative}")

    audit = value.get("independent_reproduction_audit", {})
    require(audit.get("final_receipt_read_only_0444") is True and audit.get("validator_fresh_recompile_relink_byte_replay") == "PASS_ERRORS_EMPTY", "reproduction_or_freeze_invalid")
    require(audit.get("fail_closed_test_count") == 65 and audit.get("fail_closed_test_pass_count") == 65, "test_count_invalid")
    require(audit.get("coordinate_output_only_full_frame_generation") is True and audit.get("probe_and_repair_audit_occurs_after_standard_and_cvd_event_generation") is True, "generation_boundary_invalid")
    require("finalOwnershipToken" in audit.get("eligibility_provenance", "") and "exact primitiveId" in audit.get("eligibility_provenance", ""), "semantic_provenance_invalid")
    require(audit.get("contrast_probe_or_repair_id_used_for_eligibility") is False and audit.get("source_token_batch_primitive_or_arc_growth") == 0, "probe_or_growth_invalid")
    require(audit.get("ordered_contrast_probe_count") == 81 and audit.get("ordered_contrast_probe_digest") == "33511aa46717a208f3db43acd278416b8fe8393e2e003484da8f44dabffe5645", "probe_inventory_invalid")
    require(audit.get("exact_repair_probe_ids_in_order") == REPAIRS and audit.get("palettes") == ["standard", "color_vision_safe"] and audit.get("sample_profiles") == ["center_1x", "asymmetric_2x", "asymmetric_4x"] and audit.get("exact_case_count") == 18, "case_matrix_invalid")
    require(audit.get("standard_cvd_topology_and_raster_metrics_identical") is True and audit.get("one_two_four_sample_metrics_are_real_not_relabels") is True, "palette_or_sample_audit_invalid")

    finding = value.get("independent_failure_finding", {})
    require(finding.get("classification") == "V17_PRIORITY_TWO_STRICT_INTERSECTION_GRAMMAR_INFEASIBLE", "failure_classification_invalid")
    require(finding.get("actual_descriptor_count") == 81 and finding.get("missing_actual_semantic_descriptor_count") == 0 and finding.get("predicted_dash_on_descriptor_count") == 68 and finding.get("missing_predicted_semantic_descriptor_count") == 0, "descriptor_inventory_invalid")
    require(finding.get("evaluated_pair_count") == 5508 and finding.get("pair_count_identity") == "81 * 68 = 5508" and 81 * 68 == 5508 and finding.get("disjoint_pair_count") == 5508, "pair_partition_invalid")
    require(finding.get("strict_interior_nonparallel_event_count") == 0 and finding.get("event_inventory_digest") == hashlib.sha256(b"").hexdigest() and finding.get("repair_case_result") == "0_FEASIBLE_18_INFEASIBLE", "event_or_case_result_invalid")
    require("all 5508 pairs reach" in finding.get("why_this_is_not_the_v1_sparse_probe_failure", "") and "finite-width raster silhouettes overlap" in finding.get("why_the_selected_grammar_cannot_emit", ""), "root_cause_explanation_invalid")
    require(finding.get("synthetic_bracket_fixture_passed") is True and finding.get("synthetic_fixture_is_frozen_scene_feasibility") is False and finding.get("frozen_bracket_emitted") is False, "synthetic_fixture_boundary_invalid")
    require(finding.get("implementation_acceptance") is False and finding.get("gpu_or_readiness_may_follow") is False, "failure_authority_invalid")

    choices = value.get("two_choice_disposition", {})
    require(choices.get("v15_gate_defined_exactly_two_mutually_exclusive_bounded_choices") is True and choices.get("bounded_choice_remaining") is False and choices.get("product_implementation_path_remaining_under_current_two_choice_gate") is False and choices.get("automatic_fallback_or_scope_expansion_allowed") is False, "two_choice_summary_invalid")
    require(choices.get("priority_one", {}).get("disposition") == "REJECTED_BY_V16_REPRODUCIBLE_FULL_FRAME_PREMISE_FALSIFICATION" and choices.get("priority_one", {}).get("may_resume_under_existing_gate") is False, "priority_one_disposition_invalid")
    require(choices.get("priority_two", {}).get("disposition") == "REJECTED_BY_V17_CORRECTED_REPRODUCIBLE_STRICT_INTERSECTION_INFEASIBILITY" and choices.get("priority_two", {}).get("may_resume_under_existing_gate") is False, "priority_two_disposition_invalid")

    successor = value.get("successor_product_decision_requirement", {})
    require(successor.get("new_root_product_decision_required") is True and successor.get("candidate_decision_for_evaluation_not_selected") == "PROJECTED_NEAR_OR_COAXIAL_NON_OCCLUDING_GRAMMAR", "successor_decision_requirement_invalid")
    require(len(successor.get("minimum_questions_for_root", [])) == 4 and len(successor.get("constraints_if_root_later_selects_a_successor", [])) == 5, "successor_question_or_constraint_inventory_invalid")
    require(successor.get("candidate_selected_now") is False and successor.get("successor_gate_created_here") is False and successor.get("implementation_authorized_here") is False, "successor_false_authorization")
    authorization = value.get("authorization_boundary", {})
    require(len(authorization) == 18 and all(flag is False for flag in authorization.values()), "authorization_boundary_must_be_all_false")
    return errors


def main() -> int:
    target = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT
    errors = validate(target)
    if errors:
        for error in errors:
            print("FAIL: " + error, file=sys.stderr)
        return 1
    print("PASS_PHASE4_V17_INDEPENDENT_TA_FEASIBILITY_FAILURE_ADDENDUM")
    print("priority1=rejected priority2=rejected bounded_path=false new_product_decision_required=true production=false gpu=false")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
