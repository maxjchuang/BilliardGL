#!/usr/bin/env python3
"""Validate the Phase4 V17 explicit crossing-grammar product-decision gate."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/art/phase4-v17-explicit-non-occluding-crossing-grammar-product-decision-gate-v1.json"
SCHEMA = "billiardgl.phase4.v17.explicit_non_occluding_crossing_grammar_product_decision_gate.v1"
BINDINGS = {
    "docs/art/phase4-v16-local-cross-track-actual-continuity-independent-ta-implementation-failure-addendum-v1.json": "6ccbb8fc7a149c146b755709a41558bd98a2592cf353f7a639660f1c910ce8ef",
    "docs/art/phase4-v16-local-cross-track-actual-continuity-premise-falsification-v1/premise-failure-receipt-v1.json": "6e5c729016795a8744fddd7351ae8d9f9d584f9cde91946a33bcbd83f982fa96",
    "docs/art/phase4-v15-independent-ta-diagnostic-acceptance-decision-gate-v1.json": "2628104e6d219a98fb38fa599058ec3dd22f11c658d7fe3aa1ffc82850bcf23b",
    "docs/art/phase4-v15-broadcast-overwrite-provenance-diagnostic-v1/provenance-ledger-v1.json": "bb2c3ecafb01d42cfddb8eec89a22619bf646ddb64773846747c5c76d2f5538b",
}


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

    require(value.get("schema") == SCHEMA, "schema_invalid")
    require(value.get("status") == "AWAITING_ROOT_PRODUCT_SELECTION_DESIGN_ONLY_NO_PRODUCTION_NO_GPU_NO_AUTH", "status_invalid")
    observed = {item.get("path"): item.get("sha256") for item in value.get("frozen_bindings", [])}
    require(observed == BINDINGS, "binding_inventory_or_sha_invalid")
    for relative, digest in BINDINGS.items():
        target = ROOT / relative
        require(target.is_file() and sha(target) == digest, f"binding_missing_or_drift:{relative}")
    closed = value.get("closed_priority_one", {})
    require(closed.get("decision_id") == "LOCAL_CROSS_TRACK_ACTUAL_CONTINUITY_PRIORITY" and closed.get("disposition") == "REJECTED_NOT_RESUMABLE_UNDER_V16_GATE" and closed.get("relaxing_largest8_or_fail_closed_is_allowed") is False, "priority_one_closure_invalid")
    candidate = value.get("successor_candidate", {})
    require(candidate.get("decision_id") == "EXPLICIT_NON_OCCLUDING_CROSSING_GRAMMAR" and candidate.get("priority") == 2 and candidate.get("recommendation") == "RECOMMENDED_AS_ONLY_REMAINING_BOUNDED_V15_PRODUCT_PATH", "priority_two_candidate_invalid")
    require(candidate.get("root_selection_required") is True and candidate.get("selected") is False and candidate.get("implementation_authorized") is False, "selection_boundary_invalid")
    contract = value.get("bounded_design_contract_if_later_selected", {})
    require("acceptance result are forbidden inputs" in contract.get("semantic_event_input", ""), "coordinate_independence_invalid")
    require(len(contract.get("event_normalization", [])) == 4 and len(contract.get("visual_grammar", [])) == 4, "event_or_visual_contract_invalid")
    controls = contract.get("frozen_regression_controls", {})
    require(controls.get("ordered_source_probe_count") == 81 and controls.get("ordered_source_probe_digest") == "33511aa46717a208f3db43acd278416b8fe8393e2e003484da8f44dabffe5645" and len(controls.get("exact_repair_probe_ids_in_order", [])) == 3, "source_regression_contract_invalid")
    next_gate = value.get("required_next_gate_after_explicit_root_selection", {})
    require(next_gate.get("stage") == "V17_CPU_STATIC_VISUAL_GRAMMAR_PROTOTYPE_DESIGN_GATE" and next_gate.get("must_bind_exact_root_selection_statement_and_sha") is True and len(next_gate.get("minimum_cpu_static_evidence", [])) == 5, "next_gate_contract_invalid")
    require(next_gate.get("future_gpu_requires_separate_readiness_and_single_use_authorization") is True and next_gate.get("production_allowlist_defined_by_this_gate") is False, "future_authorization_boundary_invalid")
    require(len(value.get("forbidden", [])) == 5, "forbidden_inventory_invalid")
    authorization = value.get("authorization_boundary", {})
    require(len(authorization) == 17 and all(item is False for item in authorization.values()), "authorization_boundary_must_be_all_false")
    return errors


def main() -> int:
    target = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT
    errors = validate(target)
    if errors:
        for error in errors:
            print("FAIL: " + error, file=sys.stderr)
        return 1
    print("PASS_PHASE4_V17_EXPLICIT_NON_OCCLUDING_CROSSING_GRAMMAR_PRODUCT_DECISION_GATE")
    print("priority1=rejected priority2=recommended selected=false production=false gpu=false")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
