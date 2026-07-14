import hashlib
import json
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
FREEZE = ROOT / "physics_models/candidates/phase3_integrated_v2/freeze.json"
PROFILE_PATH = "physics_models/profiles/chinese_pool_full_game_v2.json"
INVENTORY = ROOT / "physics_models/promotion/phase3_candidates_v2.json"


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


class Phase3V2FreezeTests(unittest.TestCase):
    def setUp(self):
        self.freeze = json.loads(FREEZE.read_text(encoding="utf-8"))

    def test_freeze_binds_committed_profile_source(self):
        raw = subprocess.run(
            ["git", "show", f"{self.freeze['source_revision']}:{PROFILE_PATH}"],
            cwd=ROOT, check=True, capture_output=True, text=True,
        ).stdout
        profile = json.loads(raw)["runtime_profile"]
        self.assertEqual(profile["id"], "chinese_pool_full_game_v2")
        self.assertEqual(profile["formula_version"], "full_game_integration_v2")

    def test_two_clean_build_digests_and_profile_outputs_match(self):
        self.assertEqual(len(self.freeze["clean_build_sha256"]), 2)
        self.assertEqual(len(set(self.freeze["clean_build_sha256"])), 1)
        self.assertEqual(self.freeze["executable_sha256"],
                         self.freeze["clean_build_sha256"][0])
        self.assertEqual(len(self.freeze["clean_profile_sha256"]), 2)
        self.assertEqual(len(set(self.freeze["clean_profile_sha256"])), 1)
        self.assertEqual(self.freeze["canonical_profile_sha256"],
                         self.freeze["clean_profile_sha256"][0])

    def test_freeze_covers_the_exact_pre_freeze_inventory(self):
        inventory = json.loads(INVENTORY.read_text(encoding="utf-8"))
        expected = [inventory["profile"], inventory["full_game_matrix"],
                    inventory["performance_budget"]]
        expected.extend(inventory["calibration_reports"])
        expected.extend(inventory["confirmation_packages"])
        expected.extend(inventory["metric_contracts"])
        expected_paths = {artifact["path"] for artifact in expected}
        frozen_paths = {artifact["path"] for artifact in self.freeze["artifacts"]}
        self.assertEqual(frozen_paths, expected_paths)
        for artifact in self.freeze["artifacts"]:
            with self.subTest(path=artifact["path"]):
                self.assertEqual(artifact["sha256"],
                                 sha256(ROOT / artifact["path"]))

    def test_freeze_is_pre_confirmation_and_self_excluding(self):
        serialized = json.dumps(self.freeze, sort_keys=True)
        self.assertNotIn("validation_receipt", serialized)
        self.assertNotIn("confirmation_result", serialized)
        self.assertNotIn(FREEZE.relative_to(ROOT).as_posix(),
                         {item["path"] for item in self.freeze["artifacts"]})


if __name__ == "__main__":
    unittest.main()
