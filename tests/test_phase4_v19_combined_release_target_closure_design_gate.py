#!/usr/bin/env python3
"""Fail-closed mutations for the V19 combined Release target-closure gate."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import validate_phase4_v19_combined_release_target_closure_design_gate as validator

ROOT = Path(__file__).resolve().parents[1]
GATE = ROOT / "docs/art/phase4-v19-combined-release-target-closure-design-gate-v1.json"


class CombinedReleaseTargetClosureDesignGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(GATE.read_text(encoding="utf-8"))

    def errors_for(self, value: dict) -> list[str]:
        with tempfile.TemporaryDirectory(prefix="v19-target-closure-gate-") as directory:
            path = Path(directory) / "gate.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            return validator.validate(path)

    def reject(self, mutation) -> None:
        value = copy.deepcopy(self.value)
        mutation(value)
        self.assertTrue(self.errors_for(value))

    def test_committed_gate_passes(self) -> None:
        self.assertEqual(validator.validate(GATE), [])

    def test_rejects_checklist_drift(self) -> None:
        self.reject(lambda v: v["frozen_bindings"][0].update(sha256="0" * 64))

    def test_rejects_cp4_acceptance_drift(self) -> None:
        self.reject(lambda v: v["frozen_bindings"][1].update(sha256="0" * 64))

    def test_rejects_cmake_baseline_drift(self) -> None:
        self.reject(lambda v: v["frozen_bindings"][3].update(sha256="0" * 64))

    def test_rejects_d6_a_design_lineage_drift(self) -> None:
        self.reject(lambda v: v["frozen_bindings"][4].update(sha256="0" * 64))

    def test_rejects_source_binding_drift(self) -> None:
        self.reject(lambda v: v["current_source_bindings"].update({validator.V19_SOURCES[2]: "0" * 64}))

    def test_rejects_missing_gap_source(self) -> None:
        self.reject(lambda v: v["observed_current_cmake_gap"]["zero_reference_sources_exact"].pop())

    def test_rejects_archive_as_closure(self) -> None:
        self.reject(lambda v: v["observed_current_cmake_gap"].update(archive_only_or_unrelated_smoke_success_is_final_link_closure=True))

    def test_rejects_smoke_as_closure(self) -> None:
        self.reject(lambda v: v["observed_current_cmake_gap"].update(existing_smoke_may_not_pull_modern_renderer_object=False))

    def test_rejects_false_current_closure(self) -> None:
        self.reject(lambda v: v["observed_current_cmake_gap"].update(fresh_release_closure_currently_proven=True))

    def test_rejects_owner_rename(self) -> None:
        self.reject(lambda v: v["root_selected_future_target_graph"]["implementation_owner"].update(target="BilliardsTrajectoryInset"))

    def test_rejects_owner_type_change(self) -> None:
        self.reject(lambda v: v["root_selected_future_target_graph"]["implementation_owner"].update(type="OBJECT"))

    def test_rejects_owner_source_loss(self) -> None:
        self.reject(lambda v: v["root_selected_future_target_graph"]["implementation_owner"]["sources_exact"].pop())

    def test_rejects_duplicate_source_ownership(self) -> None:
        self.reject(lambda v: v["root_selected_future_target_graph"]["implementation_owner"].update(source_occurrences_across_all_cmake_target_source_lists_each=2))

    def test_rejects_duplicate_compilation_permission(self) -> None:
        self.reject(lambda v: v["root_selected_future_target_graph"]["implementation_owner"].update(duplicate_compilation_into_executables_or_other_libraries_forbidden=False))

    def test_rejects_missing_modern_consumer_edge(self) -> None:
        self.reject(lambda v: v["root_selected_future_target_graph"]["implementation_owner"].update(consumer_edge="none"))

    def test_rejects_cpu_target_rename(self) -> None:
        self.reject(lambda v: v["root_selected_future_target_graph"]["formal_v19_cpu_test"].update(target="OldSmoke"))

    def test_rejects_cpu_source_swap(self) -> None:
        self.reject(lambda v: v["root_selected_future_target_graph"]["formal_v19_cpu_test"].update(source="tests/other.cpp"))

    def test_rejects_cpu_without_actual_core41(self) -> None:
        self.reject(lambda v: v["root_selected_future_target_graph"]["formal_v19_cpu_test"].update(actual_core41_link_shim_required=False))

    def test_rejects_final_target_direct_owner_link(self) -> None:
        self.reject(lambda v: v["root_selected_future_target_graph"]["modern_renderer_cp4_final_link_test"].update(must_not_link_implementation_owner_directly=False))

    def test_rejects_final_target_without_modern_renderer(self) -> None:
        self.reject(lambda v: v["root_selected_future_target_graph"]["modern_renderer_cp4_final_link_test"].update(must_instantiate_modern_renderer=False))

    def test_rejects_final_target_dependency_drift(self) -> None:
        self.reject(lambda v: v["root_selected_future_target_graph"]["modern_renderer_cp4_final_link_test"]["direct_link_dependencies_exact"].pop())

    def test_rejects_final_symbol_anchor_loss(self) -> None:
        self.reject(lambda v: v["root_selected_future_target_graph"]["modern_renderer_cp4_final_link_test"]["required_symbol_anchors_exact"].pop())

    def test_rejects_whole_archive_substitution(self) -> None:
        self.reject(lambda v: v["root_selected_future_target_graph"]["modern_renderer_cp4_final_link_test"].update(whole_archive_or_force_load_substitution_for_real_references_forbidden=False))

    def test_rejects_gpu_execution(self) -> None:
        self.reject(lambda v: v["root_selected_future_target_graph"]["modern_renderer_cp4_final_link_test"].update(creates_gl_context_or_executes_gpu=True))

    def test_rejects_stale_comment_retention(self) -> None:
        self.reject(lambda v: v["root_selected_future_target_graph"]["stale_comment_correction"].update(old_comment_must_be_absent=False))

    def test_rejects_allowlist_growth(self) -> None:
        self.reject(lambda v: v["future_implementation_allowlist_after_d6_a_rebind_only"].append("src/Billiards/render/modern/modern_renderer.cpp"))

    def test_rejects_debug_build(self) -> None:
        self.reject(lambda v: v["future_release_closure_acceptance_not_run"].update(cmake_build_type="Debug"))

    def test_rejects_missing_required_target(self) -> None:
        self.reject(lambda v: v["future_release_closure_acceptance_not_run"]["required_targets_exact"].pop())

    def test_rejects_false_final_link_pass(self) -> None:
        self.reject(lambda v: v["future_release_closure_acceptance_not_run"].update(final_link_pass=True))

    def test_rejects_unknown_cmake_auto_rebind(self) -> None:
        self.reject(lambda v: v["d6_a_successor_lineage_barrier"].update(unknown_future_cmake_sha_is_accepted_by_this_gate=True))

    def test_rejects_non_d6_a_successor(self) -> None:
        self.reject(lambda v: v["d6_a_successor_lineage_barrier"].update(d6_a_is_the_only_preimplementation_cmake_successor_lineage_allowed=False))

    def test_rejects_automatic_rebind(self) -> None:
        self.reject(lambda v: v["d6_a_successor_lineage_barrier"].update(automatic_rebind_or_hash_wildcard_forbidden=False))

    def test_rejects_false_d6_a_stability(self) -> None:
        self.reject(lambda v: v["d6_a_successor_lineage_barrier"].update(d6_a_cmake_source_stable_received=True))

    def test_rejects_implementation_before_rebind(self) -> None:
        self.reject(lambda v: v["d6_a_successor_lineage_barrier"].update(implementation_may_start=True))

    def test_rejects_cmake_authority(self) -> None:
        self.reject(lambda v: v["authorization_boundary"].update(cmake_edit_authorized=True))

    def test_rejects_build_authority(self) -> None:
        self.reject(lambda v: v["authorization_boundary"].update(configure_or_build_authorized=True))

    def test_rejects_gpu_authority(self) -> None:
        self.reject(lambda v: v["authorization_boundary"].update(gpu_readiness_capture_or_authorization=True))


if __name__ == "__main__":
    unittest.main()
