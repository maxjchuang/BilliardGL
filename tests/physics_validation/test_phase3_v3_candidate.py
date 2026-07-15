import hashlib
import json
import subprocess
import unittest
from pathlib import Path

from tools.physics_validation.build_v3_profile import build_v3_profile


ROOT = Path(__file__).resolve().parents[2]
EXECUTABLE = ROOT / "build/Billiards"
PROFILE = ROOT / "physics_models/profiles/chinese_pool_full_game_v3.json"
INVENTORY = ROOT / "physics_models/promotion/phase3_candidates_v3.json"
V2_PROFILE = ROOT / "physics_models/profiles/chinese_pool_full_game_v2.json"
BALL_FIT = ROOT / "physics_models/calibration/ball_collision_fit_v3.json"
CUSHION_FIT = ROOT / "physics_models/calibration/cushion_fit_v3.json"


class Phase3V3CandidateTests(unittest.TestCase):
    def test_runtime_default_is_exact_v3_profile(self):
        emitted = json.loads(subprocess.run(
            [str(EXECUTABLE), "--print-physics-profile"], check=True,
            capture_output=True, text=True).stdout)
        committed = json.loads(PROFILE.read_text(encoding="utf-8"))
        query = committed["runtime_query"]
        self.assertEqual(emitted["id"], query["id"])
        self.assertEqual(emitted["formula_version"], query["formula_version"])
        self.assertEqual(
            hashlib.sha256(emitted["canonical_text"].encode("utf-8")).hexdigest(),
            query["canonical_text_sha256"],
        )

    def test_profile_is_reproducible_from_v2_and_v3_fits(self):
        expected = build_v3_profile(
            json.loads(V2_PROFILE.read_text(encoding="utf-8")),
            json.loads(BALL_FIT.read_text(encoding="utf-8")),
            json.loads(CUSHION_FIT.read_text(encoding="utf-8")),
        )
        self.assertEqual(
            json.loads(PROFILE.read_text(encoding="utf-8")), expected)
        self.assertEqual(expected["runtime_profile"]["id"],
                         "chinese_pool_full_game_v3")

    def test_inventory_contains_every_pre_freeze_artifact(self):
        inventory = json.loads(INVENTORY.read_text(encoding="utf-8"))
        self.assertEqual(inventory["candidate_id"], "phase3_integrated_v3")
        self.assertEqual(
            {package["package_id"]
             for package in inventory["confirmation_packages"]},
            {"derby_fuller_1999", "han_2005"},
        )
        serialized = json.dumps(inventory, sort_keys=True)
        self.assertNotIn("confirmation_consumption", serialized)
        self.assertNotIn("validation_receipt", serialized)
        artifacts = [
            inventory["profile"], inventory["full_game_matrix"],
            inventory["performance_budget"],
            *inventory["calibration_reports"],
            *inventory["confirmation_packages"],
            *inventory["metric_contracts"],
        ]
        for artifact in artifacts:
            path = ROOT / artifact["path"]
            self.assertTrue(path.is_file(), artifact["path"])
            self.assertEqual(
                hashlib.sha256(path.read_bytes()).hexdigest(),
                artifact["sha256"], artifact["path"])

    def test_v3_matrix_preserves_all_v2_acceptance_semantics(self):
        v2 = json.loads((ROOT / "physics_models/promotion/full_game_matrix_v2.json")
                        .read_text(encoding="utf-8"))
        v3 = json.loads((ROOT / "physics_models/promotion/full_game_matrix_v3.json")
                        .read_text(encoding="utf-8"))
        self.assertEqual(v3["cases"], v2["cases"])
        self.assertEqual(len(v3["cases"]), 12)
        self.assertNotEqual(v3["artifact_root"], v2["artifact_root"])


if __name__ == "__main__":
    unittest.main()
