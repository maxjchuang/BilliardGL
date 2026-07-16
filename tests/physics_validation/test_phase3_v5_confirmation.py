import hashlib
import json
import math
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CANDIDATE = ROOT / "physics_models/candidates/phase3_integrated_v5"
OUTPUT = CANDIDATE / "confirmation/cross_2016_newtons_cradle"
LEDGER = CANDIDATE / "confirmation_consumption.json"
REJECTION = ROOT / "physics_models/promotion/phase3_integrated_v5_rejection.json"


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def speed(ball):
    velocity = ball["velocity_cm_s"]
    return math.sqrt(sum(velocity[axis] ** 2 for axis in "xyz"))


class Phase3V5ConfirmationTests(unittest.TestCase):
    def setUp(self):
        self.ledger = json.loads(LEDGER.read_text(encoding="utf-8"))
        self.receipt = json.loads((
            OUTPUT / "validation_receipt.json").read_text(encoding="utf-8"))
        self.report = json.loads((
            OUTPUT / "reference_report.json").read_text(encoding="utf-8"))

    def test_cross_has_exactly_one_finalized_failed_attempt(self):
        self.assertEqual(len(self.ledger["attempts"]), 1)
        self.assertEqual(len(self.ledger["records"]), 1)
        started = self.ledger["attempts"][0]
        finalized = self.ledger["records"][0]
        self.assertEqual(started["attempt_id"], finalized["attempt_id"])
        self.assertEqual(finalized, self.receipt)
        self.assertEqual(finalized["dataset_id"],
                         "cross_2016_newtons_cradle")
        self.assertEqual(finalized["result"], "FAILED")

    def test_receipt_hashes_every_complete_numeric_artifact(self):
        for relative, digest in self.receipt["files"].items():
            path = OUTPUT / relative
            self.assertTrue(path.is_file(), relative)
            self.assertEqual(sha256(path), digest, relative)
        first = OUTPUT / "execution/cross_cue_frozen_pair-first.json"
        second = OUTPUT / "execution/cross_cue_frozen_pair-second.json"
        self.assertEqual(first.read_bytes(), second.read_bytes())
        self.assertEqual(
            json.loads(first.read_text())["trace_sha256"],
            sha256(OUTPUT / "traces/cross_cue_frozen_pair.json"))

    def test_contact_execution_is_complete_finite_passive_and_released(self):
        metrics = self.report["summary_metrics"]
        for gate in (
            "complete_frames_passed", "contact_passed",
            "deterministic_repeated_execution_passed",
            "finite_microtrace_passed", "finite_state_passed",
            "microtrace_complete_passed", "no_recontact_passed",
            "nonincreasing_total_energy_passed",
            "passive_microtrace_passed", "release_passed",
        ):
            self.assertTrue(metrics[gate], gate)
        trace = json.loads((
            OUTPUT / "traces/cross_cue_frozen_pair.json").read_text())
        contacts = [frame["cue_contact"] for frame in trace
                    if frame.get("cue_contact", {}).get("applied") is True]
        self.assertEqual(len(contacts), 1)
        self.assertEqual(contacts[0]["regime"], "released")
        self.assertEqual(contacts[0]["error_code"], "")
        self.assertEqual(contacts[0]["microtrace_schema_version"], 1)
        self.assertEqual(len(contacts[0]["microsteps"]), 544)

    def test_failure_is_the_preregistered_absolute_stability_gate(self):
        metrics = self.report["summary_metrics"]
        self.assertFalse(metrics["stable_release_passed"])
        self.assertFalse(metrics["uncertainty_aware_equal_speed_passed"])
        self.assertIsNone(metrics["back_to_front_speed_ratio"])
        trace = json.loads((
            OUTPUT / "traces/cross_cue_frozen_pair.json").read_text())
        samples = []
        for frame in trace:
            balls = {ball["index"]: ball for ball in frame["balls"]}
            samples.append((speed(balls[0]), speed(balls[1])))
        previous, final = samples[-2:]
        relative_change = max(
            abs(final[0] - previous[0]) / previous[0],
            abs(final[1] - previous[1]) / previous[1],
        )
        ratio = final[0] / final[1]
        self.assertGreater(relative_change, 0.001)
        self.assertAlmostEqual(relative_change, 0.013657418039445297)
        self.assertAlmostEqual(ratio, 0.9999981407287482)
        self.assertLessEqual(abs(ratio - 1.0), metrics["equal_speed_limit"])

    def test_han_remains_unopened_and_rejection_is_hash_bound(self):
        entries = self.ledger["attempts"] + self.ledger["records"]
        self.assertFalse(any(row.get("dataset_id") == "han_2005"
                             for row in entries))
        self.assertFalse((CANDIDATE / "confirmation/han_2005").exists())
        rejection = json.loads(REJECTION.read_text(encoding="utf-8"))
        self.assertEqual(rejection["disposition"], "REJECTED")
        self.assertEqual(rejection["han_2005"], "NOT_EXECUTED")
        self.assertEqual(rejection["ledger_sha256"], sha256(LEDGER))
        self.assertEqual(rejection["cross_2016"]["receipt_sha256"],
                         sha256(OUTPUT / "validation_receipt.json"))
        self.assertEqual(
            rejection["root_cause"]["category"],
            "CONFIRMATION_EVALUATION_CONTRACT_INTEGRATION_FAILURE")


if __name__ == "__main__":
    unittest.main()
