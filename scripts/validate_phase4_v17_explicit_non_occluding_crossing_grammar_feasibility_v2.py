#!/usr/bin/env python3
"""Fail-closed validator for the corrected Phase4 V17 CPU feasibility receipt."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import subprocess
import tempfile


ROOT = pathlib.Path(__file__).resolve().parents[1]
DEFAULT_RECEIPT = ROOT / "docs/art/phase4-v17-explicit-crossing-grammar-feasibility-diagnostic-v2/feasibility-receipt-v2.json"
DEFAULT_SOURCE = ROOT / "docs/art/phase4-v17-explicit-crossing-grammar-feasibility-diagnostic-v2/phase4_v17_crossing_feasibility_cpu_diagnostic.cpp"
DEFAULT_INTERSECTION = ROOT / "docs/art/phase4-v17-explicit-crossing-grammar-feasibility-diagnostic-v2/intersection-ledger-v2.json"
DEFAULT_RASTER = ROOT / "docs/art/phase4-v17-explicit-crossing-grammar-feasibility-diagnostic-v2/raster-ledger-v2.json"
V2_DIR = ROOT / "docs/art/phase4-v17-explicit-crossing-grammar-feasibility-diagnostic-v2"
EXPECTED_V2_NAMES = {
    "phase4_v17_crossing_feasibility_cpu_diagnostic.cpp",
    "feasibility-receipt-v2.json",
    "intersection-ledger-v2.json",
    "raster-ledger-v2.json",
}
EXPECTED_BASELINES = {
    "src/Billiards/training/trajectory_overlay_renderer.cpp": "0c108c506641ab4828290a1a4c7b6b5ebbd51ab4fc7c12845bde0f5deb18a840",
    "src/Billiards/training/trajectory_overlay_renderer.h": "9805e6a5f56c17d7645200576505425cddf0d642970e5dc5cec186e003bd564e",
    "src/Billiards/render/modern/modern_renderer.cpp": "b00cc6b370122275a42f106967f935dbb34bd5d3f81f2a750ca9ba6e503af191",
    "src/Billiards/render/modern/modern_renderer.h": "936de5b77c9e3485f2e51f10e9de26c59c8ec94674865e0b50ae125725183c9d",
    "CMakeLists.txt": "f41563c35224dd809f64acedba6455355e70082697c41305c2d1b05426f71d41",
}
EXPECTED_BINDINGS = {
    "correction_gate": ("docs/art/phase4-v17-semantic-batch-provenance-cpu-feasibility-correction-gate-v1.json", "601a9322d5452178499e0922e8b423ae403d89eb7e71fd3f70198e85b1855856"),
    "product_decision_gate": ("docs/art/phase4-v17-explicit-non-occluding-crossing-grammar-product-decision-gate-v1.json", "c08b7f2c15fa9f53c0ecc76c7c44f07265500b520dfa377097568296d56d8329"),
    "v16_ta_failure_addendum": ("docs/art/phase4-v16-local-cross-track-actual-continuity-independent-ta-implementation-failure-addendum-v1.json", "6ccbb8fc7a149c146b755709a41558bd98a2592cf353f7a639660f1c910ce8ef"),
    "v16_premise_failure_receipt": ("docs/art/phase4-v16-local-cross-track-actual-continuity-premise-falsification-v1/premise-failure-receipt-v1.json", "6e5c729016795a8744fddd7351ae8d9f9d584f9cde91946a33bcbd83f982fa96"),
    "v15_provenance_ledger": ("docs/art/phase4-v15-broadcast-overwrite-provenance-diagnostic-v1/provenance-ledger-v1.json", "bb2c3ecafb01d42cfddb8eec89a22619bf646ddb64773846747c5c76d2f5538b"),
    "diagnostic_source": ("docs/art/phase4-v17-explicit-crossing-grammar-feasibility-diagnostic-v2/phase4_v17_crossing_feasibility_cpu_diagnostic.cpp", "f672feddebf477ae1c4822b787a0c977b7f0f62228d55429f0bb391ca9998cf5"),
    "intersection_ledger": ("docs/art/phase4-v17-explicit-crossing-grammar-feasibility-diagnostic-v2/intersection-ledger-v2.json", "70dddabe3f760da1e80aa1779b7010441e5cbc66018ba02d6df9b390f60a1503"),
    "raster_ledger": ("docs/art/phase4-v17-explicit-crossing-grammar-feasibility-diagnostic-v2/raster-ledger-v2.json", "18258250388c015969ddf0c0de54d0fec67310a781efe94a789f871d3c54ee8a"),
    "link_input_manifest": ("docs/art/phase4-v16-local-cross-track-actual-continuity-premise-falsification-v1/inherited-link-inputs-v1.sha256", "21cf034173b9c33cd9155ed49a8fe7cfd922dc70601e28511f9e183fdf2b830a"),
    "trajectory_renderer_cpp_baseline": ("src/Billiards/training/trajectory_overlay_renderer.cpp", "0c108c506641ab4828290a1a4c7b6b5ebbd51ab4fc7c12845bde0f5deb18a840"),
    "trajectory_renderer_h_baseline": ("src/Billiards/training/trajectory_overlay_renderer.h", "9805e6a5f56c17d7645200576505425cddf0d642970e5dc5cec186e003bd564e"),
    "modern_renderer_cpp_baseline": ("src/Billiards/render/modern/modern_renderer.cpp", "b00cc6b370122275a42f106967f935dbb34bd5d3f81f2a750ca9ba6e503af191"),
    "modern_renderer_h_baseline": ("src/Billiards/render/modern/modern_renderer.h", "936de5b77c9e3485f2e51f10e9de26c59c8ec94674865e0b50ae125725183c9d"),
    "cmake_baseline": ("CMakeLists.txt", "f41563c35224dd809f64acedba6455355e70082697c41305c2d1b05426f71d41"),
}
EXPECTED_REPAIRS = [
    "d6d850a1a10c7c7ce403e6de4afd4330e4c6e89b9100c72f4c23fbab1f6f978b",
    "1890fd0652c8f74a8b03a35a6cee1ad2cab43062987cab73efb1f6b455da468a",
    "1bf60bcd841a0d754c82876b55a4d3d6c29ebad582e069d4f4a608a4778c33a9",
]
EXPECTED_COORDS = [[783, 413], [892, 491], [940, 525]]
EXPECTED_SAMPLES = {
    "center_1x": [[0.5, 0.5]],
    "asymmetric_2x": [[0.25, 0.75], [0.75, 0.25]],
    "asymmetric_4x": [[0.375, 0.875], [0.875, 0.625], [0.125, 0.375], [0.625, 0.125]],
}


def sha256(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_json(path: pathlib.Path):
    return json.loads(path.read_text())


def validate(receipt_path: pathlib.Path, source_path: pathlib.Path,
             intersection_path: pathlib.Path, raster_path: pathlib.Path,
             recompile: bool) -> list[str]:
    errors: list[str] = []

    def need(condition: bool, code: str) -> None:
        if not condition:
            errors.append(code)

    for path, code in [(receipt_path, "receipt_missing"),
                       (source_path, "source_missing"),
                       (intersection_path, "intersection_missing"),
                       (raster_path, "raster_missing")]:
        need(path.is_file(), code)
    if errors:
        return errors
    try:
        receipt = load_json(receipt_path)
        intersection = load_json(intersection_path)
        raster = load_json(raster_path)
    except Exception as exc:
        return [f"json_parse_failed:{type(exc).__name__}"]

    need(receipt.get("schema") == "billiardgl.phase4.v17.crossing_feasibility_receipt.v2", "receipt_schema_invalid")
    need(receipt.get("status") == "CORRECTED_CPU_DIAGNOSTIC_COMPLETE_STRICT_GRAMMAR_INFEASIBLE_NO_IMPLEMENTATION", "receipt_status_invalid")
    result = receipt.get("result", {})
    expected_result = {
        "classification": "EXPLICIT_NON_OCCLUDING_GRAMMAR_INFEASIBLE_FOR_FROZEN_THREE_REGIONS",
        "actual_descriptor_count": 81,
        "predicted_dash_on_descriptor_count": 68,
        "missing_actual_semantic_descriptor_count": 0,
        "missing_predicted_semantic_descriptor_count": 0,
        "evaluated_pair_count": 5508,
        "disjoint_pair_count": 5508,
        "strict_interior_nonparallel_event_count": 0,
        "exact_case_count": 18,
        "feasible_case_count": 0,
        "infeasible_case_count": 18,
        "sparse_probe_false_invalid_pair_count": 0,
    }
    for key, value in expected_result.items():
        need(result.get(key) == value, f"receipt_result_invalid:{key}")
    provenance = receipt.get("provenance_contract", {})
    need(provenance.get("eligibility") == "exact existing semantic batch range + uniform nonzero finalOwnershipToken + track + arcSegmentId + exact primitiveId + Core/Core role", "receipt_eligibility_string_invalid")
    need(provenance.get("normalized_stem_use") == "event deduplication metadata only after exact primitive association", "receipt_normalized_stem_string_invalid")
    need(provenance.get("contrast_probe_used_for_eligibility") is False, "receipt_probe_eligibility_invalid")
    need(provenance.get("repair_id_used_for_eligibility") is False, "receipt_repair_eligibility_invalid")
    need(provenance.get("coordinates_used_for_generation") is False, "receipt_coordinate_generation_invalid")
    need(provenance.get("full_frame_generation_before_probe_and_repair_audit") is True, "receipt_generation_order_invalid")
    for key, value in [("semantic_batch_count", 486), ("nonzero_final_ownership_token_count", 486),
                       ("semantic_batch_growth", 0), ("semantic_token_growth", 0), ("source_probe_growth", 0)]:
        need(provenance.get(key) == value, f"receipt_provenance_invalid:{key}")
    regression = receipt.get("post_generation_regression", {})
    need(regression.get("ordered_contrast_probe_count") == 81, "receipt_probe_count_invalid")
    need(regression.get("ordered_contrast_probe_digest") == "33511aa46717a208f3db43acd278416b8fe8393e2e003484da8f44dabffe5645", "receipt_probe_digest_invalid")
    need(regression.get("repair_probe_ids_in_order") == EXPECTED_REPAIRS, "receipt_repair_ids_invalid")
    need(regression.get("repair_coordinates_output_only") == EXPECTED_COORDS, "receipt_coordinates_invalid")
    need(regression.get("real_sample_profiles") == ["center_1x", "asymmetric_2x", "asymmetric_4x"], "receipt_sample_profiles_invalid")

    binding_map = {item.get("role"): item for item in receipt.get("bindings", [])}
    need(set(binding_map) == set(EXPECTED_BINDINGS), "receipt_binding_roles_invalid")
    for role, item in binding_map.items():
        expected = EXPECTED_BINDINGS.get(role)
        need(expected is not None and
             (item.get("path"), item.get("sha256")) == expected,
             f"binding_identity_invalid:{role}")
        path = ROOT / item.get("path", "")
        need(path.is_file(), f"binding_missing:{role}")
        if path.is_file():
            need(sha256(path) == item.get("sha256"), f"binding_hash_invalid:{role}")
    need(binding_map.get("diagnostic_source", {}).get("sha256") == sha256(source_path), "selected_source_hash_invalid")
    need(binding_map.get("intersection_ledger", {}).get("sha256") == sha256(intersection_path), "selected_intersection_hash_invalid")
    need(binding_map.get("raster_ledger", {}).get("sha256") == sha256(raster_path), "selected_raster_hash_invalid")

    for relative, expected in EXPECTED_BASELINES.items():
        need(sha256(ROOT / relative) == expected, f"production_baseline_drift:{relative}")
    need(V2_DIR.is_dir() and {p.name for p in V2_DIR.iterdir() if p.is_file()} == EXPECTED_V2_NAMES, "v2_directory_allowlist_invalid")
    need((ROOT / "scripts/validate_phase4_v17_explicit_non_occluding_crossing_grammar_feasibility_v2.py").is_file(), "validator_missing")
    need((ROOT / "tests/test_phase4_v17_explicit_non_occluding_crossing_grammar_feasibility_v2.py").is_file(), "tests_missing")

    need(intersection.get("schema") == "billiardgl.phase4.v17.crossing_intersection_ledger.v2", "intersection_schema_invalid")
    need(intersection.get("generation_scope") == "full_frame_coordinate_free_before_matcher", "intersection_generation_scope_invalid")
    expected_intersection = {
        "ordered_source_probe_count": 81,
        "ordered_source_probe_digest": "33511aa46717a208f3db43acd278416b8fe8393e2e003484da8f44dabffe5645",
        "eligibility_provenance": "exact_existing_semantic_batch_token_track_arc_primitive",
        "contrast_probe_used_for_eligibility": False,
        "post_generation_probe_audit": True,
        "semantic_batch_count": 486,
        "nonzero_final_ownership_token_count": 486,
        "semantic_batch_growth": 0,
        "semantic_token_growth": 0,
        "source_probe_growth": 0,
        "actual_descriptor_count": 81,
        "predicted_dash_on_descriptor_count": 68,
        "missing_exact_actual_semantic_descriptor_count": 0,
        "missing_exact_predicted_semantic_descriptor_count": 0,
        "event_count": 0,
        "event_inventory_digest": hashlib.sha256(b"").hexdigest(),
        "predicate_fixture_count": 33,
        "forbidden_generation_input_mutations_rejected": 8,
        "diagnostic_bracket_fixture_emitted": True,
        "diagnostic_bracket_paired": True,
        "diagnostic_bracket_zero_protected_overlap_1x_2x_4x": True,
        "diagnostic_bracket_route_graph_traceable": True,
        "diagnostic_bracket_removal_of_either_half_fails": True,
        "diagnostic_bracket_half_count": 2,
        "diagnostic_bracket_topology_digest": "8d2cef32f9a5e9ef532442f422a1444a7888ea38ce2398ad93ee032f92a915f9",
        "standard_cvd_grayscale_bracket_topology_identical": True,
        "frozen_bracket_emitted": False,
        "human_comprehension_claimed": False,
    }
    for key, value in expected_intersection.items():
        need(intersection.get(key) == value, f"intersection_invalid:{key}")
    need(intersection.get("repair_ids_preserved") == EXPECTED_REPAIRS, "intersection_repair_ids_invalid")
    need(intersection.get("rejection_counts") == {"disjoint": 5508}, "intersection_rejections_invalid")
    need(intersection.get("events") == [], "intersection_events_not_empty")

    need(raster.get("schema") == "billiardgl.phase4.v17.crossing_raster_ledger.v2", "raster_schema_invalid")
    need(raster.get("matcher_boundary") == "post_generation_output_only_v15_radius8", "raster_matcher_boundary_invalid")
    need(raster.get("sample_positions") == EXPECTED_SAMPLES, "raster_sample_positions_invalid")
    cases = raster.get("cases", [])
    need(raster.get("case_count") == 18 and len(cases) == 18, "raster_case_count_invalid")
    expected_tuples = []
    for palette in ["standard", "color_vision_safe"]:
        for profile, samples in EXPECTED_SAMPLES.items():
            for region in range(3):
                expected_tuples.append((palette, profile, len(samples), region))
    actual_tuples = [(case.get("palette"), case.get("sample_profile"), case.get("sample_count"), case.get("repair_region_index")) for case in cases]
    need(actual_tuples == expected_tuples, "raster_case_matrix_or_order_invalid")
    need([case.get("case_ordinal") for case in cases] == list(range(1, 19)), "raster_ordinals_invalid")
    metric_keys = ["actual_covered_samples", "predicted_covered_samples", "actual_predicted_overlap_samples", "actual_any_sample_pixels", "predicted_any_sample_pixels", "overlap_any_sample_pixels"]
    for index, case in enumerate(cases):
        region = index % 3
        need(case.get("repair_probe_id") == EXPECTED_REPAIRS[region], f"raster_repair_id_invalid:{index}")
        need(case.get("output_only_coordinate") == EXPECTED_COORDS[region], f"raster_coordinate_invalid:{index}")
        need(case.get("matching_event_count") == 0 and case.get("matching_event_keys") == [] and case.get("feasible") is False, f"raster_feasibility_invalid:{index}")
        for key in metric_keys:
            need(isinstance(case.get(key), int) and case.get(key) > 0, f"raster_metric_invalid:{index}:{key}")
    for index in range(9):
        std = cases[index]
        cvd = cases[index + 9]
        for key in metric_keys:
            need(std.get(key) == cvd.get(key), f"palette_metric_drift:{index}:{key}")
    for region in range(3):
        actual_samples = [cases[profile * 3 + region]["actual_covered_samples"] for profile in range(3)]
        need(len(set(actual_samples)) == 3, f"sample_profiles_relabelled:{region}")
    need(raster.get("original_actual_predicted_masks_changed") is False, "raster_original_masks_changed")
    need(raster.get("semantic_token_growth") == 0 and raster.get("source_probe_growth") == 0, "raster_inventory_growth")
    need(raster.get("diagnostic_bracket_fixture_emitted") is True, "raster_bracket_missing")
    need(raster.get("diagnostic_bracket_zero_protected_overlap_1x_2x_4x") is True, "raster_bracket_overlap")
    need(raster.get("diagnostic_bracket_removal_negative_passed") is True, "raster_bracket_removal_invalid")
    need(raster.get("diagnostic_bracket_half_count") == 2, "raster_bracket_half_count_invalid")
    need(raster.get("frozen_bracket_emitted") is False, "raster_frozen_bracket_claimed")
    need(raster.get("overall_result") == result.get("classification"), "raster_result_mismatch")

    source = source_path.read_text()
    build_start = source.find("Inventory buildInventory")
    build_end = source.find("long double vectorCross")
    main_start = source.find("int main(")
    first_generate = source.find("generateEvents(standard", main_start)
    first_source_audit = source.find("sourceIdentities(standard.package)", main_start)
    need(build_start >= 0 and build_end > build_start, "source_build_inventory_missing")
    if build_start >= 0 and build_end > build_start:
        need("sourceIdentities" not in source[build_start:build_end], "source_probe_used_during_inventory")
    need(first_generate >= 0 and first_source_audit > first_generate, "source_generation_audit_order_invalid")
    for marker in ["exactSemanticProvenance", "batch.track != track", "batch.primitiveId != primitive", "batch.arcSegmentId != arc", "batch.finalOwnershipToken == 0", "package.vertices[vertex].finalOwnershipToken", "exactBatchCount == 1", "StrictInteriorNonparallel", "determinantBound", "positiveT > tBound", "positiveU > uBound", "normalizedStem(predicted.primitiveId)"]:
        need(marker in source, f"source_contract_marker_missing:{marker}")
    intersect_start = source.find("IntersectionResult intersect")
    intersect_end = source.find("const char* className", intersect_start)
    generate_start = source.find("std::vector<Event> generateEvents")
    generate_end = source.find("bool eventInsideOutputOnlyRegion", generate_start)
    eligibility_text = source[intersect_start:generate_end]
    need("contrastProbes" not in eligibility_text and "SourceIdentity" not in eligibility_text and "kRepairIds" not in eligibility_text and "kOutputOnlyRepairCoordinates" not in eligibility_text, "source_forbidden_eligibility_dependency")
    need("nearestExistingSource" not in source and "nearest" not in source.lower(), "source_nearest_fallback_present")
    for fixture in ["fixture_contrast_probe_absent_semantic_batch_ineligible", "fixture_contrast_probe_without_semantic_batch_eligible", "fixture_normalized_halo_substituted_for_exact_core", "fixture_zero_batch_token_eligible", "fixture_mixed_batch_token_eligible", "fixture_foreign_track_batch_eligible", "fixture_duplicate_exact_batch_eligible", "fixture_invalid_semantic_range_eligible", "fixture_diagnostic_bracket_failed"]:
        need(fixture in source, f"source_fixture_missing:{fixture}")
    forbidden_terms = ["glReadPixels", "glDraw", "NSOpenGL", "CGL", "GPU_AUTH", "readiness"]
    for term in forbidden_terms:
        need(term not in source, f"source_forbidden_term:{term}")

    boundary = receipt.get("authorization_boundary", {})
    for key, value in boundary.items():
        need(value is False, f"authorization_boundary_true:{key}")

    if recompile and not errors:
        contract = receipt.get("reproducible_cpu_contract", {})
        manifest_path = ROOT / binding_map["link_input_manifest"]["path"]
        manifest_lines = [line.split(maxsplit=1) for line in manifest_path.read_text().splitlines() if line.strip()]
        need(len(manifest_lines) == contract.get("link_input_count") == 47, "link_input_count_invalid")
        link_inputs: list[str] = []
        for expected_hash, raw_path in manifest_lines:
            path = pathlib.Path(raw_path)
            need(path.is_file(), f"link_input_missing:{raw_path}")
            if path.is_file():
                need(sha256(path) == expected_hash, f"link_input_hash_invalid:{raw_path}")
                link_inputs.append(str(path))
        if not errors:
            with tempfile.TemporaryDirectory(prefix="phase4-v17-v2-") as temporary:
                temp = pathlib.Path(temporary)
                obj = temp / "diagnostic.o"
                binary = temp / "diagnostic"
                compile_cmd = [contract["compiler"], *contract["compile_arguments"], str(source_path), "-o", str(obj)]
                linked = subprocess.run(compile_cmd, cwd=ROOT, capture_output=True, text=True)
                need(linked.returncode == 0, "candidate_recompile_invalid")
                if linked.returncode == 0:
                    need(sha256(obj) == contract.get("diagnostic_object_sha256"), "candidate_object_hash_invalid")
                    link_cmd = [contract["compiler"], *contract["link_arguments"], str(obj), *link_inputs, "-o", str(binary)]
                    linked = subprocess.run(link_cmd, cwd=ROOT, capture_output=True, text=True)
                    need(linked.returncode == 0, "candidate_relink_invalid")
                if linked.returncode == 0:
                    need(sha256(binary) == contract.get("diagnostic_binary_sha256"), "candidate_binary_hash_invalid")
                    for mode, expected_path, code in [
                        ("--intersection-ledger", intersection_path, "intersection_replay_mismatch"),
                        ("--raster-ledger", raster_path, "raster_replay_mismatch")]:
                        replay = subprocess.run([str(binary), mode], cwd=ROOT, capture_output=True)
                        need(replay.returncode == 0, f"{code}:exit")
                        need(replay.stdout == expected_path.read_bytes(), code)
                        need(replay.stderr == b"", f"{code}:stderr")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--receipt", type=pathlib.Path, default=DEFAULT_RECEIPT)
    parser.add_argument("--source", type=pathlib.Path, default=DEFAULT_SOURCE)
    parser.add_argument("--intersection-ledger", type=pathlib.Path, default=DEFAULT_INTERSECTION)
    parser.add_argument("--raster-ledger", type=pathlib.Path, default=DEFAULT_RASTER)
    parser.add_argument("--skip-recompile", action="store_true")
    args = parser.parse_args()
    errors = validate(args.receipt, args.source, args.intersection_ledger,
                      args.raster_ledger, not args.skip_recompile)
    print(json.dumps({"ok": not errors, "errors": errors}, sort_keys=True))
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
