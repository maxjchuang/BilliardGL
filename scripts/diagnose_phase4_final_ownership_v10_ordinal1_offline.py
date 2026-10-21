#!/usr/bin/env python3
"""Freeze and diagnose the consumed Phase4 v10 ordinal-1 failure offline."""

from __future__ import annotations

from collections import Counter
import hashlib
import json
from pathlib import Path
import stat


ROOT = Path(__file__).resolve().parents[1]
READY = ROOT / ("docs/art/phase4-training-replay-overlay-v4-final-ownership-"
                "targeted11-ready-v10")
OUTPUT = ROOT / ("docs/art/phase4-training-replay-overlay-v4-final-ownership-"
                 "targeted11-output-v10")
OUT = ROOT / "docs/art/phase4-final-ownership-v10-ordinal1-offline-diagnostic-v1"
AUTH = READY / "root-one-shot-gpu-authorization-v10-run1.json"
CONSUMED = Path(str(AUTH) + ".consumed")
READINESS = READY / "targeted11-readiness-v10.json"
SOURCE_BINDINGS = READY / "source-bindings-v1.json"
PLAN = OUTPUT / "phase4-final-ownership-targeted11-plan.tsv"
RESULTS = OUTPUT / "results.jsonl"
TELEMETRY = OUTPUT / "results.jsonl.final-ownership.jsonl"
PPM = OUTPUT / "camera_player_low_1080.ppm"
FAILURE = OUTPUT / "phase4-final-ownership-dispatch-failure-receipt-v1.json"
V9_TELEMETRY = ROOT / ("docs/art/phase4-training-replay-overlay-v4-final-"
                       "ownership-targeted11-output-v9/"
                       "results.jsonl.final-ownership.jsonl")
TRAJECTORY_CPP = ROOT / "src/Billiards/training/trajectory_overlay_renderer.cpp"
MODERN_CPP = ROOT / "src/Billiards/render/modern/modern_renderer.cpp"
RUNNER_SOURCE = ROOT / ("build-integrated-final-shared-phase4-v10-phase5-v5-"
                        "phase2-v9/generated/phase4_overlay_v4_gpu_capture_runner.mm")

