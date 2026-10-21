from __future__ import annotations

import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts.validate_phase4_v14_broadcast_predicted_cap_prior_actual_core_mask_design_gate import (  # noqa: E402
    ALLOWED_PRODUCTION_FILES,
    BINDINGS,
    DEFAULT_GATE,
    FALSE_FIELDS,
    FIXTURE_IDS,
    load_json,
    validate,
    validate_bindings,
    validate_boundary,
    validate_feasibility,
    validate_fixtures,
    validate_implementation_contract,
    validate_scope,
)


class Phase4V14PriorActualCoreMaskDesignGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.gate = load_json(DEFAULT_GATE)

    def test_committed_gate_passes_as_design_only(self) -> None:
        result = validate(DEFAULT_GATE)
        self.assertEqual(result["status"],
                         "PASS_PHASE4_V14_PRIOR_ACTUAL_CORE_MASK_DESIGN_GATE")
        self.assertEqual(result["binding_count"], 8)
        self.assertEqual(result["allowed_production_file_count"], 4)
        self.assertEqual(result["negative_fixture_count"], 14)
        self.assertFalse(result["renderer_only_feasible"])
        self.assertTrue(result["modern_mask_required"])
        self.assertEqual(result["cpu_static_fixtures"], "NOT_RUN")
        self.assertFalse(result["production_modified"])
        self.assertFalse(result["gpu_authorized"])

    def test_json_round_trip(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "gate.json"
            path.write_text(json.dumps(self.gate, indent=2) + "\n")
            self.assertEqual(json.loads(path.read_text()), self.gate)

    def test_all_frozen_bindings_fail_closed_on_drift(self) -> None:
        for index in range(len(BINDINGS)):
            gate = copy.deepcopy(self.gate)
            gate["frozen_bindings"][index]["sha256"] = "0" * 64
            with self.assertRaises(RuntimeError):
                validate_bindings(gate)

    def test_renderer_only_geometry_cannot_be_relabelled_feasible(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["feasibility_conclusion"][
            "trajectory_renderer_two_file_cpu_geometry_only"] = "FEASIBLE"
        with self.assertRaises(RuntimeError):
            validate_feasibility(gate)
        gate = copy.deepcopy(self.gate)
        gate["feasibility_conclusion"]["current_v13_allowlist_sufficient"] = True
        with self.assertRaises(RuntimeError):
            validate_feasibility(gate)

    def test_mask_role_contract_excludes_ticks_endpoints_and_halo(self) -> None:
        text = "\n".join(self.gate["minimum_implementable_contract"][
            "trajectory_package_tagging"])
        self.assertIn("ActualRouteBodyCore", text)
        self.assertIn("PredictedInnerCap", text)
        self.assertIn("actual halo/rim/ticks/endpoints/markers", text)
        gate = copy.deepcopy(self.gate)
        gate["minimum_implementable_contract"]["trajectory_package_tagging"] = [
            "all core sublayers write mask"]
        with self.assertRaises(RuntimeError):
            validate_implementation_contract(gate)

    def test_mask_must_be_integer_non_color_and_any_sample_resolved(self) -> None:
        for needle in ("GL_R8UI or GL_R32UI",
                       "explicit integer any-sample/max resolve",
                       "never rely on normalized-color averaging"):
            gate = copy.deepcopy(self.gate)
            gate["minimum_implementable_contract"]["actual_core_mask_pipeline"] = [
                row.replace(needle, "color mask") for row in
                gate["minimum_implementable_contract"]["actual_core_mask_pipeline"]]
            with self.assertRaises(RuntimeError):
                validate_implementation_contract(gate)

    def test_exact_four_file_allowlist_cannot_expand_or_contract(self) -> None:
        self.assertEqual([row["path"] for row in self.gate["allowed_production_files"]],
                         ALLOWED_PRODUCTION_FILES)
        gate = copy.deepcopy(self.gate)
        gate["allowed_production_files"].append({
            "path": "src/Billiards/training/final_semantic_ownership.cpp",
            "allowed_symbols_or_regions": "ownership",
        })
        with self.assertRaises(RuntimeError):
            validate_scope(gate)
        gate = copy.deepcopy(self.gate)
        gate["allowed_production_files"] = gate["allowed_production_files"][:2]
        with self.assertRaises(RuntimeError):
            validate_scope(gate)

    def test_color_heuristic_and_draw_order_workarounds_remain_forbidden(self) -> None:
        gate = copy.deepcopy(self.gate)
        gate["forbidden_changes"] = ["No product exposure flag"]
        with self.assertRaises(RuntimeError):
            validate_scope(gate)
        joined = "\n".join(self.gate["forbidden_changes"])
        self.assertIn("HDR/RGB/color-equality", joined)
        self.assertIn("extra trajectory color draw", joined)
        self.assertIn("source ID/count/order/coordinate", joined)

    def test_negative_fixture_matrix_is_exact_and_not_run(self) -> None:
        group = self.gate["required_cpu_and_static_negative_fixtures"]
        self.assertEqual(group["status"], "NOT_RUN")
        self.assertEqual([row["id"] for row in group["fixtures"]], FIXTURE_IDS)
        for index in range(len(FIXTURE_IDS)):
            gate = copy.deepcopy(self.gate)
            del gate["required_cpu_and_static_negative_fixtures"]["fixtures"][index]
            with self.assertRaises(RuntimeError):
                validate_fixtures(gate)

    def test_actual_halo_scene_color_predicted_core_and_lateral_are_negative(self) -> None:
        requirements = {row["id"]: row["requirement"] for row in
                        self.gate["required_cpu_and_static_negative_fixtures"]["fixtures"]}
        for fixture_id in ("actual_halo_negative", "scene_color_negative",
                           "predicted_core_negative", "lateral_annulus_negative",
                           "no_actual_negative"):
            self.assertIn(fixture_id, requirements)
        gate = copy.deepcopy(self.gate)
        gate["required_cpu_and_static_negative_fixtures"]["fixtures"] = [
            row for row in gate["required_cpu_and_static_negative_fixtures"]["fixtures"]
            if row["id"] != "scene_color_negative"]
        with self.assertRaises(RuntimeError):
            validate_fixtures(gate)

    def test_invalid_or_stale_mask_must_fail_closed(self) -> None:
        fixtures = {row["id"]: row["requirement"] for row in
                    self.gate["required_cpu_and_static_negative_fixtures"]["fixtures"]}
        self.assertIn("wrong-format", fixtures["stale_or_invalid_mask_fail_closed"])
        self.assertIn("stale-frame", fixtures["stale_or_invalid_mask_fail_closed"])
        self.assertIn("explicit error", fixtures["stale_or_invalid_mask_fail_closed"])

    def test_no_boundary_can_claim_execution_or_acceptance(self) -> None:
        for field in FALSE_FIELDS:
            gate = copy.deepcopy(self.gate)
            gate["proof_boundary"][field] = True
            with self.assertRaises(RuntimeError):
                validate_boundary(gate)


if __name__ == "__main__":
    unittest.main()
