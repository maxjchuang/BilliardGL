from __future__ import annotations

import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
VALIDATOR = ROOT / \
    "scripts/validate_phase4_final_ownership_v8_ordinal2_offline_diagnostic.py"
ARTIFACTS = ROOT / \
    "docs/art/phase4-final-ownership-v8-ordinal2-offline-diagnostic-v1"
DIAGNOSTIC = ROOT / \
    "scripts/diagnose_phase4_final_ownership_v8_ordinal2_offline.py"


class Phase4V8Ordinal2OfflineDiagnosticTests(unittest.TestCase):
    def run_validator(self, artifacts: Path, script: Path = DIAGNOSTIC):
        return subprocess.run(
            ["python3", str(VALIDATOR), str(artifacts),
             "--diagnostic-script", str(script)],
            cwd=ROOT, text=True, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, check=False)

    def mutate_json(self, name: str, callback) -> subprocess.CompletedProcess:
        with tempfile.TemporaryDirectory() as temporary:
            copied = Path(temporary) / "artifacts"
            shutil.copytree(ARTIFACTS, copied)
            path = copied / name
            value = json.loads(path.read_text())
            callback(value)
            path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")
            return self.run_validator(copied)

    def test_canonical_passes(self):
        result = self.run_validator(ARTIFACTS)
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn('"status": "PASS"', result.stdout)
        self.assertIn('"gpu_cocoa_opengl_invoked": false', result.stdout)

    def test_corrected_authority_mutation_fails(self):
        result = self.mutate_json(
            "failure-evidence-receipt-v1.json",
            lambda value: value["ordinal2_authority_separation"][
                "corrected_integrated"].update({"visible": 40}))
        self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_legacy_promotion_mutation_fails(self):
        result = self.mutate_json(
            "failure-evidence-receipt-v1.json",
            lambda value: value["ordinal2_authority_separation"][
                "legacy_ppm_diagnostic_only"].update({"authoritative": True}))
        self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_algorithm_contamination_mutation_fails(self):
        result = self.mutate_json(
            "failure-evidence-receipt-v1.json",
            lambda value: value["ownership_and_arbitration"].update(
                {"association_or_owner_failure_count": 1}))
        self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_selected_association_mutation_fails(self):
        result = self.mutate_json(
            "failure-evidence-receipt-v1.json",
            lambda value: value["selected_boundary_association"].update(
                {"selected_count": 82}))
        self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_contrast_inventory_mutation_fails(self):
        result = self.mutate_json(
            "failure-evidence-receipt-v1.json",
            lambda value: value["selected_boundary_final_ldr_contrast"].update(
                {"serialized_contrast_failure_count": 8}))
        self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_coaxial_telemetry_mutation_fails(self):
        result = self.mutate_json(
            "failure-evidence-receipt-v1.json",
            lambda value: value["screen_space_coaxial_stress"][
                "actual_to_predicted_nearest"].update(
                    {"within_12px_count": 0}))
        self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_counterfactual_cannot_claim_machine_pass(self):
        result = self.mutate_json(
            "failure-evidence-receipt-v1.json",
            lambda value: value[
                "analytic_visual_only_counterfactual_non_final"].update(
                    {"authoritative": True}))
        self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_gate_authorization_mutation_fails(self):
        result = self.mutate_json(
            "minimal-visual-style-correction-design-gate-v1.json",
            lambda value: value.update({"capture_authorized": True}))
        self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_gate_threshold_mutation_fails(self):
        result = self.mutate_json(
            "minimal-visual-style-correction-design-gate-v1.json",
            lambda value: value["immutable_invariants"].update(
                {"minimum_visible_coverage": 0.86}))
        self.assertNotEqual(result.returncode, 0, result.stdout)

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
