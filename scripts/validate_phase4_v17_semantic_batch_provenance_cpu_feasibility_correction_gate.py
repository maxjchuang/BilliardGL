#!/usr/bin/env python3
"""Validate the Phase4 V17 semantic-batch provenance CPU correction gate."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/art/phase4-v17-semantic-batch-provenance-cpu-feasibility-correction-gate-v1.json"
SCHEMA = "billiardgl.phase4.v17.semantic_batch_provenance_cpu_feasibility_correction_gate.v1"
STATUS = "ROOT_APPROVED_CORRECTED_STANDALONE_CPU_DIAGNOSTIC_ONLY_NO_PRODUCTION_NO_CMAKE_NO_GPU"
STATEMENT = "ROOT CORRECTION DECISION: V17 intersection eligibility provenance is existing semantic batch/finalOwnershipToken + track + arcSegmentId + exact primitive; contrast probe IDs are post-generation regression only."
STATEMENT_SHA = "0ac89b218723cc2ba0dbd4fde130e3431cc8760f4d745006996a99f46fbb7f47"
SOURCE_DIGEST = "33511aa46717a208f3db43acd278416b8fe8393e2e003484da8f44dabffe5645"
BINDINGS = {
    "docs/art/phase4-v17-explicit-non-occluding-crossing-grammar-cpu-feasibility-diagnostic-gate-v1.json": "b6cbe5deac5d947c9c607b104f8f807fc722641b7c7bf904f273183e559d3c07",
    "docs/art/phase4-v17-explicit-non-occluding-crossing-grammar-product-decision-gate-v1.json": "c08b7f2c15fa9f53c0ecc76c7c44f07265500b520dfa377097568296d56d8329",
    "docs/art/phase4-v16-local-cross-track-actual-continuity-independent-ta-implementation-failure-addendum-v1.json": "6ccbb8fc7a149c146b755709a41558bd98a2592cf353f7a639660f1c910ce8ef",
    "docs/art/phase4-v16-local-cross-track-actual-continuity-premise-falsification-v1/premise-failure-receipt-v1.json": "6e5c729016795a8744fddd7351ae8d9f9d584f9cde91946a33bcbd83f982fa96",
    "docs/art/phase4-v15-broadcast-overwrite-provenance-diagnostic-v1/provenance-ledger-v1.json": "bb2c3ecafb01d42cfddb8eec89a22619bf646ddb64773846747c5c76d2f5538b",
    "src/Billiards/training/trajectory_overlay_renderer.cpp": "0c108c506641ab4828290a1a4c7b6b5ebbd51ab4fc7c12845bde0f5deb18a840",
    "src/Billiards/training/trajectory_overlay_renderer.h": "9805e6a5f56c17d7645200576505425cddf0d642970e5dc5cec186e003bd564e",
    "src/Billiards/render/modern/modern_renderer.cpp": "b00cc6b370122275a42f106967f935dbb34bd5d3f81f2a750ca9ba6e503af191",
    "src/Billiards/render/modern/modern_renderer.h": "936de5b77c9e3485f2e51f10e9de26c59c8ec94674865e0b50ae125725183c9d",
    "CMakeLists.txt": "f41563c35224dd809f64acedba6455355e70082697c41305c2d1b05426f71d41",
}
ALLOWED = [
    "docs/art/phase4-v17-explicit-crossing-grammar-feasibility-diagnostic-v2/phase4_v17_crossing_feasibility_cpu_diagnostic.cpp",
    "docs/art/phase4-v17-explicit-crossing-grammar-feasibility-diagnostic-v2/feasibility-receipt-v2.json",
    "docs/art/phase4-v17-explicit-crossing-grammar-feasibility-diagnostic-v2/intersection-ledger-v2.json",
    "docs/art/phase4-v17-explicit-crossing-grammar-feasibility-diagnostic-v2/raster-ledger-v2.json",
    "scripts/validate_phase4_v17_explicit_non_occluding_crossing_grammar_feasibility_v2.py",
    "tests/test_phase4_v17_explicit_non_occluding_crossing_grammar_feasibility_v2.py",
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
    decision = value.get("root_correction_binding", {})
    require(
        decision.get("decision_id") == "V17_SEMANTIC_BATCH_PROVENANCE_CORRECTION"
        and decision.get("statement") == STATEMENT
        and decision.get("statement_sha256") == STATEMENT_SHA
        and hashlib.sha256(STATEMENT.encode()).hexdigest() == STATEMENT_SHA
        and decision.get("authorizes_corrected_standalone_cpu_diagnostic_only") is True,
        "root_correction_binding_invalid",
    )

    supersession = value.get("supersession", {})
    require(
        supersession.get("superseded_gate_sha256") == BINDINGS["docs/art/phase4-v17-explicit-non-occluding-crossing-grammar-cpu-feasibility-diagnostic-gate-v1.json"]
        and "sparse contrast-probe" in supersession.get("superseded_reason", "")
        and len(supersession.get("still_preserved_from_superseded_gate", [])) == 7,
        "supersession_invalid",
    )

    observed = {item.get("path"): item.get("sha256") for item in value.get("frozen_bindings", [])}
    require(observed == BINDINGS, "binding_inventory_or_sha_invalid")
    for relative, digest in BINDINGS.items():
        target = ROOT / relative
        require(target.is_file() and sha(target) == digest, f"binding_missing_or_drift:{relative}")

    finding = value.get("sparse_probe_failure_finding", {})
    require(
        finding.get("classification") == "PRECORRECTION_INTERIM_OBSERVATION_NOT_A_REPRODUCIBLE_FORMAL_RECEIPT"
        and finding.get("observed_actual_descriptor_count") == 81
        and finding.get("missing_exact_contrast_probe_actual_descriptor_count") == 72
        and finding.get("observed_predicted_descriptor_count") == 68
        and finding.get("missing_exact_contrast_probe_predicted_descriptor_count") == 0
        and finding.get("false_invalid_pair_count") == 4896
        and finding.get("false_invalid_pair_identity") == "72 * 68 = 4896"
        and 72 * 68 == 4896
        and finding.get("remaining_disjoint_pair_count") == 612
        and finding.get("observed_event_count") == 0,
        "sparse_probe_count_or_identity_invalid",
    )
    require(
        finding.get("source_snapshot_retained") is False
        and finding.get("formal_reproducibility_claimed") is False
        and "cannot prove geometric infeasibility" in finding.get("valid_conclusion", ""),
        "interim_observation_proof_boundary_invalid",
    )

    eligibility = value.get("corrected_eligibility_provenance_contract", {})
    fields = eligibility.get("required_identity_fields", [])
    require(
        fields == ["existing semantic batch range", "existing nonzero finalOwnershipToken identical across the batch vertices", "track", "arcSegmentId", "exact primitiveId"],
        "eligibility_identity_fields_invalid",
    )
    require(
        "not the sparse contrast-probe" in eligibility.get("source_of_truth", "")
        and "Event deduplication only" in eligibility.get("normalized_stem_use", "")
        and "never associate" in eligibility.get("normalized_stem_use", "")
        and "exact primitiveId" in eligibility.get("event_key", ""),
        "eligibility_or_dedupe_contract_invalid",
    )
    false_keys = [
        "new_semantic_batch_allowed", "new_final_ownership_token_allowed", "new_source_probe_allowed",
        "new_primitive_or_arc_id_allowed", "color_or_framebuffer_derived_provenance_allowed",
    ]
    require(all(eligibility.get(key) is False for key in false_keys) and eligibility.get("missing_foreign_or_mixed_batch_fails_closed") is True, "source_growth_or_fail_closed_invalid")

    regression = value.get("post_generation_regression_only_contract", {})
    require(regression.get("ordered_contrast_probe_count") == 81 and regression.get("ordered_contrast_probe_digest") == SOURCE_DIGEST and len(regression.get("exact_repair_probe_ids_in_order", [])) == 3, "post_generation_identity_invalid")
    require(len(regression.get("permitted_uses", [])) == 3 and len(regression.get("forbidden_uses", [])) == 5 and "descriptor eligibility" in regression.get("forbidden_uses", []), "post_generation_use_boundary_invalid")

    preserved = value.get("preserved_feasibility_contract", {})
    profiles = preserved.get("sample_profiles", [])
    require(preserved.get("generation_is_full_frame_before_repair_matching") is True and preserved.get("coordinates_are_output_only") is True, "coordinate_output_only_invalid")
    require(preserved.get("repair_coordinates") == [[783, 413], [892, 491], [940, 525]] and preserved.get("palettes") == ["standard", "color_vision_safe"], "repair_or_palette_inventory_invalid")
    require([p.get("sample_count") for p in profiles] == [1, 2, 4] and preserved.get("required_case_count") == 18, "profile_or_case_count_invalid")
    require("strict interior" in preserved.get("predicate", "") and "nonparallel" in preserved.get("predicate", "") and preserved.get("source_and_token_inventory_must_not_grow") is True, "predicate_or_growth_boundary_invalid")

    require(value.get("allowed_corrected_cpu_diagnostic_files") == ALLOWED, "allowed_file_inventory_invalid")
    require(len(value.get("required_fail_closed_fixtures", [])) == 12, "fail_closed_fixture_inventory_invalid")
    auth = value.get("authorization_boundary", {})
    require(auth.get("corrected_standalone_cpu_diagnostic_authorized") is True and len(auth) == 17 and all(flag is False for key, flag in auth.items() if key != "corrected_standalone_cpu_diagnostic_authorized"), "authorization_boundary_invalid")
    proof = value.get("proof_boundary", {})
    require(proof.get("correction_design_gate_only") is True and len(proof) == 12 and all(flag is False for key, flag in proof.items() if key != "correction_design_gate_only"), "proof_boundary_invalid")
    return errors


def main() -> int:
    target = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT
    errors = validate(target)
    if errors:
        for error in errors:
            print("FAIL: " + error, file=sys.stderr)
        return 1
    print("PASS_PHASE4_V17_SEMANTIC_BATCH_PROVENANCE_CPU_FEASIBILITY_CORRECTION_GATE")
    print("correction=semantic_batch_token_exact_primitive cases=18 production=false cmake=false gpu=false")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
