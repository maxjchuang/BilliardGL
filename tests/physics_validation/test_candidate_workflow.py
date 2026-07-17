import json
import hashlib
import subprocess
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
DOCUMENTATION = REPO_ROOT / "docs/reference-data-packages.md"
WORKFLOW = REPO_ROOT / ".github/workflows/physics-reference-full.yml"
EXECUTABLE = REPO_ROOT / "build/Billiards"
PROFILE = REPO_ROOT / "physics_models/profiles/chinese_pool_full_game_v2.json"
INVENTORY = REPO_ROOT / "physics_models/promotion/phase3_candidates_v2.json"
SURFACE_FIT = REPO_ROOT / "physics_models/calibration/surface_fit_v2.json"
BALL_FIT = REPO_ROOT / "physics_models/calibration/ball_collision_fit_v2.json"
CUSHION_FIT = REPO_ROOT / "physics_models/calibration/cushion_fit_v2.json"


class CandidateWorkflowContractTests(unittest.TestCase):
    def test_rejected_successor_is_not_the_runtime_default(self):
        completed = subprocess.run(
            [str(EXECUTABLE), "--print-physics-profile"],
            check=True, capture_output=True, text=True,
        )
        self.assertEqual(
            json.loads(completed.stdout)["id"],
            "chinese_pool_legacy_v1",
        )

    def test_integrated_profile_uses_committed_fit_outputs(self):
        profile = json.loads(PROFILE.read_text(encoding="utf-8"))["runtime_profile"]
        surface = json.loads(SURFACE_FIT.read_text(encoding="utf-8"))["fit"]
        ball = json.loads(BALL_FIT.read_text(encoding="utf-8"))["fit"]
        cushion = json.loads(CUSHION_FIT.read_text(encoding="utf-8"))["fit"]
        self.assertEqual(
            profile["surface"]["sliding_friction_coefficient"],
            surface["sliding_friction_coefficient"],
        )
        self.assertEqual(
            profile["surface"]["rolling_resistance_acceleration_cm_s2"],
            surface["rolling_resistance_acceleration_cm_s2"],
        )
        self.assertEqual(profile["ball"]["normal_restitution"],
                         ball["normal_restitution"])
        self.assertEqual(profile["ball"]["friction_coefficient"],
                         ball["friction_coefficient"])
        self.assertEqual(profile["cushion"]["restitution_intercept"],
                         cushion["e_intercept"])
        self.assertEqual(profile["cushion"]["restitution_slope_per_mps"],
                         cushion["e_slope_per_mps"])
        self.assertEqual(profile["cushion"]["minimum_restitution"],
                         cushion["e_min"])
        self.assertEqual(profile["cushion"]["maximum_restitution"],
                         cushion["e_max"])

    def test_pre_freeze_inventory_is_complete_and_contains_no_results(self):
        inventory = json.loads(INVENTORY.read_text(encoding="utf-8"))
        self.assertEqual(inventory["schema_version"], 2)
        self.assertEqual(inventory["candidate_id"], "phase3_integrated_v2")
        for required in (
            "profile", "calibration_reports", "confirmation_packages",
            "full_game_matrix", "performance_budget", "metric_contracts",
        ):
            self.assertIn(required, inventory)
        serialized = json.dumps(inventory, sort_keys=True)
        for forbidden in ("result_artifact", "validation_receipt", "confirmation_result"):
            self.assertNotIn(forbidden, serialized)
        artifacts = [inventory["profile"], inventory["full_game_matrix"],
                     inventory["performance_budget"]]
        artifacts.extend(inventory["calibration_reports"])
        artifacts.extend(inventory["confirmation_packages"])
        artifacts.extend(inventory["metric_contracts"])
        for artifact in artifacts:
            path = REPO_ROOT / artifact["path"]
            with self.subTest(path=artifact["path"]):
                self.assertTrue(path.is_file())
                self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(),
                                 artifact["sha256"])

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
