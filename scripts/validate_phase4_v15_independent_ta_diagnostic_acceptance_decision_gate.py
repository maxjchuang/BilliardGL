#!/usr/bin/env python3
"""Validate the V15 independent TA diagnostic acceptance/decision gate."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from scripts import validate_phase4_v15_broadcast_overwrite_provenance_diagnostic as upstream  # noqa: E402


DEFAULT = ROOT / "docs/art/phase4-v15-independent-ta-diagnostic-acceptance-decision-gate-v1.json"
RECEIPT = ROOT / "docs/art/phase4-v15-broadcast-overwrite-provenance-diagnostic-v1/diagnostic-receipt-v1.json"
LEDGER = ROOT / "docs/art/phase4-v15-broadcast-overwrite-provenance-diagnostic-v1/provenance-ledger-v1.json"
RESULTS = ROOT / "docs/art/phase4-training-replay-overlay-v4-final-ownership-targeted11-output-v11/results.jsonl.final-ownership.jsonl"
BINDINGS = [
    ("v15_corrected_diagnostic_receipt", str(RECEIPT.relative_to(ROOT)),
     "05e0bc2de8fd9c0978bf66547f2ce444b9bee864ca27dab871f2482b4df80dcf"),
    ("v15_corrected_provenance_ledger", str(LEDGER.relative_to(ROOT)),
     "bb2c3ecafb01d42cfddb8eec89a22619bf646ddb64773846747c5c76d2f5538b"),
    ("v15_diagnostic_validator", "scripts/validate_phase4_v15_broadcast_overwrite_provenance_diagnostic.py",
     "67d781c8d8d2e93e29e10a7d764fffe7433f251c66e634fafa204d006093d4fa"),
    ("v15_diagnostic_fail_closed_tests", "tests/test_phase4_v15_broadcast_overwrite_provenance_diagnostic.py",
     "6825aa0a2c13ce919beb7bb5efe14a44262c920fd012db29c71364765a7c919e"),
    ("v14_independent_ta_addendum", "docs/art/phase4-v14-cap-mask-premise-independent-ta-failure-addendum-v1.json",
     "5ad37167219cc6d06464c9bf4a9a885131bc82d215ca5d6d803e3dad2cdc3aee"),
    ("v14_reproducible_receipt_v2", "docs/art/phase4-v14-cap-mask-premise-falsification-v2/premise-falsification-receipt-v2.json",
     "bf320f08453a1b55f48235193ecd92ed3a88ccf671b506ee0cea36276e9f97f8"),
    ("v11_81_source_inventory", str(RESULTS.relative_to(ROOT)),
     "4e7e884569eec1197ef11d39d256e2e43eef11c49ee7c84ff150b93c3c6d61ef"),
]
GAPS = [(783, 413), (892, 491), (940, 525)]
EXPECTED_GAPS = [(84, 18, 66, 18, True), (96, 1, 95, 1, False),
                 (88, 0, 88, 0, False)]
REPAIR_IDS = [
    "d6d850a1a10c7c7ce403e6de4afd4330e4c6e89b9100c72f4c23fbab1f6f978b",
    "1890fd0652c8f74a8b03a35a6cee1ad2cab43062987cab73efb1f6b455da468a",
    "1bf60bcd841a0d754c82876b55a4d3d6c29ebad582e069d4f4a608a4778c33a9",
]
SOURCE_DIGEST = "33511aa46717a208f3db43acd278416b8fe8393e2e003484da8f44dabffe5645"
TOPOLOGY_DIGEST = "77c42c0d66a566b4a73140e2d8e5f68a766bc6af4a98488a953323b77a52e87f"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def canonical_sha(value: Any) -> str:
    return hashlib.sha256(json.dumps(value, sort_keys=True,
        separators=(",", ":")).encode()).hexdigest()


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
                    if dx == 0 and dy == 0:
                        continue
                    point = (x + dx, y + dy)
                    if point in remaining:
                        remaining.remove(point)
                        stack.append(point)
        largest = max(largest, size)
    return largest


def broadcast_row() -> dict[str, Any]:
    rows = [json.loads(line) for line in RESULTS.read_text(
        encoding="utf-8").splitlines() if line]
    matches = [row for row in rows if row.get("capture_id") ==
               "camera_broadcast_1080"]
    if len(matches) != 1:
        raise RuntimeError("v11_broadcast_row_not_unique")
    return matches[0]


def validate(path: Path = DEFAULT) -> list[str]:
    errors: list[str] = []
    try:
        value = load(path)
        receipt = load(RECEIPT)
        ledger = load(LEDGER)
        source_row = broadcast_row()
    except (OSError, ValueError, RuntimeError) as error:
        return ["input_invalid:" + str(error)]

    def require(condition: bool, message: str) -> None:
        if not condition:
            errors.append(message)

    require(value.get("schema") ==
            "billiardgl.phase4.v15.independent_ta_diagnostic_acceptance_decision_gate.v1",
            "schema_invalid")
    require(value.get("status") ==
            "PASS_DIAGNOSTIC_ACCEPTED_INFEASIBLE_UNDER_FROZEN_SEMANTICS_PRODUCT_DECISION_REQUIRED_NO_AUTH",
            "status_invalid")
    observed = [(item.get("role"), item.get("path"), item.get("sha256"))
                for item in value.get("frozen_bindings", [])]
    require(observed == BINDINGS, "binding_inventory_invalid")
    for _, relative, digest in BINDINGS:
        target = ROOT / relative
        require(target.is_file() and sha(target) == digest,
                "binding_missing_or_drift:" + relative)
    require(upstream.validate(run_replay=False) == [],
            "upstream_corrected_diagnostic_invalid")

    corrected = value.get("corrected_classification_acceptance", {})
    require(corrected.get("distinct_provenance_key") ==
            "arc_segment_id + normalized_primitive_stem"
            and corrected.get("same_stem_core_and_halo_is_cross") is False
            and corrected.get("different_stem_is_cross") is True
            and corrected.get("different_arc_is_cross") is True
            and corrected.get("color_selects_classification") is False
            and corrected.get("standard_cvd_topology_identical") is True,
            "corrected_cross_provenance_contract_invalid")
    require(receipt.get("classification_contract", {}) == {
        "distinct_provenance_key": "arc_segment_id + normalized_primitive_stem",
        "same_stem_core_and_halo_is_cross": False,
        "different_stem_is_cross": True,
        "different_arc_is_cross": True,
        "color_selects_classification": False,
        "standard_cvd_topology_identical": True,
    }, "receipt_cross_provenance_contract_invalid")

    inventory = value.get("source_inventory_contract", {})
    ids = source_row.get("ordered_source_probe_ids", [])
    digest = hashlib.sha256("".join(item + "\n" for item in ids).encode()).hexdigest()
    require(len(ids) == 81 and len(set(ids)) == 81 and digest == SOURCE_DIGEST,
            "upstream_exact_81_ids_or_digest_invalid")
    require(inventory.get("ordered_source_probe_ids") == ids
            and inventory.get("ordered_source_probe_count") == 81
            and inventory.get("ordered_source_probe_digest") == SOURCE_DIGEST
            and inventory.get("v11_broadcast_row_canonical_sha256") ==
                canonical_sha(source_row)
            and inventory.get("exact_repair_probe_ids_in_order") == REPAIR_IDS
            and all(repair in ids for repair in REPAIR_IDS),
            "gate_source_or_repair_inventory_invalid")
    require(inventory.get("foreign_evidence_count") == 0
            and inventory.get("double_assignment_count") == 0
            and inventory.get("unowned_evidence_count") == 0,
            "gate_ownership_contamination_invalid")

    recomputed = value.get("independent_recomputed_diagnostic", {})
    expected_rows = recomputed.get("per_palette_three_gap_results_in_order", [])
    require(len(expected_rows) == 3, "gate_gap_inventory_invalid")
    palettes = ledger.get("palettes", [])
    require([item.get("palette") for item in palettes] ==
            ["standard", "color_vision_safe"], "ledger_palette_order_invalid")
    topology_digests: list[str] = []
    for palette in palettes:
        samples = palette.get("samples", [])
        topology = [{key: item for key, item in sample.items()
                     if key != "palette"} for sample in samples]
        topology_digests.append(canonical_sha(topology))
        by_pixel = {tuple(sample["physical_pixel"]): sample for sample in samples}
        for index, ((gx, gy), expected) in enumerate(zip(GAPS, EXPECTED_GAPS)):
            region = [sample for (x, y), sample in by_pixel.items()
                      if abs(x - gx) <= 8 and abs(y - gy) <= 8]
            eligible = {tuple(sample["physical_pixel"]) for sample in region
                        if not sample.get("final_predicted_primitive_id") or
                        sample.get("would_be_candidate_for_future_gap_only_non_overwrite") is True}
            protected = {tuple(sample["physical_pixel"]) for sample in region
                         if sample.get("final_predicted_primitive_id") and
                         sample.get("would_be_candidate_for_future_gap_only_non_overwrite") is not True}
            actual = (len(region), len(eligible), len(protected),
                      largest8(eligible), len(eligible) >= 2 and largest8(eligible) >= 2)
            require(actual == expected, "ledger_gap_metric_invalid:" +
                    palette.get("palette", "unknown") + ":" + str(GAPS[index]))
            row = expected_rows[index]
            require(row == {
                "coordinate": [gx, gy],
                "prior_actual_route_body_samples": expected[0],
                "eligible_samples": expected[1],
                "protected_or_ambiguous_samples": expected[2],
                "eligible_largest8": expected[3],
                "required_largest8": 2,
                "meets_requirement": expected[4],
            }, "gate_gap_metric_invalid:" + str(GAPS[index]))
        chains = palette.get("repair_probe_owner_chains", [])
        require([item.get("repair_probe_id") for item in chains] == REPAIR_IDS,
                "ledger_exact_repair_chain_order_invalid")
        require(palette.get("foreign_evidence_count") == 0
                and palette.get("double_assignment_count") == 0
                and palette.get("unowned_evidence_count") == 0,
                "ledger_ownership_contamination_invalid")
    require(topology_digests == [TOPOLOGY_DIGEST, TOPOLOGY_DIGEST]
            and recomputed.get("standard_cvd_topology_sha256") == TOPOLOGY_DIGEST,
            "standard_cvd_topology_digest_invalid")

    require(recomputed.get("blocking_940_525_fragment_partition") == {
        "core_base_body": 48, "core_start_cap_extension": 28,
        "core_end_cap_extension": 5, "inner_central_body": 0,
        "inner_lateral_annulus": 1, "inner_start_cap_annulus": 4,
        "inner_end_cap_annulus": 2, "outer_side_band": 0,
        "endpoint_or_tick_or_marker": 0, "sum": 88,
    }, "blocking_fragment_partition_invalid")
    require(recomputed.get("blocking_940_525_interval_partition") == {
        "dash_on_interior": 5, "dash_gap_intrusion": 0,
        "route_endpoint_envelope": 0,
        "cross_segment_or_cross_source_overlap": 83, "sum": 88,
    }, "blocking_interval_partition_invalid")

    decision = value.get("independent_ta_decision", {})
    require(decision.get("classification") ==
            "INFEASIBLE_UNDER_FROZEN_SEMANTICS"
            and decision.get("accepted_diagnostic") is True
            and decision.get("visual_acceptance") is False
            and decision.get("failed_coordinates") == [[892, 491], [940, 525]]
            and decision.get("automatic_scope_expansion") is False
            and decision.get("implementation_or_visual_change_authorized") is False
            and decision.get("new_product_or_design_decision_required") is True,
            "independent_infeasible_decision_invalid")
    candidates = value.get("product_decision_candidates_in_priority_order", [])
    require([item.get("priority") for item in candidates] == [1, 2]
            and [item.get("decision_id") for item in candidates] == [
                "LOCAL_CROSS_TRACK_ACTUAL_CONTINUITY_PRIORITY",
                "EXPLICIT_NON_OCCLUDING_CROSSING_GRAMMAR"]
            and [item.get("recommended") for item in candidates] == [True, False]
            and candidates[0].get("mutually_exclusive_with") ==
                candidates[1].get("decision_id")
            and candidates[1].get("mutually_exclusive_with") ==
                candidates[0].get("decision_id")
            and all(len(item.get("allowed_if_selected", [])) == 2
                    and len(item.get("forbidden_if_selected", [])) == 2
                    for item in candidates)
            and all(item.get("authorized_by_this_gate") is False
                    for item in candidates),
            "product_decision_candidate_priority_or_authority_invalid")
    successor = value.get("required_successor_gate_after_product_decision", {})
    require(successor.get("required") is True
            and successor.get("must_bind_selected_decision_id") is True
            and successor.get("must_define_new_allowlist_and_negative_fixtures") is True
            and successor.get("must_recompute_all_three_gap_topologies_standard_and_cvd") is True
            and successor.get("must_preserve_exact_81_source_ids_digest_and_three_repair_ids") is True
            and successor.get("must_require_CPU_static_before_any_future_GPU_or_TA") is True
            and successor.get("gpu_or_capture_authorized_here") is False,
            "successor_gate_contract_invalid")
    forbidden = "\n".join(value.get("forbidden", []))
    for required in ("largest8>=2", "same-stem _core/_halo", "all predicted core",
                     "color-only", "production", "Do not run GPU"):
        require(required in forbidden, "forbidden_clause_missing:" + required)
    proof = value.get("proof_boundary", {})
    require(proof.get("diagnostic_receipt_accepted") is True
            and proof.get("infeasible_under_frozen_semantics") is True
            and len(proof) == 18
            and all(item is False for key, item in proof.items()
                    if key not in ("diagnostic_receipt_accepted",
                                   "infeasible_under_frozen_semantics")),
            "proof_boundary_invalid")
    return errors


def main() -> int:
    target = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT
    errors = validate(target)
    if errors:
        for error in errors:
            print("FAIL: " + error, file=sys.stderr)
        return 1
    print("PASS_PHASE4_V15_INDEPENDENT_TA_DIAGNOSTIC_ACCEPTANCE_DECISION_GATE")
    print("decision=INFEASIBLE_UNDER_FROZEN_SEMANTICS product_decision_required=true")
    print("visual_change=false gpu=false auth=false")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
