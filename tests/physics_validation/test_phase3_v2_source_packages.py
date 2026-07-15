import csv
import importlib
import importlib.util
import unittest
from pathlib import Path

from tools.physics_validation.data_lifecycle import load_data_lifecycle
from tools.physics_validation.reference_package import load_reference_package
from tools.physics_validation.reference_point import read_reference_points
from tools.physics_validation.reference_split import load_reference_split


ROOT = Path(__file__).resolve().parents[2]
STATUS = ROOT / "tests/physics_validation/validation_data_status.json"
REFERENCE_ROOT = ROOT / "tests/physics_validation/reference_data"
EXPECTED_SPENT = {
    "domenech_2023_ball_collision",
    "mathavan_2009_high_speed",
    "mathavan_2010_cushion",
    "sudo_2002",
}
EXPECTED_HOLDOUT_STATUS = {
    "sudo_2002": "spent",
    "derby_fuller_1999": "confirmation",
    "han_2005": "confirmation",
}
EXPECTED_SCALARS = {
    "sudo_2002": {
        "ball_mass_mean", "ball_mass_sd", "ball_diameter_mean",
        "ball_diameter_sd", "cushion_e_low_speed", "cushion_e_all_speed",
        "cushion_contact_time_plateau", "ball_ball_e_head_on",
        "separation_angle_mean", "transverse_momentum_deficit",
    },
    "derby_fuller_1999": {
        "ball_mass", "ball_diameter", "camera_frame_rate", "initial_speed",
        "cue_sliding_acceleration", "target_sliding_acceleration",
        "cue_sliding_time", "target_sliding_time", "cue_final_speed",
        "target_final_speed", "momentum_before", "momentum_after",
        "kinetic_energy_loss",
    },
    "han_2005": {
        "coefficient_a", "coefficient_b", "coefficient_c",
        "han_speed_050", "han_speed_100", "han_speed_150",
        "han_speed_200", "han_speed_250",
    },
}


def csv_rows(path):
    with Path(path).open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream))


class Phase3V2SourcePackageTests(unittest.TestCase):
    def test_v1_experimental_holdouts_are_spent(self):
        registry = load_data_lifecycle(STATUS)
        for dataset_id in EXPECTED_SPENT:
            self.assertEqual(
                registry.entry(dataset_id, "1.0.0").holdout_status,
                "spent",
            )

    def test_successor_source_packages_are_complete_and_hash_verified(self):
        registry = load_data_lifecycle(STATUS)
        for package_id, expected_ids in EXPECTED_SCALARS.items():
            package_path = REFERENCE_ROOT / package_id
            self.assertTrue(package_path.is_dir(), f"missing package {package_id}")
            package = load_reference_package(package_path)
            version = package.manifest["dataset_version"]
            self.assertFalse(package.manifest["evidence"]["candidate_selection_input"])
            self.assertEqual(
                registry.entry(package_id, version).holdout_status,
                EXPECTED_HOLDOUT_STATUS[package_id],
            )
            rows = csv_rows(package.files["scalars"])
            self.assertEqual({row["point_id"] for row in rows}, expected_ids)
            target_ids = {
                row["point_id"] for row in rows
                if row["role"] == "confirmation_target"
            }
            normalized = read_reference_points(package.files["normalized"], package_id)
            self.assertEqual({point.point_id for point in normalized}, target_ids)
            split = load_reference_split(
                package.files["split"], normalized, package_id, version)
            self.assertEqual(split.calibration_groups, frozenset())
            self.assertEqual(
                split.holdout_groups,
                frozenset(point.group_id for point in normalized),
            )

    def test_normalization_scripts_reproduce_committed_bytes(self):
        for package_id in EXPECTED_SCALARS:
            module_name = f"tools.physics_validation.extract_{package_id}"
            self.assertIsNotNone(
                importlib.util.find_spec(module_name),
                f"missing deterministic extractor for {package_id}",
            )
            module = importlib.import_module(module_name)
            package = REFERENCE_ROOT / package_id
            self.assertEqual(
                module.normalized_bytes(package / "raw_extracted.csv"),
                (package / "normalized.csv").read_bytes(),
            )

    def test_packages_do_not_redistribute_publication_expression(self):
        for package_id in EXPECTED_SCALARS:
            package = REFERENCE_ROOT / package_id
            prohibited = {
                path.suffix.lower() for path in package.rglob("*")
                if path.suffix.lower() in {".pdf", ".png", ".jpg", ".jpeg", ".tif"}
            }
            self.assertEqual(prohibited, set())


if __name__ == "__main__":
    unittest.main()
