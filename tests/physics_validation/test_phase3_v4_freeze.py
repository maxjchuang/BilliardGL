import hashlib
import json
import subprocess
import unittest
from pathlib import Path

from tools.physics_validation.rebuild_frozen import frozen_build_paths


ROOT = Path(__file__).resolve().parents[2]
FREEZE = ROOT / "physics_models/candidates/phase3_integrated_v4/freeze.json"
INVENTORY = ROOT / "physics_models/promotion/phase3_candidates_v4.json"
SELECTED_REVISION = "8728e1a227af209d9bd3b4f06085571f8d9a9f1c"


def revision_digest(path):
    content = subprocess.run(
        ["git", "show", f"{SELECTED_REVISION}:{path}"],
        cwd=ROOT, check=True, capture_output=True).stdout
    return hashlib.sha256(content).hexdigest()


class Phase3V4FreezeTests(unittest.TestCase):
    def setUp(self):
        self.freeze = json.loads(FREEZE.read_text(encoding="utf-8"))

    def test_freeze_binds_candidate_commit_and_two_clean_builds(self):
        self.assertEqual(self.freeze["candidate_id"], "phase3_integrated_v4")
        self.assertEqual(self.freeze["source_revision"], SELECTED_REVISION)
        self.assertEqual(len(self.freeze["clean_build_sha256"]), 2)
        self.assertEqual(len(set(self.freeze["clean_build_sha256"])), 1)
        self.assertEqual(len(self.freeze["clean_profile_sha256"]), 2)
        self.assertEqual(len(set(self.freeze["clean_profile_sha256"])), 1)

    def test_freeze_covers_exact_v4_inventory_at_selected_revision(self):
        inventory = json.loads(INVENTORY.read_text(encoding="utf-8"))
        expected = [inventory["profile"], inventory["full_game_matrix"],
                    inventory["performance_budget"],
                    *inventory["calibration_reports"],
                    *inventory["confirmation_packages"],
                    *inventory["metric_contracts"]]
        self.assertEqual(
            {item["path"] for item in self.freeze["artifacts"]},
            {item["path"] for item in expected},
        )
        for item in self.freeze["artifacts"]:
            self.assertEqual(revision_digest(item["path"]), item["sha256"])

    def test_recipe_recreates_the_stable_frozen_build_identity(self):
        recipe = self.freeze["build_recipe"]
        paths = frozen_build_paths(self.freeze)
        self.assertEqual(recipe["worktree_leaf"],
                         "billiardgl-phase3-freeze-worktree")
        self.assertEqual(recipe["configuration"], "Release")
        self.assertEqual(paths.checkout.name, recipe["worktree_leaf"])
        self.assertEqual(paths.executable.relative_to(paths.checkout).as_posix(),
                         recipe["executable_relative_path"])

    def test_selected_revision_contains_no_confirmation_state(self):
        for path in (
                "physics_models/candidates/phase3_integrated_v4/confirmation",
                "physics_models/candidates/phase3_integrated_v4/"
                "confirmation_consumption.json"):
            result = subprocess.run(
                ["git", "cat-file", "-e", f"{SELECTED_REVISION}:{path}"],
                cwd=ROOT, capture_output=True)
            self.assertNotEqual(result.returncode, 0, path)

    def test_selected_revision_is_the_v4_candidate_commit(self):
        subject = subprocess.run(
            ["git", "show", "-s", "--format=%s", SELECTED_REVISION],
            cwd=ROOT, check=True, capture_output=True, text=True).stdout.strip()
        self.assertEqual(
            subject, "feat: select physics-identical phase 3 candidate v4")


if __name__ == "__main__":
    unittest.main()
