#!/usr/bin/env python3
"""Fail-closed validator for the standalone Phase4 V18 CPU diagnosis."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "docs/art/phase4-v18-projected-near-coaxial-grammar-feasibility-diagnostic-v1"
RECEIPT_PATH = EVIDENCE / "feasibility-receipt-v1.json"
COMPONENT_PATH = EVIDENCE / "component-ledger-v1.json"
RASTER_PATH = EVIDENCE / "raster-ledger-v1.json"
SOURCE_PATH = EVIDENCE / "phase4_v18_projected_near_coaxial_feasibility_cpu_diagnostic.cpp"

EXPECTED_BINDINGS = {
    "v18_gate": ("docs/art/phase4-v18-projected-near-coaxial-non-occluding-grammar-cpu-diagnostic-gate-v1.json", "33fc72c744717a6fa3f5c9953eed49eaf449487b22a5cb9fbb8922934c4cefe7"),
    "v18_gate_validator": ("scripts/validate_phase4_v18_projected_near_coaxial_non_occluding_grammar_cpu_diagnostic_gate.py", "2051d509dd78daff3402a20f17fec673d5665c28ee2045871e1595eff58991c2"),
    "v18_gate_tests": ("tests/test_phase4_v18_projected_near_coaxial_non_occluding_grammar_cpu_diagnostic_gate.py", "8557aa8239df3226296a52fb7bea20363f3bbfa8f3dbca7089c43eecada4e8ff"),
    "signed_normal_correction_gate": ("docs/art/phase4-v18-signed-normal-axis-cpu-diagnostic-interpretation-correction-gate-v1.json", "b5011c0e7f765b958631d5fbd5750dc3bd9de130fd2dd3c12b5018976906798f"),
    "signed_normal_correction_validator": ("scripts/validate_phase4_v18_signed_normal_axis_cpu_diagnostic_interpretation_correction_gate.py", "a6e61a3a5cc4a84877e2ed3b84a3b3d38f8fb6de9863cba760f9af7b95c1dee3"),
    "signed_normal_correction_tests": ("tests/test_phase4_v18_signed_normal_axis_cpu_diagnostic_interpretation_correction_gate.py", "aa832f99670efe88b9b1e0c230cc5fcd466f5597be6a6c3cc758a7d44814ca2d"),
    "v17_failure_addendum": ("docs/art/phase4-v17-explicit-crossing-grammar-independent-ta-feasibility-failure-addendum-v1.json", "150d1d8f1cb3cf0b43935a01a795d56ceadc51a6df330d0a68e4d01f51a36b3c"),
    "v17_v2_receipt": ("docs/art/phase4-v17-explicit-crossing-grammar-feasibility-diagnostic-v2/feasibility-receipt-v2.json", "03caa540de8d6f297f30e97ef3f8e69d22a8400521577aac470894131778594a"),
    "diagnostic_source": ("docs/art/phase4-v18-projected-near-coaxial-grammar-feasibility-diagnostic-v1/phase4_v18_projected_near_coaxial_feasibility_cpu_diagnostic.cpp", "a05396941065526bc29cf01c1b4859d12c3076a3dbe49e88fd770f47eb0bd32d"),
    "component_ledger": ("docs/art/phase4-v18-projected-near-coaxial-grammar-feasibility-diagnostic-v1/component-ledger-v1.json", "2102de09e9665f0aeb2d089862a2f82052574d00f46c66eae5e66c7e8b481c09"),
    "raster_ledger": ("docs/art/phase4-v18-projected-near-coaxial-grammar-feasibility-diagnostic-v1/raster-ledger-v1.json", "5c817b951d1f14e5b89c3f48e03e92cb0b2a4a51676860ae2cef82957d3615b1"),
    "trajectory_renderer_cpp_baseline": ("src/Billiards/training/trajectory_overlay_renderer.cpp", "0c108c506641ab4828290a1a4c7b6b5ebbd51ab4fc7c12845bde0f5deb18a840"),
    "trajectory_renderer_h_baseline": ("src/Billiards/training/trajectory_overlay_renderer.h", "9805e6a5f56c17d7645200576505425cddf0d642970e5dc5cec186e003bd564e"),
    "modern_renderer_cpp_baseline": ("src/Billiards/render/modern/modern_renderer.cpp", "b00cc6b370122275a42f106967f935dbb34bd5d3f81f2a750ca9ba6e503af191"),
    "modern_renderer_h_baseline": ("src/Billiards/render/modern/modern_renderer.h", "936de5b77c9e3485f2e51f10e9de26c59c8ec94674865e0b50ae125725183c9d"),
    "cmake_baseline": ("CMakeLists.txt", "f41563c35224dd809f64acedba6455355e70082697c41305c2d1b05426f71d41"),
    "diagnostic_validator": ("scripts/validate_phase4_v18_projected_near_coaxial_grammar_feasibility.py", None),
    "diagnostic_tests": ("tests/test_phase4_v18_projected_near_coaxial_grammar_feasibility.py", None),
}

PROFILE_EXPECTED = [
    ("center_1x", 1, 243, 204, 4634, 4347, 476, 4, 4, 0, 0, "1df36b39d9778c54875759193f6f15b23b90194b2bb46f6ed56a907ded2b617f"),
    ("asymmetric_2x", 2, 243, 204, 9221, 8626, 956, 280, 266, 263, 249, "5ee96753892fbe6ae1d9ce4f86d95d8f3f5776af5509f380a6ccd906306fc744"),
    ("asymmetric_4x", 4, 243, 204, 18451, 17272, 1904, 832, 791, 791, 750, "4bda9a03eec13bd2647426fb0941638cb5d28f3de66fdfecbb5d7b8ea0d4d6bf"),
]
EMPTY_EVALUATED = "2a53d331726eb68da715243483143cda9eb1f70a708c8e90c5738be9880f0553"
EMPTY_VALID = "85fdd3431d28ee276fc2c823fd0650e9558a975dac20a92344df08d6053adf3c"
PACKAGE_DIGEST = "9702bcf10fcff0a0339ac6618177eef38e06bc01e69503eed9a3b8209ccc218c"
BOUNDED_CASE_COUNTS = [1, 0, 0, 44, 46, 25, 119, 147, 70] * 2
REPAIR_IDS = [
    "d6d850a1a10c7c7ce403e6de4afd4330e4c6e89b9100c72f4c23fbab1f6f978b",
    "1890fd0652c8f74a8b03a35a6cee1ad2cab43062987cab73efb1f6b455da468a",
    "1bf60bcd841a0d754c82876b55a4d3d6c29ebad582e069d4f4a608a4778c33a9",
]


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def add(errors: list[str], condition: bool, name: str) -> None:
    if not condition:
        errors.append(name)


def link_inputs(receipt: dict[str, Any], errors: list[str]) -> list[Path]:
    contract = receipt.get("reproducible_cpu_contract", {})
    link_path = ROOT / str(contract.get("link_recipe_path", ""))
    add(errors, link_path.is_file(), "link_recipe_missing")
    if not link_path.is_file():
        return []
    add(errors, sha(link_path) == contract.get("link_recipe_sha256"), "link_recipe_sha_invalid")
    result: list[Path] = []
    for item in link_path.read_text().split():
        if not item.endswith(".o") or "tests/phase4_system_tests.cpp.o" in item or "trajectory_overlay_renderer.cpp.o" in item:
            continue
        result.append(ROOT / "build-phase4-v16-continuity-cpu" / item)
    add(errors, len(result) == contract.get("link_input_count") == 49, "link_input_count_invalid")
    canonical = "".join(f"{sha(path)}  {path.relative_to(ROOT)}\n" for path in result if path.is_file())
    add(errors, len(result) == sum(path.is_file() for path in result), "link_input_missing")
    add(errors, hashlib.sha256(canonical.encode()).hexdigest() == contract.get("ordered_link_input_ledger_sha256"), "link_input_ledger_invalid")
    return result


def validate(receipt: dict[str, Any] | None = None,
             component: dict[str, Any] | None = None,
             raster: dict[str, Any] | None = None,
             replay: bool = True) -> list[str]:
    errors: list[str] = []
    try:
        receipt = receipt if receipt is not None else json.loads(RECEIPT_PATH.read_text())
        component = component if component is not None else json.loads(COMPONENT_PATH.read_text())
        raster = raster if raster is not None else json.loads(RASTER_PATH.read_text())
    except Exception as exc:
        return [f"json_load_failed:{exc}"]

    add(errors, receipt.get("schema") == "billiardgl.phase4.v18.projected_near_coaxial_grammar_feasibility_receipt.v1", "receipt_schema_invalid")
    add(errors, receipt.get("status") == "CPU_DIAGNOSTIC_COMPLETE_GRAMMAR_INFEASIBLE_NO_PRODUCT_AUTHORITY", "receipt_status_invalid")
    add(errors, receipt.get("conclusion") == "PROJECTED_NEAR_COAXIAL_GRAMMAR_INFEASIBLE_FOR_FROZEN_THREE_REGIONS", "receipt_conclusion_invalid")
    add(errors, receipt.get("route_lattice_rule") == "All exact IEEE-754 projected T values derived from covered actual route-body core samples, ordered and deduplicated by exact double equality; no integer-pixel longitudinal rounding.", "route_lattice_rule_invalid")
    add(errors, receipt.get("both_safe_formula") == "bounded - (missing_leave + missing_rejoin - missing_both)", "both_safe_formula_invalid")
    add(errors, receipt.get("exact_case_count") == 18 and receipt.get("exact18_candidate_match_counts") == [0] * 18, "receipt_exact18_invalid")
    add(errors, receipt.get("ordered_source_probe_count") == 81 and receipt.get("ordered_source_probe_digest") == "33511aa46717a208f3db43acd278416b8fe8393e2e003484da8f44dabffe5645", "receipt_probe_inventory_invalid")
    add(errors, receipt.get("repair_ids_in_order") == REPAIR_IDS, "receipt_repair_ids_invalid")
    add(errors, receipt.get("empty_evaluated_candidate_universe_digest") == EMPTY_EVALUATED and receipt.get("empty_valid_candidate_inventory_digest") == EMPTY_VALID, "receipt_empty_digest_invalid")
    receipt_profiles = receipt.get("sample_profiles", [])
    add(errors, len(receipt_profiles) == 3, "receipt_profile_count_invalid")
    for index, expected in enumerate(PROFILE_EXPECTED):
        if index >= len(receipt_profiles):
            continue
        item = receipt_profiles[index]
        receipt_tuple = (item.get("id"), item.get("samples"), item.get("actual_members"), item.get("predicted_members"), item.get("coverage_cells"), item.get("complete_protected_predicted_samples"), item.get("components"), item.get("bounded"), item.get("missing_leave"), item.get("missing_rejoin"), item.get("missing_both"), item.get("component_inventory_digest"))
        add(errors, receipt_tuple == expected, f"receipt_profile_invalid:{expected[0]}")
        add(errors, item.get("both_safe") == 0 and item.get("evaluated_candidates") == item.get("valid_candidates") == item.get("minimum_candidates") == 0, f"receipt_profile_candidate_invalid:{expected[0]}")
    boundary = receipt.get("authorization_boundary", {})
    add(errors, boundary and not any(boundary.values()), "authorization_boundary_invalid")

    bindings = receipt.get("bindings", [])
    binding_map = {item.get("role"): (item.get("path"), item.get("sha256")) for item in bindings if isinstance(item, dict)}
    add(errors, set(binding_map) == set(EXPECTED_BINDINGS), "binding_role_set_invalid")
    for role, (path_text, pinned_digest) in EXPECTED_BINDINGS.items():
        declared_path, declared_digest = binding_map.get(role, (None, None))
        add(errors, declared_path == path_text, f"binding_path_invalid:{role}")
        add(errors, isinstance(declared_digest, str) and len(declared_digest) == 64, f"binding_declared_sha_invalid:{role}")
        path = ROOT / path_text
        add(errors, path.is_file(), f"binding_missing:{role}")
        if path.is_file():
            add(errors, sha(path) == declared_digest, f"binding_sha_invalid:{role}")
            if pinned_digest is not None:
                add(errors, declared_digest == pinned_digest, f"binding_pinned_sha_invalid:{role}")

    add(errors, component.get("schema") == "billiardgl.phase4.v18.projected_near_coaxial_component_ledger.v1", "component_schema_invalid")
    for flag in ("contrast_probe_used_for_generation", "repair_id_used_for_generation", "coordinate_used_for_generation"):
        add(errors, component.get(flag) is False, f"component_generation_flag_invalid:{flag}")
    palettes = component.get("palettes", [])
    add(errors, [item.get("palette") for item in palettes] == ["standard", "color_vision_safe"], "palette_order_invalid")
    if len(palettes) == 2:
        add(errors, palettes[0].get("profiles") == palettes[1].get("profiles"), "standard_cvd_component_inventory_mismatch")
    for palette in palettes:
        profiles = palette.get("profiles", [])
        add(errors, len(profiles) == 3, "profile_count_invalid")
        for index, expected in enumerate(PROFILE_EXPECTED):
            if index >= len(profiles):
                continue
            profile = profiles[index]
            keys = ("id", "sample_count", "actual_exact_member_count", "predicted_exact_member_count", "coverage_cell_count", "complete_protected_predicted_sample_count", "relationship_component_count", "bounded_component_count", "bounded_missing_leave_position_count", "bounded_missing_rejoin_position_count", "bounded_missing_both_positions_count", "component_inventory_digest")
            add(errors, tuple(profile.get(key) for key in keys) == expected, f"profile_metrics_invalid:{palette.get('palette')}:{expected[0]}")
            bounded = profile.get("bounded_component_count")
            leave = profile.get("bounded_missing_leave_position_count")
            rejoin = profile.get("bounded_missing_rejoin_position_count")
            both = profile.get("bounded_missing_both_positions_count")
            add(errors, bounded - (leave + rejoin - both) == 0, f"both_safe_formula_nonzero:{palette.get('palette')}:{expected[0]}")
            for field in ("minimum_candidate_count", "evaluated_candidate_count", "valid_candidate_count", "protected_overlap_rejected_candidate_count", "graph_safety_rejected_candidate_count"):
                add(errors, profile.get(field) == 0, f"profile_candidate_count_nonzero:{expected[0]}:{field}")
            add(errors, profile.get("package_before_digest") == profile.get("package_after_digest") == PACKAGE_DIGEST, f"package_digest_invalid:{expected[0]}")
            components = profile.get("components", [])
            add(errors, len(components) == expected[6], f"component_list_count_invalid:{expected[0]}")
            disposition_sum = sum(profile.get("candidate_disposition_counts", {}).values())
            add(errors, disposition_sum == len(components), f"disposition_partition_invalid:{expected[0]}")
            bounded_components = [item for item in components if item.get("bounded")]
            add(errors, len(bounded_components) == expected[7], f"bounded_component_list_invalid:{expected[0]}")
            for item in bounded_components:
                add(errors, item.get("candidate_failure_reason") == "no_safe_actual-core_leave_or_rejoin_lattice_position", f"bounded_disposition_invalid:{expected[0]}")
                add(errors, item.get("evaluated_candidate_count") == item.get("valid_candidate_count") == item.get("candidate_count") == 0, f"bounded_candidate_nonzero:{expected[0]}")
                add(errors, item.get("evaluated_candidate_universe_digest") == EMPTY_EVALUATED and item.get("valid_candidate_inventory_digest") == EMPTY_VALID, f"bounded_empty_digest_invalid:{expected[0]}")
                add(errors, item.get("maximum_shell_index") == 1920 and len(item.get("route_lattice_digest", "")) == 64, f"bounded_lattice_contract_invalid:{expected[0]}")
                add(errors, item.get("leave_lattice_position_count") == 0 or item.get("rejoin_lattice_position_count") == 0, f"bounded_both_safe_unexpected:{expected[0]}")
    for key in ("top_left_y_down_half_open_raster_fixtures_1x_2x_4x", "synthetic_midpoint_protected_nearest_lattice_safe", "synthetic_exact_projection_avoids_rounding_loss", "synthetic_exact_projection_order_deterministic", "synthetic_cross_side_equal_best_fails_closed", "synthetic_graph_invalid_cannot_outrank_valid", "synthetic_removing_side_changes_digest"):
        add(errors, component.get(key) is True, f"fixture_missing:{key}")
    add(errors, component.get("synthetic_fixture_count") == 33, "synthetic_fixture_count_invalid")

    add(errors, raster.get("schema") == "billiardgl.phase4.v18.projected_near_coaxial_raster_ledger.v1", "raster_schema_invalid")
    cases = raster.get("cases", [])
    add(errors, raster.get("case_count") == len(cases) == 18, "raster_case_count_invalid")
    for index, case in enumerate(cases):
        palette = "standard" if index < 9 else "color_vision_safe"
        within = index % 9
        profile = PROFILE_EXPECTED[within // 3][0]
        region = within % 3
        add(errors, (case.get("case_ordinal"), case.get("palette"), case.get("sample_profile"), case.get("repair_region_index"), case.get("repair_probe_id")) == (index + 1, palette, profile, region, REPAIR_IDS[region]), f"case_identity_invalid:{index + 1}")
        add(errors, case.get("matching_bounded_component_count") == BOUNDED_CASE_COUNTS[index], f"case_bounded_count_invalid:{index + 1}")
        add(errors, case.get("matching_candidate_component_count") == 0 and case.get("feasible") is False, f"case_candidate_result_invalid:{index + 1}")
    add(errors, raster.get("overall_result") == "PROJECTED_NEAR_COAXIAL_GRAMMAR_INFEASIBLE_FOR_FROZEN_THREE_REGIONS", "raster_overall_invalid")
    add(errors, raster.get("standard_cvd_topology_identical") is True and raster.get("original_draw_package_unchanged") is True, "raster_regression_invalid")
    for key in ("semantic_batch_growth", "semantic_token_growth", "source_probe_growth", "primitive_growth", "arc_growth"):
        add(errors, raster.get(key) == 0, f"growth_nonzero:{key}")
    add(errors, raster.get("gpu_run") is False, "gpu_run_invalid")

    text = SOURCE_PATH.read_text()
    required_source = ["exact_ieee754_double_projection_t", "for (double side : {-1.0, 1.0})", "cross_side_equal_best_geometry_rank", "candidateSafetyEligible(candidate)", "topLeftRasterFixtures", "pointTouchesProtected", "buildFullFrameInventory", "matchCases"]
    for token in required_source:
        add(errors, token in text, f"source_contract_missing:{token}")
    add(errors, text.index("candidateSafetyEligible(candidate)") < text.index("++output.validCandidateCount"), "safety_after_valid_count")
    add(errors, text.index("buildFullFrameInventory") < text.index("matchCases"), "matcher_before_inventory")

    inputs = link_inputs(receipt, errors)
    if replay and not errors:
        contract = receipt["reproducible_cpu_contract"]
        with tempfile.TemporaryDirectory(prefix="phase4-v18-validate-") as temp_text:
            temp = Path(temp_text)
            obj = temp / "diagnostic.o"
            binary = temp / "diagnostic"
            compile_cmd = [contract["compiler"], *contract["compile_arguments"], str(SOURCE_PATH), "-o", str(obj)]
            compiled = subprocess.run(compile_cmd, cwd=ROOT, capture_output=True)
            add(errors, compiled.returncode == 0 and compiled.stderr == b"", "fresh_compile_failed")
            if obj.is_file():
                add(errors, sha(obj) == contract.get("diagnostic_object_sha256"), "fresh_object_sha_invalid")
            if not errors:
                linked = subprocess.run([contract["compiler"], *contract["link_arguments"], str(obj), *map(str, inputs), "-o", str(binary)], cwd=ROOT, capture_output=True)
                add(errors, linked.returncode == 0 and linked.stderr == b"", "fresh_link_failed")
            if binary.is_file():
                add(errors, sha(binary) == contract.get("diagnostic_binary_sha256"), "fresh_binary_sha_invalid")
            for mode, frozen in (("--component-ledger", COMPONENT_PATH), ("--raster-ledger", RASTER_PATH)):
                if not binary.is_file():
                    break
                run = subprocess.run([str(binary), mode], cwd=ROOT, capture_output=True)
                add(errors, run.returncode == 0 and run.stderr == b"", f"fresh_replay_failed:{mode}")
                add(errors, run.stdout == frozen.read_bytes(), f"fresh_replay_bytes_invalid:{mode}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--skip-replay", action="store_true")
    args = parser.parse_args()
    errors = validate(replay=not args.skip_replay)
    print(json.dumps({"schema":"billiardgl.phase4.v18.projected_near_coaxial_grammar_feasibility_validation.v1","valid":not errors,"errors":errors}, separators=(",", ":")))
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
