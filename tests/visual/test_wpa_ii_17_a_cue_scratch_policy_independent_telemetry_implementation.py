from __future__ import annotations

import copy
import json
from pathlib import Path
import unittest

from scripts.validate_wpa_ii_17_a_cue_scratch_policy_independent_telemetry_implementation import validate


ROOT = Path(__file__).resolve().parents[2]
RECEIPT = ROOT / "docs/rules/wpa-ii-17-a-cue-scratch-policy-independent-telemetry-implementation-receipt-v1.json"


class WpaIi17aCueScratchTelemetryImplementationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.value = json.loads(RECEIPT.read_text(encoding="utf-8"))

    def assertRejected(self, mutate) -> None:
        value = copy.deepcopy(self.value)
        mutate(value)
        self.assertTrue(validate(value, check_files=False))

    def test_canonical_receipt_and_live_bindings_pass(self) -> None:
        self.assertEqual([], validate(self.value, check_files=True))

    def test_rejects_identity_scope_or_lineage_drift(self) -> None:
        self.assertRejected(lambda v: v.__setitem__("status", "WPA_II_17_COMPLETE"))
        self.assertRejected(lambda v: v.__setitem__("scope", ["WPA-II-17(a)-(e)"]))
        self.assertRejected(lambda v: v["lineage"]["frozen_audit"]["row"].__setitem__("status", "IMPLEMENTED"))

    def test_rejects_source_binding_drift(self) -> None:
        self.assertRejected(lambda v: v["source_bindings"]["collector"].__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda v: v["source_bindings"].pop("rules"))

    def test_rejects_ball_state_inference(self) -> None:
        self.assertRejected(lambda v: v["authoritative_event_contract"].__setitem__("ball_state_pocketed_true_required", True))
        self.assertRejected(lambda v: v["authoritative_event_contract"].__setitem__("same_tick_event_required", False))

    def test_rejects_incomplete_capture_identity(self) -> None:
        self.assertRejected(lambda v: v["authoritative_event_contract"]["required_contact"].__setitem__("capture_sequence_minimum", 0))
        self.assertRejected(lambda v: v["authoritative_event_contract"]["event_fields"].remove("physics_tick"))
        self.assertRejected(lambda v: v["authoritative_event_contract"]["event_fields"].remove("canonical_event_ordinal"))

    def test_rejects_dedupe_or_failed_step_rollback_loss(self) -> None:
        self.assertRejected(lambda v: v["collector_contract"].__setitem__("exact_duplicates_deduplicated", False))
        self.assertRejected(lambda v: v["collector_contract"].__setitem__("failed_step_has_zero_evidence_mutation", False))
        self.assertRejected(lambda v: v["collector_contract"].__setitem__("more_than_one_distinct_capture_fails_completeness", False))

    def test_rejects_partial_event_consumption(self) -> None:
        self.assertRejected(lambda v: v["collector_contract"].__setitem__("partial_evidence_never_sets_cue_ball_pocketed_fact", False))
        self.assertRejected(lambda v: v["adjudicator_contract"].__setitem__("cue_ball_scratch_consumed_only_when_evidence_is_complete", False))

    def test_rejects_synthetic_scratch_authorization(self) -> None:
        self.assertRejected(lambda v: v["adjudicator_contract"].__setitem__("direct_or_synthetic_cue_ball_pocketed_rejected", False))
        self.assertRejected(lambda v: v["adjudicator_contract"].__setitem__("stable_error_code", "none"))

    def test_rejects_runtime_physical_proof_erosion(self) -> None:
        self.assertRejected(lambda v: v["runtime_proof"].__setitem__("uses_update_physics", False))
        self.assertRejected(lambda v: v["runtime_proof"].__setitem__("capture_count", 0))
        self.assertRejected(lambda v: v["runtime_proof"].__setitem__("cue_ball_state_pocketed_after_replacement", True))

    def test_rejects_negative_fixture_loss(self) -> None:
        for item in ("FAILED_STEP_CAPTURE_ROLLS_BACK",
                     "CONFLICTING_DUPLICATE_CAPTURE_IDENTITY_FAILS_CLOSED",
                     "COLLECTOR_BEGIN_RESETS_PRIOR_CAPTURE",
                     "AUTOMATION_UNATTESTED_SCRATCH_REJECTED"):
            self.assertRejected(lambda v, item=item: v["negative_fixtures"].remove(item))

    def test_rejects_false_cpu_or_gpu_result(self) -> None:
        self.assertRejected(lambda v: v["cpu_results"][0].__setitem__("result", "FAIL"))
        self.assertRejected(lambda v: v["cpu_results"][0].__setitem__("gpu", True))

    def test_rejects_object_off_table_scope_expansion(self) -> None:
        self.assertRejected(lambda v: v["non_scope"].__setitem__("object_ball_off_table_changed", True))
        self.assertRejected(lambda v: v["non_scope"].__setitem__("wpa_ii_12_3_vs_17_e_status", "RESOLVED"))
        self.assertRejected(lambda v: v["claims"].__setitem__("object_ball_off_table_policy_resolved", True))

    def test_rejects_global_ui_cmake_or_renderer_expansion(self) -> None:
        self.assertRejected(lambda v: v["non_scope"].__setitem__("global_rules_decision_ui_product_exposed", True))
        self.assertRejected(lambda v: v["non_scope"].__setitem__("cmake_source_changed", True))
        self.assertRejected(lambda v: v["non_scope"].__setitem__("phase4_or_phase5_changed", True))

    def test_rejects_full_section_promotion(self) -> None:
        for key in ("wpa_ii_17_complete", "wpa_ii_17_a_cue_ball_off_table_complete",
                    "wpa_ii_17_e_object_ball_off_table_complete", "audit_row_promoted", "gpu_executed"):
            self.assertRejected(lambda v, key=key: v["claims"].__setitem__(key, True))

    def test_rejects_non_object_bad_input(self) -> None:
        for value in (None, [], "receipt", 1):
            self.assertTrue(validate(value, check_files=False))


if __name__ == "__main__":
    unittest.main()
