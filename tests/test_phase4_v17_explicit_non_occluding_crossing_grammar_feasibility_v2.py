#!/usr/bin/env python3
"""Canonical replay plus fail-closed mutations for the Phase4 V17 v2 receipt."""

from __future__ import annotations

import copy
import json
import pathlib
import shutil
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
VALIDATOR = ROOT / "scripts/validate_phase4_v17_explicit_non_occluding_crossing_grammar_feasibility_v2.py"
EVIDENCE = ROOT / "docs/art/phase4-v17-explicit-crossing-grammar-feasibility-diagnostic-v2"
CANONICAL = {
    "receipt": EVIDENCE / "feasibility-receipt-v2.json",
    "source": EVIDENCE / "phase4_v17_crossing_feasibility_cpu_diagnostic.cpp",
    "intersection": EVIDENCE / "intersection-ledger-v2.json",
    "raster": EVIDENCE / "raster-ledger-v2.json",
}


class Phase4V17V2FeasibilityTests(unittest.TestCase):
    maxDiff = None

    def run_validator(self, paths: dict[str, pathlib.Path], recompile: bool):
        command = ["python3", str(VALIDATOR), "--receipt", str(paths["receipt"]),
                   "--source", str(paths["source"]), "--intersection-ledger",
                   str(paths["intersection"]), "--raster-ledger", str(paths["raster"])]
        if not recompile:
            command.append("--skip-recompile")
        return subprocess.run(command, cwd=ROOT, capture_output=True, text=True)

    def test_canonical_recompile_relink_replay(self):
        result = self.run_validator(CANONICAL, True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(json.loads(result.stdout), {"errors": [], "ok": True})

    def exercise_mutation(self, target: str, mutate):
        with tempfile.TemporaryDirectory(prefix="phase4-v17-v2-mutation-") as temporary:
            root = pathlib.Path(temporary)
            paths = {}
            for role, canonical in CANONICAL.items():
                path = root / canonical.name
                shutil.copyfile(canonical, path)
                paths[role] = path
            path = paths[target]
            if path.suffix == ".json":
                value = json.loads(path.read_text())
                mutate(value)
                path.write_text(json.dumps(value, separators=(",", ":")) + "\n")
            else:
                path.write_text(mutate(path.read_text()))
            result = self.run_validator(paths, False)
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
            parsed = json.loads(result.stdout)
            self.assertFalse(parsed["ok"])
            self.assertTrue(parsed["errors"])


def set_path(path, value):
    def mutate(document):
        cursor = document
        for part in path[:-1]:
            cursor = cursor[part]
        cursor[path[-1]] = value
    return mutate


MUTATIONS = [
    ("receipt_schema", "receipt", set_path(["schema"], "mutated")),
    ("receipt_status", "receipt", set_path(["status"], "mutated")),
    ("result_class", "receipt", set_path(["result", "classification"], "FEASIBLE")),
    ("result_actual", "receipt", set_path(["result", "actual_descriptor_count"], 80)),
    ("result_predicted", "receipt", set_path(["result", "predicted_dash_on_descriptor_count"], 67)),
    ("result_actual_missing", "receipt", set_path(["result", "missing_actual_semantic_descriptor_count"], 1)),
    ("result_pred_missing", "receipt", set_path(["result", "missing_predicted_semantic_descriptor_count"], 1)),
    ("result_pairs", "receipt", set_path(["result", "evaluated_pair_count"], 5507)),
    ("result_disjoint", "receipt", set_path(["result", "disjoint_pair_count"], 5507)),
    ("result_event", "receipt", set_path(["result", "strict_interior_nonparallel_event_count"], 1)),
    ("result_cases", "receipt", set_path(["result", "exact_case_count"], 17)),
    ("result_feasible", "receipt", set_path(["result", "feasible_case_count"], 1)),
    ("result_infeasible", "receipt", set_path(["result", "infeasible_case_count"], 17)),
    ("probe_eligibility", "receipt", set_path(["provenance_contract", "contrast_probe_used_for_eligibility"], True)),
    ("repair_eligibility", "receipt", set_path(["provenance_contract", "repair_id_used_for_eligibility"], True)),
    ("coordinate_generation", "receipt", set_path(["provenance_contract", "coordinates_used_for_generation"], True)),
    ("generation_order", "receipt", set_path(["provenance_contract", "full_frame_generation_before_probe_and_repair_audit"], False)),
    ("eligibility_string", "receipt", set_path(["provenance_contract", "eligibility"], "probe")),
    ("normalized_stem_string", "receipt", set_path(["provenance_contract", "normalized_stem_use"], "association")),
    ("semantic_batch_count", "receipt", set_path(["provenance_contract", "semantic_batch_count"], 485)),
    ("token_count", "receipt", set_path(["provenance_contract", "nonzero_final_ownership_token_count"], 485)),
    ("semantic_growth", "receipt", set_path(["provenance_contract", "semantic_batch_growth"], 1)),
    ("token_growth", "receipt", set_path(["provenance_contract", "semantic_token_growth"], 1)),
    ("source_growth", "receipt", set_path(["provenance_contract", "source_probe_growth"], 1)),
    ("probe_count", "receipt", set_path(["post_generation_regression", "ordered_contrast_probe_count"], 80)),
    ("probe_digest", "receipt", set_path(["post_generation_regression", "ordered_contrast_probe_digest"], "0" * 64)),
    ("repair_ids", "receipt", set_path(["post_generation_regression", "repair_probe_ids_in_order"], [])),
    ("repair_coords", "receipt", set_path(["post_generation_regression", "repair_coordinates_output_only"], [[0, 0]])),
    ("sample_profile_order", "receipt", set_path(["post_generation_regression", "real_sample_profiles"], ["asymmetric_4x", "center_1x", "asymmetric_2x"])),
    ("auth_gpu", "receipt", set_path(["authorization_boundary", "gpu_run"], True)),
    ("auth_production", "receipt", set_path(["authorization_boundary", "production_modified"], True)),
    ("auth_machine", "receipt", set_path(["authorization_boundary", "machine_pass"], True)),
    ("auth_ta", "receipt", set_path(["authorization_boundary", "ta_pass"], True)),
    ("binding_source_hash", "receipt", lambda d: d["bindings"][5].update(sha256="0" * 64)),
    ("binding_gate_hash", "receipt", lambda d: d["bindings"][0].update(sha256="0" * 64)),
    ("binding_removed", "receipt", lambda d: d["bindings"].pop()),
    ("binding_path_and_hash_redirect", "receipt", lambda d: d["bindings"][0].update(path=d["bindings"][1]["path"], sha256=d["bindings"][1]["sha256"])),
    ("intersection_schema", "intersection", set_path(["schema"], "mutated")),
    ("intersection_scope", "intersection", set_path(["generation_scope"], "coordinate_first")),
    ("intersection_probe_count", "intersection", set_path(["ordered_source_probe_count"], 80)),
    ("intersection_probe_eligibility", "intersection", set_path(["contrast_probe_used_for_eligibility"], True)),
    ("intersection_batch_count", "intersection", set_path(["semantic_batch_count"], 485)),
    ("intersection_actual_missing", "intersection", set_path(["missing_exact_actual_semantic_descriptor_count"], 1)),
    ("intersection_event", "intersection", set_path(["event_count"], 1)),
    ("intersection_rejections", "intersection", set_path(["rejection_counts"], {"invalid_or_missing_provenance": 5508})),
    ("intersection_fixture", "intersection", set_path(["predicate_fixture_count"], 32)),
    ("intersection_bracket", "intersection", set_path(["diagnostic_bracket_fixture_emitted"], False)),
    ("intersection_frozen_bracket", "intersection", set_path(["frozen_bracket_emitted"], True)),
    ("raster_schema", "raster", set_path(["schema"], "mutated")),
    ("raster_case_count", "raster", set_path(["case_count"], 17)),
    ("raster_samples", "raster", set_path(["sample_positions", "asymmetric_2x"], [[0.5, 0.5], [0.5, 0.5]])),
    ("raster_ordinal", "raster", lambda d: d["cases"][0].update(case_ordinal=2)),
    ("raster_repair_id", "raster", lambda d: d["cases"][0].update(repair_probe_id="0" * 64)),
    ("raster_coordinate", "raster", lambda d: d["cases"][0].update(output_only_coordinate=[0, 0])),
    ("raster_match", "raster", lambda d: d["cases"][0].update(matching_event_count=1)),
    ("raster_feasible", "raster", lambda d: d["cases"][0].update(feasible=True)),
    ("raster_metric", "raster", lambda d: d["cases"][0].update(actual_covered_samples=0)),
    ("raster_palette_drift", "raster", lambda d: d["cases"][9].update(actual_covered_samples=999)),
    ("raster_growth", "raster", set_path(["semantic_token_growth"], 1)),
    ("raster_bracket", "raster", set_path(["diagnostic_bracket_removal_negative_passed"], False)),
    ("raster_result", "raster", set_path(["overall_result"], "FEASIBLE")),
    ("source_semantic_helper", "source", lambda s: s.replace("exactSemanticProvenance", "mutatedSemanticProvenance", 1)),
    ("source_fixture", "source", lambda s: s.replace("fixture_duplicate_exact_batch_eligible", "mutated_fixture", 1)),
    ("source_generator_order", "source", lambda s: s.replace("sourceIdentities(standard.package)", "sourceIdentities(standard.package) /* mutated */", 1)),
]


def install_mutation_test(name, target, mutate):
    def test(self):
        self.exercise_mutation(target, mutate)
    setattr(Phase4V17V2FeasibilityTests, f"test_mutation_{name}", test)


for mutation_name, mutation_target, mutation in MUTATIONS:
    install_mutation_test(mutation_name, mutation_target, mutation)


if __name__ == "__main__":
    unittest.main()
