import json
import tempfile
import unittest
from dataclasses import replace
from pathlib import Path

from tools.physics_validation.reference_point import read_reference_points
from tools.physics_validation.reference_split import (
    load_reference_split,
)


FIXTURE_ROOT = Path(__file__).parent / "fixtures/reference_package_v1"


class ReferenceSplitTests(unittest.TestCase):
    def setUp(self):
        self.points = read_reference_points(
            FIXTURE_ROOT / "normalized.csv", "synthetic_reference")
        self.document = json.loads(
            (FIXTURE_ROOT / "split.json").read_text(encoding="utf-8"))

    def _write_split(self, document=None):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        path = Path(temporary.name) / "split.json"
        path.write_text(
            json.dumps(document or self.document, ensure_ascii=False, indent=2,
                       sort_keys=True, allow_nan=False) + "\n",
            encoding="utf-8")
        return path

    def test_loads_exhaustive_split_and_assigns_each_point(self):
        split = load_reference_split(
            FIXTURE_ROOT / "split.json", self.points,
            "synthetic_reference", "1.0.0")

        self.assertEqual(split.dataset_id, "synthetic_reference")
        self.assertEqual(split.dataset_version, "1.0.0")
        self.assertEqual(split.calibration_groups, frozenset({"series_calibration"}))
        self.assertEqual(split.holdout_groups, frozenset({"series_holdout"}))
        self.assertEqual(split.partition_for(self.points[0]), "CALIBRATION")
        self.assertEqual(split.partition_for(self.points[1]), "HOLDOUT")

    def test_rejects_duplicate_or_overlapping_groups(self):
        documents = []
        duplicate = dict(self.document)
        duplicate["calibration_groups"] = ["series_calibration", "series_calibration"]
        documents.append(duplicate)
        overlap = dict(self.document)
        overlap["holdout_groups"] = ["series_calibration", "series_holdout"]
        documents.append(overlap)

        for document in documents:
            with self.subTest(document=document):
                with self.assertRaises(ValueError):
                    load_reference_split(
                        self._write_split(document), self.points,
                        "synthetic_reference", "1.0.0")

    def test_rejects_missing_or_unknown_groups(self):
        missing = dict(self.document)
        missing["holdout_groups"] = []
        unknown = dict(self.document)
        unknown["holdout_groups"] = ["series_holdout", "unknown_group"]

        for document in (missing, unknown):
            with self.subTest(document=document):
                with self.assertRaises(ValueError):
                    load_reference_split(
                        self._write_split(document), self.points,
                        "synthetic_reference", "1.0.0")

    def test_rejects_a_case_that_spans_groups(self):
        points = (
            self.points[0],
            replace(self.points[1], case_id=self.points[0].case_id),
        )

        with self.assertRaisesRegex(ValueError, "case_id"):
            load_reference_split(
                self._write_split(), points, "synthetic_reference", "1.0.0")

    def test_rejects_a_group_that_spans_normalized_partitions(self):
        points = (
            self.points[0],
            replace(
                self.points[1], group_id=self.points[0].group_id,
                case_id="other_case", partition="HOLDOUT"),
        )
        document = dict(self.document)
        document["holdout_groups"] = []

        with self.assertRaisesRegex(ValueError, "group_id"):
            load_reference_split(
                self._write_split(document), points,
                "synthetic_reference", "1.0.0")

    def test_rejects_normalized_partition_disagreement(self):
        points = (replace(self.points[0], partition="HOLDOUT"), self.points[1])

        with self.assertRaisesRegex(ValueError, "partition"):
            load_reference_split(
                self._write_split(), points, "synthetic_reference", "1.0.0")

    def test_rejects_wrong_dataset_or_version(self):
        for field, value in (("dataset_id", "other_dataset"),
                             ("dataset_version", "2.0.0")):
            document = dict(self.document)
            document[field] = value
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, field):
                load_reference_split(
                    self._write_split(document), self.points,
                    "synthetic_reference", "1.0.0")

    def test_rejects_unsafe_ids_and_non_schema_keys(self):
        unsafe_group = dict(self.document)
        unsafe_group["calibration_groups"] = ["../group"]
        extra_key = dict(self.document)
        extra_key["override"] = "HOLDOUT"

        for document in (unsafe_group, extra_key):
            with self.subTest(document=document), self.assertRaises(ValueError):
                load_reference_split(
                    self._write_split(document), self.points,
                    "synthetic_reference", "1.0.0")

    def test_partition_for_rejects_unregistered_point(self):
        split = load_reference_split(
            FIXTURE_ROOT / "split.json", self.points,
            "synthetic_reference", "1.0.0")
        unknown = replace(self.points[0], group_id="unregistered_group")

        with self.assertRaisesRegex(ValueError, "exactly one"):
            split.partition_for(unknown)


if __name__ == "__main__":
    unittest.main()
