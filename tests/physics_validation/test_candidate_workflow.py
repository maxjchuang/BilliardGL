import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
DOCUMENTATION = REPO_ROOT / "docs/reference-data-packages.md"
WORKFLOW = REPO_ROOT / ".github/workflows/physics-reference-full.yml"


class CandidateWorkflowContractTests(unittest.TestCase):
    def test_documentation_covers_the_complete_candidate_lifecycle(self):
        text = DOCUMENTATION.read_text(encoding="utf-8")
        for required in (
            "python3 -m tools.physics_validation.calibration_run",
            "python3 -m tools.physics_validation.freeze_candidate",
            "python3 -m tools.physics_validation.validation_run",
            "validation_receipt.json",
            "validation_data_status.json",
            "spent",
            "process isolation rather than secrecy",
        ):
            with self.subTest(required=required):
                self.assertIn(required, text)
        self.assertIn("calibration/reference_report.json", text)
        self.assertIn("--verify", text)

    def test_manual_workflow_verifies_and_uploads_frozen_artifacts(self):
        text = WORKFLOW.read_text(encoding="utf-8")
        for required in (
            "candidate_freeze_path",
            "candidate_profile_path",
            "candidate_package_path",
            "tools.physics_validation.freeze_candidate",
            "tools.physics_validation.validation_run",
            "validation_receipt.json",
            "calibration/reference_report.json",
            "actions/upload-artifact@v4",
        ):
            with self.subTest(required=required):
                self.assertIn(required, text)
        self.assertNotIn("--split", text)
        self.assertIn('cron: "17 3 * * 1"', text)


if __name__ == "__main__":
    unittest.main()
