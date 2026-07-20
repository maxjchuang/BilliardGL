import json
import tempfile
import unittest
from pathlib import Path

from tools.physics_validation.admission_report import write_admission_report


ROOT = Path(__file__).resolve().parents[2]
CROSS_PACKAGE = ROOT / (
    "tests/physics_validation/reference_data/cross_2023_cue_impact")
NUMERIC_PACKAGE = ROOT / "tests/physics_validation/fixtures/reference_package_v1"


class AdmissionReportTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.executable = self.root / "Billiards"
        self.executable.write_bytes(b"synthetic executable")

    def test_zero_data_package_writes_only_accounted_limitations(self):
        output = self.root / "report"

        self.assertEqual(
            write_admission_report(self.executable, CROSS_PACKAGE, output), 0)

        report = json.loads(
            (output / "reference_report.json").read_text(encoding="utf-8"))
        accounting = report["accounting"]
        self.assertEqual(len(accounting["known_limitations"]), 3)
        self.assertEqual(accounting["known_model_mismatches"], [])
        self.assertEqual(accounting["unallowlistable_failures"], [])
        self.assertEqual(report["metadata"]["scenarios"], {})

    def test_numeric_package_is_rejected_before_report_generation(self):
        output = self.root / "report"

        with self.assertRaisesRegex(ValueError, "zero normalized points"):
            write_admission_report(
                self.executable, NUMERIC_PACKAGE, output)

        self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
