import csv
import hashlib
import json
import unittest
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
PACKAGE = (
    REPO_ROOT
    / "tests/physics_validation/reference_data/mathavan_2009_high_speed"
)


class Mathavan2009AdmissionTests(unittest.TestCase):
    def _json(self, name):
        return json.loads((PACKAGE / name).read_text(encoding="utf-8"))

    def _csv(self, name):
        with (PACKAGE / name).open("r", encoding="utf-8", newline="") as source:
            return list(csv.DictReader(source))

    def test_manifest_records_audited_source_and_apparatus(self):
        manifest = self._json("manifest.json")

        self.assertEqual(manifest["dataset_id"], "mathavan_2009_high_speed")
        self.assertEqual(manifest["dataset_version"], "1.0.0")
        self.assertEqual(manifest["adapter_id"], "mathavan_2009_v1")
        self.assertEqual(manifest["source"]["doi"], "10.1119/1.3157159")
        self.assertEqual(manifest["source"]["journal_pages"], "788-794")
        self.assertEqual(manifest["source"]["figure_locators"], ["Fig. 6", "Fig. 9", "Table I"])
        self.assertEqual(
            manifest["acquisition"]["url"],
            "https://doi.org/10.1119/1.3157159",
        )
        self.assertEqual(
            manifest["acquisition"]["source_sha256"],
            "sha256:bc3dd86cbb214080f57503301776e1b90a34c61862cee81717f67958729c2659",
        )
        self.assertFalse(manifest["acquisition"]["redistributable"])
        self.assertEqual(manifest["apparatus"]["ball_diameter_mm"], 52.4)
        self.assertEqual(manifest["apparatus"]["camera_spatial_resolution_mm"], 1.0)
        self.assertEqual(manifest["apparatus"]["pool_applicability"], "TREND_ONLY")
        self.assertTrue(manifest["extraction_review"]["operator"])
        self.assertTrue(manifest["extraction_review"]["reviewed_by"])

    def test_raw_inventory_preserves_reported_counts_without_inventing_overlap(self):
        rows = self._csv("raw_extracted.csv")
        ranges = [row for row in rows if row["record_type"] == "reported_range"]
        cushion = [row for row in rows if row["record_type"] == "figure_marker"]
        collisions = [row for row in rows if row["record_type"] == "table_shot"]

        self.assertEqual(len(ranges), 2)
        self.assertEqual(
            [(row["y_lower"], row["y_upper"], row["y_unit"]) for row in ranges],
            [("0.124", "0.126", "m/s^2"), ("1.75", "2.40", "m/s^2")],
        )
        self.assertEqual(len(cushion), 31)
        self.assertEqual(
            {row["status"] for row in cushion},
            {"ADMITTED", "REFERENCE_LIMITATION"},
        )
        self.assertEqual(sum(row["status"] == "ADMITTED" for row in cushion), 8)
        self.assertEqual(
            sum(row["status"] == "REFERENCE_LIMITATION" for row in cushion), 23)
        for row in cushion:
            if row["status"] == "REFERENCE_LIMITATION":
                self.assertEqual(row["x_value"], "")
                self.assertEqual(row["y_value"], "")

        self.assertEqual(len(collisions), 5)
        measured = [
            (
                row["x_value"],
                row["cut_angle_degrees"],
                row["y_value"],
                row["secondary_value"],
            )
            for row in collisions
        ]
        self.assertEqual(
            measured,
            [
                ("1.539", "33.83", "0.816", "0.836"),
                ("1.032", "26.36", "0.520", "0.629"),
                ("1.364", "40.52", "0.925", "0.700"),
                ("1.731", "46.50", "1.275", "0.787"),
                ("0.942", "18.05", "0.365", "0.581"),
            ],
        )

    def test_admitted_figure_points_have_two_independent_extraction_passes(self):
        raw = self._csv("raw_extracted.csv")
        admitted = {
            row["point_id"]
            for row in raw
            if row["record_type"] == "figure_marker" and row["status"] == "ADMITTED"
        }
        digitized = self._csv("digitization.csv")

        self.assertEqual(len(digitized), 2 * len(admitted))
        self.assertEqual({row["point_id"] for row in digitized}, admitted)
        self.assertEqual({row["extraction_pass"] for row in digitized}, {"manual", "color_centroid"})
        required = {
            "figure_id",
            "series_id",
            "point_id",
            "pixel_x",
            "pixel_y",
            "axis_x0_pixel",
            "axis_x4_pixel",
            "axis_y0_pixel",
            "axis_y3_5_pixel",
            "converted_x_m_s",
            "converted_y_m_s",
            "extraction_pass",
        }
        self.assertTrue(required <= set(digitized[0]))
        for point_id in admitted:
            self.assertEqual(
                {row["extraction_pass"] for row in digitized if row["point_id"] == point_id},
                {"manual", "color_centroid"},
            )

    def test_overlapped_markers_are_an_explicit_reference_limitation(self):
        limitations = self._json("expected_reference_limitations.json")["failures"]
        entry = next(
            item for item in limitations
            if item["case_id"] == "fig9_unresolved_markers"
        )
        self.assertEqual(entry["code"], "REFERENCE_LIMITATION")
        self.assertEqual(entry["metric"], "cushion_rebound_speed_cm_s")
        self.assertIn("23", entry["missing_evidence"])
        self.assertIn("author", entry["resolution_condition"].lower())


