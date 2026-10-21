#!/usr/bin/env python3
"""Validate the Phase4 V18 signed-normal diagnostic interpretation gate."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/art/phase4-v18-signed-normal-axis-cpu-diagnostic-interpretation-correction-gate-v1.json"
SCHEMA = "billiardgl.phase4.v18.signed_normal_axis_cpu_diagnostic_interpretation_correction_gate.v1"
STATUS = "ROOT_CLARIFIED_STANDALONE_CPU_DIAGNOSTIC_ONLY_NO_PRODUCTION_NO_CMAKE_NO_GPU"
STATEMENT = (
    "ROOT CLARIFICATION: fixed clockwise normal defines a deterministic oriented axis; "
    "V18 must enumerate both signed +/- sides. Side cannot be selected by coordinates, "
    "probe IDs or repair IDs; candidate comparison records signed side only after the "
    "geometry/safety rank, and any equal best geometry rank across opposite sides fails closed."
)
STATEMENT_SHA = "211df29c2107e3dfee4fb27e4ba6b0d69abb11cc26016c1472614c53f93180a2"
SOURCE_DIGEST = "33511aa46717a208f3db43acd278416b8fe8393e2e003484da8f44dabffe5645"
BINDINGS = {
    "docs/art/phase4-v18-projected-near-coaxial-non-occluding-grammar-cpu-diagnostic-gate-v1.json": "33fc72c744717a6fa3f5c9953eed49eaf449487b22a5cb9fbb8922934c4cefe7",
    "docs/art/phase4-v17-explicit-crossing-grammar-independent-ta-feasibility-failure-addendum-v1.json": "150d1d8f1cb3cf0b43935a01a795d56ceadc51a6df330d0a68e4d01f51a36b3c",
    "docs/art/phase4-v17-explicit-crossing-grammar-feasibility-diagnostic-v2/feasibility-receipt-v2.json": "03caa540de8d6f297f30e97ef3f8e69d22a8400521577aac470894131778594a",
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
    root = value.get("root_clarification_binding", {})
    require(
        root.get("decision_id") == "V18_SIGNED_CLOCKWISE_NORMAL_AXIS_CLARIFICATION"
        and root.get("authority") == "ROOT_PROJECT_MANAGER"
        and root.get("statement") == STATEMENT
        and root.get("statement_sha256") == STATEMENT_SHA
        and hashlib.sha256(STATEMENT.encode()).hexdigest() == STATEMENT_SHA
        and root.get("authorizes_corrected_standalone_cpu_diagnostic_only") is True,
        "root_clarification_binding_invalid",
    )

    supersession = value.get("supersession", {})
    require(
        supersession.get("corrected_gate_path") in BINDINGS
        and supersession.get("corrected_gate_sha256") == BINDINGS[supersession.get("corrected_gate_path")]
        and supersession.get("correction_is_minimal") is True
        and "positive-side-only" in supersession.get("corrected_interpretation", "")
        and len(supersession.get("preserved_without_change", [])) == 9,
        "minimal_supersession_invalid",
    )

    observed = {item.get("path"): item.get("sha256") for item in value.get("frozen_bindings", [])}
    require(observed == BINDINGS, "binding_inventory_or_sha_invalid")
    for relative, digest in BINDINGS.items():
        target = ROOT / relative
        require(target.is_file() and sha(target) == digest, f"binding_missing_or_drift:{relative}")

    generation = value.get("signed_axis_generation_contract", {})
    require(
        generation.get("required_signed_sides_in_order") == [-1, 1]
        and "clockwise perpendicular" in generation.get("axis_construction", "")
        and "complete finite route-order leave/rejoin lattice" in generation.get("generation_order", "")
        and "canonical all-candidate digest" in generation.get("generation_order", ""),
        "signed_axis_generation_invalid",
    )
    false_generation = [
        "signed_side_is_source_identity",
        "signed_side_is_product_authority",
        "signed_side_may_depend_on_coordinates_probes_ids_palette_or_color",
        "opposite_side_candidates_may_be_silently_dropped",
        "positive_side_only_interpretation_allowed",
        "negative_side_only_interpretation_allowed",
    ]
    require(all(generation.get(key) is False for key in false_generation), "signed_axis_forbidden_input_or_side_drop_invalid")

    selection = value.get("selection_and_ambiguity_contract", {})
    require(
        selection.get("geometry_rank_in_order") == [
            "maximum absolute normal shell index",
            "total added route length",
            "vertex count",
        ]
        and "never a preferred side" in selection.get("signed_side_recording", "")
        and "before signed side or topology digest" in selection.get("cross_side_equal_best_rule", "")
        and selection.get("coordinate_or_id_tie_break_allowed") is False
        and selection.get("palette_or_color_tie_break_allowed") is False
        and selection.get("cross_side_equal_minimum_fails_closed") is True,
        "selection_or_cross_side_ambiguity_invalid",
    )

    exact = value.get("exact_case_and_regression_contract", {})
    require(
        exact.get("palettes") == ["standard", "color_vision_safe"]
        and exact.get("sample_profiles_in_order") == ["center_1x", "asymmetric_2x", "asymmetric_4x"]
        and exact.get("repair_region_count") == 3
        and exact.get("required_case_count") == 18
        and exact.get("ordered_contrast_probe_count") == 81
        and exact.get("ordered_contrast_probe_digest") == SOURCE_DIGEST
        and len(exact.get("exact_repair_probe_ids_in_order", [])) == 3
        and exact.get("coordinates_probes_and_repair_ids_are_output_only") is True
        and exact.get("standard_cvd_signed_candidate_topology_must_match") is True
        and exact.get("all_candidate_inventory_digest_required_before_matcher") is True,
        "case_or_post_generation_regression_invalid",
    )

    require(value.get("allowed_corrected_standalone_cpu_diagnostic_files") == ALLOWED, "allowed_file_inventory_invalid")
    fixtures = value.get("required_fail_closed_fixtures", [])
    require(
        len(fixtures) == 13
        and any("opposite signed sides fails closed" in fixture for fixture in fixtures)
        and any("old fixed-midpoint" in fixture for fixture in fixtures)
        and any("all valid candidates" in fixture for fixture in fixtures),
        "fail_closed_fixture_inventory_invalid",
    )

    auth = value.get("authorization_boundary", {})
    require(
        auth.get("corrected_standalone_cpu_diagnostic_authorized") is True
        and len(auth) == 17
        and all(flag is False for key, flag in auth.items() if key != "corrected_standalone_cpu_diagnostic_authorized"),
        "authorization_boundary_invalid",
    )
    proof = value.get("proof_boundary", {})
    require(
        proof.get("interpretation_correction_gate_only") is True
        and len(proof) == 12
        and all(flag is False for key, flag in proof.items() if key != "interpretation_correction_gate_only"),
        "proof_boundary_invalid",
    )
    return errors


def main() -> int:
    target = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT
    errors = validate(target)
    if errors:
        for error in errors:
            print("FAIL: " + error, file=sys.stderr)
        return 1
    print("PASS_PHASE4_V18_SIGNED_NORMAL_AXIS_CPU_DIAGNOSTIC_INTERPRETATION_CORRECTION_GATE")
    print("signed_sides=-1,+1 cases=18 production=false cmake=false gpu=false")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
