import json
import tempfile
import unittest
from pathlib import Path

from tools.physics_validation.analyzer import Failure, KnownFailureMatch, ScenarioResult
from tools.physics_validation.report import write_reports


class ReportTests(unittest.TestCase):
    def test_report_keeps_known_failures_visibly_failed_and_sorted(self):
        results = [
            ScenarioResult("z_pass", True, "C", {"finite_state": True}, ()),
            ScenarioResult("a_known", False, "C", {}, (
                Failure("NUMERICAL_FAILURE", "missed_collision", "missed", False, True),)),
            ScenarioResult("m_reference", False, "A", {}, (
                Failure("REFERENCE_LIMITATION", "reference_data", "missing", True, None),)),
        ]
        known_tuple = ("a_known", "NUMERICAL_FAILURE", "missed_collision")
        matching = KnownFailureMatch({known_tuple}, set(), set())

        with tempfile.TemporaryDirectory() as directory:
            json_path, markdown_path = write_reports(
                results, matching, Path(directory), {"executable": "/tmp/Billiards"})
            payload = json.loads(json_path.read_text(encoding="utf-8"))
            markdown = markdown_path.read_text(encoding="utf-8")

        self.assertEqual(payload["summary"], {
            "passed": 1, "failed_known": 1, "failed_new": 0, "reference_limited": 1})
        self.assertLess(markdown.index("a_known"), markdown.index("m_reference"))
        self.assertLess(markdown.index("m_reference"), markdown.index("z_pass"))
        self.assertIn("FAILED (KNOWN)", markdown)
        self.assertIn("REFERENCE LIMITED", markdown)
        self.assertNotIn("a_known | PASSED", markdown)


if __name__ == "__main__":
    unittest.main()
