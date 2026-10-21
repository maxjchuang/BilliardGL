from __future__ import annotations

import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
VALIDATOR = ROOT / \
    "scripts/validate_phase4_final_ownership_v9_ordinal1_offline_diagnostic.py"
ARTIFACTS = ROOT / \
    "docs/art/phase4-final-ownership-v9-ordinal1-offline-diagnostic-v1"
DIAGNOSTIC = ROOT / \
    "scripts/diagnose_phase4_final_ownership_v9_ordinal1_offline.py"


class Phase4V9Ordinal1OfflineDiagnosticTests(unittest.TestCase):
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
            lambda value: value["authority_separation"][
                "corrected_integrated"].update({"visible": 47}))
        self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_legacy_promotion_mutation_fails(self):
        result = self.mutate_json(
            "failure-evidence-receipt-v1.json",
            lambda value: value["authority_separation"][
                "legacy_ppm_diagnostic_only"].update({"authoritative": True}))
        self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_algorithm_contamination_mutation_fails(self):
        result = self.mutate_json(
            "failure-evidence-receipt-v1.json",
            lambda value: value["contrast_failure_partition"].update(
                {"area_or_association_failure_count": 1}))
        self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_inner_partition_mutation_fails(self):
        result = self.mutate_json(
            "failure-evidence-receipt-v1.json",
            lambda value: value["contrast_failure_partition"].update(
                {"same_track_inner_edge": 13}))
        self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_same_primitive_cap_mutation_fails(self):
        result = self.mutate_json(
            "failure-evidence-receipt-v1.json",
            lambda value: value["contrast_failure_partition"].update(
                {"same_primitive_stem_inner_failures": 0}))
        self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_fixed_rim_partition_mutation_fails(self):
        result = self.mutate_json(
            "failure-evidence-receipt-v1.json",
            lambda value: value["contrast_failure_partition"].update(
                {"predicted_white_rim_vs_pale_background": 0}))
        self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_probe_digest_comparison_mutation_fails(self):
        result = self.mutate_json(
            "failure-evidence-receipt-v1.json",
            lambda value: value["v8_to_v9_comparison"].update(
                {"source_probe_ids_and_ordered_digest_equal": False}))
        self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_root_cause_algorithm_mutation_fails(self):
        result = self.mutate_json(
            "failure-evidence-receipt-v1.json",
            lambda value: value["root_cause"].update(
                {"not_ownership_or_association_algorithm": False}))
        self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_product_clarity_observation_mutation_fails(self):
        result = self.mutate_json(
            "failure-evidence-receipt-v1.json",
            lambda value: value[
                "root_original_resolution_observation_crosscheck"].update(
                    {"product_clarity_conclusion":
                     "Widen all three bands globally."}))
        self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_gate_authorization_mutation_fails(self):
        result = self.mutate_json(
            "minimal-compositing-style-correction-design-gate-v1.json",
            lambda value: value.update({"capture_authorized": True}))
        self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_gate_threshold_mutation_fails(self):
        result = self.mutate_json(
            "minimal-compositing-style-correction-design-gate-v1.json",
            lambda value: value["immutable_invariants"].update(
                {"minimum_visible_coverage": 0.89}))
        self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_gate_adaptive_polarity_removal_fails(self):
        def mutate(value):
            value["allowed_next_implementation_scope"] = [
                item for item in value["allowed_next_implementation_scope"]
                if "deterministic local pre-overlay backdrop" not in item]
        result = self.mutate_json(
            "minimal-compositing-style-correction-design-gate-v1.json", mutate)
        self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_gate_widening_mutation_fails(self):
        result = self.mutate_json(
            "minimal-compositing-style-correction-design-gate-v1.json",
            lambda value: value["forbidden"].pop())
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
