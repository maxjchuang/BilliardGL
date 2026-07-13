import csv
import json
import unittest
from pathlib import Path

from tools.physics_validation.extract_mathavan_2010 import normalize_rows, write_normalized


PACKAGE = Path(__file__).parent / "reference_data/mathavan_2010_cushion"


class Mathavan2010ExtractionTests(unittest.TestCase):
    def _json(self, name):
        return json.loads((PACKAGE / name).read_text(encoding="utf-8"))

    def _csv(self, name):
        with (PACKAGE / name).open("r", encoding="utf-8", newline="") as source:
            return list(csv.DictReader(source))

    def test_manifest_separates_experiment_from_theory_and_locks_source(self):
        manifest = self._json("manifest.json")
        self.assertEqual(manifest["dataset_id"], "mathavan_2010_cushion")
        self.assertEqual(manifest["dataset_version"], "1.0.0")
        self.assertEqual(manifest["adapter_id"], "mathavan_2010_v1")
        self.assertEqual(manifest["source"]["doi"], "10.1243/09544062JMES1964")
        self.assertEqual(manifest["acquisition"]["repository_url"], "https://dspace.lboro.ac.uk/2134/15087")
        self.assertEqual(manifest["acquisition"]["source_sha256"], "sha256:8c4ad418408b1e1728577632a99ba4789320a91b4b13d95ddd0a231c5fb2f093")
        self.assertFalse(manifest["acquisition"]["source_media_committed"])
        self.assertEqual(manifest["evidence"]["experimental_marker_count"], 19)
        self.assertEqual(
            {item["figure"]: item["classification"] for item in manifest["evidence"]["inventory"]},
            {"Fig. 7": "EXPERIMENT_PLUS_THEORY", "Fig. 8": "THEORY_ONLY", "Fig. 9": "THEORY_ONLY", "Fig. 10": "THEORY_ONLY"},
        )

    def test_every_experimental_marker_has_two_coordinate_passes(self):
        raw = self._csv("raw_extracted.csv")
        digitization = self._csv("digitization.csv")
        self.assertEqual(len(raw), 19)
        self.assertTrue(all(row["evidence_kind"] == "experimental_marker" for row in raw))
        self.assertEqual(len(digitization), 38)
        for row in raw:
            passes = [item for item in digitization if item["point_id"] == row["point_id"]]
            self.assertEqual({item["pass_id"] for item in passes}, {"1", "2"})

    def test_normalization_preserves_domain_flags_split_and_exact_bytes(self):
        rows = normalize_rows(PACKAGE / "raw_extracted.csv", PACKAGE / "digitization.csv", PACKAGE / "extraction.json")
        self.assertEqual(len(rows), 19)
        self.assertEqual({row["unit"] for row in rows}, {"cm/s"})
        self.assertEqual({row["pool_applicability"] for row in rows}, {"TREND_ONLY"})
        raw = {row["point_id"]: row for row in self._csv("raw_extracted.csv")}
        self.assertTrue(any(row["fit_subset"] == "true" for row in raw.values()))
        self.assertTrue(any(row["rigid_cushion_domain"] == "false" for row in raw.values()))
        generated = write_normalized(PACKAGE)
        self.assertEqual(generated, write_normalized(PACKAGE))
        self.assertEqual(generated, (PACKAGE / "normalized.csv").read_bytes())

    def test_limitations_and_excluded_model_output_are_explicit(self):
        evidence = self._json("manifest.json")["evidence"]
        limitations = self._json("expected_reference_limitations.json")["failures"]
        self.assertEqual(set(evidence["excluded_model_output"]), {"Fig. 7 numerical squares", "Figs. 8-10 numerical curves"})
        self.assertTrue({
            "oblique_experimental_rebound_angle_unavailable",
            "experimental_spin_change_unavailable",
            "snooker_cushion_to_pool_material_conversion_missing",
            "rigid_cushion_domain_warning",
        } <= {item["case_id"] for item in limitations})


if __name__ == "__main__":
    unittest.main()
