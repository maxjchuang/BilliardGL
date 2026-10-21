import copy
import json
import tempfile
import unittest
from pathlib import Path

from scripts.validate_phase4_v16_local_cross_track_actual_continuity_premise_failure import DEFAULT, validate


def set_path(document, path, value):
    target = document
    for key in path[:-1]:
        target = target[key]
    target[path[-1]] = value


MUTATIONS = {
    "schema": (("schema",), "wrong"),
    "status": (("status",), "IMPLEMENTED"),
    "scope": (("scope",), "GPU"),
    "gate_path": (("gate_binding", "path"), "missing"),
    "gate_sha": (("gate_binding", "sha256"), "0" * 64),
    "v15_receipt": (("lineage", "v15_diagnostic_receipt", "sha256"), "0" * 64),
    "v15_ledger": (("lineage", "v15_provenance_ledger", "sha256"), "0" * 64),
    "v14_receipt": (("lineage", "v14_reproducible_receipt", "sha256"), "0" * 64),
    "candidate_trajectory_cpp": (("reproducible_evidence", "candidate_trajectory_cpp", "sha256"), "0" * 64),
    "candidate_trajectory_h": (("reproducible_evidence", "candidate_trajectory_h", "sha256"), "0" * 64),
    "candidate_modern_cpp": (("reproducible_evidence", "candidate_modern_cpp", "sha256"), "0" * 64),
    "candidate_modern_h": (("reproducible_evidence", "candidate_modern_h", "sha256"), "0" * 64),
    "candidate_cmake": (("reproducible_evidence", "candidate_cmake", "sha256"), "0" * 64),
    "diagnostic_source": (("reproducible_evidence", "diagnostic_source", "sha256"), "0" * 64),
    "stderr": (("reproducible_evidence", "frozen_stderr", "sha256"), "0" * 64),
    "metrics": (("reproducible_evidence", "metrics", "sha256"), "0" * 64),
    "link_manifest_sha": (("reproducible_evidence", "link_inputs", "sha256"), "0" * 64),
    "link_manifest_count": (("reproducible_evidence", "link_inputs", "count"), 46),
    "test_object": (("reproducible_evidence", "diagnostic_test_object", "sha256"), "0" * 64),
    "renderer_object": (("reproducible_evidence", "candidate_renderer_object", "sha256"), "0" * 64),
    "binary": (("reproducible_evidence", "diagnostic_binary", "sha256"), "0" * 64),
    "compile_arch": (("compile_contract", "architecture"), "x86_64"),
    "compile_include_order": (("compile_contract", "candidate_include_precedes_workspace_include"), False),
    "compile_exit": (("compile_contract", "expected_exit_code"), 0),
    "compile_stdout": (("compile_contract", "expected_stdout_sha256"), "0" * 64),
    "observed_1x": (("observed", "single_sample_unsatisfiable_islands_standard_cvd"), [0, 0]),
    "observed_4x": (("observed", "asymmetric_four_sample_unsatisfiable_islands_standard_cvd"), [27, 27]),
    "observed_selected": (("observed", "fail_closed_selected_count"), 1),
    "observed_topology": (("observed", "standard_cvd_topology_counts_identical"), False),
    "observed_coordinate": (("observed", "sample_coordinate_conversion"), "bottom-left"),
    "observed_gate": (("observed", "gate_expected_0_1_2_is_not_achievable_for_generic_full_frame"), False),
    "observed_filter": (("observed", "coordinate_or_repair_id_filter_would_violate_gate"), False),
    "observed_upload": (("observed", "latent_gl_upload_error_fail_open_remains"), False),
    "baseline_trajectory_cpp": (("restored_baselines", "trajectory_cpp", "sha256"), "0" * 64),
    "baseline_trajectory_h": (("restored_baselines", "trajectory_h", "sha256"), "0" * 64),
    "baseline_modern_cpp": (("restored_baselines", "modern_cpp", "sha256"), "0" * 64),
    "baseline_modern_h": (("restored_baselines", "modern_h", "sha256"), "0" * 64),
    "baseline_cmake": (("restored_baselines", "cmake", "sha256"), "0" * 64),
    "claim_implementation": (("claims", "v16_implementation_complete"), True),
    "claim_source_stable": (("claims", "source_stable_for_build"), True),
    "claim_production": (("claims", "production_change_retained"), True),
    "claim_build": (("claims", "final_build_authorized"), True),
    "claim_readiness": (("claims", "readiness_authorized"), True),
    "claim_gpu": (("claims", "gpu_authorized"), True),
    "claim_capture": (("claims", "capture_authorized"), True),
    "claim_pass": (("claims", "machine_or_ta_pass_claimed"), True),
    "claim_successor": (("claims", "successor_design_gate_required"), False),
}


class Phase4V16PremiseFailureTests(unittest.TestCase):
    def test_00_canonical_full_recompile_relink_replay(self):
        self.assertEqual(validate(DEFAULT, recompile=True), [])


def make_mutation_test(path, replacement):
    def test(self):
        canonical = json.loads(DEFAULT.read_text())
        set_path(canonical, path, replacement)
        with tempfile.TemporaryDirectory() as directory:
            receipt = Path(directory) / "receipt.json"
            receipt.write_text(json.dumps(canonical))
            self.assertTrue(validate(receipt, recompile=False))
    return test


for mutation_name, (mutation_path, replacement) in MUTATIONS.items():
    setattr(Phase4V16PremiseFailureTests, "test_mutation_" + mutation_name,
        make_mutation_test(mutation_path, replacement))


if __name__ == "__main__":
    unittest.main()
