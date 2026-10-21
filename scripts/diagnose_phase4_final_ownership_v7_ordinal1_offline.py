#!/usr/bin/env python3
"""Freeze and diagnose the consumed Phase4 v7 ordinal-1 capture offline.

This program reads immutable evidence only.  It never launches Billiards, the
capture runner, Cocoa, or OpenGL and never creates readiness/authorization.
"""

from __future__ import annotations

from collections import Counter, defaultdict
import hashlib
import json
import math
from pathlib import Path
import re
import stat
import statistics


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs/art/phase4-final-ownership-v7-ordinal1-offline-diagnostic-v1"

AUTH = ROOT / "docs/evidence/phase4-final-ownership-targeted11-v7-authorization-root-v1.json"
CONSUMED = Path(str(AUTH) + ".consumed")
OUTPUT = ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v7"
FAILURE = OUTPUT / "phase4-final-ownership-dispatch-failure-receipt-v1.json"
TELEMETRY = OUTPUT / "results.jsonl.final-ownership.jsonl"
PPM = OUTPUT / "camera_player_low_1080.ppm"
RESULTS = OUTPUT / "results.jsonl"
PLAN = OUTPUT / "phase4-final-ownership-targeted11-plan.tsv"
READINESS = ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v7/targeted11-readiness-v7.json"
PAIRING_GATE = ROOT / "docs/art/phase4-final-ownership-v4-ordinal1-offline-diagnostic-v1/minimal-pairing-correction-design-gate-v1.json"

