import subprocess
import sys
import unittest
from pathlib import Path

from tools.physics_validation.extract_cross_2023 import normalize_rows, write_normalized
from tools.physics_validation.reference_package import load_reference_package
from tools.physics_validation.reference_point import read_reference_points


PACKAGE = Path(__file__).parent / "reference_data/cross_2023_cue_impact"


class Cross2023ExtractionTests(unittest.TestCase):
    def test_admission_record_is_exact_and_does_not_invent_numbers(self):
        package = load_reference_package(PACKAGE)
        self.assertEqual(package.manifest["dataset_id"], "cross_2023_cue_impact")
        self.assertEqual(package.manifest["dataset_version"], "1.0.0-admission-blocked")
        self.assertEqual(package.manifest["adapter_id"], "cross_2023_v1")
        source = package.manifest["source"]
        self.assertEqual(source["doi"], "10.1177/17543371231184011")
        self.assertEqual(source["first_published_online"], "2023-06-29")
        self.assertEqual(source["journal_pages"], "239(4):647-651")
        self.assertEqual(package.manifest["evidence"]["numeric_admission"], "NONE")
        self.assertEqual(read_reference_points(
            package.files["normalized"], package.manifest["dataset_id"]), ())

    def test_extraction_is_byte_identical_and_admission_gated(self):
        self.assertEqual(write_normalized(PACKAGE), (PACKAGE / "normalized.csv").read_bytes())
        self.assertEqual(normalize_rows(
            PACKAGE / "raw_extracted.csv", PACKAGE / "digitization.csv",
            PACKAGE / "extraction.json"), ())
        result = subprocess.run([
            sys.executable, "-m", "tools.physics_validation.extract_cross_2023",
            "--package", str(PACKAGE), "--check",
        ], cwd=Path(__file__).parents[2], check=False)
        self.assertEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
