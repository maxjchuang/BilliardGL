import hashlib
import json
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CANDIDATE_ID = "phase3_integrated_v5"
CANDIDATE = ROOT / "physics_models/candidates" / CANDIDATE_ID
FREEZE = CANDIDATE / "freeze.json"
PROOF = CANDIDATE / "confirmation_contract_proof.json"
READINESS = CANDIDATE / "confirmation_readiness.json"
INVENTORY = ROOT / "physics_models/promotion/phase3_candidates_v5.json"
SELECTED_REVISION = "0245f115850e94a94917e65c49556c354eba20f3"


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def revision_sha256(relative):
    content = subprocess.run(
        ["git", "show", f"{SELECTED_REVISION}:{relative}"],
        cwd=ROOT, check=True, capture_output=True).stdout
    return hashlib.sha256(content).hexdigest()


class Phase3V5ReadinessTests(unittest.TestCase):
    def setUp(self):
        self.freeze = json.loads(FREEZE.read_text(encoding="utf-8"))
        self.inventory = json.loads(INVENTORY.read_text(encoding="utf-8"))
        self.readiness = json.loads(READINESS.read_text(encoding="utf-8"))

    def test_freeze_binds_clean_selected_revision_and_exact_inventory(self):
        self.assertEqual(self.freeze["candidate_id"], CANDIDATE_ID)
        self.assertEqual(self.freeze["source_revision"], SELECTED_REVISION)
        subject = subprocess.run(
            ["git", "show", "-s", "--format=%s", SELECTED_REVISION],
            cwd=ROOT, check=True, capture_output=True, text=True,
        ).stdout.strip()
        self.assertEqual(
            subject, "feat: select phase 3 v5 coupled cue contact candidate")
        expected = [
            self.inventory["profile"], self.inventory["full_game_matrix"],
            self.inventory["performance_budget"],
            *self.inventory["calibration_reports"],
            *self.inventory["confirmation_packages"],
            *self.inventory["metric_contracts"],
        ]
        self.assertEqual(
            {item["path"] for item in self.freeze["artifacts"]},
            {item["path"] for item in expected},
        )
        for artifact in self.freeze["artifacts"]:
            self.assertEqual(
                revision_sha256(artifact["path"]), artifact["sha256"],
                artifact["path"])

    def test_two_clean_builds_and_profiles_are_identical(self):
        builds = self.freeze["clean_build_sha256"]
        profiles = self.freeze["clean_profile_sha256"]
        self.assertEqual(len(builds), 2)
        self.assertEqual(len(set(builds)), 1)
        self.assertEqual(builds[0], self.freeze["executable_sha256"])
        self.assertEqual(len(profiles), 2)
        self.assertEqual(len(set(profiles)), 1)
        self.assertEqual(
            profiles[0], self.freeze["canonical_profile_sha256"])

    def test_v5_contract_artifacts_are_bound_before_confirmation(self):
        bound = {item["path"] for item in self.freeze["artifacts"]}
        for relative in (
            "physics_models/promotion/phase3_v5_ordinary_equivalence.json",
            "physics_models/regression/phase3_v4_ordinary_shot_baseline.json",
            "physics_models/calibration/frozen_cue_contact_v1_residuals.csv",
            "physics_models/calibration/frozen_cue_contact_v1_sensitivity.csv",
            "physics_models/calibration/alciatore_frozen_contact_v5_residuals.csv",
            "physics_models/calibration/alciatore_frozen_contact_v5_sensitivity.csv",
            "src/Billiards/automation_protocol.cpp",
            "src/Billiards/physics_scenario.cpp",
        ):
            self.assertIn(relative, bound)

    def test_real_path_proof_matches_the_frozen_executable(self):
        proof = json.loads(PROOF.read_text(encoding="utf-8"))
        self.assertEqual(proof["result"], "PASSED")
        self.assertTrue(proof["parse_succeeded"])
        self.assertEqual(
            proof["executable_sha256"], self.freeze["executable_sha256"])
        self.assertEqual(
            proof["first_trace_sha256"], proof["second_trace_sha256"])
        proof_text = PROOF.read_text(encoding="utf-8")
        self.assertNotIn("cross_2016_newtons_cradle", proof_text)
        self.assertNotIn("han_2005", proof_text)

    def test_readiness_is_complete_and_binds_all_checkpoint_trees(self):
        self.assertEqual(self.readiness["candidate_id"], CANDIDATE_ID)
        self.assertEqual(self.readiness["status"], "READY")
        self.assertEqual(self.readiness["failures"], [])
        self.assertEqual(self.readiness["freeze_sha256"], sha256(FREEZE))
        self.assertEqual(self.readiness["inventory_sha256"], sha256(INVENTORY))
        self.assertEqual(
            self.readiness["confirmation_contract_proof_sha256"],
            sha256(PROOF))
        self.assertEqual(self.readiness["full_game_case_count"], 12)
        self.assertTrue(all(self.readiness["checks"].values()))

    def test_cross_and_han_are_preregistered_ready_and_unopened(self):
        packages = self.readiness["confirmation_packages"]
        self.assertEqual(
            set(packages), {"cross_2016_newtons_cradle", "han_2005"})
        self.assertTrue(all(
            item["attempt"] == "UNOPENED" and item["ready"]
            for item in packages.values()))
        self.assertFalse((CANDIDATE / "confirmation_consumption.json").exists())
        self.assertFalse((CANDIDATE / "confirmation").exists())


if __name__ == "__main__":
    unittest.main()
