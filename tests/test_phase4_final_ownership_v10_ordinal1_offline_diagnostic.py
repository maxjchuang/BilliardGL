from __future__ import annotations

import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
VALIDATOR = ROOT / \
    "scripts/validate_phase4_final_ownership_v10_ordinal1_offline_diagnostic.py"
ARTIFACTS = ROOT / \
    "docs/art/phase4-final-ownership-v10-ordinal1-offline-diagnostic-v1"
DIAGNOSTIC = ROOT / \
    "scripts/diagnose_phase4_final_ownership_v10_ordinal1_offline.py"


class Phase4V10Ordinal1OfflineDiagnosticTests(unittest.TestCase):
    def run_validator(self, artifacts: Path, diagnostic: Path = DIAGNOSTIC):
        return subprocess.run(
            ["python3", str(VALIDATOR), str(artifacts),
             "--diagnostic-script", str(diagnostic)],
            cwd=ROOT, text=True, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, check=False)

    def mutate(self, name: str, callback):
        with tempfile.TemporaryDirectory() as temporary:
            copied = Path(temporary) / "artifacts"
            shutil.copytree(ARTIFACTS, copied)
            path = copied / name
            value = json.loads(path.read_text())
            callback(value)
            path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")
            return self.run_validator(copied)

    def assert_mutation_fails(self, name: str, callback):
        result = self.mutate(name, callback)
        self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_canonical_passes(self):
        result = self.run_validator(ARTIFACTS)
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn('"status": "PASS"', result.stdout)
        self.assertIn('"capture_retry_authorized": false', result.stdout)

    def test_authoritative_visibility_pass_mutation_fails(self):
        self.assert_mutation_fails("failure-evidence-receipt-v1.json",
            lambda value: value["authority_separation"]["corrected_integrated"].update(
                {"predicted_threshold_pass": False}))

    def test_legacy_promotion_mutation_fails(self):
        self.assert_mutation_fails("failure-evidence-receipt-v1.json",
            lambda value: value["authority_separation"][
                "legacy_ppm_diagnostic_only"].update({"authoritative": True}))

    def test_draw_submission_count_mutation_fails(self):
        self.assert_mutation_fails("failure-evidence-receipt-v1.json",
            lambda value: value["static_machine_predicate_evidence"].update(
                {"package_reported_draw_submissions_by_definition": 1}))

    def test_draw_predicate_false_mutation_fails(self):
        self.assert_mutation_fails("failure-evidence-receipt-v1.json",
            lambda value: value["static_machine_predicate_evidence"].update(
                {"draw_predicate_evaluates": True}))

    def test_exclusivity_overclaim_mutation_fails(self):
        self.assert_mutation_fails("failure-evidence-receipt-v1.json",
            lambda value: value["root_cause"].update(
                {"exclusivity_of_this_single_failure_proven": True}))

    def test_complete_predicate_ledger_overclaim_mutation_fails(self):
        self.assert_mutation_fails("failure-evidence-receipt-v1.json",
            lambda value: value["root_cause"].update(
                {"other_runtime_predicates_individually_serialized": True}))

    def test_algorithm_contamination_mutation_fails(self):
        self.assert_mutation_fails("failure-evidence-receipt-v1.json",
            lambda value: value["root_cause"].update(
                {"not_visibility_threshold_or_ownership_algorithm": False}))

    def test_gate_authorization_mutation_fails(self):
        self.assert_mutation_fails(
            "minimal-machine-contract-correction-design-gate-v1.json",
            lambda value: value.update({"capture_authorized": True}))

    def test_gate_threshold_mutation_fails(self):
        self.assert_mutation_fails(
            "minimal-machine-contract-correction-design-gate-v1.json",
            lambda value: value["immutable_invariants"].update(
                {"minimum_visible_coverage": .89}))

    def test_gate_exact_two_removal_fails(self):
        def mutate(value):
            value["allowed_next_implementation_scope"] = [item for item in
                value["allowed_next_implementation_scope"]
                if "exact-two" not in item]
        self.assert_mutation_fails(
            "minimal-machine-contract-correction-design-gate-v1.json", mutate)

    def test_gate_predicate_ledger_removal_fails(self):
        def mutate(value):
            value["allowed_next_implementation_scope"] = [item for item in
                value["allowed_next_implementation_scope"]
                if "machine-predicate ledger" not in item]
        self.assert_mutation_fails(
            "minimal-machine-contract-correction-design-gate-v1.json", mutate)

    def test_gate_visual_change_mutation_fails(self):
        self.assert_mutation_fails(
            "minimal-machine-contract-correction-design-gate-v1.json",
            lambda value: value["immutable_invariants"].update(
                {"visual_colors_geometry_sublayers_backdrops_assets_scene_ui":
                 "MAY_CHANGE"}))

    def test_missing_artifact_fails(self):
        with tempfile.TemporaryDirectory() as temporary:
            copied = Path(temporary) / "artifacts"
            shutil.copytree(ARTIFACTS, copied)
            (copied / "failure-evidence-receipt-v1.json").unlink()
            result = self.run_validator(copied)
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertIn("artifact_missing", result.stdout)

    def test_diagnostic_script_mutation_fails(self):
        with tempfile.TemporaryDirectory() as temporary:
            copied = Path(temporary) / "diagnostic.py"
            copied.write_text(DIAGNOSTIC.read_text() + "\n# drift\n")
            result = self.run_validator(ARTIFACTS, copied)
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertIn("diagnostic_script_hash_drift", result.stdout)


if __name__ == "__main__":
    unittest.main()
