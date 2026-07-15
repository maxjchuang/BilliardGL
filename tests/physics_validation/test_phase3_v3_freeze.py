import hashlib
import json
import subprocess
import unittest
from pathlib import Path

from tools.physics_validation.rebuild_frozen import frozen_build_paths


ROOT = Path(__file__).resolve().parents[2]
FREEZE = ROOT / "physics_models/candidates/phase3_integrated_v3/freeze.json"
INVENTORY = ROOT / "physics_models/promotion/phase3_candidates_v3.json"
SELECTED_REVISION = "db1c24b2b7d4d1c1145f4ebc7998fcd89109bc7a"


def revision_digest(path):
    content = subprocess.run(
        ["git", "show", f"{SELECTED_REVISION}:{path}"],
        cwd=ROOT, check=True, capture_output=True).stdout
    return hashlib.sha256(content).hexdigest()


class Phase3V3FreezeTests(unittest.TestCase):
    def setUp(self):
        self.freeze = json.loads(FREEZE.read_text(encoding="utf-8"))

    def test_freeze_binds_selected_revision_and_two_clean_builds(self):
        self.assertEqual(self.freeze["candidate_id"], "phase3_integrated_v3")
        self.assertEqual(self.freeze["source_revision"], SELECTED_REVISION)
        self.assertEqual(len(self.freeze["clean_build_sha256"]), 2)
        self.assertEqual(len(set(self.freeze["clean_build_sha256"])), 1)
        self.assertEqual(len(self.freeze["clean_profile_sha256"]), 2)
        self.assertEqual(len(set(self.freeze["clean_profile_sha256"])), 1)

    def test_freeze_covers_exact_v3_inventory(self):
        inventory = json.loads(INVENTORY.read_text(encoding="utf-8"))
        expected = [inventory["profile"], inventory["full_game_matrix"],
                    inventory["performance_budget"]]
        expected.extend(inventory["calibration_reports"])
        expected.extend(inventory["confirmation_packages"])
        expected.extend(inventory["metric_contracts"])
        self.assertEqual(
            {item["path"] for item in self.freeze["artifacts"]},
            {item["path"] for item in expected},
        )
        for item in self.freeze["artifacts"]:
            self.assertEqual(revision_digest(item["path"]), item["sha256"])

    def test_recipe_recreates_the_same_stable_build_identity(self):
        recipe = self.freeze["build_recipe"]
        self.assertEqual(recipe["worktree_leaf"],
                         "billiardgl-phase3-freeze-worktree")
        self.assertEqual(recipe["configuration"], "Release")
        self.assertEqual(recipe["executable_relative_path"], "build/Billiards")
        self.assertEqual(recipe["full_game_runner_relative_path"],
                         "build/BilliardsFullGameStress")
        paths = frozen_build_paths(self.freeze)
        self.assertEqual(paths.checkout.name, recipe["worktree_leaf"])
        self.assertEqual(paths.executable.relative_to(paths.checkout).as_posix(),
                         recipe["executable_relative_path"])

    def test_freeze_is_pre_confirmation_and_rebuild_does_not_open_packages(self):
        for path in (
                "physics_models/candidates/phase3_integrated_v3/confirmation",
                "physics_models/candidates/phase3_integrated_v3/"
                "confirmation_consumption.json"):
            result = subprocess.run(
                ["git", "cat-file", "-e", f"{SELECTED_REVISION}:{path}"],
                cwd=ROOT, capture_output=True)
            self.assertNotEqual(
                result.returncode, 0,
                f"frozen source revision unexpectedly contains {path}")
        source = (ROOT / "tools/physics_validation/rebuild_frozen.py") \
            .read_text(encoding="utf-8")
        self.assertNotIn("reference_data", source)
        self.assertNotIn("confirmation_declaration", source)

    def test_selected_revision_is_the_candidate_commit(self):
        subject = subprocess.run(
            ["git", "show", "-s", "--format=%s", SELECTED_REVISION],
            cwd=ROOT, check=True, capture_output=True, text=True).stdout.strip()
        self.assertEqual(subject, "feat: select integrated phase 3 candidate v3")


if __name__ == "__main__":
    unittest.main()
