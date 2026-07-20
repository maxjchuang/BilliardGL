import unittest
from pathlib import Path


WORKFLOW = Path(__file__).parents[2] / ".github/workflows/physics-reference-full.yml"


class FullReferenceWorkflowTests(unittest.TestCase):
    def test_manual_and_scheduled_workflow_runs_every_package_without_case_filter(self):
        text = WORKFLOW.read_text(encoding="utf-8")

        self.assertIn("workflow_dispatch:", text)
        self.assertIn("schedule:", text)
        self.assertNotIn(" --case ", text)
        for package in (
                "mathavan_2009_high_speed",
                "domenech_2023_ball_collision",
                "mathavan_2010_cushion",
                "cross_2023_cue_impact"):
            self.assertEqual(text.count(f"--package tests/physics_validation/reference_data/{package}"), 1)
        self.assertIn("actions/upload-artifact@v4", text)


if __name__ == "__main__":
    unittest.main()
