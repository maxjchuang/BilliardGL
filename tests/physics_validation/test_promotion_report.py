import json
import tempfile
import unittest
from pathlib import Path

from tools.physics_validation.promotion_report import build_report, markdown, write_report
from tools.physics_validation.promotion import validate_release_manifest


ROOT = Path(__file__).resolve().parents[2]
RELEASE = ROOT / "physics_models/promotion/phase3_release_v1.json"
REPORT = ROOT / "physics_models/promotion/phase3_promotion_report_v1.json"
MARKDOWN = ROOT / "docs/phase3-physics-promotion-report.md"


class PromotionReportTests(unittest.TestCase):
    def test_committed_reports_reconstruct_byte_for_byte(self):
        with tempfile.TemporaryDirectory() as directory:
            json_path = Path(directory) / "report.json"
            markdown_path = Path(directory) / "report.md"
            write_report(RELEASE, ROOT, json_path, markdown_path)
            self.assertEqual(json_path.read_bytes(), REPORT.read_bytes())
            self.assertEqual(markdown_path.read_bytes(), MARKDOWN.read_bytes())

    def test_report_preserves_limitations_and_hard_gates(self):
        report = build_report(RELEASE, ROOT)
        self.assertEqual(report["release"]["status"], "PASSED_WITH_DECLARED_LIMITATIONS")
        self.assertGreaterEqual(len(report["limitations"]), 6)
        self.assertEqual(report["hard_gates"]["unexplained_regressions"], 0)
        self.assertEqual(report["stress"]["duplicate_contacts"], 0)
        self.assertEqual(validate_release_manifest(RELEASE, ROOT), [])


if __name__ == "__main__":
    unittest.main()
