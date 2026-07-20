import hashlib
import json
import unittest
from pathlib import Path

from tools.physics_validation.build_v4_profile import (
    build_v4_profile,
    canonical_runtime_text,
    physics_values,
    write_v4_candidate,
)


ROOT = Path(__file__).resolve().parents[2]
V3_PROFILE = ROOT / "physics_models/profiles/chinese_pool_full_game_v3.json"
V4_PROFILE = ROOT / "physics_models/profiles/chinese_pool_full_game_v4.json"
INVENTORY = ROOT / "physics_models/promotion/phase3_candidates_v4.json"
EQUIVALENCE = ROOT / "physics_models/promotion/phase3_v4_physics_equivalence.json"


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class Phase3V4CandidateTests(unittest.TestCase):
    def test_builder_changes_identity_without_changing_physics(self):
        v3 = json.loads(V3_PROFILE.read_text(encoding="utf-8"))
        v4 = build_v4_profile(v3)
        self.assertEqual(v4["runtime_profile"]["id"],
                         "chinese_pool_full_game_v4")
        self.assertEqual(v4["runtime_profile"]["formula_version"],
                         v3["runtime_profile"]["formula_version"])
        self.assertEqual(physics_values(v4), physics_values(v3))

    def test_committed_v4_runtime_query_is_exact_profile_snapshot(self):
        document = json.loads(V4_PROFILE.read_text(encoding="utf-8"))
        profile = document["runtime_profile"]
        query = document["runtime_query"]
        self.assertEqual(profile["id"], query["id"])
        self.assertEqual(profile["formula_version"], query["formula_version"])
        self.assertEqual(
            hashlib.sha256(
                canonical_runtime_text(profile).encode("utf-8")
            ).hexdigest(),
            query["canonical_text_sha256"],
        )

    def test_equivalence_evidence_proves_every_runtime_section(self):
        evidence = json.loads(EQUIVALENCE.read_text(encoding="utf-8"))
        self.assertTrue(evidence["physics_identical"])
        self.assertEqual(
            set(evidence["sections"]),
            {"ball", "surface", "cue", "cushion", "table_boundary", "solver"},
        )
        for section in evidence["sections"].values():
            self.assertTrue(section["matches"])
            self.assertEqual(section["v3_sha256"], section["v4_sha256"])

    def test_generated_runtime_assignments_are_identical_after_identity(self):
        v3 = (ROOT / "src/Billiards/generated/phase3_v3_profile.inc") \
            .read_text(encoding="utf-8").splitlines()
        v4 = (ROOT / "src/Billiards/generated/phase3_v4_profile.inc") \
            .read_text(encoding="utf-8").splitlines()
        self.assertEqual(v4[1], 'profile.id = "chinese_pool_full_game_v4";')
        self.assertEqual(v4[2:], v3[2:])

    def test_matrix_and_budget_change_only_candidate_identity(self):
        v3_matrix = json.loads((
            ROOT / "physics_models/promotion/full_game_matrix_v3.json"
        ).read_text(encoding="utf-8"))
        v4_matrix = json.loads((
            ROOT / "physics_models/promotion/full_game_matrix_v4.json"
        ).read_text(encoding="utf-8"))
        self.assertEqual(v4_matrix["cases"], v3_matrix["cases"])
        self.assertEqual(v4_matrix["physics_profile_id"],
                         "chinese_pool_full_game_v4")
        self.assertEqual(
            v4_matrix["artifact_root"],
            "physics_models/candidates/phase3_integrated_v4/full_game",
        )
        self.assertEqual(
            (ROOT / "physics_models/promotion/full_game_performance_budget_v4.json")
            .read_bytes(),
            (ROOT / "physics_models/promotion/full_game_performance_budget_v3.json")
            .read_bytes(),
        )

    def test_inventory_is_complete_and_contains_no_confirmation_results(self):
        inventory = json.loads(INVENTORY.read_text(encoding="utf-8"))
        self.assertEqual(inventory["candidate_id"], "phase3_integrated_v4")
        self.assertEqual(
            {package["package_id"]
             for package in inventory["confirmation_packages"]},
            {"alciatore_2005_tp_a15", "han_2005"},
        )
        serialized = json.dumps(inventory, sort_keys=True)
        for forbidden in ("confirmation_consumption", "validation_receipt",
                          "confirmation_result", "result_artifact"):
            self.assertNotIn(forbidden, serialized)
        artifacts = [
            inventory["profile"], inventory["full_game_matrix"],
            inventory["performance_budget"], *inventory["calibration_reports"],
            *inventory["confirmation_packages"], *inventory["metric_contracts"],
        ]
        for artifact in artifacts:
            path = ROOT / artifact["path"]
            self.assertTrue(path.is_file(), artifact["path"])
            self.assertEqual(sha256(path), artifact["sha256"], artifact["path"])

    def test_generator_does_not_modify_any_v3_candidate_artifact(self):
        v3_paths = [
            V3_PROFILE,
            ROOT / "src/Billiards/generated/phase3_v3_profile.inc",
            ROOT / "physics_models/promotion/phase3_candidates_v3.json",
            ROOT / "physics_models/promotion/full_game_matrix_v3.json",
            ROOT / "physics_models/promotion/full_game_performance_budget_v3.json",
        ]
        before = {path: sha256(path) for path in v3_paths}
        write_v4_candidate(ROOT)
        self.assertEqual(before, {path: sha256(path) for path in v3_paths})


if __name__ == "__main__":
    unittest.main()
