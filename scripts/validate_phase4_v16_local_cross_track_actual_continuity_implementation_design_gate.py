#!/usr/bin/env python3
"""Validate the Phase4 V16 selected crossing-priority implementation gate."""

from __future__ import annotations

import hashlib
import itertools
import json
from pathlib import Path
import re
import sys
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from scripts import validate_phase4_v15_independent_ta_diagnostic_acceptance_decision_gate as v15_gate_validator  # noqa: E402


DEFAULT = ROOT / "docs/art/phase4-v16-local-cross-track-actual-continuity-implementation-design-gate-v1.json"
DECISION_GATE = ROOT / "docs/art/phase4-v15-independent-ta-diagnostic-acceptance-decision-gate-v1.json"
LEDGER = ROOT / "docs/art/phase4-v15-broadcast-overwrite-provenance-diagnostic-v1/provenance-ledger-v1.json"
RESULTS = ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v11/results.jsonl.final-ownership.jsonl"
DECISION_GATE_SHA = "2628104e6d219a98fb38fa599058ec3dd22f11c658d7fe3aa1ffc82850bcf23b"
SOURCE_DIGEST = "33511aa46717a208f3db43acd278416b8fe8393e2e003484da8f44dabffe5645"
REPAIR_IDS = [
    "d6d850a1a10c7c7ce403e6de4afd4330e4c6e89b9100c72f4c23fbab1f6f978b",
    "1890fd0652c8f74a8b03a35a6cee1ad2cab43062987cab73efb1f6b455da468a",
    "1bf60bcd841a0d754c82876b55a4d3d6c29ebad582e069d4f4a608a4778c33a9",
]
BINDINGS = [
    ("selected_v15_ta_decision_gate", str(DECISION_GATE.relative_to(ROOT)), DECISION_GATE_SHA),
    ("v15_corrected_diagnostic_receipt", "docs/art/phase4-v15-broadcast-overwrite-provenance-diagnostic-v1/diagnostic-receipt-v1.json", "05e0bc2de8fd9c0978bf66547f2ce444b9bee864ca27dab871f2482b4df80dcf"),
    ("v15_corrected_provenance_ledger", str(LEDGER.relative_to(ROOT)), "bb2c3ecafb01d42cfddb8eec89a22619bf646ddb64773846747c5c76d2f5538b"),
    ("v14_reproducible_receipt_v2", "docs/art/phase4-v14-cap-mask-premise-falsification-v2/premise-falsification-receipt-v2.json", "bf320f08453a1b55f48235193ecd92ed3a88ccf671b506ee0cea36276e9f97f8"),
    ("v11_exact_source_inventory", str(RESULTS.relative_to(ROOT)), "4e7e884569eec1197ef11d39d256e2e43eef11c49ee7c84ff150b93c3c6d61ef"),
    ("baseline_trajectory_renderer_cpp", "src/Billiards/training/trajectory_overlay_renderer.cpp", "0c108c506641ab4828290a1a4c7b6b5ebbd51ab4fc7c12845bde0f5deb18a840"),
    ("baseline_trajectory_renderer_h", "src/Billiards/training/trajectory_overlay_renderer.h", "9805e6a5f56c17d7645200576505425cddf0d642970e5dc5cec186e003bd564e"),
    ("baseline_modern_renderer_cpp", "src/Billiards/render/modern/modern_renderer.cpp", "b00cc6b370122275a42f106967f935dbb34bd5d3f81f2a750ca9ba6e503af191"),
    ("baseline_modern_renderer_h", "src/Billiards/render/modern/modern_renderer.h", "936de5b77c9e3485f2e51f10e9de26c59c8ec94674865e0b50ae125725183c9d"),
    ("baseline_cmake", "CMakeLists.txt", "f41563c35224dd809f64acedba6455355e70082697c41305c2d1b05426f71d41"),
]
ALLOWED_PATHS = [
    "src/Billiards/training/trajectory_overlay_renderer.cpp",
    "src/Billiards/training/trajectory_overlay_renderer.h",
    "src/Billiards/render/modern/modern_renderer.cpp",
    "src/Billiards/render/modern/modern_renderer.h",
    "CMakeLists.txt",
    "tests/phase4_v16_local_cross_track_actual_continuity_cpu_tests.cpp",
    "scripts/validate_phase4_v16_local_cross_track_actual_continuity_implementation.py",
    "tests/test_phase4_v16_local_cross_track_actual_continuity_implementation.py",
]
GAPS = [(783, 413), (892, 491), (940, 525)]


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def largest8(points: set[tuple[int, int]]) -> int:
    remaining = set(points)
    largest = 0
    while remaining:
        stack = [remaining.pop()]
        size = 0
        while stack:
            x, y = stack.pop()
            size += 1
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    if not dx and not dy:
                        continue
                    item = (x + dx, y + dy)
                    if item in remaining:
                        remaining.remove(item)
                        stack.append(item)
        largest = max(largest, size)
    return largest


