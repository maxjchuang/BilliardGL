import csv
import tempfile
import unittest
from dataclasses import FrozenInstanceError, replace
from pathlib import Path

from tools.physics_validation.reference_point import (
    ReferencePoint,
    read_reference_points,
)


FIXTURE = Path(__file__).parent / "fixtures/reference_package_v1/normalized.csv"
HEADER = [
    "dataset_id", "series_id", "group_id", "case_id", "point_id", "partition",
    "metric", "expected", "unit", "measurement_uncertainty",
    "digitization_uncertainty", "conversion_uncertainty", "coverage_factor",
    "engineering_absolute_tolerance", "engineering_relative_tolerance",
    "source_locator", "pool_applicability",
]


def point(**overrides):
    values = {
        "dataset_id": "dataset",
        "series_id": "series",
        "group_id": "group",
        "case_id": "case",
        "point_id": "point",
        "partition": "CALIBRATION",
        "metric": "stopping_distance_cm",
        "expected": 10.0,
        "unit": "cm",
        "measurement_uncertainty": 0.3,
        "digitization_uncertainty": 0.4,
        "conversion_uncertainty": 0.0,
        "coverage_factor": 2.0,
        "engineering_absolute_tolerance": 0.0,
        "engineering_relative_tolerance": 0.0,
        "source_locator": "synthetic:row-1",
        "pool_applicability": "NOT_APPLICABLE",
    }
    values.update(overrides)
    return ReferencePoint(**values)


class ReferencePointTests(unittest.TestCase):
    def test_accepts_acceleration_unit_for_experimental_deceleration(self):
        path = self._write_rows([self._valid_row(unit="cm/s^2")])

        points = read_reference_points(path, "synthetic_reference")

        self.assertEqual(points[0].unit, "cm/s^2")

    def _write_rows(self, rows, header=None):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        path = Path(temporary.name) / "normalized.csv"
        with path.open("w", encoding="utf-8", newline="") as output:
            writer = csv.DictWriter(output, fieldnames=header or HEADER)
            writer.writeheader()
            for row in rows:
                writer.writerow({field: row.get(field, "") for field in writer.fieldnames})
        return path

    def _valid_row(self, **overrides):
        row = {
            "dataset_id": "synthetic_reference",
            "series_id": "series",
            "group_id": "group",
            "case_id": "case",
            "point_id": "point",
            "partition": "CALIBRATION",
            "metric": "stopping_distance_cm",
            "expected": "10.0",
            "unit": "cm",
            "measurement_uncertainty": "0.3",
            "digitization_uncertainty": "0.4",
            "conversion_uncertainty": "0.0",
            "coverage_factor": "2.0",
            "engineering_absolute_tolerance": "0.0",
            "engineering_relative_tolerance": "0.0",
            "source_locator": "synthetic:row-1",
            "pool_applicability": "NOT_APPLICABLE",
        }
        row.update(overrides)
        return row

    def test_reference_point_is_immutable_and_combines_uncertainty_by_rss(self):
        reference = point()

        self.assertEqual(reference.combined_standard_uncertainty, 0.5)
        self.assertEqual(reference.acceptance_half_width, 1.0)
        self.assertEqual(reference.acceptance_interval, (9.0, 11.0))
        with self.assertRaises(FrozenInstanceError):
            reference.expected = 11.0

    def test_acceptance_uses_widest_evidence_backed_half_width(self):
        absolute = replace(point(), engineering_absolute_tolerance=1.25)
        relative = replace(
            point(), expected=100.0, engineering_relative_tolerance=0.02)

        self.assertEqual(absolute.acceptance_half_width, 1.25)
        self.assertEqual(relative.acceptance_half_width, 2.0)

    def test_reads_fixture_in_csv_order(self):
        points = read_reference_points(FIXTURE, "synthetic_reference")

        self.assertEqual(
            [item.point_id for item in points],
            ["stop_distance_cal_01", "stop_distance_holdout_01"],
        )
        self.assertEqual(points[0].partition, "CALIBRATION")
        self.assertEqual(points[1].partition, "HOLDOUT")

    def test_blank_coverage_factor_defaults_to_two(self):
        path = self._write_rows([self._valid_row(coverage_factor="")])

        parsed = read_reference_points(path, "synthetic_reference")

        self.assertEqual(parsed[0].coverage_factor, 2.0)

    def test_rejects_nonfinite_or_negative_uncertainty_values(self):
        invalid = (
            ("expected", "nan"),
            ("expected", "inf"),
            ("measurement_uncertainty", "-0.1"),
            ("digitization_uncertainty", "nan"),
            ("conversion_uncertainty", "inf"),
            ("coverage_factor", "-1"),
            ("engineering_absolute_tolerance", "-0.1"),
            ("engineering_relative_tolerance", "-0.1"),
        )
        for field, value in invalid:
            with self.subTest(field=field, value=value):
                path = self._write_rows([self._valid_row(**{field: value})])
                with self.assertRaises(ValueError):
                    read_reference_points(path, "synthetic_reference")

    def test_rejects_duplicate_point_id_and_dataset_mismatch(self):
        duplicate = self._valid_row()
        duplicate["case_id"] = "other_case"
        duplicate_path = self._write_rows([self._valid_row(), duplicate])
        mismatch_path = self._write_rows([
            self._valid_row(dataset_id="different_dataset")])

        with self.assertRaisesRegex(ValueError, "duplicate point_id"):
            read_reference_points(duplicate_path, "synthetic_reference")
        with self.assertRaisesRegex(ValueError, "dataset_id"):
            read_reference_points(mismatch_path, "synthetic_reference")

    def test_rejects_invalid_ids_partition_units_applicability_and_locator(self):
        invalid = (
            ("series_id", "../series"),
            ("group_id", "group/name"),
            ("case_id", "case name"),
            ("point_id", ""),
            ("metric", "bad metric"),
            ("partition", "TRAIN"),
            ("unit", "miles/hour"),
            ("pool_applicability", "MAYBE"),
            ("source_locator", "   "),
        )
        for field, value in invalid:
            with self.subTest(field=field):
                path = self._write_rows([self._valid_row(**{field: value})])
                with self.assertRaises(ValueError):
                    read_reference_points(path, "synthetic_reference")

    def test_rejects_extra_missing_or_reordered_columns(self):
        column_sets = (
            HEADER + ["extra"],
            HEADER[:-1],
            list(reversed(HEADER)),
        )
        for header in column_sets:
            with self.subTest(header=header):
                row = self._valid_row()
                if "extra" in header:
                    row["extra"] = "value"
                path = self._write_rows([row], header=header)
                with self.assertRaisesRegex(ValueError, "header"):
                    read_reference_points(path, "synthetic_reference")


if __name__ == "__main__":
    unittest.main()
