from __future__ import annotations

import copy
import json
from pathlib import Path
import unittest

from scripts.validate_wpa_ii_06_d_head_string_half_ball_classification_design_gate import validate


ROOT = Path(__file__).resolve().parents[2]
GATE = ROOT / "docs/rules/wpa-ii-06-d-head-string-half-ball-classification-design-gate-v1.json"


class HeadStringHalfBallClassificationDesignGateTests(unittest.TestCase):
    def setUp(self) -> None:
        self.value = json.loads(GATE.read_text(encoding="utf-8"))

    def assertRejected(self, mutate) -> None:  # noqa: N802 - unittest style
        bad = copy.deepcopy(self.value)
        mutate(bad)
        self.assertTrue(validate(bad, check_files=False))

    def test_frozen_gate_and_live_bindings_pass(self) -> None:
        self.assertEqual([], validate(self.value))

    def test_rejects_measurement_time_guess(self) -> None:
        def mutate(value):
            decision = value["classification_decisions"]["authoritative_measurement_time"]
            decision["selected"] = "successful_break_placement_commit"
            decision["status"] = "RESOLVED"
        self.assertRejected(mutate)

    def test_rejects_missing_measurement_candidate(self) -> None:
        self.assertRejected(lambda value: value["classification_decisions"]
                            ["authoritative_measurement_time"]
                            ["allowed_candidates"].pop())

    def test_rejects_history_reset_scope_guess(self) -> None:
        def mutate(value):
            decision = value["classification_decisions"]["per_player_history_reset_scope"]
            decision["selected"] = "match"
            decision["status"] = "RESOLVED"
        self.assertRejected(mutate)

    def test_rejects_numeric_tolerance_guess(self) -> None:
        self.assertRejected(lambda value: value["classification_decisions"]
                            ["exact_boundary_numeric_policy"]
                            .__setitem__("selected", "exact_authoritative_double"))

    def test_rejects_exact_half_as_offense(self) -> None:
        self.assertRejected(lambda value: value["geometry_contract"]
                            .__setitem__("offense_geometry_predicate", "signed_crossing_cm >= 0"))

    def test_rejects_nonfinite_geometry_acceptance(self) -> None:
        self.assertRejected(lambda value: value["geometry_contract"]
                            .__setitem__("finite_inputs_required", False))

    def test_rejects_preview_counting(self) -> None:
        self.assertRejected(lambda value: value["product_submission_contract"]
                            ["must_not_count"].remove(
                                "drag preview samples, including repeated samples"))

    def test_rejects_rejected_confirm_counting(self) -> None:
        self.assertRejected(lambda value: value["product_submission_contract"]
                            ["must_not_count"].remove(
                                "confirm attempts rejected before an authoritative ACK"))

    def test_rejects_count_precondition_substitution(self) -> None:
        self.assertRejected(lambda value: value["product_submission_contract"]
                            ["future_count_preconditions"].__setitem__(
                                0, "preview is an authoritative measurement"))

    def test_rejects_generic_section8_promotion(self) -> None:
        self.assertRejected(lambda value: value["existing_core_g_mapping"]
                            .__setitem__("reuse_without_distinct_break_context_forbidden", False))

    def test_rejects_global_instead_of_per_player_history(self) -> None:
        self.assertRejected(lambda value: value["per_player_history_contract"]
                            .__setitem__("key", "global_match_history"))

    def test_rejects_first_offense_foul_or_shared_history(self) -> None:
        self.assertRejected(lambda value: value["per_player_history_contract"]
                            .__setitem__(
                                "first_geometric_offense_for_player",
                                "increment and mark a foul"))
        self.assertRejected(lambda value: value["per_player_history_contract"]
                            .__setitem__("other_player_history_is_independent", False))

    def test_rejects_partial_commit_or_non_idempotent_retry(self) -> None:
        self.assertRejected(lambda value: value["atomic_mutation_contract"]
                            .__setitem__("partial_commit_allowed", True))
        self.assertRejected(lambda value: value["atomic_mutation_contract"]
                            .__setitem__("idempotent_retry", "increment again"))

    def test_rejects_missing_positive_or_negative_fixture(self) -> None:
        self.assertRejected(lambda value: value["required_future_fixtures"]["positive"].pop())
        self.assertRejected(lambda value: value["required_future_fixtures"]["negative"].pop())

    def test_rejects_implementation_or_audit_promotion(self) -> None:
        self.assertRejected(lambda value: value["classification_decisions"]
                            .__setitem__("production_implementation_authorized", True))
        self.assertRejected(lambda value: value["classification_decisions"]
                            .__setitem__("audit_row_may_be_promoted", True))
        self.assertRejected(lambda value: value["claims"]
                            .__setitem__("implementation_complete", True))

    def test_rejects_source_sha_or_path_traversal(self) -> None:
        self.assertRejected(lambda value: value["source_bindings"]["runtime"]
                            .__setitem__("sha256", "0" * 64))
        self.assertRejected(lambda value: value["source_bindings"]["runtime"]
                            .__setitem__("path", "../game_runtime.cpp"))
        self.assertRejected(lambda value: value["official_source"]
                            .__setitem__("path", "../../250816-Rules-of-Heyball.pdf"))

    def test_rejects_gpu_cmake_or_renderer_scope_expansion(self) -> None:
        self.assertRejected(lambda value: value["forbidden_scope"].remove(
            "GPU, Cocoa, or OpenGL execution"))
        self.assertRejected(lambda value: value["forbidden_scope"].remove("CMake changes"))
        self.assertRejected(lambda value: value["future_implementation_allowlist"]
                            ["production_files"].append(
                                "src/Billiards/render/modern/modern_renderer.cpp"))

    def test_rejects_non_object_bad_input(self) -> None:
        self.assertTrue(validate(None, check_files=False))
        self.assertTrue(validate([], check_files=False))


if __name__ == "__main__":
    unittest.main()
