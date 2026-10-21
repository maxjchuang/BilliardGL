#!/usr/bin/env python3
"""Fail-closed mutations for CP4-D acceptance and successor checklist."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import validate_phase4_v19_cp4_d_acceptance_and_release_readiness_checklist as validator


class AcceptanceAndSuccessorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.addendum = json.loads(validator.ADDENDUM.read_text())
        cls.checklist = json.loads(validator.CHECKLIST.read_text())

    def errors(self, addendum: dict, checklist: dict) -> list[str]:
        with tempfile.TemporaryDirectory(prefix="phase4-v19-release-gate-") as directory:
            root = Path(directory)
            first, second = root / "addendum.json", root / "checklist.json"
            first.write_text(json.dumps(addendum))
            second.write_text(json.dumps(checklist))
            return validator.validate(first, second)

    def reject_addendum(self, mutation) -> None:
        addendum, checklist = copy.deepcopy(self.addendum), copy.deepcopy(self.checklist)
        mutation(addendum)
        self.assertTrue(self.errors(addendum, checklist))

    def reject_checklist(self, mutation) -> None:
        addendum, checklist = copy.deepcopy(self.addendum), copy.deepcopy(self.checklist)
        mutation(checklist)
        self.assertTrue(self.errors(addendum, checklist))

    def test_canonical(self) -> None:
        self.assertEqual(validator.validate(), [])

    def test_mutations(self) -> None:
        addendum_mutations = [
            lambda d: d.update(schema="bad"),
            lambda d: d.update(status="PRODUCT_PASS"),
            lambda d: d["frozen_bindings"][0].update(sha256="0" * 64),
            lambda d: d["independent_reproduction"].update(post_freeze_receipt_validator="FAIL"),
            lambda d: d["accepted_cp4_d_axes"].update(core41_internal_restore_zero_and_outer_restore_exact_one=False),
            lambda d: d["acceptance_boundary"].update(cp4_cpu_static_accepted=False),
            lambda d: d["acceptance_boundary"].update(gpu_authorized=True),
            lambda d: d["acceptance_boundary"].update(product_m3_golden_beta_pass_claimed=True),
        ]
        checklist_mutations = [
            lambda d: d.update(schema="bad"),
            lambda d: d.update(status="GPU_READY"),
            lambda d: d["frozen_bindings"][0].update(sha256="0" * 64),
            lambda d: d["current_source_bindings"].update({"src/Billiards/render/modern/modern_renderer.cpp": "0" * 64}),
            lambda d: d["rules_audit_boundary"]["audit_v10_summary"].update(implemented=40, missing=0),
            lambda d: d["rules_audit_boundary"].update(profile_complete=True),
            lambda d: d["future_fresh_release_cpu_build_closure_not_run"].update(build_type="Debug"),
            lambda d: d["future_fresh_release_cpu_build_closure_not_run"].update(build_executed=True),
            lambda d: d["future_fresh_release_cpu_build_closure_not_run"]["minimum_library_targets_after_target_closure"].pop(),
            lambda d: d["future_fresh_release_cpu_build_closure_not_run"]["current_cmake_gap"].update(static_archive_success_is_not_link_closure=False),
            lambda d: d["future_fresh_release_cpu_build_closure_not_run"]["current_cmake_gap"]["zero_reference_sources_exact"].pop(),
            lambda d: d["future_fresh_release_cpu_build_closure_not_run"]["current_cmake_gap"]["zero_reference_sources_exact"].__setitem__(0, "src/not-v19.cpp"),
            lambda d: d["future_fresh_release_cpu_build_closure_not_run"]["required_separate_cmake_target_closure_gate_before_configure"].update(gate_frozen=True),
            lambda d: d["future_fresh_release_cpu_build_closure_not_run"]["required_separate_cmake_target_closure_gate_before_configure"]["requirements"].__setitem__(3, "archive-only is sufficient"),
            lambda d: d["future_fresh_release_cpu_build_closure_not_run"]["minimum_cpu_or_compile_targets_after_target_closure"].__setitem__(6, "BilliardsModernGpuResourcesSmoke"),
            lambda d: d["capture_producer_and_readiness_preconditions"].update(readiness_created=True),
            lambda d: d["capture_producer_and_readiness_preconditions"].update(existing_old_phase4_world_overlay_runner_is_not_accepted_as_v19_inset_producer_by_name_only=False),
            lambda d: d["capture_producer_and_readiness_preconditions"].update(historical_evidence_not_inheritable="old evidence accepted"),
            lambda d: d["future_one_shot_gpu_minimum_plan_not_authorized"]["cases_exact"].pop(),
            lambda d: d["future_one_shot_gpu_minimum_plan_not_authorized"]["cases_exact"].__setitem__(3, "old_world_overlay_broadcast"),
            lambda d: d["future_one_shot_gpu_minimum_plan_not_authorized"].update(gpu_authorized=True),
            lambda d: d["future_one_shot_gpu_minimum_plan_not_authorized"].update(machine_pass_claimed=True),
            lambda d: d["future_ta_and_product_boundary"].update(ta_visual_pass_claimed=True),
            lambda d: d["authorization_boundary"].update(release_build_authorized_by_this_checklist=True),
            lambda d: d["authorization_boundary"].update(product_pass_claimed=True),
        ]
        for mutation in addendum_mutations:
            with self.subTest(kind="addendum"):
                self.reject_addendum(mutation)
        for mutation in checklist_mutations:
            with self.subTest(kind="checklist"):
                self.reject_checklist(mutation)


if __name__ == "__main__":
    unittest.main()