def minimal_selection(baseline: set[tuple[int, int]],
                      crossing: set[tuple[int, int]]) -> tuple[tuple[int, int], ...]:
    ordered = sorted(crossing, key=lambda point: (point[1], point[0]))
    for count in range(0, min(2, len(ordered)) + 1):
        for selected in itertools.combinations(ordered, count):
            if largest8(baseline | set(selected)) >= 2:
                return selected
    return ()


def broadcast_source_row() -> dict[str, Any]:
    rows = [json.loads(line) for line in RESULTS.read_text(
        encoding="utf-8").splitlines() if line]
    matches = [row for row in rows if row.get("capture_id") ==
               "camera_broadcast_1080"]
    if len(matches) != 1:
        raise RuntimeError("broadcast_source_row_not_unique")
    return matches[0]


def validate(path: Path = DEFAULT) -> list[str]:
    errors: list[str] = []
    try:
        value = load(path)
        decision_gate = load(DECISION_GATE)
        ledger = load(LEDGER)
        source_row = broadcast_source_row()
    except (OSError, ValueError, RuntimeError) as error:
        return ["input_invalid:" + str(error)]

    def require(condition: bool, message: str) -> None:
        if not condition:
            errors.append(message)

    require(value.get("schema") ==
            "billiardgl.phase4.v16.local_cross_track_actual_continuity_implementation_design_gate.v1",
            "schema_invalid")
    require(value.get("status") ==
            "DESIGN_GATE_CPU_STATIC_IMPLEMENTATION_AUTHORIZED_GPU_READINESS_AUTH_NOT_AUTHORIZED",
            "status_invalid")
    require(v15_gate_validator.validate(DECISION_GATE) == [],
            "selected_v15_decision_gate_invalid")
    observed = [(row.get("role"), row.get("path"), row.get("sha256"))
                for row in value.get("frozen_bindings", [])]
    require(observed == BINDINGS, "binding_inventory_invalid")
    for _, relative, digest in BINDINGS:
        target = ROOT / relative
        require(target.is_file() and sha(target) == digest,
                "binding_missing_or_drift:" + relative)

    selected = value.get("root_product_decision_binding", {})
    statement = selected.get("selection_statement", "")
    require(selected.get("source_gate_path") == str(DECISION_GATE.relative_to(ROOT))
            and selected.get("source_gate_sha256") == DECISION_GATE_SHA
            and selected.get("source_candidate_json_pointer") ==
                "/product_decision_candidates_in_priority_order/0"
            and selected.get("selected_decision_id") ==
                "LOCAL_CROSS_TRACK_ACTUAL_CONTINUITY_PRIORITY"
            and selected.get("selected_priority") == 1
            and selected.get("selected_recommended") is True
            and selected.get("mutually_exclusive_alternative_rejected") ==
                "EXPLICIT_NON_OCCLUDING_CROSSING_GRAMMAR"
            and selected.get("selection_authority") ==
                "ROOT_PROJECT_MANAGER_PRODUCT_DECISION"
            and selected.get("selection_statement_sha256") ==
                hashlib.sha256(statement.encode()).hexdigest()
            and selected.get("selection_is_not_gpu_or_capture_authorization") is True,
            "root_product_decision_binding_invalid")
    source_candidate = decision_gate["product_decision_candidates_in_priority_order"][0]
    require(source_candidate.get("decision_id") == selected.get("selected_decision_id")
            and source_candidate.get("recommended") is True
            and source_candidate.get("authorized_by_this_gate") is False,
            "selected_source_candidate_invalid")

    source = value.get("exact_source_and_repair_contract", {})
    ids = source_row.get("ordered_source_probe_ids", [])
    digest = hashlib.sha256("".join(item + "\n" for item in ids).encode()).hexdigest()
    require(len(ids) == 81 and len(set(ids)) == 81 and digest == SOURCE_DIGEST,
            "bound_exact_81_source_inventory_invalid")
    require(source.get("ordered_source_probe_count") == 81
            and source.get("ordered_source_probe_digest") == SOURCE_DIGEST
            and source.get("exact_repair_probe_ids_in_order") == REPAIR_IDS
            and all(item in ids for item in REPAIR_IDS)
            and source.get("source_id_count_order_digest_must_remain_exact") is True
            and source.get("foreign_double_unowned_must_remain_zero") is True,
            "gate_source_repair_contract_invalid")

    candidate = value.get("candidate_eligibility_contract", {})
    require(candidate.get("same_stem_core_halo_is_cross") is False
            and candidate.get("different_normalized_stem_is_cross") is True
            and candidate.get("different_arc_is_cross") is True
            and candidate.get("color_selects_eligibility") is False
            and candidate.get("allowed_candidate") ==
                "prior_actual_route_body && true_distinct_predicted_crossing",
            "candidate_eligibility_contract_invalid")
    forbidden_candidates = "\n".join(candidate.get("forbidden_candidates", []))
    for token in ("non-cross dash-on", "single predicted stem", "endpoint",
                  "actual halo", "color-derived", "foreign"):
        require(token in forbidden_candidates,
                "forbidden_candidate_missing:" + token)

    algorithm = value.get("minimum_connected_selection_algorithm", {})
    require(algorithm.get("coordinate_whitelist_allowed") is False
            and algorithm.get("connectivity").startswith("8-connected")
            and "minimum-cardinality subset S" in algorithm.get("objective", "")
            and "largest8" in algorithm.get("objective", "")
            and "select zero" in algorithm.get("already_satisfied", "")
            and "select zero" in algorithm.get("fail_closed", "")
            and algorithm.get("deterministic_tie_break") == [
                "minimum selected sample count",
                "minimum row-major ordered tuple list using (sample_index, physical_y, physical_x)",
                "minimum ordered provenance-key tuple list"],
            "minimum_connected_selector_contract_invalid")

    expected_outputs = [
        ([783, 413], 18, 0, None, 18),
        ([892, 491], 1, 1, [[894, 492]], 2),
        ([940, 525], 0, 2, [[932, 517], [933, 517]], 2),
    ]
    gate_outputs = value.get("frozen_fixture_expected_selector_result_per_palette", [])
    require(len(gate_outputs) == 3, "selector_output_inventory_invalid")
    selections_by_palette: list[list[tuple[tuple[int, int], ...]]] = []
    for palette in ledger.get("palettes", []):
        by_pixel = {tuple(sample["physical_pixel"]): sample
                    for sample in palette.get("samples", [])}
        palette_selections = []
        for index, ((gx, gy), expected) in enumerate(zip(GAPS, expected_outputs)):
            region = [sample for (x, y), sample in by_pixel.items()
                      if abs(x - gx) <= 8 and abs(y - gy) <= 8]
            baseline = {tuple(sample["physical_pixel"]) for sample in region
                        if not sample.get("final_predicted_primitive_id") or
                        sample.get("would_be_candidate_for_future_gap_only_non_overwrite") is True}
            crossing = {tuple(sample["physical_pixel"]) for sample in region
                        if sample.get("prior_actual_route_body_boolean") is True
                        and sample.get("semantic_interval_class") ==
                            "cross_segment_or_cross_source_overlap"}
            selected_points = minimal_selection(baseline, crossing)
            palette_selections.append(selected_points)
            coordinate, baseline_count, selected_count, selected_output, final_largest = expected
            require(len(baseline) == baseline_count
                    and len(selected_points) == selected_count
                    and largest8(baseline | set(selected_points)) == final_largest,
                    "ledger_minimal_selection_invalid:" +
                    palette.get("palette", "unknown") + ":" + str(coordinate))
            gate_row = gate_outputs[index]
            expected_row = {
                "probe_coordinate": coordinate,
                "baseline_eligible_count": baseline_count,
                "selected_true_crossing_count": selected_count,
                "final_actual_largest8": final_largest,
                "meets_required_largest8": True,
            }
            if selected_output is not None:
                expected_row["canonical_selected_output_only_not_input"] = selected_output
            require(gate_row == expected_row,
                    "gate_selector_expected_output_invalid:" + str(coordinate))
            if selected_points:
                for remove in selected_points:
                    require(largest8(baseline | (set(selected_points) - {remove})) < 2,
                            "selector_not_minimal:" + str(coordinate))
        selections_by_palette.append(palette_selections)
    require(len(selections_by_palette) == 2
            and selections_by_palette[0] == selections_by_palette[1],
            "standard_cvd_selector_topology_invalid")

    implementation = value.get("implementation_contract", {})
    joined = json.dumps(implementation, ensure_ascii=False)
    for token in ("without reading capture ID", "selected-crossing",
                  "ActualRouteBodyCore", "MSAA", "exactly two",
                  "Predicted yellow", "18/12/30", "2px", "Standard and CVD"):
        require(token in joined, "implementation_contract_missing:" + token)
    allowed = value.get("allowed_files", [])
    require([item.get("path") for item in allowed] == ALLOWED_PATHS,
            "allowed_file_inventory_invalid")
    acceptance = value.get("cpu_static_acceptance_required_before_any_future_gpu", {})
    fixtures = acceptance.get("required_fixtures", [])
    require(acceptance.get("status") == "NOT_RUN"
            and acceptance.get("required_fixture_count") == 16
            and len(fixtures) == 16 and len(set(fixtures)) == 16
            and acceptance.get("passing_cpu_static_is_not_gpu_or_ta_acceptance") is True,
            "cpu_static_acceptance_contract_invalid")

    forbidden = "\n".join(value.get("forbidden_changes", []))
    for token in ("No capture ID", "No global actual-over-predicted",
                  "No non-cross dash-on", "No threshold", "No route",
                  "No source ID", "No extra color draw", "No files outside",
                  "no GPU execution"):
        require(token in forbidden, "forbidden_change_missing:" + token)
    authorization = value.get("authorization_boundary", {})
    require(authorization == {
        "cpu_static_scoped_implementation_authorized": True,
        "production_files_authorized_only_as_allowlisted": True,
        "product_decision_bound": True,
        "alternative_crossing_grammar_authorized": False,
        "gpu_authorized": False,
        "readiness_authorized": False,
        "capture_authorization_created": False,
        "machine_or_ta_acceptance": False,
        "product_exposure_authorized": False,
    }, "authorization_boundary_invalid")
    future = value.get("future_gpu_and_ta_boundary", {})
    require(future == {
        "status": "NOT_AUTHORIZED_NOT_RUN",
        "minimum_future_cases": ["camera_player_low_1080",
            "camera_elevated_1080", "camera_overhead_1080",
            "camera_broadcast_1080", "actual_retina_ui135_cvd"],
        "requires_frozen_implementation_receipt": True,
        "requires_new_readiness": True,
        "requires_single_use_gpu_authorization": True,
        "machine_five_of_five_before_original_resolution_ta": True,
        "existing_targeted5_template_cannot_be_marked_from_cpu_only": True,
        "targeted11_formal68_m3_product_remain_false": True,
    }, "future_gpu_ta_boundary_invalid")
    proof = value.get("proof_boundary", {})
    require(len(proof) == 16 and proof.get("design_gate_only") is True
            and all(item is False for key, item in proof.items()
                    if key != "design_gate_only"),
            "proof_boundary_invalid")
    return errors


def main() -> int:
    target = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT
    errors = validate(target)
    if errors:
        for error in errors:
            print("FAIL: " + error, file=sys.stderr)
        return 1
    print("PASS_PHASE4_V16_LOCAL_CROSS_TRACK_ACTUAL_CONTINUITY_IMPLEMENTATION_DESIGN_GATE")
    print("selected=LOCAL_CROSS_TRACK_ACTUAL_CONTINUITY_PRIORITY cpu_static_authorized=true")
    print("gpu=false readiness=false capture_auth=false")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
