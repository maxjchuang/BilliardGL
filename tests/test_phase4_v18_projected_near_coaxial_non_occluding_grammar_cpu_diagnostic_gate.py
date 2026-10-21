#!/usr/bin/env python3
"""Fail-closed tests for the Phase4 V18 projected-near/coaxial CPU gate."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import validate_phase4_v18_projected_near_coaxial_non_occluding_grammar_cpu_diagnostic_gate as validator

ROOT = Path(__file__).resolve().parents[1]
GATE = ROOT / "docs/art/phase4-v18-projected-near-coaxial-non-occluding-grammar-cpu-diagnostic-gate-v1.json"


class V18ProjectedNearCoaxialCpuDiagnosticGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(GATE.read_text(encoding="utf-8"))

    def errors_for(self, value: dict) -> list[str]:
        with tempfile.TemporaryDirectory(prefix="v18-near-coaxial-gate-") as directory:
            path = Path(directory) / "gate.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            return validator.validate(path)

    def reject(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(self.errors_for(value))

    def test_committed_gate_passes(self) -> None:
        self.assertEqual(validator.validate(GATE), [])

    def test_rejects_root_decision_drift(self) -> None:
        self.reject(lambda v: v["root_product_decision_binding"].update(statement_sha256="0" * 64))

    def test_rejects_v17_failure_binding_drift(self) -> None:
        self.reject(lambda v: v["frozen_bindings"][0].update(sha256="0" * 64))

    def test_rejects_coordinate_before_inventory(self) -> None:
        self.reject(lambda v: v["case_acceptance_contract"].update(full_frame_inventory_frozen_before_matcher=False))

    def test_rejects_coordinate_generation_input(self) -> None:
        self.reject(lambda v: v["coordinate_free_full_silhouette_source_contract"]["forbidden_generation_inputs"].remove("repair coordinates or radius"))

    def test_rejects_contrast_probe_eligibility(self) -> None:
        self.reject(lambda v: v["coordinate_free_full_silhouette_source_contract"]["forbidden_generation_inputs"].remove("contrast-probe membership"))

    def test_rejects_centerline_intersection_requirement(self) -> None:
        self.reject(lambda v: v["relationship_component_contract"].update(strict_centerline_intersection_required=True))

    def test_rejects_normalized_family_as_exact_source_substitute(self) -> None:
        self.reject(lambda v: v["relationship_component_contract"].update(family_aggregation_rule="Associate all layers by normalized stem"))

    def test_rejects_tunable_near_distance(self) -> None:
        self.reject(lambda v: v["relationship_component_contract"].update(node="Use a tunable 4px near threshold"))

    def test_rejects_whole_route_overlap(self) -> None:
        self.reject(lambda v: v["relationship_component_contract"]["explicit_infeasible_classes"].remove("whole-primitive or whole-route overlap"))

    def test_rejects_ambiguous_frame(self) -> None:
        self.reject(lambda v: v["relationship_component_contract"].update(deterministic_frame="Either normal is acceptable"))

    def test_rejects_nonunique_minimum(self) -> None:
        self.reject(lambda v: v["diagnostic_bypass_bracket_contract"].update(minimality="Pick the first candidate"))

    def test_rejects_predicted_occlusion(self) -> None:
        self.reject(lambda v: v["diagnostic_bypass_bracket_contract"].update(protected_predicted_non_occlusion="May overwrite predicted core"))

    def test_rejects_missing_sample_profile(self) -> None:
        self.reject(lambda v: v["frozen_case_contract"]["sample_profiles"].pop())

    def test_rejects_source_digest_drift(self) -> None:
        self.reject(lambda v: v["frozen_case_contract"].update(ordered_contrast_probe_digest="0" * 64))

    def test_rejects_production_file_allowlist(self) -> None:
        self.reject(lambda v: v["allowed_standalone_cpu_diagnostic_files"].append("src/Billiards/training/trajectory_overlay_renderer.cpp"))

    def test_rejects_gl_upload_fail_closed_removal(self) -> None:
        self.reject(lambda v: v["future_implementation_preconditions_not_authorized_here"].update(gl_buffer_and_texture_upload_errors_must_be_checked=False))

    def test_rejects_product_or_gpu_authorization(self) -> None:
        def mutate(value: dict) -> None:
            value["authorization_boundary"]["product_implementation_authorized"] = True
            value["authorization_boundary"]["gpu_authorized"] = True
        self.reject(mutate)

    def test_rejects_false_feasibility_claim(self) -> None:
        self.reject(lambda v: v["proof_boundary"].update(feasibility_result_exists=True))


if __name__ == "__main__":
    unittest.main()