class Mathavan2009NormalizationTests(unittest.TestCase):
    def test_extraction_metadata_locks_inputs_script_source_and_output(self):
        extraction = json.loads(
            (PACKAGE / "extraction.json").read_text(encoding="utf-8"))

        self.assertEqual(extraction["schema_version"], 2)
        self.assertEqual(
            extraction["uncertainty_interpretation"],
            "reported_bounded_range",
        )
        self.assertEqual(
            extraction["source_sha256"],
            "sha256:bc3dd86cbb214080f57503301776e1b90a34c61862cee81717f67958729c2659",
        )
        self.assertEqual(
            extraction["script"]["module"],
            "tools.physics_validation.extract_mathavan_2009",
        )
        for item in extraction["inputs"]:
            digest = hashlib.sha256((PACKAGE / {
                "raw_extracted": "raw_extracted.csv",
                "digitization": "digitization.csv",
            }[item["file_id"]]).read_bytes()).hexdigest()
            self.assertEqual(item["sha256"], "sha256:" + digest)
        normalized_digest = hashlib.sha256(
            (PACKAGE / "normalized.csv").read_bytes()).hexdigest()
        self.assertEqual(
            extraction["output_sha256"], "sha256:" + normalized_digest)

    def test_normalizes_units_uncertainty_and_partitions_without_dropping_data(self):
        from tools.physics_validation.extract_mathavan_2009 import normalize_rows

        rows = normalize_rows(
            PACKAGE / "raw_extracted.csv",
            PACKAGE / "digitization.csv",
            PACKAGE / "extraction.json",
        )

        self.assertEqual(len(rows), 20)
        self.assertEqual(
            [(row["series_id"], row["group_id"], row["case_id"], row["point_id"])
             for row in rows],
            sorted(
                (row["series_id"], row["group_id"], row["case_id"], row["point_id"])
                for row in rows
            ),
        )
        by_id = {row["point_id"]: row for row in rows}
        rolling = by_id["rolling_deceleration_range"]
        self.assertEqual(rolling["expected"], "12.5")
        self.assertEqual(rolling["unit"], "cm/s^2")
        self.assertEqual(rolling["measurement_uncertainty"], "0.1")
        self.assertEqual(rolling["coverage_factor"], "1")
        self.assertEqual(rolling["partition"], "CALIBRATION")

        cushion = by_id["fig9_visible_01"]
        self.assertEqual(cushion["expected"], "21.7341")
        self.assertEqual(cushion["unit"], "cm/s")
        self.assertEqual(cushion["digitization_uncertainty"], "0.22945")
        self.assertEqual(cushion["partition"], "HOLDOUT")

        cue = by_id["table1_shot_01_cue_speed"]
        obj = by_id["table1_shot_01_object_speed"]
        self.assertEqual((cue["expected"], obj["expected"]), ("81.6", "83.6"))
        self.assertEqual((cue["partition"], obj["partition"]), ("CALIBRATION", "CALIBRATION"))
        self.assertFalse(any("unresolved" in row["point_id"] for row in rows))

    def test_write_normalized_is_byte_deterministic(self):
        from tools.physics_validation.extract_mathavan_2009 import write_normalized

        first = write_normalized(PACKAGE)
        second = write_normalized(PACKAGE)

        self.assertEqual(first, second)
        self.assertEqual(first, (PACKAGE / "normalized.csv").read_bytes())

    def test_figure_values_are_audited_against_pixel_reconstruction(self):
        with (PACKAGE / "digitization.csv").open(
                "r", encoding="utf-8", newline="") as source:
            rows = list(csv.DictReader(source))
        rows[0]["converted_y_m_s"] = "999"
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "digitization.csv"
            with path.open("w", encoding="utf-8", newline="") as output:
                writer = csv.DictWriter(output, fieldnames=rows[0])
                writer.writeheader()
                writer.writerows(rows)
            from tools.physics_validation.extract_mathavan_2009 import normalize_rows
            with self.assertRaisesRegex(ValueError, "converted_y_m_s"):
                normalize_rows(PACKAGE / "raw_extracted.csv", path, PACKAGE / "extraction.json")


if __name__ == "__main__":
    unittest.main()
