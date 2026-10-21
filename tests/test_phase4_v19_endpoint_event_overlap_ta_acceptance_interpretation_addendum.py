#!/usr/bin/env python3
"""Fail-closed tests for the V19 endpoint/event TA interpretation."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import validate_phase4_v19_endpoint_event_overlap_ta_acceptance_interpretation_addendum as validator


ROOT = Path(__file__).resolve().parents[1]
ADDENDUM = ROOT / "docs/art/phase4-v19-endpoint-event-overlap-ta-acceptance-interpretation-addendum-v1.json"


class V19EndpointEventOverlapTaInterpretationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(ADDENDUM.read_text(encoding="utf-8"))

    def errors_for(self, value: dict) -> list[str]:
        with tempfile.TemporaryDirectory(prefix="v19-endpoint-overlap-ta-") as directory:
            path = Path(directory) / "addendum.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            return validator.validate(path)

    def reject(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(self.errors_for(value))

    def test_committed_addendum_passes(self) -> None:
        self.assertEqual(validator.validate(ADDENDUM), [])

    def test_rejects_gate_sha_drift(self) -> None:
        self.reject(lambda v: v["frozen_gate_bindings"][0].update(sha256="0" * 64))

    def test_rejects_zero_overlap_reinstatement(self) -> None:
        self.reject(lambda v: v["why_zero_geometry_overlap_is_impossible"].update(
            not_a_relaxation_of_identity_or_visibility=False))

    def test_rejects_notch(self) -> None:
        self.reject(lambda v: v["locked_endpoint_geometry"].update(
            open_notch_in_closed_diamond_allowed=True))

    def test_rejects_tail_truncation(self) -> None:
        self.reject(lambda v: v["locked_endpoint_geometry"].update(
            tail_event_truncation_or_suppression_allowed=True))

    def test_rejects_tail_interval_drift(self) -> None:
        self.reject(lambda v: v["locked_endpoint_geometry"].update(
            edge_corner_reference_tail_event_interval_px=[550.0, 551.0]))

    def test_rejects_marker_radius_drift(self) -> None:
        self.reject(lambda v: v["locked_endpoint_geometry"].update(
            predicted_endpoint_marker_l1_band_outer_to_inner_px=[3.5, 3.0]))

    def test_rejects_capsule_dimension_drift(self) -> None:
        self.reject(lambda v: v["locked_endpoint_geometry"].update(
            actual_capsule_tangent_by_normal_extent_px=[6.0, 2.5]))

    def test_rejects_capsule_offset_drift(self) -> None:
        self.reject(lambda v: v["locked_endpoint_geometry"].update(
            actual_capsule_center_offsets_from_canonical_endpoint_px=[-7.0, 7.0]))

    def test_rejects_wrong_overlap_owner(self) -> None:
        self.reject(lambda v: v["deterministic_overlap_ownership_contract"].update(
            analytic_overlap_samples_are_owned_by_later_endpoint_band=False))

    def test_rejects_non_repeatable_ownership(self) -> None:
        self.reject(lambda v: v["deterministic_overlap_ownership_contract"].update(
            ownership_must_be_top_left_half_open_and_repeatable=False))

    def test_rejects_palette_topology_drift(self) -> None:
        self.reject(lambda v: v["deterministic_overlap_ownership_contract"].update(
            palette_may_change_color_but_not_topology_or_ownership=False))

    def test_rejects_missing_cvd(self) -> None:
        self.reject(lambda v: v["cp1_endpoint_acceptance_delta_not_yet_executed"].update(
            palettes=["standard"]))

    def test_rejects_missing_sample_profile(self) -> None:
        self.reject(lambda v: v["cp1_endpoint_acceptance_delta_not_yet_executed"].update(
            sample_profiles=["center_1x", "asymmetric_2x"]))

    def test_rejects_premature_cp1_acceptance(self) -> None:
        self.reject(lambda v: v["cp1_endpoint_acceptance_delta_not_yet_executed"].update(
            cp1_accepted=True))

    def test_rejects_gpu_or_production_authority(self) -> None:
        def mutate(value: dict) -> None:
            value["authorization_boundary"]["gpu_authorized"] = True
            value["authorization_boundary"]["production_edit_authorized_by_this_addendum"] = True
        self.reject(mutate)


if __name__ == "__main__":
    unittest.main()