EXPECTED_SHA256 = {
    AUTH: "1cd2a1483929b863e5ac9914eec94dfb28643578a23d79c08a155ac4c94ebe05",
    CONSUMED: "9bd6ef6688af125fbc73ada51cb0da13d824f423c52d5ae3a62000283128c592",
    FAILURE: "e14979e634c03d676d3ddd99e1dc714352a9c2c6e9ecd1c01b9af8fdeadba66e",
    TELEMETRY: "ecfecdcc506adbebf21fa327e4ad50c08ec8a947b43359557c1318971bf6570a",
    PPM: "f3626f44ead6cfe4729e6ff3b76168747154b2452d0ca2afd70e1520b0e09744",
    RESULTS: "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    PLAN: "3b969b34e52fe2e01e31e34b148c4ce7c211682db65b3c609a43274366c8717d",
    READINESS: "7a2bd592f7d1e6766af51658c73053c7a44e1ec875ecc184215b420ec4278486",
    PAIRING_GATE: "103c6b20ba5647fbb6cc82a7748cd362d378de34bffa2cd01f7f610a8469e269",
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def mode(path: Path) -> str:
    return f"{stat.S_IMODE(path.stat().st_mode):04o}"


def rgb_luminance(rgb: list[int]) -> float:
    def channel(value: int) -> float:
        encoded = value / 255.0
        return encoded / 12.92 if encoded <= 0.04045 else \
            ((encoded + 0.055) / 1.055) ** 2.4
    return 0.2126 * channel(rgb[0]) + 0.7152 * channel(rgb[1]) + \
        0.0722 * channel(rgb[2])


def primitive_stem(value: str) -> str:
    return re.sub(r"_(core|halo)$", "", value)


def sorted_counter(values) -> dict[str, int]:
    return dict(sorted(Counter(values).items()))


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def selected_summary(boundaries: list[dict]) -> dict:
    selected = [row for row in boundaries if row["selected"]]
    groups: dict[tuple[str, str], list[dict]] = defaultdict(list)
    for row in selected:
        groups[(row["owner_component"], row["boundary_class"])].append(row)
    by_role_and_class = {}
    for (role, boundary_class), rows in sorted(groups.items()):
        contrasts = [row["edge_contrast_ratio"] for row in rows]
        owner_luma = [rgb_luminance(row["owner_final_rgb_srgb8"])
                      for row in rows]
        adjacent_luma = [rgb_luminance(row["adjacent_final_rgb_srgb8"])
                         for row in rows]
        by_role_and_class[f"{role}|{boundary_class}"] = {
            "count": len(rows),
            "contrast_min": min(contrasts),
            "contrast_median": statistics.median(contrasts),
            "contrast_max": max(contrasts),
            "owner_luminance_median": statistics.median(owner_luma),
            "adjacent_luminance_median": statistics.median(adjacent_luma),
        }
    return {
        "selected_count": len(selected),
        "selected_boundary_class_counts": sorted_counter(
            row["boundary_class"] for row in selected),
        "selected_below_4_count": sum(
            row["edge_contrast_ratio"] < 4.0 for row in selected),
        "by_owner_role_and_boundary_class": by_role_and_class,
    }


def qualifying_global_boundary(source_role: str, owner_primitive: str,
                               probe_x: float, probe_y: float,
                               boundaries: list[dict]) -> tuple[dict, float]:
    stem = primitive_stem(owner_primitive)
    qualifying = []
    for row in boundaries:
        owner_stem = primitive_stem(row["owner_primitive_id"])
        adjacent_stem = primitive_stem(row["adjacent_primitive_id"])
        if source_role == "outer_halo_edge":
            valid = row["boundary_class"] == "compound_outer_silhouette" and \
                owner_stem == stem and row["owner_component"] == \
                "outer_halo_edge"
        else:
            valid = row["boundary_class"] == \
                "same_track_compound_inner_edge" and \
                (owner_stem == stem or adjacent_stem == stem) and \
                {row["owner_component"], row["adjacent_component"]} == \
                {"core", "outer_halo_edge"}
        if not valid:
            continue
        distance = math.hypot(row["owner_physical_pixel"][0] - probe_x,
                              row["owner_physical_pixel"][1] - probe_y)
        qualifying.append((distance, row["owner_physical_pixel"],
                           row["adjacent_physical_pixel"], row))
    require(bool(qualifying), f"global qualifying boundary missing:{owner_primitive}")
    qualifying.sort(key=lambda item: (item[0], item[1], item[2],
                                      item[3]["sample_index"]))
    return qualifying[0][3], qualifying[0][0]


def main() -> None:
    for path, expected in EXPECTED_SHA256.items():
        require(path.is_file(), f"frozen input missing:{path}")
        require(sha(path) == expected, f"frozen input hash drift:{path}")
    for path in (AUTH, CONSUMED, FAILURE, TELEMETRY, PPM, RESULTS, PLAN):
        require(mode(path) == "0444", f"frozen input mode is not 0444:{path}")

    auth = json.loads(AUTH.read_text())
    consumed_nonce = CONSUMED.read_text().strip()
    failure = json.loads(FAILURE.read_text())
    readiness = json.loads(READINESS.read_text())
    rows = [json.loads(line) for line in TELEMETRY.read_text().splitlines()
            if line.strip()]
    require(len(rows) == 1, "v7 must contain exactly one ordinal telemetry row")
    row = rows[0]
    require(auth["single_use"] is True and auth["authorized"] is True,
            "authorization receipt semantics invalid")
    require(consumed_nonce == auth["authorization_nonce"],
            "consumed marker nonce mismatch")
    require(failure["authorization_consumed_once"] is True and
            failure["authorization_nonce"] == consumed_nonce and
            failure["status"] == "CAPTURE_FAILED_TELEMETRY_PRESERVED" and
            [failure[k] for k in ("completed_count", "failed_count",
                                  "unrun_count")] == [0, 1, 10],
            "failure receipt semantics invalid")
    require(failure["telemetry_sha256"] == EXPECTED_SHA256[TELEMETRY] and
            failure["ready_manifest_sha256"] == EXPECTED_SHA256[READINESS],
            "failure receipt bindings invalid")
    require(readiness["schema"] ==
            "billiardgl.phase4.final_ownership_targeted11_ready.v7",
            "readiness schema mismatch")

    require(row["capture_id"] == "camera_player_low_1080" and
            row["plan_ordinal_one_based"] == 1 and
            row["machine_status"] == "FAILED" and
            row["machine_error_code"] == "compound_visibility_below_contract",
            "ordinal identity/status mismatch")
    require(row["authoritative_visibility_gate"] ==
            "corrected_integrated_final_semantic_ownership" and
            row["integrated_final_ownership"] is True and
            row["legacy_ppm_diagnostic_only"] is True,
            "corrected/legacy authority semantics invalid")
    require(row["source_probe_count"] == 95 and
            row["visible_probe_count"] == 86 and
            row["hidden_probe_count"] == 9,
            "corrected aggregate counts mismatch")

    predicted = row["predicted_source_visible_hidden_and_coverage"]
    actual = row["actual_source_visible_hidden_and_coverage"]
    require([predicted[k] for k in ("source", "visible", "hidden")] ==
            [76, 67, 9] and predicted["coverage"] < 0.9,
            "predicted-track failure mismatch")
    require([actual[k] for k in ("source", "visible", "hidden")] ==
            [19, 19, 0] and actual["coverage"] == 1.0,
            "actual-track counts mismatch")
    require(row["aggregate_visible_probe_coverage"] >= 0.9,
            "aggregate should pass while predicted track fails")

    candidates = row["ownership_candidate_records"]
    boundaries = row["component_aware_boundary_records"]
    hidden = row["hidden_probe_id_track_role_reason_coordinates"]
    require(len(candidates) == 95 and len(hidden) == 9 and
            len({item["source_probe_id"] for item in candidates}) == 95,
            "candidate/hidden inventory incomplete")
    candidate_by_id = {item["source_probe_id"]: item for item in candidates}
    boundaries_by_id: dict[str, list[dict]] = defaultdict(list)
    for boundary in boundaries:
        boundaries_by_id[boundary["source_probe_id"]].append(boundary)

    lost_boundary_proof = []
    low_contrast_selected = []
    for hidden_row in hidden:
        source_id = hidden_row["source_probe_id"]
        candidate = candidate_by_id[source_id]
        local = boundaries_by_id[source_id]
        selected = [item for item in local if item["selected"]]
        if candidate["eligibility_or_rejection_reason"] == \
                "no_eligible_component_aware_boundary":
            require(candidate["resolved_sample_count"] > 0 and
                    not any(item["eligible_for_role"] for item in local) and
                    not selected,
                    f"lost-boundary local proof invalid:{source_id}")
            global_boundary, distance = qualifying_global_boundary(
                hidden_row["component_role"], candidate["owner_primitive_id"],
                hidden_row["x_physical_px"], hidden_row["y_physical_px"],
                boundaries)
            require(distance <= 2.0 and
                    global_boundary["edge_contrast_ratio"] >= 4.0 and
                    global_boundary["source_probe_id"] != source_id,
                    f"cross-partition boundary proof invalid:{source_id}")
            lost_boundary_proof.append({
                "source_probe_id": source_id,
                "track": hidden_row["track"],
                "component_role": hidden_row["component_role"],
                "candidate_owner_primitive_id": candidate["owner_primitive_id"],
                "resolved_sample_count": candidate["resolved_sample_count"],
                "resolved_pixel_equivalent_area":
                    candidate["resolved_pixel_equivalent_area"],
                "local_boundary_count": len(local),
                "local_eligible_boundary_count": 0,
                "serialized_hidden_reason": hidden_row["reason"],
                "candidate_rejection_reason":
                    candidate["eligibility_or_rejection_reason"],
                "nearest_qualifying_boundary_owned_by_other_probe": {
                    "other_source_probe_id": global_boundary["source_probe_id"],
                    "distance_from_source_probe_px": distance,
                    "boundary_class": global_boundary["boundary_class"],
                    "owner_physical_pixel":
                        global_boundary["owner_physical_pixel"],
                    "adjacent_physical_pixel":
                        global_boundary["adjacent_physical_pixel"],
                    "owner_primitive_id":
                        global_boundary["owner_primitive_id"],
                    "adjacent_primitive_id":
                        global_boundary["adjacent_primitive_id"],
                    "owner_final_rgb_srgb8":
                        global_boundary["owner_final_rgb_srgb8"],
                    "adjacent_final_rgb_srgb8":
                        global_boundary["adjacent_final_rgb_srgb8"],
                    "edge_contrast_ratio":
                        global_boundary["edge_contrast_ratio"],
                },
            })
        elif candidate["eligibility_or_rejection_reason"] == \
                "final_edge_contrast_below_4":
            require(len(selected) == 1 and
                    selected[0]["boundary_class"] ==
                    "compound_outer_silhouette" and
                    selected[0]["edge_contrast_ratio"] < 4.0,
                    f"low contrast selected proof invalid:{source_id}")
            boundary = selected[0]
            low_contrast_selected.append({
                "source_probe_id": source_id,
                "track": hidden_row["track"],
                "component_role": hidden_row["component_role"],
                "boundary_class": boundary["boundary_class"],
                "owner_physical_pixel": boundary["owner_physical_pixel"],
                "adjacent_physical_pixel": boundary["adjacent_physical_pixel"],
                "owner_final_rgb_srgb8": boundary["owner_final_rgb_srgb8"],
                "adjacent_final_rgb_srgb8":
                    boundary["adjacent_final_rgb_srgb8"],
                "owner_luminance":
                    rgb_luminance(boundary["owner_final_rgb_srgb8"]),
                "adjacent_luminance":
                    rgb_luminance(boundary["adjacent_final_rgb_srgb8"]),
                "edge_contrast_ratio": boundary["edge_contrast_ratio"],
                "deterministic_sort_key": boundary["deterministic_sort_key"],
            })
        else:
            raise RuntimeError(f"unexpected hidden candidate reason:{source_id}")

    require(len(lost_boundary_proof) == 6 and
            len(low_contrast_selected) == 3,
            "expected six partition-loss and three real low-contrast records")

    corrected_capture = {
        "source": row["source_probe_count"],
        "visible": row["visible_probe_count"],
        "hidden": row["hidden_probe_count"],
        "coverage": row["aggregate_visible_probe_coverage"],
        "predicted": predicted,
        "actual": actual,
        "component_source_counts": row["component_source_counts"],
        "component_visible_counts": row["component_visible_counts"],
        "component_hidden_reason_counts":
            row["component_hidden_reason_counts"],
        "machine_error_code": row["machine_error_code"],
        "actual_gate_failure":
            "predicted coverage 67/76=0.881578947368421 is below 0.90; "
            "aggregate 86/95 and actual 19/19 pass their unchanged floors",
    }
    analytic_visible = row["visible_probe_count"] + len(lost_boundary_proof)
    analytic_predicted_visible = predicted["visible"] + \
        len(lost_boundary_proof)
    receipt = {
        "schema":
            "billiardgl.phase4.final_ownership_v7_ordinal1_offline_diagnostic.v1",
        "status": "ALGORITHM_DEFECT_ISOLATED_NO_VISUAL_REPAIR_AUTHORIZED",
        "gpu_cocoa_opengl_invoked": False,
        "capture_retry_authorized": False,
        "new_readiness_or_authorization_created": False,
        "frozen_artifacts": {
            str(path.relative_to(ROOT)): {
                "sha256": EXPECTED_SHA256[path], "mode": mode(path)
            } for path in (AUTH, CONSUMED, FAILURE, TELEMETRY, PPM, RESULTS,
                           PLAN)
        },
        "bound_readiness": {
            "path": str(READINESS.relative_to(ROOT)),
            "sha256": EXPECTED_SHA256[READINESS],
        },
        "bound_pairing_gate": {
            "path": str(PAIRING_GATE.relative_to(ROOT)),
            "sha256": EXPECTED_SHA256[PAIRING_GATE],
        },
        "failure_dispatch": {
            "status": failure["status"],
            "completed_failed_unrun": [0, 1, 10],
            "authorization_consumed_once": True,
        },
        "authoritative_corrected_capture": corrected_capture,
        "legacy_ppm_diagnostic_only": {
            "visible": row["legacy_ppm_visible_probe_count"],
            "hidden": row["legacy_ppm_hidden_probe_count"],
            "coverage": row["legacy_ppm_visible_probe_coverage"],
            "contrast_ok": row["legacy_ppm_contrast_ok"],
            "authoritative": False,
            "explanation":
                "46/49 is retained comparison telemetry only and does not "
                "participate in machinePass",
        },
        "boundary_inventory": {
            "record_count": len(boundaries),
            "eligible_count": sum(item["eligible_for_role"]
                                  for item in boundaries),
            "all_boundary_class_counts": sorted_counter(
                item["boundary_class"] for item in boundaries),
            **selected_summary(boundaries),
        },
        "hidden_inventory": {
            "candidate_rejection_reason_counts": sorted_counter(
                item["eligibility_or_rejection_reason"]
                for item in candidates),
            "component_hidden_reason_counts":
                row["component_hidden_reason_counts"],
            "lost_eligible_boundary_due_to_per_probe_arbitration_count":
                len(lost_boundary_proof),
            "genuine_selected_outer_silhouette_below_4_count":
                len(low_contrast_selected),
        },
        "lost_boundary_cross_partition_proof": lost_boundary_proof,
        "genuine_low_contrast_selected_boundaries": low_contrast_selected,
        "root_cause": {
            "isolated": True,
            "classification":
                "ALGORITHM_DEFECT_PER_PROBE_ARBITRATION_PARTITIONS_BOUNDARY_SET",
            "proof":
                "Six hidden predicted probes own 4..36 resolved samples but "
                "their local sets contain zero eligible boundaries. The same "
                "frozen global telemetry contains gate-approved >=4 boundaries "
                "for the exact primitive stems only 1..2 px away, assigned to "
                "other probes. Therefore A did not serialize/associate the "
                "complete final semantic island boundary set per source probe.",
            "secondary_product_signal_not_yet_authoritative":
                "Three selected predicted outer silhouettes are genuinely below "
                "4.0 in this capture (1.287806, 3.892312, 3.902846), but visual "
                "repair is not authorized until the partition defect is fixed "
                "and a separately authorized fresh capture confirms the residual.",
            "serialized_reason_defect":
                "Six candidates say no_eligible_component_aware_boundary while "
                "hidden records collapse this to no_same_track_final_owner even "
                "though each candidate owns resolved samples.",
        },
        "analytic_non_final_reassociation": {
            "label": "COUNTERFACTUAL_ONLY_NOT_CORRECTED_EVIDENCE",
            "visible": analytic_visible,
            "hidden": 95 - analytic_visible,
            "coverage": analytic_visible / 95,
            "predicted_visible": analytic_predicted_visible,
            "predicted_hidden": 76 - analytic_predicted_visible,
            "predicted_coverage": analytic_predicted_visible / 76,
            "warning":
                "This demonstrates decision materiality only; it is not a "
                "replacement machine result and authorizes no capture or claim.",
        },
        "next_action":
            "Algorithm-only boundary association correction design gate; no "
            "product overlay style/color change is currently authorized.",
    }

    report = {
        "schema":
            "billiardgl.phase4.final_ownership_v7_root_cause_report.v1",
        "status": receipt["status"],
        "conclusion": receipt["root_cause"],
        "authoritative_corrected_capture": corrected_capture,
        "legacy_ppm_is_diagnostic_only": True,
        "boundary_inventory": receipt["boundary_inventory"],
        "hidden_inventory": receipt["hidden_inventory"],
        "analytic_non_final_reassociation":
            receipt["analytic_non_final_reassociation"],
        "gpu_cocoa_opengl_invoked": False,
    }

    gate = {
        "schema":
            "billiardgl.phase4.final_ownership_v7_owned_island_boundary_association_design_gate.v1",
        "status": "DESIGN_ONLY_NO_IMPLEMENTATION_NO_CAPTURE_AUTHORIZATION",
        "bound_failure_evidence_receipt_path": str(
            (OUT / "failure-evidence-receipt-v1.json").relative_to(ROOT)),
        "bound_failure_evidence_receipt_sha256": None,
        "scope":
            "Correct only the per-source association of complete final semantic "
            "island boundary alternatives before any product visual change.",
        "required_algorithm_correction": [
            "Keep exclusive sample arbitration for owned-area accounting, but do "
            "not restrict boundary provenance to samples won by one source probe.",
            "For each source probe, seed from its same-track resolved ownership "
            "and associate every gate-approved boundary in the same final semantic "
            "compound island/neighborhood within the unchanged tangent/normal bounds.",
            "Traverse same-track same-component token seams and approved core/halo "
            "links; never traverse foreign-track or later-UI ownership.",
            "outer_halo_edge may still select only a true compound outer silhouette; "
            "core/tick/endpoint may select only approved inner core-halo edges or "
            "true outer silhouettes.",
            "Retain deterministic role, distance, coordinate, direction ordering "
            "and exact final-LDR >=4 contrast without threshold changes.",
            "Serialize local seed ownership, complete associated island boundary "
            "alternatives, selected provenance, and a distinct rejection reason; "
            "never map owned-but-no-edge to no_same_track_final_owner.",
        ],
        "required_cpu_fixtures": [
            "P1 core probe may use a qualifying core-halo edge won by another "
            "same-track source without sharing owned-area credit",
            "P2 outer-halo probe may use a true silhouette won by another "
            "same-track source within bounds",
            "N1 same-token and same-component token seams alone remain ineligible",
            "N2 foreign-track and later-UI boundaries remain ineligible",
            "N3 boundary outside unchanged tangent/normal bounds remains ineligible",
            "N4 missing/truncated island alternatives fail closed",
            "N5 pixel/source iteration order cannot change selection",
            "P3 frozen v7 offline replay proves all six 1..2 px partition-loss "
            "records are discovered but makes no corrected capture claim",
            "P4 the three frozen selected outer silhouettes below 4 remain below "
            "4 in analytic replay and are not silently rescued",
        ],
        "immutable_invariants": {
            "source_probe_count": 95,
            "minimum_visible_coverage": 0.9,
            "minimum_edge_contrast_ratio": 4.0,
            "minimum_owned_area_pixel_equivalents": 1.5,
            "probe_route_offset_z_order_unchanged": True,
            "layout_assets_ui_unchanged": True,
            "production_overlay_style_and_color_unchanged_in_this_gate": True,
        },
        "forbidden": [
            "GPU/Cocoa/OpenGL execution or capture retry",
            "readiness or authorization creation",
            "threshold, 95 denominator, probe, route, offset or z-order change",
            "product overlay palette/color/halo/stroke/asset/UI change",
            "rewriting any v7 frozen authorization, consumed marker, output, "
            "failure receipt, telemetry, PPM, results or plan",
        ],
        "exit_conditions_before_fresh_readiness": [
            "All CPU fixtures and frozen v7 read-only replay pass",
            "Static validation proves boundary association is independent of "
            "exclusive per-source area arbitration",
            "Frozen v7 hashes and 0444 modes remain unchanged",
            "No corrected count or product-style conclusion is claimed from the "
            "analytic counterfactual",
        ],
        "capture_authorized": False,
        "technical_art_review_authorized": False,
        "formal68_authorized": False,
    }

    OUT.mkdir(parents=True, exist_ok=False)
    evidence_path = OUT / "failure-evidence-receipt-v1.json"
    evidence_path.write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n")
    gate["bound_failure_evidence_receipt_sha256"] = sha(evidence_path)
    (OUT / "root-cause-diagnostic-report-v1.json").write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n")
    (OUT / "minimal-owned-island-correction-design-gate-v1.json").write_text(
        json.dumps(gate, indent=2, sort_keys=True) + "\n")
    print(json.dumps({
        "status": receipt["status"],
        "corrected_visible_hidden": [86, 9],
        "predicted_visible_hidden": [67, 9],
        "lost_boundary_partition_records": 6,
        "selected_low_contrast_records": 3,
        "gpu_cocoa_opengl_invoked": False,
    }, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