EXPECTED = {
    AUTH: "dc0c60ddba2bef6a86ee4c1e328727543e24de5153260af905afd5f8d383fa8f",
    CONSUMED: "6cd0c9ec5d07b4ecd637aca6bff25048897e51541bb13e0d29571c3635c0a33c",
    READINESS: "878d69d0833b1a8becd6322fbb29e3cdb20f57723a8d1d3ff43c985c10750f37",
    SOURCE_BINDINGS: "66a34080e3934e60c5c7a021d14846dd6d54b3f1a1d19e2c579b63174a5e2b6a",
    PLAN: "3b969b34e52fe2e01e31e34b148c4ce7c211682db65b3c609a43274366c8717d",
    RESULTS: "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    TELEMETRY: "d8e06f0aa0e77d0e677af72ce2250a563d2595b70b790b05ad5728c9cc59c94d",
    PPM: "82d6f9e5aba1821b3b27c00f2b76977fff035cf8541c768bd5361c1ee4ee744a",
    FAILURE: "5b5923a8efbce6f293af4d2fd71ce2d4c016d2faaabc5dbd4d19600ea10772ae",
    V9_TELEMETRY: "3f40581fc4f66861a3a5f0953d6350619ca3195ad0c560f1202c5458dd66f62a",
    TRAJECTORY_CPP: "09852cd4ad0a9c8aebeeac5b026d84f2d3c24624deddf7aab46330cfb1afd09e",
    MODERN_CPP: "634d0b5605134c578f93c0a13efcc3fbba26fe6e5ce567dd11d21925992a8390",
    RUNNER_SOURCE: "9528af34f96dbf90916115770ccfad6009010d2c7e006dc55eb1997e65bfa286",
}
FROZEN_0444 = (AUTH, CONSUMED, PLAN, RESULTS, TELEMETRY, PPM, FAILURE)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def mode(path: Path) -> str:
    return f"{stat.S_IMODE(path.stat().st_mode):04o}"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def write_json(path: Path, value: dict) -> None:
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def main() -> None:
    require(not OUT.exists(), "diagnostic output already exists")
    for path, expected in EXPECTED.items():
        require(path.is_file() and sha(path) == expected,
                f"bound input hash drift:{path}")
    for path in FROZEN_0444:
        require(mode(path) == "0444", f"frozen mode is not 0444:{path}")
    require(mode(OUTPUT) == "0555", "frozen output mode is not 0555")

    auth = json.loads(AUTH.read_text())
    nonce = CONSUMED.read_text().strip()
    failure = json.loads(FAILURE.read_text())
    row = json.loads(TELEMETRY.read_text())
    v9 = json.loads(V9_TELEMETRY.read_text())
    require(auth.get("authorized") is True and auth.get("single_use") is True and
            nonce == auth.get("authorization_nonce"), "auth consumption invalid")
    require(failure.get("status") == "CAPTURE_FAILED_TELEMETRY_PRESERVED" and
            failure.get("authorization_consumed_once") is True and
            [failure.get(k) for k in ("completed_count", "failed_count",
                                      "unrun_count")] == [0, 1, 10] and
            failure.get("validation_errors") == [] and
            failure.get("telemetry_sha256") == EXPECTED[TELEMETRY] and
            failure.get("results_sha256") == EXPECTED[RESULTS],
            "dispatch failure semantics invalid")
    require(RESULTS.stat().st_size == 0 and row.get("plan_ordinal_one_based") == 1 and
            row.get("capture_id") == "camera_player_low_1080" and
            row.get("machine_status") == "FAILED" and
            row.get("machine_error_code") == "phase4_overlay_machine_gate_failed" and
            row.get("integrated_final_ownership") is True and
            row.get("authoritative_visibility_gate") ==
            "corrected_integrated_final_semantic_ownership",
            "v10 ordinal1 authority semantics invalid")

    predicted = row["predicted_source_visible_hidden_and_coverage"]
    actual = row["actual_source_visible_hidden_and_coverage"]
    require([row[k] for k in ("source_probe_count", "visible_probe_count",
                              "hidden_probe_count")] == [95, 89, 6] and
            row["aggregate_visible_probe_coverage"] == 89 / 95 and
            predicted == {"source": 76, "visible": 70, "hidden": 6,
                          "coverage": 70 / 76} and
            actual == {"source": 19, "visible": 19, "hidden": 0,
                       "coverage": 1}, "authoritative counts invalid")
    require(min(row["aggregate_visible_probe_coverage"], predicted["coverage"],
                actual["coverage"]) >= .90, "ownership gate did not pass")

    candidates = row["ownership_candidate_records"]
    selected = [value for value in row["component_aware_boundary_records"]
                if value["selected"]]
    hidden = row["hidden_probe_id_track_role_reason_coordinates"]
    require(len(candidates) == len(selected) == 95 and len(hidden) == 6 and
            len(row["exclusive_arbitration_records"]) == 5808 and
            all(value["reason"] == "final_edge_contrast_below_4" and
                value["track"] == "predicted" and
                value["component_role"] == "outer_halo_edge"
                for value in hidden), "ownership inventory invalid")
    candidate_by_id = {value["source_probe_id"]: value for value in candidates}
    selected_by_id = {value["source_probe_id"]: value for value in selected}
    hidden_details = []
    for value in hidden:
        candidate = candidate_by_id[value["source_probe_id"]]
        boundary = selected_by_id[value["source_probe_id"]]
        hidden_details.append({
            **value,
            "owner_primitive_id": candidate["owner_primitive_id"],
            "owner_visual_sublayer": candidate["owner_visual_sublayer"],
            "resolved_pixel_equivalent_area":
                candidate["resolved_pixel_equivalent_area"],
            "selected_boundary_class": boundary["boundary_class"],
            "selected_association_mode": boundary["association_mode"],
            "selected_owner_visual_sublayer":
                boundary["owner_visual_sublayer"],
            "selected_adjacent_visual_sublayer":
                boundary["adjacent_visual_sublayer"],
            "selected_owner_rgb_srgb8": boundary["owner_final_rgb_srgb8"],
            "selected_adjacent_rgb_srgb8":
                boundary["adjacent_final_rgb_srgb8"],
            "selected_edge_contrast_ratio": boundary["edge_contrast_ratio"],
        })

    trajectory_source = TRAJECTORY_CPP.read_text()
    modern_source = MODERN_CPP.read_text()
    runner_source = RUNNER_SOURCE.read_text()
    require("result.additionalDrawCalls =\n        (result.actual.vertexCount > 0 ? 1u : 0u) +\n        (result.predicted.vertexCount > 0 ? 1u : 0u);" in
            trajectory_source and
            "frameMetrics.trajectoryOverlayDrawCalls = package.additionalDrawCalls;"
            in modern_source and
            "metrics.trajectoryOverlayDrawCalls == 1" in runner_source,
            "static draw predicate evidence invalid")
    require(predicted["source"] > 0 and actual["source"] > 0,
            "two nonempty tracks not proven")

    v9_hidden = {value["source_probe_id"] for value in
                 v9["hidden_probe_id_track_role_reason_coordinates"]}
    v10_hidden = {value["source_probe_id"] for value in hidden}
    selected_modes = Counter(value["association_mode"] for value in selected)
    selected_classes = Counter(value["boundary_class"] for value in selected)
    selected_sublayers = Counter((value["owner_visual_sublayer"],
                                  value["adjacent_visual_sublayer"])
                                 for value in selected)
    OUT.mkdir(parents=True)
    frozen = {str(path.relative_to(ROOT)): {"sha256": EXPECTED[path],
                                             "mode": "0444"}
              for path in FROZEN_0444}
    receipt = {
        "schema":
            "billiardgl.phase4.final_ownership_v10_ordinal1_offline_diagnostic.v1",
        "status":
            "STALE_EXACT_ONE_DRAW_MACHINE_CONTRACT_SUFFICIENT_FALSE_CONJUNCT_ISOLATED_NO_RETRY_AUTHORIZED",
        "gpu_cocoa_opengl_invoked": False,
        "capture_retry_authorized": False,
        "new_readiness_or_authorization_created": False,
        "frozen_evidence": frozen,
        "frozen_output_directory_mode": "0555",
        "bound_readiness": {"sha256": EXPECTED[READINESS],
                            "mode": mode(READINESS)},
        "bound_source_bindings": {"sha256": EXPECTED[SOURCE_BINDINGS],
                                  "mode": mode(SOURCE_BINDINGS)},
        "dispatch_outcome": {"completed": 0, "failed": 1, "unrun": 10,
                             "failed_capture_id": "camera_player_low_1080",
                             "authorization_consumed_once": True,
                             "validation_errors": []},
        "authority_separation": {
            "authoritative_gate":
                "corrected_integrated_final_semantic_ownership",
            "corrected_integrated": {"source": 95, "visible": 89,
                "hidden": 6, "coverage": 89 / 95,
                "aggregate_threshold_pass": True,
                "predicted_threshold_pass": True,
                "actual_threshold_pass": True},
            "legacy_ppm_diagnostic_only": {"source": 95, "visible": 50,
                "hidden": 45, "coverage": 50 / 95, "authoritative": False},
        },
        "track_counts": {"predicted": predicted, "actual": actual},
        "component_counts": {"source": row["component_source_counts"],
            "visible": row["component_visible_counts"],
            "hidden_reason": row["component_hidden_reason_counts"]},
        "ownership_and_association": {
            "candidate_count": len(candidates),
            "selected_boundary_count": len(selected),
            "arbitration_record_count": len(row["exclusive_arbitration_records"]),
            "contested_arbitration_record_count": sum(
                len(value["claiming_source_probe_ids"]) > 1
                for value in row["exclusive_arbitration_records"]),
            "foreign_unowned_double_counts": [row["foreign_track_evidence_count"],
                row["unowned_evidence_count"], row["double_assignment_count"]],
            "candidate_reason_counts": dict(sorted(Counter(
                value["eligibility_or_rejection_reason"]
                for value in candidates).items())),
            "selected_association_mode_counts": dict(sorted(selected_modes.items())),
            "selected_boundary_class_counts": dict(sorted(selected_classes.items())),
            "selected_sublayer_pair_counts": {
                f"{owner}|{adjacent}": count for (owner, adjacent), count in
                sorted(selected_sublayers.items())},
            "area_failure_count": sum(
                value["resolved_pixel_equivalent_area"] < 1.5
                for value in candidates),
        },
        "hidden_probe_details": hidden_details,
        "v9_to_v10_comparison": {
            "source_probe_ids_and_digest_equal":
                v9["ordered_source_probe_ids"] == row["ordered_source_probe_ids"] and
                v9["ordered_source_probe_digest"] ==
                row["ordered_source_probe_digest"],
            "v9_visible_hidden": [66, 29], "v10_visible_hidden": [89, 6],
            "visible_delta": 23, "common_hidden": len(v9_hidden & v10_hidden),
            "resolved_hidden": len(v9_hidden - v10_hidden),
            "new_hidden": len(v10_hidden - v9_hidden),
        },
        "static_machine_predicate_evidence": {
            "trajectory_cpp_sha256": EXPECTED[TRAJECTORY_CPP],
            "modern_cpp_sha256": EXPECTED[MODERN_CPP],
            "runner_source_sha256": EXPECTED[RUNNER_SOURCE],
            "actual_track_nonempty": True, "predicted_track_nonempty": True,
            "package_reported_draw_submissions_by_definition": 2,
            "frame_metric_copies_package_additional_draw_calls": True,
            "frozen_runner_required_exact_draw_submissions": 1,
            "draw_predicate_evaluates": False,
            "draw_predicate_is_a_sufficient_false_conjunct": True,
        },
        "root_cause": {
            "sufficient_false_conjunct_isolated": True,
            "primary": "STALE_EXACT_ONE_DRAW_MACHINE_ACCEPTANCE_CONTRACT",
            "authoritative_ownership_gate_passed": True,
            "not_visibility_threshold_or_ownership_algorithm": True,
            "not_legacy_ppm_gate": True,
            "other_runtime_predicates_individually_serialized": False,
            "exclusivity_of_this_single_failure_proven": False,
            "conclusion":
                "The exact-one draw predicate is deterministically false and alone forces machinePass false; telemetry does not preserve a complete predicate ledger, so it is sufficient but not proven exclusive.",
        },
    }
    receipt_path = OUT / "failure-evidence-receipt-v1.json"
    write_json(receipt_path, receipt)
    gate = {
        "schema":
            "billiardgl.phase4.final_ownership_v10_machine_contract_correction_design_gate.v1",
        "status": "DESIGN_ONLY_NO_IMPLEMENTATION_NO_CAPTURE_AUTHORIZATION",
        "bound_failure_evidence_receipt_sha256": sha(receipt_path),
        "capture_authorized": False,
        "formal68_authorized": False,
        "technical_art_review_authorized": False,
        "allowed_next_implementation_scope": [
            "Serialize a complete named machine-predicate ledger before the machinePass require so every failure preserves measured value, expected value, and boolean result.",
            "Replace only the stale exact-one trajectory draw-submission predicate with exact-two, and only when nonempty actual is submitted, the actual-composited backdrop is frozen, then nonempty predicted is submitted in the immutable actual-then-predicted order.",
            "Keep the draw metric meaning as trajectory GL submissions; do not weaken exact-two into an upper bound and do not count backdrop blits as trajectory submissions.",
        ],
        "required_cpu_static_negative_fixtures": [
            "A frozen v10 ordinal-1 replay proves 89/95, 70/76 predicted, and 19/19 actual all meet the unchanged 0.90 visibility threshold but remains analytic non-final.",
            "The machine predicate ledger rejects missing, duplicate, renamed, non-boolean, or post-require predicates and preserves it on failure.",
            "The ordered submission contract accepts exactly actual draw, midpoint actual-composited backdrop freeze, predicted draw; it rejects 0, 1, 3, reversed order, or missing/late midpoint freeze.",
            "A counterfactual may show the known draw predicate changes false to true, but may not claim overall machine pass unless every serialized runtime predicate is true in a fresh authorized capture.",
        ],
        "immutable_invariants": {
            "minimum_visible_coverage": 0.90,
            "minimum_local_contrast_ratio": 4.0,
            "minimum_owned_area_pixel_equivalents": 1.5,
            "ordinal1_source_probe_count": 95,
            "ordinal1_ordered_source_probe_digest":
                "d533924980efc85aeb94e4f1d31831d93e2bfce744be3b66481d08c56d10289a",
            "probe_ids_denominator_route_offsets_z_order": "UNCHANGED",
            "ownership_arbitration_association_hidden_reason_algorithm":
                "UNCHANGED",
            "visual_colors_geometry_sublayers_backdrops_assets_scene_ui":
                "UNCHANGED",
        },
        "forbidden": [
            "No retry, GPU, Cocoa, OpenGL, formal68, or TA execution under this gate.",
            "No rewrite or reuse of the v10 authorization, consumed marker, readiness, output, results, telemetry, PPM, plan, or failure receipt.",
            "No production trajectory geometry, color, halo, stroke, backdrop, ownership, threshold, probe, denominator, route, offset, z-order, camera, layout, asset, scene, or UI change.",
            "No claim that the known false draw predicate was the exclusive runtime failure until a complete preserved predicate ledger proves every other predicate true.",
        ],
        "exit_condition":
            "CPU/static predicate-ledger and exact ordered-two-submission fixtures must pass, followed by a fresh Release runner/readiness with 11 NOT_RUN and no authorization; capture requires a separate one-shot authorization.",
    }
    write_json(OUT / "minimal-machine-contract-correction-design-gate-v1.json",
               gate)
    print(receipt_path)


if __name__ == "__main__":
    main()
