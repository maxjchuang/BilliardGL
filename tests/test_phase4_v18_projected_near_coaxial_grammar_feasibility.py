#!/usr/bin/env python3

from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
VALIDATOR_PATH = ROOT / "scripts/validate_phase4_v18_projected_near_coaxial_grammar_feasibility.py"
SPEC = importlib.util.spec_from_file_location("phase4_v18_validator", VALIDATOR_PATH)
assert SPEC and SPEC.loader
V = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(V)


class Phase4V18FeasibilityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.receipt = json.loads(V.RECEIPT_PATH.read_text())
        cls.component = json.loads(V.COMPONENT_PATH.read_text())
        cls.raster = json.loads(V.RASTER_PATH.read_text())

    def validate(self, r=None, c=None, x=None):
        return V.validate(r or copy.deepcopy(self.receipt),
                          c or copy.deepcopy(self.component),
                          x or copy.deepcopy(self.raster), replay=False)

    def test_canonical(self):
        self.assertEqual([], self.validate())

    def test_fail_closed_mutations(self):
        mutations = []

        def receipt(name, fn):
            mutations.append((name, "r", fn))

        def component(name, fn):
            mutations.append((name, "c", fn))

        def raster(name, fn):
            mutations.append((name, "x", fn))

        receipt("schema", lambda d: d.__setitem__("schema", "bad"))
        receipt("status", lambda d: d.__setitem__("status", "PASS"))
        receipt("conclusion", lambda d: d.__setitem__("conclusion", "FEASIBLE"))
        receipt("route_lattice", lambda d: d.__setitem__("route_lattice_rule", "rounded"))
        receipt("formula", lambda d: d.__setitem__("both_safe_formula", "0"))
        receipt("case_count", lambda d: d.__setitem__("exact_case_count", 17))
        receipt("case_results", lambda d: d["exact18_candidate_match_counts"].__setitem__(0, 1))
        receipt("probe_count", lambda d: d.__setitem__("ordered_source_probe_count", 80))
        receipt("probe_digest", lambda d: d.__setitem__("ordered_source_probe_digest", "0" * 64))
        receipt("repair_id", lambda d: d["repair_ids_in_order"].__setitem__(0, "x"))
        receipt("empty_eval_digest", lambda d: d.__setitem__("empty_evaluated_candidate_universe_digest", "0" * 64))
        receipt("empty_valid_digest", lambda d: d.__setitem__("empty_valid_candidate_inventory_digest", "0" * 64))
        receipt("authority", lambda d: d["authorization_boundary"].__setitem__("gpu_run", True))
        receipt("binding_path_and_sha_redirect", lambda d: d["bindings"][0].update(path=d["bindings"][1]["path"], sha256=d["bindings"][1]["sha256"]))
        receipt("binding_sha", lambda d: d["bindings"][0].__setitem__("sha256", "0" * 64))
        receipt("binding_missing", lambda d: d["bindings"].pop())
        receipt("profile_count", lambda d: d["sample_profiles"].pop())
        receipt("profile_id", lambda d: d["sample_profiles"][0].__setitem__("id", "rounded_1x"))
        receipt("profile_members", lambda d: d["sample_profiles"][0].__setitem__("actual_members", 242))
        receipt("profile_coverage", lambda d: d["sample_profiles"][1].__setitem__("coverage_cells", 9222))
        receipt("profile_bounded", lambda d: d["sample_profiles"][2].__setitem__("bounded", 831))
        receipt("profile_missing", lambda d: d["sample_profiles"][2].__setitem__("missing_leave", 790))
        receipt("profile_both_safe", lambda d: d["sample_profiles"][0].__setitem__("both_safe", 1))
        receipt("profile_evaluated", lambda d: d["sample_profiles"][0].__setitem__("evaluated_candidates", 1))
        receipt("link_path", lambda d: d["reproducible_cpu_contract"].__setitem__("link_recipe_path", "CMakeLists.txt"))
        receipt("link_sha", lambda d: d["reproducible_cpu_contract"].__setitem__("link_recipe_sha256", "0" * 64))
        receipt("link_count", lambda d: d["reproducible_cpu_contract"].__setitem__("link_input_count", 48))
        receipt("link_ledger", lambda d: d["reproducible_cpu_contract"].__setitem__("ordered_link_input_ledger_sha256", "0" * 64))

        component("schema", lambda d: d.__setitem__("schema", "bad"))
        component("probe_generation", lambda d: d.__setitem__("contrast_probe_used_for_generation", True))
        component("repair_generation", lambda d: d.__setitem__("repair_id_used_for_generation", True))
        component("coordinate_generation", lambda d: d.__setitem__("coordinate_used_for_generation", True))
        component("palette_order", lambda d: d["palettes"].reverse())
        component("cvd_drift", lambda d: d["palettes"][1]["profiles"][0].__setitem__("coverage_cell_count", 1))
        component("profile_missing", lambda d: d["palettes"][0]["profiles"].pop())
        component("member_count", lambda d: d["palettes"][0]["profiles"][0].__setitem__("actual_exact_member_count", 1))
        component("protected_count", lambda d: d["palettes"][0]["profiles"][0].__setitem__("complete_protected_predicted_sample_count", 0))
        component("component_count", lambda d: d["palettes"][0]["profiles"][0].__setitem__("relationship_component_count", 1))
        component("formula_nonzero", lambda d: d["palettes"][0]["profiles"][0].__setitem__("bounded_missing_leave_position_count", 3))
        component("minimum_candidate", lambda d: d["palettes"][0]["profiles"][0].__setitem__("minimum_candidate_count", 1))
        component("evaluated_candidate", lambda d: d["palettes"][0]["profiles"][0].__setitem__("evaluated_candidate_count", 1))
        component("valid_candidate", lambda d: d["palettes"][0]["profiles"][0].__setitem__("valid_candidate_count", 1))
        component("package_after", lambda d: d["palettes"][0]["profiles"][0].__setitem__("package_after_digest", "0" * 64))
        component("component_list", lambda d: d["palettes"][0]["profiles"][0]["components"].pop())
        component("disposition_partition", lambda d: d["palettes"][0]["profiles"][0]["candidate_disposition_counts"].__setitem__("x", 1))
        component("bounded_reason", lambda d: next(x for x in d["palettes"][0]["profiles"][0]["components"] if x["bounded"]).__setitem__("candidate_failure_reason", "none"))
        component("bounded_candidate", lambda d: next(x for x in d["palettes"][0]["profiles"][0]["components"] if x["bounded"]).__setitem__("candidate_count", 1))
        component("bounded_eval_digest", lambda d: next(x for x in d["palettes"][0]["profiles"][0]["components"] if x["bounded"]).__setitem__("evaluated_candidate_universe_digest", "0" * 64))
        component("bounded_valid_digest", lambda d: next(x for x in d["palettes"][0]["profiles"][0]["components"] if x["bounded"]).__setitem__("valid_candidate_inventory_digest", "0" * 64))
        component("bounded_shell", lambda d: next(x for x in d["palettes"][0]["profiles"][0]["components"] if x["bounded"]).__setitem__("maximum_shell_index", 1919))
        component("bounded_route_digest", lambda d: next(x for x in d["palettes"][0]["profiles"][0]["components"] if x["bounded"]).__setitem__("route_lattice_digest", ""))
        component("bounded_both_safe", lambda d: (next(x for x in d["palettes"][0]["profiles"][0]["components"] if x["bounded"]).__setitem__("leave_lattice_position_count", 1), next(x for x in d["palettes"][0]["profiles"][0]["components"] if x["bounded"]).__setitem__("rejoin_lattice_position_count", 1)))
        component("top_left_fixture", lambda d: d.__setitem__("top_left_y_down_half_open_raster_fixtures_1x_2x_4x", False))
        component("rounding_fixture", lambda d: d.__setitem__("synthetic_exact_projection_avoids_rounding_loss", False))
        component("signed_fixture", lambda d: d.__setitem__("synthetic_cross_side_equal_best_fails_closed", False))
        component("safety_fixture", lambda d: d.__setitem__("synthetic_graph_invalid_cannot_outrank_valid", False))
        component("fixture_count", lambda d: d.__setitem__("synthetic_fixture_count", 32))

        raster("schema", lambda d: d.__setitem__("schema", "bad"))
        raster("case_count", lambda d: d.__setitem__("case_count", 17))
        raster("case_removed", lambda d: d["cases"].pop())
        raster("case_ordinal", lambda d: d["cases"][0].__setitem__("case_ordinal", 2))
        raster("case_palette", lambda d: d["cases"][0].__setitem__("palette", "color"))
        raster("case_profile", lambda d: d["cases"][0].__setitem__("sample_profile", "rounded"))
        raster("case_region", lambda d: d["cases"][0].__setitem__("repair_region_index", 1))
        raster("case_repair", lambda d: d["cases"][0].__setitem__("repair_probe_id", "x"))
        raster("case_bounded", lambda d: d["cases"][0].__setitem__("matching_bounded_component_count", 2))
        raster("case_candidate", lambda d: d["cases"][0].__setitem__("matching_candidate_component_count", 1))
        raster("case_feasible", lambda d: d["cases"][0].__setitem__("feasible", True))
        raster("overall", lambda d: d.__setitem__("overall_result", "FEASIBLE"))
        raster("topology", lambda d: d.__setitem__("standard_cvd_topology_identical", False))
        raster("package", lambda d: d.__setitem__("original_draw_package_unchanged", False))
        raster("growth", lambda d: d.__setitem__("semantic_token_growth", 1))
        raster("gpu", lambda d: d.__setitem__("gpu_run", True))

        self.assertEqual(len(mutations), 73)
        for name, kind, mutate in mutations:
            with self.subTest(name=name):
                r = copy.deepcopy(self.receipt)
                c = copy.deepcopy(self.component)
                x = copy.deepcopy(self.raster)
                target = {"r": r, "c": c, "x": x}[kind]
                mutate(target)
                self.assertTrue(V.validate(r, c, x, replay=False), name)
        print("phase4_v18_fail_closed_mutations=73/73")


if __name__ == "__main__":
    unittest.main()
