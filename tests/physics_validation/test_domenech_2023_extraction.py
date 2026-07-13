import csv
import json
import unittest
from pathlib import Path

from tools.physics_validation.extract_domenech_2023 import (
    normalize_rows,
    write_normalized,
)


PACKAGE = (
    Path(__file__).parent
    / "reference_data/domenech_2023_ball_collision"
)


class Domenech2023AdmissionTests(unittest.TestCase):
    def _json(self, name):
        return json.loads((PACKAGE / name).read_text(encoding="utf-8"))

    def _csv(self, name):
        with (PACKAGE / name).open("r", encoding="utf-8", newline="") as source:
            return list(csv.DictReader(source))

    def test_manifest_locks_license_source_assets_and_request_status(self):
        manifest = self._json("manifest.json")

        self.assertEqual(manifest["dataset_id"], "domenech_2023_ball_collision")
        self.assertEqual(manifest["dataset_version"], "1.0.0")
        self.assertEqual(manifest["adapter_id"], "domenech_2023_v1")
        self.assertEqual(manifest["source"]["doi"], "10.1016/j.mechrescom.2023.104149")
        self.assertEqual(
            manifest["acquisition"]["license_uri"],
            "https://creativecommons.org/licenses/by-nc-nd/4.0/",
        )
        self.assertEqual(
            manifest["acquisition"]["figure_bundle_sha256"],
            "sha256:0a216010cf12f84dc2594ab22c7ae9dc96490a958ce8b959b9a13e2160fca1e7",
        )
        self.assertEqual(
            manifest["acquisition"]["version_record_pdf_status"],
            "BLOCKED_BY_PUBLISHER_AUTOMATION_CHALLENGE",
        )
        self.assertEqual(
            manifest["acquisition"]["author_data_request_status"],
            "NOT_SENT_USER_AUTHORIZATION_REQUIRED",
        )
        self.assertFalse(manifest["acquisition"]["source_media_committed"])

    def test_manifest_records_all_apparatus_without_material_conflation(self):
        apparatus = self._json("manifest.json")["apparatus"]

        self.assertEqual(apparatus["support_surface"], "PVC laboratory bench")
        self.assertEqual(apparatus["camera_height_cm"], 75.0)
        self.assertEqual(apparatus["launch_speed_m_s"], 0.80)
        self.assertEqual(apparatus["launch_speed_bound_m_s"], 0.05)
        self.assertEqual(apparatus["track_end_before_contact_cm"], 50.0)
        self.assertEqual(apparatus["materials"], {
            "billiard": {"diameter_cm": 6.1, "mass_g": 205.0},
            "brass": {"diameter_cm": 2.5, "mass_g": 68.20},
            "rubber": {"diameter_cm": 4.6, "mass_g": 46.40},
            "steel": {"diameter_cm": 2.5, "mass_g": 70.30},
        })

    def test_inventory_and_raw_counts_cover_each_experimental_marker_series(self):
        manifest = self._json("manifest.json")
        inventory = manifest["evidence"]["inventory"]
        rows = self._csv("raw_extracted.csv")

        self.assertEqual(
            {(item["figure"], item["series_id"]) for item in inventory},
            {
                ("Fig. 5", "billiard_delta2"),
                ("Fig. 6", "billiard_alpha1"),
                ("Fig. 6", "brass_alpha1"),
                ("Fig. 7", "steel_alpha1"),
                ("Fig. 7", "steel_beta1"),
                ("Fig. 8", "rubber_delta2"),
                ("Fig. 9", "rubber_lambda2"),
            },
        )
        for item in inventory:
            self.assertTrue(item["section_locator"])
            self.assertTrue(item["axis_definitions"])
            self.assertTrue(item["marker_legend"])
            self.assertEqual(item["value_origin"], "dual_digitization")
            series_rows = [row for row in rows if row["series_id"] == item["series_id"]]
            admitted = [row for row in series_rows if row["status"] == "ADMITTED"]
            self.assertEqual(len(admitted), item["admitted_marker_count"])

    def test_normalization_preserves_every_admitted_point_and_uncertainty_source(self):
        rows = normalize_rows(
            PACKAGE / "raw_extracted.csv",
            PACKAGE / "digitization.csv",
            PACKAGE / "extraction.json",
        )

        self.assertEqual(len(rows), 214)
        self.assertEqual({row["unit"] for row in rows}, {"degree"})
        self.assertEqual(
            {row["pool_applicability"] for row in rows},
            {"CONVERTED", "TREND_ONLY"},
        )
        self.assertTrue(all(float(row["measurement_uncertainty"]) == 0 for row in rows))
        self.assertTrue(all(float(row["digitization_uncertainty"]) >= 0 for row in rows))
        self.assertTrue(any(float(row["digitization_uncertainty"]) > 0 for row in rows))
        self.assertTrue(all(float(row["conversion_uncertainty"]) > 0 for row in rows))

    def test_normalized_bytes_are_stable_and_match_committed_output(self):
        first = write_normalized(PACKAGE)
        second = write_normalized(PACKAGE)

        self.assertEqual(first, second)
        self.assertEqual(first, (PACKAGE / "normalized.csv").read_bytes())

    def test_theory_curves_and_unfinished_admission_steps_are_explicit_limitations(self):
        evidence = self._json("manifest.json")["evidence"]
        limitations = self._json("expected_reference_limitations.json")["failures"]

        self.assertEqual(
            set(evidence["excluded_evidence"]),
            {"IFR theoretical curves", "fitted restitution/friction coefficients"},
        )
        self.assertTrue({
            "author_data_request_pending",
            "version_record_pdf_audit_pending",
        } <= {item["case_id"] for item in limitations})


if __name__ == "__main__":
    unittest.main()
