#!/usr/bin/env python3
"""Validate the CPU-only Phase4 V17 crossing-grammar feasibility gate."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/art/phase4-v17-explicit-non-occluding-crossing-grammar-cpu-feasibility-diagnostic-gate-v1.json"
SCHEMA = "billiardgl.phase4.v17.explicit_non_occluding_crossing_grammar_cpu_feasibility_diagnostic_gate.v1"
STATUS = "ROOT_SELECTED_CPU_DIAGNOSTIC_ONLY_AUTHORIZED_NO_PRODUCTION_NO_CMAKE_NO_GPU"
STATEMENT = "ROOT PRODUCT DECISION: SELECTED EXPLICIT_NON_OCCLUDING_CROSSING_GRAMMAR from Phase4 V17 product-decision gate SHA c08b7f2c15fa9f53c0ecc76c7c44f07265500b520dfa377097568296d56d8329."
BINDINGS = {
    "docs/art/phase4-v17-explicit-non-occluding-crossing-grammar-product-decision-gate-v1.json": "c08b7f2c15fa9f53c0ecc76c7c44f07265500b520dfa377097568296d56d8329",
    "docs/art/phase4-v16-local-cross-track-actual-continuity-independent-ta-implementation-failure-addendum-v1.json": "6ccbb8fc7a149c146b755709a41558bd98a2592cf353f7a639660f1c910ce8ef",
    "docs/art/phase4-v16-local-cross-track-actual-continuity-premise-falsification-v1/premise-failure-receipt-v1.json": "6e5c729016795a8744fddd7351ae8d9f9d584f9cde91946a33bcbd83f982fa96",
    "docs/art/phase4-v15-broadcast-overwrite-provenance-diagnostic-v1/provenance-ledger-v1.json": "bb2c3ecafb01d42cfddb8eec89a22619bf646ddb64773846747c5c76d2f5538b",
    "docs/art/phase4-v15-broadcast-overwrite-provenance-diagnostic-v1/diagnostic-receipt-v1.json": "05e0bc2de8fd9c0978bf66547f2ce444b9bee864ca27dab871f2482b4df80dcf",
    "src/Billiards/training/trajectory_overlay_renderer.cpp": "0c108c506641ab4828290a1a4c7b6b5ebbd51ab4fc7c12845bde0f5deb18a840",
    "src/Billiards/training/trajectory_overlay_renderer.h": "9805e6a5f56c17d7645200576505425cddf0d642970e5dc5cec186e003bd564e",
}
ALLOWED = [
    "docs/art/phase4-v17-explicit-crossing-grammar-feasibility-diagnostic-v1/phase4_v17_crossing_feasibility_cpu_diagnostic.cpp",
    "docs/art/phase4-v17-explicit-crossing-grammar-feasibility-diagnostic-v1/feasibility-receipt-v1.json",
    "docs/art/phase4-v17-explicit-crossing-grammar-feasibility-diagnostic-v1/intersection-ledger-v1.json",
    "docs/art/phase4-v17-explicit-crossing-grammar-feasibility-diagnostic-v1/raster-ledger-v1.json",
    "scripts/validate_phase4_v17_explicit_non_occluding_crossing_grammar_feasibility.py",
    "tests/test_phase4_v17_explicit_non_occluding_crossing_grammar_feasibility.py",
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
    require(decision.get("selected_decision_id") == "EXPLICIT_NON_OCCLUDING_CROSSING_GRAMMAR" and decision.get("selection_statement") == STATEMENT and decision.get("selection_statement_sha256") == hashlib.sha256(STATEMENT.encode()).hexdigest() and decision.get("selection_is_not_implementation_or_gpu_authorization") is True, "root_decision_binding_invalid")
    observed = {item.get("path"): item.get("sha256") for item in value.get("frozen_bindings", [])}
    require(observed == BINDINGS, "binding_inventory_or_sha_invalid")
    for relative, digest in BINDINGS.items():
        target = ROOT / relative
        require(target.is_file() and sha(target) == digest, f"binding_missing_or_drift:{relative}")

    case = value.get("frozen_case_contract", {})
    require(case.get("capture_id") == "camera_broadcast_1080" and case.get("physical_dimensions") == [1920, 1080] and case.get("palettes") == ["standard", "color_vision_safe"], "case_identity_invalid")
    profiles = case.get("sample_profiles", [])
    require([item.get("sample_count") for item in profiles] == [1, 2, 4] and case.get("required_case_count") == 18, "sample_profile_or_case_count_invalid")
    require(case.get("output_only_repair_probe_coordinates_not_algorithm_inputs") == [[783, 413], [892, 491], [940, 525]], "repair_probe_inventory_invalid")
    require(case.get("ordered_source_probe_count") == 81 and case.get("ordered_source_probe_digest") == "33511aa46717a208f3db43acd278416b8fe8393e2e003484da8f44dabffe5645" and len(case.get("exact_repair_probe_ids_in_order", [])) == 3, "source_identity_invalid")

    descriptors = value.get("descriptor_source_contract", {})
    require("cannot alone prove" in descriptors.get("ledger_limitation", "") and "No production or CMake edit" in descriptors.get("authorized_reconstruction", ""), "descriptor_reconstruction_boundary_invalid")
    require(len(descriptors.get("descriptor_fields", [])) == 10 and len(descriptors.get("generation_forbidden_inputs", [])) == 6, "descriptor_field_or_forbidden_input_invalid")
    predicate = value.get("strict_intersection_predicate", {})
    require("fails closed" in predicate.get("nonparallel_rule", "") and "no tunable angle threshold" in predicate.get("nonparallel_rule", ""), "nonparallel_rule_invalid")
    require("strictly inside" in predicate.get("strict_interior_rule", "") and predicate.get("accepted_event_classes") == ["strict_interior_nonparallel_actual_by_predicted_dash_on"], "strict_interior_rule_invalid")
    infeasible = predicate.get("explicit_infeasible_classes", [])
    require(len(infeasible) == 7 and any("collinear" in item for item in infeasible) and any("dash gap" in item for item in infeasible) and any("zero qualifying" in item for item in infeasible), "infeasible_class_inventory_invalid")

    matching = value.get("repair_region_matching_boundary", {})
    require(matching.get("generation_is_full_frame_and_coordinate_free") is True and matching.get("any_single_case_failure_forces_overall_fail") is True, "coordinate_free_or_fail_closed_matching_invalid")
    require(matching.get("overall_feasible_result") == "FEASIBLE_FOR_A_LATER_SEPARATE_IMPLEMENTATION_GATE" and matching.get("overall_fail_result") == "EXPLICIT_NON_OCCLUDING_GRAMMAR_INFEASIBLE_FOR_FROZEN_THREE_REGIONS", "diagnostic_outcome_invalid")
    topology = value.get("diagnostic_only_bracket_topology", {})
    require("not product geometry" in topology.get("purpose", "") and "outside the complete predicted" in topology.get("shape", "") and topology.get("cpu_graph_decoder_is_human_comprehension_proof") is False, "diagnostic_topology_boundary_invalid")
    require(len(topology.get("required_cpu_checks", [])) == 5, "cpu_topology_check_inventory_invalid")
    require(value.get("allowed_cpu_diagnostic_files") == ALLOWED, "allowed_file_inventory_invalid")
    require(len(value.get("required_fail_closed_fixtures", [])) == 12, "fail_closed_fixture_inventory_invalid")
    later = value.get("later_implementation_preconditions_not_authorized_here", {})
    require(len(later) == 7 and all(item is True for item in later.values()), "later_preconditions_invalid")
    require(len(value.get("forbidden", [])) == 5, "forbidden_inventory_invalid")
    auth = value.get("authorization_boundary", {})
    require(auth.get("root_product_decision_selected") is True and auth.get("cpu_diagnostic_files_authorized") is True, "diagnostic_authority_invalid")
    require(len(auth) == 18 and all(item is False for key, item in auth.items() if key not in {"root_product_decision_selected", "cpu_diagnostic_files_authorized"}), "production_or_gpu_authority_invalid")
    proof = value.get("proof_boundary", {})
    require(proof.get("design_gate_only") is True and len(proof) == 12 and all(item is False for key, item in proof.items() if key != "design_gate_only"), "proof_boundary_invalid")
    return errors


def main() -> int:
    target = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT
    errors = validate(target)
    if errors:
        for error in errors:
            print("FAIL: " + error, file=sys.stderr)
        return 1
    print("PASS_PHASE4_V17_EXPLICIT_CROSSING_GRAMMAR_CPU_FEASIBILITY_DIAGNOSTIC_GATE")
    print("decision=selected cpu_diagnostic=true production=false cmake=false gpu=false cases=18")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
