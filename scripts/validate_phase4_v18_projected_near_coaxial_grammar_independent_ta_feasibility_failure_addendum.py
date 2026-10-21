#!/usr/bin/env python3
"""Validate the independent Phase4 V18 feasibility failure addendum."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import stat
import sys


ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/art/phase4-v18-projected-near-coaxial-grammar-independent-ta-feasibility-failure-addendum-v1.json"
SCHEMA = "billiardgl.phase4.v18.projected_near_coaxial_grammar_independent_ta_feasibility_failure_addendum.v1"
STATUS = "INDEPENDENT_FAIL_SELECTED_GRAMMAR_INFEASIBLE_CPU_ONLY_NO_PRODUCTION_NO_GPU_NO_AUTH"
BINDINGS = {
    "docs/art/phase4-v18-projected-near-coaxial-grammar-feasibility-diagnostic-v1/feasibility-receipt-v1.json": "664d7b10875202571ad869fc477b5fd35b27994280217ad02ebff08e0ffe0398",
    "docs/art/phase4-v18-projected-near-coaxial-grammar-feasibility-diagnostic-v1/phase4_v18_projected_near_coaxial_feasibility_cpu_diagnostic.cpp": "a05396941065526bc29cf01c1b4859d12c3076a3dbe49e88fd770f47eb0bd32d",
    "docs/art/phase4-v18-projected-near-coaxial-grammar-feasibility-diagnostic-v1/component-ledger-v1.json": "2102de09e9665f0aeb2d089862a2f82052574d00f46c66eae5e66c7e8b481c09",
    "docs/art/phase4-v18-projected-near-coaxial-grammar-feasibility-diagnostic-v1/raster-ledger-v1.json": "5c817b951d1f14e5b89c3f48e03e92cb0b2a4a51676860ae2cef82957d3615b1",
    "scripts/validate_phase4_v18_projected_near_coaxial_grammar_feasibility.py": "86a32caa15c2ed73b80f831b3e40c501504b8063c0628d505c474b42d687fa36",
    "tests/test_phase4_v18_projected_near_coaxial_grammar_feasibility.py": "8e665d783e481e979ef753d7f61568bbce09b68c3525465ef5ab7e1d2ba752b0",
    "docs/art/phase4-v18-projected-near-coaxial-non-occluding-grammar-cpu-diagnostic-gate-v1.json": "33fc72c744717a6fa3f5c9953eed49eaf449487b22a5cb9fbb8922934c4cefe7",
    "docs/art/phase4-v18-signed-normal-axis-cpu-diagnostic-interpretation-correction-gate-v1.json": "b5011c0e7f765b958631d5fbd5750dc3bd9de130fd2dd3c12b5018976906798f",
    "docs/art/phase4-v17-explicit-crossing-grammar-independent-ta-feasibility-failure-addendum-v1.json": "150d1d8f1cb3cf0b43935a01a795d56ceadc51a6df330d0a68e4d01f51a36b3c",
    "docs/art/phase4-v16-local-cross-track-actual-continuity-independent-ta-implementation-failure-addendum-v1.json": "6ccbb8fc7a149c146b755709a41558bd98a2592cf353f7a639660f1c910ce8ef",
}
BASELINES = {
    "src/Billiards/training/trajectory_overlay_renderer.cpp": "0c108c506641ab4828290a1a4c7b6b5ebbd51ab4fc7c12845bde0f5deb18a840",
    "src/Billiards/training/trajectory_overlay_renderer.h": "9805e6a5f56c17d7645200576505425cddf0d642970e5dc5cec186e003bd564e",
    "src/Billiards/render/modern/modern_renderer.cpp": "b00cc6b370122275a42f106967f935dbb34bd5d3f81f2a750ca9ba6e503af191",
    "src/Billiards/render/modern/modern_renderer.h": "936de5b77c9e3485f2e51f10e9de26c59c8ec94674865e0b50ae125725183c9d",
    "CMakeLists.txt": "f41563c35224dd809f64acedba6455355e70082697c41305c2d1b05426f71d41",
}
PROFILES = [
    ("center_1x", 1, 243, 204, 4347, 476, 4, 4, 0, 0),
    ("asymmetric_2x", 2, 243, 204, 8626, 956, 280, 266, 263, 249),
    ("asymmetric_4x", 4, 243, 204, 17272, 1904, 832, 791, 791, 750),
]
EMPTY_EVALUATED = "2a53d331726eb68da715243483143cda9eb1f70a708c8e90c5738be9880f0553"
EMPTY_VALID = "85fdd3431d28ee276fc2c823fd0650e9558a975dac20a92344df08d6053adf3c"
BOUNDED_CASE_COUNTS = [1, 0, 0, 44, 46, 25, 119, 147, 70] * 2
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
        receipt = json.loads((ROOT / next(key for key in BINDINGS if key.endswith("feasibility-receipt-v1.json"))).read_text())
        component = json.loads((ROOT / next(key for key in BINDINGS if key.endswith("component-ledger-v1.json"))).read_text())
        raster = json.loads((ROOT / next(key for key in BINDINGS if key.endswith("raster-ledger-v1.json"))).read_text())
    except Exception as error:
        return [f"addendum_or_evidence_invalid:{error}"]

    def require(condition: bool, message: str) -> None:
        if not condition:
            errors.append(message)

    require(value.get("schema") == SCHEMA and value.get("status") == STATUS, "schema_or_status_invalid")
    require("frozen broadcast repair regions" in value.get("scope", "") and "does not implement or select a successor" in value.get("scope", ""), "scope_boundary_invalid")
    observed = {item.get("path"): item.get("sha256") for item in value.get("frozen_bindings", [])}
    require(observed == BINDINGS, "binding_inventory_or_sha_invalid")
    for relative, digest in BINDINGS.items():
        target = ROOT / relative
        require(target.is_file() and sha(target) == digest, f"binding_missing_or_drift:{relative}")

    frozen_six = [
        "docs/art/phase4-v18-projected-near-coaxial-grammar-feasibility-diagnostic-v1/feasibility-receipt-v1.json",
        "docs/art/phase4-v18-projected-near-coaxial-grammar-feasibility-diagnostic-v1/phase4_v18_projected_near_coaxial_feasibility_cpu_diagnostic.cpp",
        "docs/art/phase4-v18-projected-near-coaxial-grammar-feasibility-diagnostic-v1/component-ledger-v1.json",
        "docs/art/phase4-v18-projected-near-coaxial-grammar-feasibility-diagnostic-v1/raster-ledger-v1.json",
        "scripts/validate_phase4_v18_projected_near_coaxial_grammar_feasibility.py",
        "tests/test_phase4_v18_projected_near_coaxial_grammar_feasibility.py",
    ]
    require(all(stat.S_IMODE((ROOT / item).stat().st_mode) == 0o444 for item in frozen_six), "v18_final_evidence_not_read_only_0444")

    restored = {item.get("path"): item.get("sha256") for item in value.get("restored_baselines", {}).values()}
    require(restored == BASELINES, "baseline_inventory_or_sha_invalid")
    for relative, digest in BASELINES.items():
        require((ROOT / relative).is_file() and sha(ROOT / relative) == digest, f"baseline_missing_or_drift:{relative}")

    audit = value.get("independent_reproduction_audit", {})
    require(audit.get("final_evidence_validator_and_tests_read_only_0444") is True and audit.get("validator_fresh_recompile_relink_byte_replay") == "PASS_ERRORS_EMPTY", "reproduction_or_freeze_invalid")
    require(audit.get("fail_closed_test_count") == audit.get("fail_closed_test_pass_count") == 73, "test_count_invalid")
    require(audit.get("coordinate_output_only_full_frame_generation") is True and audit.get("probe_repair_id_and_coordinates_used_only_after_both_palette_inventories_frozen") is True, "generation_boundary_invalid")
    require("finalOwnershipToken" in audit.get("eligibility_provenance", "") and "ordered exact members retained" in audit.get("eligibility_provenance", ""), "provenance_invalid")
    require(audit.get("source_token_batch_primitive_or_arc_growth") == 0 and audit.get("top_left_y_down_half_open_raster_fixtures_1x_2x_4x") is True, "growth_or_raster_invalid")
    require(audit.get("exact_projected_t_route_lattice_without_integer_rounding") is True and audit.get("both_signed_normal_sides_generated_before_matching") is True and audit.get("cross_side_equal_best_geometry_rank_fails_closed") is True and audit.get("candidate_safety_precedes_ranking") is True, "lattice_or_signed_selection_invalid")
    require(audit.get("ordered_contrast_probe_count") == 81 and audit.get("ordered_contrast_probe_digest") == "33511aa46717a208f3db43acd278416b8fe8393e2e003484da8f44dabffe5645" and audit.get("exact_repair_probe_ids_in_order") == REPAIRS, "post_generation_probe_inventory_invalid")
    require(audit.get("palettes") == ["standard", "color_vision_safe"] and audit.get("sample_profiles") == [item[0] for item in PROFILES] and audit.get("exact_case_count") == 18, "case_matrix_invalid")
    require(audit.get("standard_cvd_full_component_ledgers_byte_equivalent_per_profile") is True and audit.get("one_two_four_sample_metrics_are_real_not_relabels") is True, "palette_or_sample_audit_invalid")

    findings = value.get("independent_profile_findings", [])
    require(len(findings) == 3, "profile_finding_count_invalid")
    for index, expected in enumerate(PROFILES):
        if index >= len(findings):
            continue
        item = findings[index]
        actual = (
            item.get("id"), item.get("sample_count"), item.get("actual_exact_member_count"),
            item.get("predicted_exact_member_count"), item.get("complete_protected_predicted_sample_count"),
            item.get("relationship_component_count"), item.get("bounded_component_count"),
            item.get("bounded_missing_leave_count"), item.get("bounded_missing_rejoin_count"),
            item.get("bounded_missing_both_count"),
        )
        require(actual == expected, f"profile_finding_invalid:{expected[0]}")
        both_safe = expected[6] - (expected[7] + expected[8] - expected[9])
        require(both_safe == item.get("bounded_with_both_safe_anchors") == 0, f"both_safe_identity_invalid:{expected[0]}")
        require(item.get("evaluated_candidate_count") == item.get("valid_candidate_count") == item.get("minimum_candidate_count") == 0, f"profile_candidate_nonzero:{expected[0]}")

    require(receipt.get("conclusion") == "PROJECTED_NEAR_COAXIAL_GRAMMAR_INFEASIBLE_FOR_FROZEN_THREE_REGIONS" and receipt.get("exact18_candidate_match_counts") == [0] * 18, "receipt_conclusion_or_exact18_invalid")
    palettes = component.get("palettes", [])
    require(len(palettes) == 2 and palettes[0].get("profiles") == palettes[1].get("profiles"), "component_palette_inventory_mismatch")
    if len(palettes) == 2:
        for index, expected in enumerate(PROFILES):
            profile = palettes[0]["profiles"][index]
            bounded = [item for item in profile.get("components", []) if item.get("bounded")]
            require(len(bounded) == expected[6], f"ledger_bounded_count_invalid:{expected[0]}")
            for item in bounded:
                require(item.get("leave_lattice_position_count") == 0 or item.get("rejoin_lattice_position_count") == 0, f"ledger_both_safe_unexpected:{expected[0]}")
                require(item.get("candidate_failure_reason") == "no_safe_actual-core_leave_or_rejoin_lattice_position", f"ledger_failure_reason_invalid:{expected[0]}")
                require(item.get("candidate_count") == item.get("evaluated_candidate_count") == item.get("valid_candidate_count") == 0, f"ledger_candidate_nonzero:{expected[0]}")
                require(item.get("evaluated_candidate_universe_digest") == EMPTY_EVALUATED and item.get("valid_candidate_inventory_digest") == EMPTY_VALID, f"ledger_empty_digest_invalid:{expected[0]}")

    cases = raster.get("cases", [])
    require(raster.get("case_count") == len(cases) == 18 and raster.get("overall_result") == "PROJECTED_NEAR_COAXIAL_GRAMMAR_INFEASIBLE_FOR_FROZEN_THREE_REGIONS", "raster_case_or_result_invalid")
    require([item.get("matching_bounded_component_count") for item in cases] == BOUNDED_CASE_COUNTS, "bounded_case_counts_invalid")
    require(all(item.get("matching_candidate_component_count") == 0 and item.get("feasible") is False for item in cases), "candidate_case_result_invalid")
    require(raster.get("standard_cvd_topology_identical") is True and raster.get("original_draw_package_unchanged") is True and raster.get("gpu_run") is False, "raster_boundary_invalid")

    failure = value.get("independent_failure_finding", {})
    require(failure.get("classification") == "V18_SELECTED_PROJECTED_NEAR_COAXIAL_PAIRED_BYPASS_GRAMMAR_INFEASIBLE", "failure_classification_invalid")
    require(failure.get("both_safe_identity") == "bounded - (missing_leave + missing_rejoin - missing_both)" and failure.get("both_safe_results_in_profile_order") == [0, 0, 0], "failure_identity_invalid")
    require(failure.get("empty_evaluated_candidate_universe_digest") == EMPTY_EVALUATED and failure.get("empty_valid_candidate_inventory_digest") == EMPTY_VALID, "failure_empty_digest_invalid")
    require(failure.get("exact18_candidate_match_counts") == [0] * 18 and failure.get("bounded_output_match_counts_standard_then_cvd") == BOUNDED_CASE_COUNTS, "failure_case_counts_invalid")
    require("same 0.5-pixel sample footprint" in failure.get("why_safe_anchor_prefilter_is_necessary_not_an_extra_threshold", "") and "no integer longitudinal rounding" in failure.get("why_adjacent_route_anchors_are_not_omitted", ""), "failure_reasoning_invalid")
    require(failure.get("synthetic_bracket_fixture_passed") is True and failure.get("synthetic_fixture_is_frozen_scene_feasibility") is False and failure.get("frozen_bracket_emitted") is False, "synthetic_boundary_invalid")
    require(failure.get("implementation_acceptance") is False and failure.get("gpu_or_readiness_may_follow") is False, "failure_authority_invalid")

    disposition = value.get("bounded_grammar_disposition", {})
    require(disposition.get("v16_local_cross_track_actual_continuity", "").startswith("CLOSED_") and disposition.get("v17_explicit_non_occluding_strict_intersection_grammar", "").startswith("CLOSED_") and disposition.get("v18_projected_near_or_coaxial_paired_bypass_grammar", "").startswith("CLOSED_"), "bounded_grammar_closure_invalid")
    require(disposition.get("production_path_remaining_under_v16_v17_v18_bounded_grammars") is False and disposition.get("all_possible_visual_grammars_proven_infeasible") is False and disposition.get("automatic_reopen_or_fallback_allowed") is False, "bounded_scope_generalization_invalid")
    require("does not evaluate" in disposition.get("why_scope_cannot_be_generalized", ""), "scope_non_generalization_reason_invalid")

    successor = value.get("successor_product_decision_requirement", {})
    require(successor.get("current_selected_grammar_may_resume_without_new_root_decision") is False and successor.get("new_root_product_decision_required") is True, "successor_decision_requirement_invalid")
    require(len(successor.get("minimum_bounded_questions_for_root", [])) == 3 and successor.get("automatic_fallback_or_scope_expansion_allowed") is False, "successor_question_or_scope_invalid")
    require(successor.get("successor_selected_here") is False and successor.get("successor_gate_created_here") is False and successor.get("implementation_authorized_here") is False, "successor_false_authorization")
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
    print("PASS_PHASE4_V18_INDEPENDENT_TA_FEASIBILITY_FAILURE_ADDENDUM")
    print("v16=closed v17=closed v18=closed all_visual_grammars=false production=false gpu=false")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
