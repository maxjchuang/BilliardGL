#!/usr/bin/env python3
"""Fail-closed tests for the independent Phase4 V18 failure addendum."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import validate_phase4_v18_projected_near_coaxial_grammar_independent_ta_feasibility_failure_addendum as validator


ROOT = Path(__file__).resolve().parents[1]
ADDENDUM = ROOT / "docs/art/phase4-v18-projected-near-coaxial-grammar-independent-ta-feasibility-failure-addendum-v1.json"


class V18IndependentTaFeasibilityFailureAddendumTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(ADDENDUM.read_text(encoding="utf-8"))

    def errors_for(self, value: dict) -> list[str]:
        with tempfile.TemporaryDirectory(prefix="v18-ta-feasibility-failure-") as directory:
            path = Path(directory) / "addendum.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            return validator.validate(path)

    def reject(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(self.errors_for(value))

    def test_committed_addendum_passes(self) -> None:
        self.assertEqual(validator.validate(ADDENDUM), [])

    def test_rejects_final_receipt_drift(self) -> None:
        self.reject(lambda v: v["frozen_bindings"][0].update(sha256="0" * 64))

    def test_rejects_false_reproduction_pass(self) -> None:
        self.reject(lambda v: v["independent_reproduction_audit"].update(validator_fresh_recompile_relink_byte_replay="FAIL"))

    def test_rejects_rounded_route_lattice(self) -> None:
        self.reject(lambda v: v["independent_reproduction_audit"].update(exact_projected_t_route_lattice_without_integer_rounding=False))

    def test_rejects_one_sided_generation(self) -> None:
        self.reject(lambda v: v["independent_reproduction_audit"].update(both_signed_normal_sides_generated_before_matching=False))

    def test_rejects_safety_after_ranking(self) -> None:
        self.reject(lambda v: v["independent_reproduction_audit"].update(candidate_safety_precedes_ranking=False))

    def test_rejects_profile_bounded_count_drift(self) -> None:
        self.reject(lambda v: v["independent_profile_findings"][1].update(bounded_component_count=279))

    def test_rejects_false_safe_anchor_pair(self) -> None:
        self.reject(lambda v: v["independent_profile_findings"][2].update(bounded_with_both_safe_anchors=1))

    def test_rejects_false_candidate(self) -> None:
        self.reject(lambda v: v["independent_failure_finding"]["exact18_candidate_match_counts"].__setitem__(0, 1))

    def test_rejects_empty_digest_drift(self) -> None:
        self.reject(lambda v: v["independent_failure_finding"].update(empty_valid_candidate_inventory_digest="0" * 64))

    def test_rejects_synthetic_as_frozen_feasibility(self) -> None:
        self.reject(lambda v: v["independent_failure_finding"].update(synthetic_fixture_is_frozen_scene_feasibility=True))

    def test_rejects_v16_reopen(self) -> None:
        self.reject(lambda v: v["bounded_grammar_disposition"].update(v16_local_cross_track_actual_continuity="OPEN"))

    def test_rejects_v17_reopen(self) -> None:
        self.reject(lambda v: v["bounded_grammar_disposition"].update(v17_explicit_non_occluding_strict_intersection_grammar="OPEN"))

    def test_rejects_v18_reopen(self) -> None:
        self.reject(lambda v: v["bounded_grammar_disposition"].update(v18_projected_near_or_coaxial_paired_bypass_grammar="OPEN"))

    def test_rejects_false_production_path(self) -> None:
        self.reject(lambda v: v["bounded_grammar_disposition"].update(production_path_remaining_under_v16_v17_v18_bounded_grammars=True))

    def test_rejects_all_visual_grammars_overclaim(self) -> None:
        self.reject(lambda v: v["bounded_grammar_disposition"].update(all_possible_visual_grammars_proven_infeasible=True))

    def test_rejects_automatic_fallback(self) -> None:
        self.reject(lambda v: v["successor_product_decision_requirement"].update(automatic_fallback_or_scope_expansion_allowed=True))

    def test_rejects_missing_new_product_decision(self) -> None:
        self.reject(lambda v: v["successor_product_decision_requirement"].update(new_root_product_decision_required=False))

    def test_rejects_successor_false_selection(self) -> None:
        self.reject(lambda v: v["successor_product_decision_requirement"].update(successor_selected_here=True))

    def test_rejects_production_or_gpu_authorization(self) -> None:
        def mutate(value: dict) -> None:
            value["authorization_boundary"]["production_edit_authorized"] = True
            value["authorization_boundary"]["gpu_authorized"] = True
        self.reject(mutate)


if __name__ == "__main__":
    unittest.main()
