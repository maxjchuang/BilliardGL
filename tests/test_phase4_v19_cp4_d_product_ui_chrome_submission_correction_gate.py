#!/usr/bin/env python3
"""Fail-closed tests for the CP4-D Product UI chrome correction gate."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import validate_phase4_v19_cp4_d_product_ui_chrome_submission_correction_gate as validator


ROOT = Path(__file__).resolve().parents[1]
GATE = ROOT / "docs/art/phase4-v19-cp4-d-product-ui-chrome-submission-correction-design-gate-v1.json"


class Cp4DProductUiChromeCorrectionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.value = json.loads(GATE.read_text(encoding="utf-8"))

    def errors_for(self, value: dict) -> list[str]:
        with tempfile.TemporaryDirectory(prefix="cp4-d-chrome-gate-") as directory:
            path = Path(directory) / "gate.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            return validator.validate(path)

    def reject(self, mutation) -> None:
        value = copy.deepcopy(self.value)
        mutation(value)
        self.assertTrue(self.errors_for(value))

    def test_committed_gate_passes(self) -> None:
        self.assertEqual(validator.validate(GATE), [])

    def test_rejects_binding_drift(self) -> None:
        self.reject(lambda v: v["frozen_bindings"][0].update(sha256="0" * 64))

    def test_rejects_nonfinal_candidate_acceptance(self) -> None:
        self.reject(lambda v: v["nonfinal_blocker_binding"].update(candidate_is_frozen_or_accepted=True))

    def test_rejects_duplicate_final_ui_option(self) -> None:
        self.reject(lambda v: v["root_selected_correction_binding"].update(final_staging_duplicate_ui_option_selected=True))

    def test_rejects_allowlist_expansion(self) -> None:
        self.reject(lambda v: v["future_candidate_exact_allowlist_not_authorized"].append("src/Billiards/render/ui/product_shell_layout.cpp"))

    def test_rejects_cp2_edit(self) -> None:
        self.reject(lambda v: v["frozen_files_forbidden_to_edit"].remove("src/Billiards/render/ui/product_shell_draw_list.cpp"))

    def test_rejects_final_staging_redraw(self) -> None:
        self.reject(lambda v: v["supersession_contract"].update(final_staging_may_draw_tone_or_submit_ordinary_ui_again=True))

    def test_rejects_old_publish_order(self) -> None:
        self.reject(lambda v: v["supersession_contract"].update(cp3_plot_publish_before_chrome_wording_in_original_checklist_is_replaced_for_option_a=False))

    def test_rejects_cached_chrome(self) -> None:
        self.reject(lambda v: v["product_ui_submission_contract"].update(cache_policy="cache shell chrome and reuse"))

    def test_rejects_missing_bilingual_row(self) -> None:
        self.reject(lambda v: v["exact_current_chrome_semantic_inventory"]["ordered_text_exact"].pop())

    def test_rejects_wrong_unfocused_count(self) -> None:
        self.reject(lambda v: v["exact_current_chrome_semantic_inventory"].update(unfocused_semantic_primitive_count=15))

    def test_rejects_focus_optional_when_focused(self) -> None:
        self.reject(lambda v: v["exact_current_chrome_semantic_inventory"].update(focus="focus may be omitted"))

    def test_rejects_trajectory_denominator_contribution(self) -> None:
        self.reject(lambda v: v["exact_current_chrome_semantic_inventory"].update(semantic_member_probe_and_denominator_contribution=[0, 0, 1]))

    def test_rejects_request_echo_receipt(self) -> None:
        self.reject(lambda v: v["resource_bound_submission_receipt_contract"].update(actual_not_echo="request echo accepted"))

    def test_rejects_receipt_field_loss(self) -> None:
        self.reject(lambda v: v["resource_bound_submission_receipt_contract"]["required_receipt_fields_in_order"].pop())

    def test_rejects_geometry_only_cutout_proof(self) -> None:
        self.reject(lambda v: v["cutout_and_final_staging_contract"].update(cutout_transparency="semantic rectangles do not overlap"))

    def test_rejects_second_tone_pass(self) -> None:
        self.reject(lambda v: v["cutout_and_final_staging_contract"].update(tone_contract="final staging applies tone again"))

    def test_rejects_world_overlay_fallback(self) -> None:
        self.reject(lambda v: v["cutout_and_final_staging_contract"].update(world_overlay_draw_calls=1))

    def test_rejects_ordinary_ldr_failure_fallback(self) -> None:
        self.reject(lambda v: v["atomic_failure_and_noncurrent_contract"].update(no_fallback="ordinary LDR fallback allowed"))

    def test_rejects_missing_present_barrier(self) -> None:
        self.reject(lambda v: v["atomic_failure_and_noncurrent_contract"].update(draw_failure="invalidate staging after draw failure"))

    def test_rejects_reduced_positive_matrix(self) -> None:
        self.reject(lambda v: v["future_cpu_static_gl_mock_acceptance_not_run"]["positive_matrix"].update(exact_case_count=48))

    def test_rejects_false_cpu_or_gpu_authority(self) -> None:
        def mutation(value: dict) -> None:
            value["authorization_boundary"].update(
                cpu_implementation_authorized_by_this_gate=True,
                gpu_authorized=True,
            )
        self.reject(mutation)

    def test_rejects_false_implementation_pass(self) -> None:
        self.reject(lambda v: v["proof_boundary"].update(implementation_pass=True))


if __name__ == "__main__":
    unittest.main()
