#!/usr/bin/env python3
"""Validate the Phase4 V18 projected-near/coaxial CPU diagnostic gate."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/art/phase4-v18-projected-near-coaxial-non-occluding-grammar-cpu-diagnostic-gate-v1.json"
SCHEMA = "billiardgl.phase4.v18.projected_near_coaxial_non_occluding_grammar_cpu_diagnostic_gate.v1"
STATUS = "ROOT_SELECTED_STANDALONE_CPU_DIAGNOSTIC_ONLY_NO_PRODUCTION_NO_CMAKE_NO_GPU"
STATEMENT = "ROOT NEW PRODUCT DECISION: SELECT PROJECTED_NEAR_OR_COAXIAL_NON_OCCLUDING_GRAMMAR."
STATEMENT_SHA = "93c804d1df764dbea1b200b9d3f9c16d6d5713b0fe4ada5fc3205b58cea768d5"
BINDINGS = {
    "docs/art/phase4-v17-explicit-crossing-grammar-independent-ta-feasibility-failure-addendum-v1.json": "150d1d8f1cb3cf0b43935a01a795d56ceadc51a6df330d0a68e4d01f51a36b3c",
    "docs/art/phase4-v17-explicit-crossing-grammar-feasibility-diagnostic-v2/feasibility-receipt-v2.json": "03caa540de8d6f297f30e97ef3f8e69d22a8400521577aac470894131778594a",
    "docs/art/phase4-v17-semantic-batch-provenance-cpu-feasibility-correction-gate-v1.json": "601a9322d5452178499e0922e8b423ae403d89eb7e71fd3f70198e85b1855856",
    "docs/art/phase4-v17-explicit-non-occluding-crossing-grammar-product-decision-gate-v1.json": "c08b7f2c15fa9f53c0ecc76c7c44f07265500b520dfa377097568296d56d8329",
    "docs/art/phase4-v16-local-cross-track-actual-continuity-independent-ta-implementation-failure-addendum-v1.json": "6ccbb8fc7a149c146b755709a41558bd98a2592cf353f7a639660f1c910ce8ef",
    "docs/art/phase4-v15-independent-ta-diagnostic-acceptance-decision-gate-v1.json": "2628104e6d219a98fb38fa599058ec3dd22f11c658d7fe3aa1ffc82850bcf23b",
    "docs/art/phase4-v15-broadcast-overwrite-provenance-diagnostic-v1/provenance-ledger-v1.json": "bb2c3ecafb01d42cfddb8eec89a22619bf646ddb64773846747c5c76d2f5538b",
    "src/Billiards/training/trajectory_overlay_renderer.cpp": "0c108c506641ab4828290a1a4c7b6b5ebbd51ab4fc7c12845bde0f5deb18a840",
    "src/Billiards/training/trajectory_overlay_renderer.h": "9805e6a5f56c17d7645200576505425cddf0d642970e5dc5cec186e003bd564e",
    "src/Billiards/render/modern/modern_renderer.cpp": "b00cc6b370122275a42f106967f935dbb34bd5d3f81f2a750ca9ba6e503af191",
    "src/Billiards/render/modern/modern_renderer.h": "936de5b77c9e3485f2e51f10e9de26c59c8ec94674865e0b50ae125725183c9d",
    "CMakeLists.txt": "f41563c35224dd809f64acedba6455355e70082697c41305c2d1b05426f71d41",
}
ALLOWED = [
    "docs/art/phase4-v18-projected-near-coaxial-grammar-feasibility-diagnostic-v1/phase4_v18_projected_near_coaxial_feasibility_cpu_diagnostic.cpp",
    "docs/art/phase4-v18-projected-near-coaxial-grammar-feasibility-diagnostic-v1/feasibility-receipt-v1.json",
    "docs/art/phase4-v18-projected-near-coaxial-grammar-feasibility-diagnostic-v1/component-ledger-v1.json",
    "docs/art/phase4-v18-projected-near-coaxial-grammar-feasibility-diagnostic-v1/raster-ledger-v1.json",
    "scripts/validate_phase4_v18_projected_near_coaxial_grammar_feasibility.py",
    "tests/test_phase4_v18_projected_near_coaxial_grammar_feasibility.py",
]


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate(path: Path = DEFAULT) -> list[str]:
    errors: list[str] = []
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        return [f"gate_invalid:{error}"]

    def require(condition: bool, message: str) -> None:
        if not condition:
            errors.append(message)

    require(value.get("schema") == SCHEMA and value.get("status") == STATUS, "schema_or_status_invalid")
    decision = value.get("root_product_decision_binding", {})
    require(decision.get("selected_decision_id") == "PROJECTED_NEAR_OR_COAXIAL_NON_OCCLUDING_GRAMMAR" and decision.get("statement") == STATEMENT and decision.get("statement_sha256") == STATEMENT_SHA and hashlib.sha256(STATEMENT.encode()).hexdigest() == STATEMENT_SHA, "root_decision_binding_invalid")
    require(decision.get("selection_authorizes_standalone_cpu_diagnostic_only") is True and decision.get("selection_does_not_authorize_product_implementation_or_gpu") is True, "root_decision_authority_invalid")
    observed = {item.get("path"): item.get("sha256") for item in value.get("frozen_bindings", [])}
    require(observed == BINDINGS, "binding_inventory_or_sha_invalid")
    for relative, digest in BINDINGS.items():
        target = ROOT / relative
        require(target.is_file() and sha(target) == digest, f"binding_missing_or_drift:{relative}")

    case = value.get("frozen_case_contract", {})
    require(case.get("capture_id") == "camera_broadcast_1080" and case.get("physical_dimensions") == [1920, 1080] and case.get("palettes") == ["standard", "color_vision_safe"], "case_identity_invalid")
    require([p.get("sample_count") for p in case.get("sample_profiles", [])] == [1, 2, 4] and case.get("required_case_count") == 18, "sample_profiles_or_case_count_invalid")
    require(case.get("output_only_repair_coordinates") == [[783, 413], [892, 491], [940, 525]], "repair_coordinates_invalid")
    require(case.get("ordered_contrast_probe_count") == 81 and case.get("ordered_contrast_probe_digest") == "33511aa46717a208f3db43acd278416b8fe8393e2e003484da8f44dabffe5645" and len(case.get("exact_repair_probe_ids_in_order", [])) == 3, "probe_inventory_invalid")

    source = value.get("coordinate_free_full_silhouette_source_contract", {})
    require("only then use output coordinates" in source.get("generation_order", "") and len(source.get("source_identity_fields", [])) == 6, "generation_order_or_identity_invalid")
    require("core, inner-halo and outer-halo" in source.get("actual_silhouette", "") and "core, inner-halo and side-only outer-rim" in source.get("protected_predicted_silhouette", ""), "full_silhouette_contract_invalid")
    require("framebuffer color is not provenance" in source.get("coverage_source", "") and len(source.get("forbidden_generation_inputs", [])) == 7 and source.get("new_source_token_batch_primitive_or_arc_allowed") is False, "source_boundary_invalid")

    relation = value.get("relationship_component_contract", {})
    require("immediately adjacent sample-lattice cell" in relation.get("node", "") and "not a tunable distance threshold" in relation.get("node", ""), "relation_node_invalid")
    require("first associated by exact semantic batch/token/track/arcSegmentId/primitiveId" in relation.get("family_aggregation_rule", "") and "can never substitute for exact member association" in relation.get("family_aggregation_rule", ""), "family_aggregation_invalid")
    require("8-neighbor" in relation.get("edge", "") and "compatible exact member identities" in relation.get("edge", "") and "ordered exact primitiveId/finalOwnershipToken member set" in relation.get("component_key", ""), "relation_connectivity_or_key_invalid")
    require(relation.get("strict_centerline_intersection_required") is False and len(relation.get("allowed_relationship_classes", [])) == 3, "selected_relationship_class_invalid")
    require("clockwise perpendicular" in relation.get("deterministic_frame", "") and "fails" in relation.get("deterministic_frame", ""), "deterministic_frame_invalid")
    require("cannot consume an entire primitive/whole route" in relation.get("boundedness", "") and "individually recorded" in relation.get("boundedness", "") and len(relation.get("explicit_infeasible_classes", [])) == 10, "boundedness_or_infeasible_inventory_invalid")

    bracket = value.get("diagnostic_bypass_bracket_contract", {})
    require(bracket.get("candidate_only_not_product_geometry") is True and "existing actual route-body finalOwnershipToken" in bracket.get("ownership", ""), "bracket_identity_invalid")
    require("before the bounded component and rejoin after it" in bracket.get("shape", "") and "neither half may erase, clip, recolor, overwrite or intersect" in bracket.get("protected_predicted_non_occlusion", ""), "bracket_shape_or_nonocclusion_invalid")
    require("minimum must be unique" in bracket.get("minimality", "") and "first sample-lattice shell" in bracket.get("immediately_outside_definition", ""), "bracket_minimality_invalid")
    require(len(bracket.get("preserved_baseline", [])) == 5 and bracket.get("cpu_graph_decoder_is_human_comprehension_proof") is False, "bracket_regression_or_comprehension_boundary_invalid")

    acceptance = value.get("case_acceptance_contract", {})
    require(acceptance.get("full_frame_inventory_frozen_before_matcher") is True and acceptance.get("repair_matcher_only_reports_components") is True and acceptance.get("all_three_regions_required_in_all_eighteen_cases") is True, "matcher_or_case_acceptance_invalid")
    require(acceptance.get("feasible_result") == "PROJECTED_NEAR_COAXIAL_GRAMMAR_FEASIBLE_FOR_A_LATER_SEPARATE_IMPLEMENTATION_GATE" and acceptance.get("infeasible_result") == "PROJECTED_NEAR_COAXIAL_GRAMMAR_INFEASIBLE_FOR_FROZEN_THREE_REGIONS" and acceptance.get("any_single_case_failure_forces_overall_infeasible") is True, "result_contract_invalid")
    require(acceptance.get("diagnostic_candidate_is_not_visual_acceptance") is True, "visual_acceptance_boundary_invalid")

    require(value.get("allowed_standalone_cpu_diagnostic_files") == ALLOWED, "allowed_file_inventory_invalid")
    require(len(value.get("required_fail_closed_fixtures", [])) == 15, "fixture_inventory_invalid")
    future = value.get("future_implementation_preconditions_not_authorized_here", {})
    require(len(future) == 7 and all(flag is True for flag in future.values()), "future_gl_or_review_preconditions_invalid")
    require(len(value.get("forbidden", [])) == 5, "forbidden_inventory_invalid")
    auth = value.get("authorization_boundary", {})
    require(len(auth) == 18 and auth.get("root_product_decision_selected") is True and auth.get("standalone_cpu_diagnostic_authorized") is True and all(flag is False for key, flag in auth.items() if key not in {"root_product_decision_selected", "standalone_cpu_diagnostic_authorized"}), "authorization_boundary_invalid")
    proof = value.get("proof_boundary", {})
    require(len(proof) == 12 and proof.get("design_gate_only") is True and all(flag is False for key, flag in proof.items() if key != "design_gate_only"), "proof_boundary_invalid")
    return errors


def main() -> int:
    target = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT
    errors = validate(target)
    if errors:
        for error in errors:
            print("FAIL: " + error, file=sys.stderr)
        return 1
    print("PASS_PHASE4_V18_PROJECTED_NEAR_COAXIAL_CPU_DIAGNOSTIC_GATE")
    print("decision=selected cpu_diagnostic=true cases=18 production=false cmake=false gpu=false")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
