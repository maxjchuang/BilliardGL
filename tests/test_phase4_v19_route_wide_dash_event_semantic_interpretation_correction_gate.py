#!/usr/bin/env python3
"""Fail-closed tests for the V19 route-wide dash semantic correction."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import validate_phase4_v19_route_wide_dash_event_semantic_interpretation_correction_gate as validator


ROOT = Path(__file__).resolve().parents[1]
GATE = ROOT / "docs/art/phase4-v19-route-wide-dash-event-semantic-interpretation-correction-gate-v1.json"


class V19RouteWideDashSemanticCorrectionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(GATE.read_text(encoding="utf-8"))

    def errors_for(self, value: dict) -> list[str]:
        with tempfile.TemporaryDirectory(prefix="v19-dash-event-correction-") as directory:
            path = Path(directory) / "gate.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            return validator.validate(path)

    def reject(self, mutation) -> None:
        value = copy.deepcopy(self.value)
        mutation(value)
        self.assertTrue(self.errors_for(value))

    def test_committed_gate_passes(self) -> None:
        self.assertEqual(validator.validate(GATE), [])

    def test_rejects_original_gate_drift(self) -> None:
        self.reject(lambda v: v["frozen_bindings"][0].update(sha256="0" * 64))

    def test_rejects_false_frozen_nonfinal_example(self) -> None:
        self.reject(lambda v: v["nonfinal_unsatisfiable_example_binding"].update(source_or_binary_frozen=True))

    def test_rejects_residual_drift(self) -> None:
        self.reject(lambda v: v["nonfinal_unsatisfiable_example_binding"].update(next_arc_residual_dash_on_px=1.0))

    def test_rejects_root_decision_drift(self) -> None:
        self.reject(lambda v: v["root_semantic_correction_decision_binding"].update(selected=False))

    def test_rejects_per_parent_dash_fragment(self) -> None:
        self.reject(lambda v: v["supersession_contract"].update(predicted_per_parent_dash_fragment_member_identity_allowed=True))

    def test_rejects_probe_skip(self) -> None:
        self.reject(lambda v: v["supersession_contract"].update(silent_probe_omission_for_emitted_event_member_allowed=True))

    def test_rejects_cadence_reset(self) -> None:
        self.reject(lambda v: v["route_wide_dash_event_contract"].update(dash_event_may_reset_at_analytic_source_boundary=True))

    def test_rejects_event_split_for_probe(self) -> None:
        self.reject(lambda v: v["route_wide_dash_event_contract"].update(event_may_be_split_only_for_source_identity_or_probe_convenience=True))

    def test_rejects_subpixel_cap_extension(self) -> None:
        self.reject(lambda v: v["route_wide_dash_event_contract"].update(subpixel_cap_or_extension_across_event_bounds_allowed=True))

    def test_rejects_contributor_field_loss(self) -> None:
        self.reject(lambda v: v["ordered_contributor_interval_lineage_contract"]["contributor_record_fields_in_order"].pop())

    def test_rejects_overlapping_contributors(self) -> None:
        self.reject(lambda v: v["ordered_contributor_interval_lineage_contract"].update(intervals_are_finite_half_open_and_nonoverlapping=False))

    def test_rejects_source_denominator_change(self) -> None:
        self.reject(lambda v: v["source_group_and_gap_denominator_contract"].update(source_group_denominator_unchanged=False))

    def test_rejects_gap_source_deletion(self) -> None:
        self.reject(lambda v: v["source_group_and_gap_denominator_contract"].update(gap_only_source_group_remains_in_source_denominator=False))

    def test_rejects_gap_token(self) -> None:
        self.reject(lambda v: v["source_group_and_gap_denominator_contract"].update(gap_records_have_no_token_and_no_probe=False))

    def test_rejects_one_profile_probe(self) -> None:
        self.reject(lambda v: v["event_semantic_and_probe_denominator_contract"].update(one_common_candidate_must_pass_all_required_sample_profiles=False))

    def test_rejects_false_cpu_pass(self) -> None:
        self.reject(lambda v: v["cp1_acceptance_delta_not_executed"].update(cpu_checkpoint_pass=True))

    def test_rejects_actual_scope_change(self) -> None:
        self.reject(lambda v: v["supersession_contract"].update(all_actual_rail_endpoint_orientation_layout_offscreen_gl_and_future_acceptance_contracts_unchanged=False))

    def test_rejects_production_or_gpu_authorization(self) -> None:
        def mutation(value: dict) -> None:
            value["authorization_boundary"].update(production_edit_authorized_by_this_gate=True, gpu_authorized=True)
        self.reject(mutation)


if __name__ == "__main__":
    unittest.main()
