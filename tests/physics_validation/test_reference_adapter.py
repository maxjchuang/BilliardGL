import json
import unittest
from dataclasses import FrozenInstanceError, replace
from pathlib import Path

from tools.physics_validation.reference_adapter import (
    ReferenceAdapterRegistry,
    default_reference_registry,
)
from tools.physics_validation.reference_package import (
    ReferencePackage,
    ReferencePackageError,
    load_reference_package,
)
from tools.physics_validation.reference_point import read_reference_points
from tools.physics_validation.reference_split import load_reference_split


FIXTURE_ROOT = Path(__file__).parent / "fixtures/reference_package_v1"


class ReferenceAdapterTests(unittest.TestCase):
    def setUp(self):
        self.package = load_reference_package(FIXTURE_ROOT)
        self.points = read_reference_points(
            self.package.files["normalized"], self.package.manifest["dataset_id"])
        self.split = load_reference_split(
            self.package.files["split"], self.points,
            self.package.manifest["dataset_id"],
            self.package.manifest["dataset_version"])

    def test_adapts_cases_in_stable_order_with_committed_partitions(self):
        cases = default_reference_registry().adapt(
            self.package, self.split, tuple(reversed(self.points)))

        self.assertEqual(
            [case.case_id for case in cases],
            ["free_roll_calibration", "free_roll_holdout"],
        )
        self.assertEqual([case.partition for case in cases], ["CALIBRATION", "HOLDOUT"])
        self.assertEqual([len(case.points) for case in cases], [1, 1])
        scenarios = [json.loads(case.scenario_json) for case in cases]
        self.assertEqual(
            [scenario["id"] for scenario in scenarios],
            [
                "synthetic_reference__free_roll_calibration",
                "synthetic_reference__free_roll_holdout",
            ],
        )
        self.assertEqual(scenarios[0]["balls"][0]["velocity_cm_s"], [20.0, 0.0, 0.0])
        self.assertEqual(scenarios[1]["balls"][0]["velocity_cm_s"], [30.0, 0.0, 0.0])

    def test_repeated_fresh_registries_produce_byte_identical_payloads(self):
        first = default_reference_registry().adapt(self.package, self.split, self.points)
        second = default_reference_registry().adapt(self.package, self.split, self.points)

        self.assertEqual(
            [(case.scenario_json, case.provenance_json) for case in first],
            [(case.scenario_json, case.provenance_json) for case in second],
        )
        self.assertTrue(all(case.scenario_json.endswith("\n") for case in first))
        self.assertTrue(all(case.provenance_json.endswith("\n") for case in first))

    def test_case_and_serialized_payload_are_isolated_from_mutation(self):
        case = default_reference_registry().adapt(
            self.package, self.split, self.points)[0]
        decoded = json.loads(case.scenario_json)
        decoded["balls"][0]["velocity_cm_s"][0] = 999.0

        self.assertEqual(
            json.loads(case.scenario_json)["balls"][0]["velocity_cm_s"][0], 20.0)
        with self.assertRaises(FrozenInstanceError):
            case.case_id = "changed"

    def test_expectations_preserve_point_interval_and_observed_metric(self):
        case = default_reference_registry().adapt(
            self.package, self.split, self.points)[0]
        expectation = json.loads(case.scenario_json)["expectations"][0]

        self.assertEqual(expectation["metric"], "value_within_interval")
        self.assertEqual(expectation["value"], {
            "ball_index": 0,
            "expected": 18.0,
            "lower": 17.75,
            "observed_metric": "stopping_distance_cm",
            "point_id": "stop_distance_cal_01",
            "unit": "cm",
            "upper": 18.25,
        })

    def test_provenance_contains_source_points_and_all_package_hashes(self):
        case = default_reference_registry().adapt(
            self.package, self.split, self.points)[0]
        provenance = json.loads(case.provenance_json)

        self.assertEqual(provenance["dataset_id"], "synthetic_reference")
        self.assertEqual(provenance["dataset_version"], "1.0.0")
        self.assertEqual(provenance["adapter_id"], "synthetic_free_roll_v1")
        self.assertEqual(provenance["point_ids"], ["stop_distance_cal_01"])
        self.assertEqual(
            provenance["source_locators"], ["synthetic:calibration:row-1"])
        self.assertEqual(
            set(provenance["package_hashes"]),
            {item["id"] for item in self.package.manifest["files"]},
        )

    def test_registry_rejects_unknown_adapter_and_duplicate_registration(self):
        registry = ReferenceAdapterRegistry()
        with self.assertRaisesRegex(ReferencePackageError, "UNKNOWN_ADAPTER"):
            registry.adapt(self.package, self.split, self.points)

        registry.register("synthetic_free_roll_v1", lambda package, split, points: ())
        with self.assertRaisesRegex(ReferencePackageError, "DUPLICATE_ADAPTER"):
            registry.register("synthetic_free_roll_v1", lambda package, split, points: ())

    def test_adapter_rejects_missing_case_mapping(self):
        points = (replace(self.points[0], case_id="unmapped_case"),)

        with self.assertRaisesRegex(ReferencePackageError, "ADAPTER_MAPPING_MISSING"):
            default_reference_registry().adapt(self.package, self.split, points)

    def test_adapter_rejects_case_mixing_committed_partitions(self):
        points = (
            self.points[0],
            replace(self.points[1], case_id=self.points[0].case_id),
        )

        with self.assertRaisesRegex(ReferencePackageError, "MIXED_PARTITION_CASE"):
            default_reference_registry().adapt(self.package, self.split, points)

    def test_adapter_rejects_point_from_another_dataset(self):
        points = (replace(self.points[0], dataset_id="other_dataset"),)

        with self.assertRaisesRegex(ReferencePackageError, "DATASET_MISMATCH"):
            default_reference_registry().adapt(self.package, self.split, points)


if __name__ == "__main__":
    unittest.main()
