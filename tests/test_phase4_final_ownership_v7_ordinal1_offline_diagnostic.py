from __future__ import annotations

import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
VALIDATOR = ROOT / \
    "scripts/validate_phase4_final_ownership_v7_ordinal1_offline_diagnostic.py"
ARTIFACTS = ROOT / \
    "docs/art/phase4-final-ownership-v7-ordinal1-offline-diagnostic-v1"
DIAGNOSTIC = ROOT / \
    "scripts/diagnose_phase4_final_ownership_v7_ordinal1_offline.py"


class Phase4V7OfflineDiagnosticTests(unittest.TestCase):
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

    def test_corrected_count_mutation_fails(self):
        result = self.mutate_json(
            "failure-evidence-receipt-v1.json",
            lambda value: value["authoritative_corrected_capture"].update(
                {"visible": 87}))
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("artifact_hash_drift", result.stdout)

    def test_legacy_authority_mutation_fails(self):
        result = self.mutate_json(
            "failure-evidence-receipt-v1.json",
            lambda value: value["legacy_ppm_diagnostic_only"].update(
                {"authoritative": True}))
        self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_lost_boundary_proof_mutation_fails(self):
        def mutate(value):
            value["lost_boundary_cross_partition_proof"][0][
                "nearest_qualifying_boundary_owned_by_other_probe"][
                    "distance_from_source_probe_px"] = 99
        result = self.mutate_json("failure-evidence-receipt-v1.json", mutate)
        self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_visual_repair_authorization_mutation_fails(self):
        result = self.mutate_json(
            "minimal-owned-island-correction-design-gate-v1.json",
            lambda value: value.update({"capture_authorized": True}))
        self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_report_authority_mutation_fails(self):
        result = self.mutate_json(
            "root-cause-diagnostic-report-v1.json",
            lambda value: value.update({"legacy_ppm_is_diagnostic_only": False}))
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
