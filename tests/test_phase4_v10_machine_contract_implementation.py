from __future__ import annotations

import copy
import importlib.util
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/validate_phase4_v10_machine_contract_implementation.py"
SPEC = importlib.util.spec_from_file_location("phase4_v10_machine_gate", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class Phase4V10MachineContractImplementationTests(unittest.TestCase):
    def setUp(self):
        self.texts = MODULE.load_texts()

    def mutation_fails(self, key: str, old: str, new: str = "REMOVED"):
        texts = copy.deepcopy(self.texts)
        self.assertIn(old, texts[key])
        texts[key] = texts[key].replace(old, new, 1)
        self.assertTrue(MODULE.validate_texts(texts))

    def test_canonical_passes(self):
        self.assertEqual(MODULE.validate_texts(self.texts), [])

    def test_zero_one_three_reverse_missing_and_late_freeze_rejected(self):
        valid = MODULE.valid_submission_sequence
        self.assertTrue(valid(
            ["actual_draw", "actual_backdrop_freeze", "predicted_draw"], 2))
        self.assertFalse(valid([], 0))
        self.assertFalse(valid(["actual_draw"], 1))
        self.assertFalse(valid(
            ["actual_draw", "actual_backdrop_freeze", "predicted_draw"], 3))
        self.assertFalse(valid(
            ["predicted_draw", "actual_backdrop_freeze", "actual_draw"], 2))
        self.assertFalse(valid(["actual_draw", "predicted_draw"], 2))
        self.assertFalse(valid(
            ["actual_draw", "predicted_draw", "actual_backdrop_freeze"], 2))

    def test_exact_two_cannot_regress_to_one(self):
        self.mutation_fails("runner_generator",
            "metrics.trajectoryOverlayDrawCalls == 2",
            "metrics.trajectoryOverlayDrawCalls == 1")

    def test_exact_two_cannot_weaken_to_upper_bound(self):
        self.mutation_fails("runner_generator",
            "metrics.trajectoryOverlayDrawCalls == 2",
            "metrics.trajectoryOverlayDrawCalls <= 2")

    def test_actual_submission_observation_required(self):
        self.mutation_fails("renderer",
            "frameMetrics.trajectoryOverlayActualDrawSubmitted = true;")

    def test_midpoint_freeze_observation_required(self):
        self.mutation_fails("renderer",
            "frameMetrics.trajectoryOverlayActualBackdropFrozen = true;")

    def test_predicted_submission_observation_required(self):
        self.mutation_fails("renderer",
            "frameMetrics.trajectoryOverlayPredictedDrawSubmitted = true;")

    def test_sequence_cannot_accept_missing_midpoint(self):
        self.mutation_fails("renderer",
            "if (submissionSequence == 1) submissionSequence = 2;",
            "submissionSequence = 2;")

    def test_sequence_cannot_accept_reverse_order(self):
        self.mutation_fails("renderer",
            "if (submissionSequence == 2) submissionSequence = 3;",
            "if (submissionSequence == 0) submissionSequence = 3;")

    def test_full_named_predicate_inventory_required(self):
        self.mutation_fails("runner_generator",
            'addMachinePredicate("ui_geometry_gate"',
            'addRemovedPredicate("ui_geometry_gate"')

    def test_machine_pass_must_be_ledger_conjunction(self):
        self.mutation_fails("runner_generator",
            "machinePass = machinePass && predicate.result;",
            "machinePass = true;")

    def test_ledger_must_be_preserved_before_require(self):
        self.mutation_fails("runner_generator",
            "training::preservePhase4TelemetryThenEnforceMachineGate(",
            "training::removedPreserve(")

    def test_missing_ledger_must_fail_closed(self):
        self.mutation_fails("instrumentation",
            "phase4_telemetry_machine_predicate_ledger_missing")

    def test_duplicate_predicate_must_fail_closed(self):
        self.mutation_fails("instrumentation",
            "phase4_telemetry_machine_predicate_ledger_invalid")

    def test_decision_mismatch_must_fail_closed(self):
        self.mutation_fails("instrumentation",
            "phase4_telemetry_machine_predicate_decision_mismatch")

    def test_visual_geometry_source_is_immutable(self):
        self.mutation_fails("trajectory",
            "style.predictedCoreCapExtensionPx + 2.0",
            "style.predictedCoreCapExtensionPx + 3.0")

    def test_ownership_algorithm_source_is_immutable(self):
        self.mutation_fails("ownership",
            "request.minimumEdgeContrastRatio",
            "request.minimumEdgeContrastRatio - 0.1")


if __name__ == "__main__":
    unittest.main()
