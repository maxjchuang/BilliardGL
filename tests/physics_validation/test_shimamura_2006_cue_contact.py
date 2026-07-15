import csv
import hashlib
import io
import json
import unittest
from pathlib import Path

from tools.physics_validation.extract_shimamura_2006_cue_contact import (
    DOI, PDF_SHA256, PDF_URL, extract_shimamura_2006,
)


ROOT = Path(__file__).resolve().parents[2]
PACKAGE = ROOT / "tests/physics_validation/reference_data/shimamura_2006_cue_contact"


class ShimamuraExtractionTests(unittest.TestCase):
    def test_package_is_complete_reproducible_and_source_media_is_excluded(self):
        generated = extract_shimamura_2006()
        self.assertEqual(set(generated), {
            "raw_digitized.csv", "normalized.csv", "scalars.csv",
            "extraction.json", "source_access_audit.json", "manifest.json"})
        for name, data in generated.items():
            self.assertEqual((PACKAGE / name).read_bytes(), data, name)
        self.assertFalse((PACKAGE / "source.pdf").exists())
        manifest = json.loads(generated["manifest.json"])
        for item in manifest["files"]:
            self.assertEqual(item["sha256"], "sha256:" + hashlib.sha256(
                generated[item["path"]]).hexdigest())

    def test_source_scalars_and_complete_trace_are_registered(self):
        generated = extract_shimamura_2006()
        manifest = json.loads(generated["manifest.json"])
        audit = json.loads(generated["source_access_audit.json"])
        rows = list(csv.DictReader(io.StringIO(
            generated["raw_digitized.csv"].decode("utf-8"))))
        scalars = {row["quantity"]: row for row in csv.DictReader(io.StringIO(
            generated["scalars.csv"].decode("utf-8")))}
        self.assertEqual(manifest["source"]["doi"], DOI)
        self.assertEqual(audit["url"], PDF_URL)
        self.assertEqual(audit["pdf_sha256"], "sha256:" + PDF_SHA256)
        self.assertEqual(audit["license_status"],
                         "free-access-no-redistribution-grant-recorded")
        self.assertTrue(manifest["evidence"]["centered_impact"])
        self.assertEqual(scalars["cue_speed"]["value"], "3.0")
        self.assertEqual(scalars["analysis_time_step"]["value"], "0.00005")
        self.assertEqual(scalars["contact_duration"]["value"], "0.001000")
        self.assertEqual(scalars["contact_duration"]["uncertainty"], "0.000025")
        self.assertEqual(len(rows), 21)
        self.assertEqual(rows[0]["time_s"], "0.000000")
        self.assertEqual(rows[-1]["time_s"], "0.001000")
        self.assertEqual(rows[0]["experimental_strain"], "0.0000")
        self.assertEqual(rows[-1]["experimental_strain"], "0.0000")

    def test_lifecycle_marks_data_as_spent_calibration(self):
        status = json.loads((ROOT /
            "tests/physics_validation/validation_data_status.json").read_text())
        record = next(item for item in status["datasets"]
                      if item["dataset_id"] == "shimamura_2006_cue_contact")
        self.assertEqual(record, {
            "calibration_status": "calibration",
            "dataset_id": "shimamura_2006_cue_contact",
            "dataset_version": "1.0.0",
            "holdout_status": "spent",
        })


if __name__ == "__main__":
    unittest.main()
