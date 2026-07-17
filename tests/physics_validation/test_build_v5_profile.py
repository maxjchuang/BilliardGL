import hashlib
import json
import os
import subprocess
import unittest
from pathlib import Path

from tools.physics_validation.build_v5_profile import (
    FORMULA_VERSION,
    PROFILE_ID,
    build_v5_profile,
    write_v5_candidate,
)


ROOT = Path(__file__).resolve().parents[2]
BUILD_DIR = Path(os.environ.get("BILLIARDGL_BUILD_DIR", ROOT / "build"))
V4_PROFILE = ROOT / "physics_models/profiles/chinese_pool_full_game_v4.json"
V5_PROFILE = ROOT / "physics_models/profiles/chinese_pool_full_game_v5.json"
FIT = ROOT / "physics_models/calibration/frozen_cue_contact_v1_fit.json"
INVENTORY = ROOT / "physics_models/promotion/phase3_candidates_v5.json"


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


class BuildV5ProfileTests(unittest.TestCase):
    def test_only_frozen_contact_and_identity_change_from_v4(self):
        v4 = json.loads(V4_PROFILE.read_text(encoding="utf-8"))
        fit = json.loads(FIT.read_text(encoding="utf-8"))
        v5 = build_v5_profile(v4, fit)
        old = v4["runtime_profile"]
        new = v5["runtime_profile"]
        self.assertEqual(new["id"], PROFILE_ID)
        self.assertEqual(new["formula_version"], FORMULA_VERSION)
        for section in ("ball", "surface", "cue", "cushion",
                        "table_boundary", "solver"):
            self.assertEqual(new[section], old[section], section)
        frozen = new["frozen_cue_contact"]
        self.assertTrue(frozen["enabled"])
        self.assertEqual(
            frozen["normal_stiffness_n_per_m32"],
            fit["winner"]["stiffness_n_per_m32"])
        self.assertEqual(
            frozen["normal_dissipation_s_per_m"],
            fit["winner"]["dissipation_s_per_m"])
        self.assertEqual(frozen["microstep_seconds"], 0.0000025)

    def test_rejected_v5_is_preserved_but_not_the_runtime_default(self):
        emitted = json.loads(subprocess.run(
            [str(BUILD_DIR / "Billiards"), "--print-physics-profile"],
            check=True, capture_output=True, text=True).stdout)
        self.assertEqual(
            (emitted["id"], emitted["formula_version"]),
            ("chinese_pool_legacy_v1", "legacy_v1"))
        policy = json.loads((ROOT /
            "physics_models/promotion/phase3_production_default.json"
        ).read_text(encoding="utf-8"))
        self.assertEqual(policy["authorized_profile_id"], emitted["id"])
        self.assertTrue(V5_PROFILE.is_file())

    def test_inventory_binds_every_v5_input_and_confirmation_manifest(self):
        inventory = json.loads(INVENTORY.read_text(encoding="utf-8"))
        self.assertEqual(inventory["candidate_id"], "phase3_integrated_v5")
        self.assertEqual(inventory["status"], "pre_freeze")
        packages = {item["package_id"]
                    for item in inventory["confirmation_packages"]}
        self.assertEqual(packages, {"cross_2016_newtons_cradle", "han_2005"})
        paths = {item["path"] for key in (
            "calibration_reports", "metric_contracts",
            "confirmation_packages") for item in inventory[key]}
        required = {
            "physics_models/calibration/frozen_cue_contact_v1_inputs.csv",
            "physics_models/calibration/frozen_cue_contact_v1_fit.json",
            "physics_models/calibration/frozen_cue_contact_v1_residuals.csv",
            "physics_models/calibration/frozen_cue_contact_v1_sensitivity.csv",
            "physics_models/calibration/alciatore_frozen_contact_v5_inputs.csv",
            "physics_models/calibration/alciatore_frozen_contact_v5_report.json",
            "physics_models/calibration/alciatore_frozen_contact_v5_residuals.csv",
            "physics_models/calibration/alciatore_frozen_contact_v5_sensitivity.csv",
            "tests/physics_validation/reference_data/shimamura_2006_cue_contact/manifest.json",
            "tests/physics_validation/reference_data/alciatore_2005_tp_a15/manifest.json",
            "tests/physics_validation/reference_data/cross_2016_newtons_cradle/manifest.json",
            "tests/physics_validation/reference_data/han_2005/manifest.json",
            "physics_models/regression/phase3_v4_ordinary_shot_baseline.json",
            "src/Billiards/automation_protocol.cpp",
            "src/Billiards/physics_scenario.cpp",
        }
        self.assertTrue(required <= paths)
        artifacts = [inventory["profile"], inventory["full_game_matrix"],
                     inventory["performance_budget"],
                     *inventory["calibration_reports"],
                     *inventory["confirmation_packages"],
                     *inventory["metric_contracts"]]
        for artifact in artifacts:
            path = ROOT / artifact["path"]
            self.assertTrue(path.is_file(), artifact["path"])
            self.assertEqual(sha256(path), artifact["sha256"], artifact["path"])
        serialized = json.dumps(inventory, sort_keys=True)
        for forbidden in ("confirmation_consumption", "confirmation_result",
                          "validation_receipt"):
            self.assertNotIn(forbidden, serialized)

    def test_matrix_budget_and_ordinary_equivalence_are_frozen(self):
        v4_matrix = json.loads((ROOT /
            "physics_models/promotion/full_game_matrix_v4.json").read_text())
        v5_matrix = json.loads((ROOT /
            "physics_models/promotion/full_game_matrix_v5.json").read_text())
        self.assertEqual(v5_matrix["cases"], v4_matrix["cases"])
        self.assertEqual(v5_matrix["physics_profile_id"], PROFILE_ID)
        self.assertEqual(v5_matrix["artifact_root"],
            "physics_models/candidates/phase3_integrated_v5/full_game")
        self.assertEqual((ROOT /
            "physics_models/promotion/full_game_performance_budget_v5.json"
        ).read_bytes(), (ROOT /
            "physics_models/promotion/full_game_performance_budget_v4.json"
        ).read_bytes())
        equivalence = json.loads((ROOT /
            "physics_models/promotion/phase3_v5_ordinary_equivalence.json"
        ).read_text())
        self.assertTrue(equivalence["ordinary_physics_identical"])
        self.assertEqual(equivalence["baseline"]["path"],
            "physics_models/regression/phase3_v4_ordinary_shot_baseline.json")

    def test_full_game_candidate_artifacts_cover_and_pass_the_v5_matrix(self):
        matrix = json.loads((ROOT /
            "physics_models/promotion/full_game_matrix_v5.json").read_text())
        output = ROOT / matrix["artifact_root"]
        summary = json.loads((output / "matrix_summary.json").read_text())
        expected = [(item["id"], item["seed"])
                    for item in matrix["cases"]]
        actual = [(item["case_id"], item["seed"])
                  for item in summary["cases"]]
        self.assertEqual(actual, expected)
        self.assertTrue(summary["passed"])
        for item in summary["cases"]:
            self.assertTrue(item["passed"], item["case_id"])
            case_root = output / item["case_id"]
            for name in ("index.csv", "summary.json", "trace.json"):
                self.assertTrue((case_root / name).is_file(),
                                f"{item['case_id']}/{name}")

    def test_generator_does_not_modify_v4_inputs(self):
        protected = [
            V4_PROFILE,
            ROOT / "src/Billiards/generated/phase3_v4_profile.inc",
            ROOT / "physics_models/promotion/phase3_candidates_v4.json",
            ROOT / "physics_models/promotion/full_game_matrix_v4.json",
            ROOT / "physics_models/promotion/full_game_performance_budget_v4.json",
            ROOT / "physics_models/regression/phase3_v4_ordinary_shot_baseline.json",
        ]
        before = {path: sha256(path) for path in protected}
        write_v5_candidate(ROOT)
        self.assertEqual(before, {path: sha256(path) for path in protected})


if __name__ == "__main__":
    unittest.main()
