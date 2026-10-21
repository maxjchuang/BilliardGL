#!/usr/bin/env python3
"""Pure-CPU/read-only diagnosis for frozen Phase4 v11 ordinal 4 telemetry."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
READY = ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-ready-v11"
OUTPUT = ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v11"
READINESS = READY / "targeted11-readiness-v11.json"
AUTH = READY / "root-one-shot-gpu-authorization-v11-run1.json"
CONSUMED = READY / "root-one-shot-gpu-authorization-v11-run1.json.consumed"
FAILURE = OUTPUT / "phase4-final-ownership-dispatch-failure-receipt-v1.json"
RESULTS = OUTPUT / "results.jsonl"
TELEMETRY = OUTPUT / "results.jsonl.final-ownership.jsonl"
BROADCAST_PPM = OUTPUT / "camera_broadcast_1080.ppm"

FROZEN_SHA256 = {
    READINESS: "0e2e112f889aa5fc6fd5713a2d454945b1fe69678f9fd34bb873338ce3bbbc06",
    AUTH: "bc8e0ec237db21dec3ae71c150f92a1f29d8c7953872a273cecbdd86aa08e8d7",
    CONSUMED: "04a15b76a677f60e1721b19cf0564338f87c9147c8976de12905dddce9d052d0",
    FAILURE: "282056928154ce78ad79b8cee0062d460d03f76c9d130f1c9f268f0dfa009a39",
    RESULTS: "0fd67fe71d6115e7fe7bebe874ec7f4280a6a48dacd4432e2a15e1fe1ec57e69",
    TELEMETRY: "4e7e884569eec1197ef11d39d256e2e43eef11c49ee7c84ff150b93c3c6d61ef",
    BROADCAST_PPM: "0b6c87cb415a2e80ad1f6e061a972be57c683b8104e28d0065ba52b6a0045878",
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def physical_pixel(identifier: str) -> tuple[int, int]:
    pair = identifier.split(":", 1)[0].split(",")
    return int(pair[0]), int(pair[1])


def largest_component(pixels: list[tuple[int, int]], diagonal: bool) -> int:
    remaining = set(pixels)
    largest = 0
    directions = [(-1, 0), (1, 0), (0, -1), (0, 1)]
    if diagonal:
        directions += [(-1, -1), (-1, 1), (1, -1), (1, 1)]
    while remaining:
        stack = [remaining.pop()]
        size = 0
        while stack:
            x, y = stack.pop()
            size += 1
            for dx, dy in directions:
                neighbor = x + dx, y + dy
                if neighbor in remaining:
                    remaining.remove(neighbor)
                    stack.append(neighbor)
        largest = max(largest, size)
    return largest


def topology(row: dict, source_probe_id: str) -> dict:
    won: list[tuple[int, int]] = []
    claimed: list[tuple[int, int]] = []
    for record in row["exclusive_arbitration_records"]:
        identifier = record["physical_sample_or_pixel_id"]
        if identifier.startswith("unowned:"):
            continue
        if record.get("winner_source_probe_id") == source_probe_id:
            won.append(physical_pixel(identifier))
        if source_probe_id in record.get("claiming_source_probe_ids", []):
            claimed.append(physical_pixel(identifier))
    return {
        "exclusive_won_pixel_count": len(set(won)),
        "exclusive_largest_four_connected": largest_component(won, False),
        "exclusive_largest_eight_connected": largest_component(won, True),
        "pre_arbitration_claim_pixel_count": len(set(claimed)),
        "pre_arbitration_largest_four_connected":
            largest_component(claimed, False),
        "pre_arbitration_largest_eight_connected":
            largest_component(claimed, True),
    }


def summarize() -> dict:
    for path, expected in FROZEN_SHA256.items():
        if not path.is_file() or sha(path) != expected:
            raise RuntimeError(f"frozen_binding_drift:{path}")
    if (OUTPUT.stat().st_mode & 0o777) != 0o555:
        raise RuntimeError("frozen_output_mode_drift")
    for path in (AUTH, CONSUMED, FAILURE, RESULTS, TELEMETRY, BROADCAST_PPM):
        if (path.stat().st_mode & 0o777) != 0o444:
            raise RuntimeError(f"frozen_file_mode_drift:{path}")

    failure = json.loads(FAILURE.read_text())
    rows = [json.loads(line) for line in TELEMETRY.read_text().splitlines()
            if line.strip()]
    if len(rows) != 4 or [row["plan_ordinal_one_based"] for row in rows] != \
            [1, 2, 3, 4]:
        raise RuntimeError("telemetry_ordinal_inventory_invalid")

    ordinal_summaries = []
    for row in rows:
        failed_predicates = [item["name"] for item in
                             row["machine_predicate_ledger"]
                             if not item["result"]]
        ordinal_summaries.append({
            "ordinal": row["plan_ordinal_one_based"],
            "capture_id": row["capture_id"],
            "machine_status": row["machine_status"],
            "source_visible_hidden": [row["source_probe_count"],
                                      row["visible_probe_count"],
                                      row["hidden_probe_count"]],
            "coverage": row["aggregate_visible_probe_coverage"],
            "predicted": row["predicted_source_visible_hidden_and_coverage"],
            "actual": row["actual_source_visible_hidden_and_coverage"],
            "predicate_count": len(row["machine_predicate_ledger"]),
            "failed_predicates": failed_predicates,
        })

    row = rows[3]
    candidates = {item["source_probe_id"]: item
                  for item in row["ownership_candidate_records"]}
    selected = {}
    for boundary in row["component_aware_boundary_records"]:
        if boundary.get("selected"):
            selected[boundary["source_probe_id"]] = boundary
    hidden_details = []
    counterfactual_recovered = []
    for hidden in row["hidden_probe_id_track_role_reason_coordinates"]:
        source_id = hidden["source_probe_id"]
        candidate = candidates[source_id]
        boundary = selected[source_id]
        support = topology(row, source_id)
        recovered = hidden["reason"] == "insufficient_owned_pixel_area" and \
            candidate["resolved_pixel_equivalent_area"] >= 1.5 and \
            support["pre_arbitration_largest_eight_connected"] >= 2
        if recovered:
            counterfactual_recovered.append(source_id)
        hidden_details.append({
            **hidden,
            "resolved_pixel_equivalent_area":
                candidate["resolved_pixel_equivalent_area"],
            "selected_boundary_class": boundary["boundary_class"],
            "selected_association_mode": boundary["association_mode"],
            "selected_owner_visual_sublayer":
                boundary["owner_visual_sublayer"],
            "selected_adjacent_visual_sublayer":
                boundary["adjacent_visual_sublayer"],
            "selected_edge_contrast_ratio": boundary["edge_contrast_ratio"],
            "selected_owner_rgb_srgb8": boundary["owner_final_rgb_srgb8"],
            "selected_adjacent_rgb_srgb8":
                boundary["adjacent_final_rgb_srgb8"],
            **support,
            "analytic_counterfactual_recovered": recovered,
        })

    recovered_by_track = {
        track: sum(item["analytic_counterfactual_recovered"] and
                   item["track"] == track for item in hidden_details)
        for track in ("predicted", "actual")
    }
    predicted = row["predicted_source_visible_hidden_and_coverage"]
    actual = row["actual_source_visible_hidden_and_coverage"]
    recovered = len(counterfactual_recovered)
    return {
        "gpu_cocoa_opengl_invoked": False,
        "capture_retry_authorized": False,
        "new_readiness_or_authorization_created": False,
        "frozen_file_sha256": {str(path.relative_to(ROOT)): digest
                               for path, digest in FROZEN_SHA256.items()},
        "dispatch": {
            "status": failure["status"],
            "completed_failed_unrun": [failure["completed_count"],
                                       failure["failed_count"],
                                       failure["unrun_count"]],
            "failed_capture_id": failure["failed_capture_id"],
            "authorization_consumed_once":
                failure["authorization_consumed_once"],
            "validation_errors": failure["validation_errors"],
        },
        "ordinals": ordinal_summaries,
        "ordinal4_hidden_details": hidden_details,
        "ordinal4_hidden_reason_counts": row["component_hidden_reason_counts"],
        "ordinal4_counterfactual": {
            "analytic_non_final_not_a_machine_pass": True,
            "keeps_exclusive_area_assignment": True,
            "keeps_minimum_resolved_pixel_equivalent_area": 1.5,
            "uses_pre_arbitration_same_track_claim_support": True,
            "uses_eight_connected_raster_continuity": True,
            "recovered_probe_ids": counterfactual_recovered,
            "recovered_by_track": recovered_by_track,
            "aggregate_source_visible_hidden": [
                row["source_probe_count"], row["visible_probe_count"] + recovered,
                row["hidden_probe_count"] - recovered],
            "aggregate_coverage":
                (row["visible_probe_count"] + recovered) /
                row["source_probe_count"],
            "predicted_source_visible_hidden": [predicted["source"],
                predicted["visible"] + recovered_by_track["predicted"],
                predicted["hidden"] - recovered_by_track["predicted"]],
            "predicted_coverage":
                (predicted["visible"] + recovered_by_track["predicted"]) /
                predicted["source"],
            "actual_source_visible_hidden": [actual["source"],
                actual["visible"] + recovered_by_track["actual"],
                actual["hidden"] - recovered_by_track["actual"]],
            "actual_coverage":
                (actual["visible"] + recovered_by_track["actual"]) /
                actual["source"],
        },
    }


def main() -> None:
    print(json.dumps(summarize(), indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
