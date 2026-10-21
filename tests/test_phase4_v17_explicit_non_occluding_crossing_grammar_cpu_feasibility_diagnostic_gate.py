#!/usr/bin/env python3
"""Fail-closed tests for the Phase4 V17 CPU feasibility diagnostic gate."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import validate_phase4_v17_explicit_non_occluding_crossing_grammar_cpu_feasibility_diagnostic_gate as validator

ROOT = Path(__file__).resolve().parents[1]
GATE = ROOT / "docs/art/phase4-v17-explicit-non-occluding-crossing-grammar-cpu-feasibility-diagnostic-gate-v1.json"


class V17ExplicitCrossingGrammarCpuFeasibilityDiagnosticGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(GATE.read_text(encoding="utf-8"))

    def errors_for(self, value: dict) -> list[str]:
        with tempfile.TemporaryDirectory(prefix="v17-crossing-feasibility-gate-") as directory:
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
        self.reject(lambda v: v["root_product_decision_binding"].update(selection_statement_sha256="0" * 64))

    def test_rejects_v16_failure_binding_drift(self) -> None:
        self.reject(lambda v: v["frozen_bindings"][2].update(sha256="0" * 64))

    def test_rejects_coordinate_as_generation_input(self) -> None:
        self.reject(lambda v: v["repair_region_matching_boundary"].update(generation_is_full_frame_and_coordinate_free=False))

    def test_rejects_missing_sample_profile(self) -> None:
        self.reject(lambda v: v["frozen_case_contract"]["sample_profiles"].pop())

    def test_rejects_source_digest_mutation(self) -> None:
        self.reject(lambda v: v["frozen_case_contract"].update(ordered_source_probe_digest="0" * 64))

    def test_rejects_collinear_acceptance(self) -> None:
        self.reject(lambda v: v["strict_intersection_predicate"]["explicit_infeasible_classes"].remove("collinear or coaxial overlap"))

    def test_rejects_angle_threshold_workaround(self) -> None:
        self.reject(lambda v: v["strict_intersection_predicate"].update(nonparallel_rule="Use adjustable angle threshold"))

    def test_rejects_missing_removal_negative(self) -> None:
        self.reject(lambda v: v["diagnostic_only_bracket_topology"]["required_cpu_checks"].pop())

    def test_rejects_production_file_allowlist(self) -> None:
        self.reject(lambda v: v["allowed_cpu_diagnostic_files"].append("src/Billiards/training/trajectory_overlay_renderer.cpp"))

    def test_rejects_gl_upload_precondition_removal(self) -> None:
        self.reject(lambda v: v["later_implementation_preconditions_not_authorized_here"].update(gl_upload_failure_must_abort_before_any_actual_or_predicted_draw=False))

    def test_rejects_product_or_gpu_authorization(self) -> None:
        def mutate(value: dict) -> None:
            value["authorization_boundary"]["product_glyph_implementation_authorized"] = True
            value["authorization_boundary"]["gpu_authorized"] = True
        self.reject(mutate)

    def test_rejects_false_feasibility_claim(self) -> None:
        self.reject(lambda v: v["proof_boundary"].update(feasibility_result_exists=True))


if __name__ == "__main__":
    unittest.main()
