import json
import tempfile
import unittest
from pathlib import Path

from tools.physics_validation.analyzer import Failure, KnownFailureMatch, ScenarioResult
from tools.physics_validation.report import write_reports
from tools.physics_validation.run import (
    _permuted_scenario,
    _scenario_paths,
    _validated_scenario_id,
)


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

    def test_single_scenario_file_is_accepted(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "one.json"
            path.write_text("{}", encoding="utf-8")
            self.assertEqual(_scenario_paths(path), [path])

    def test_scenario_directory_is_sorted(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "z.json").write_text("{}", encoding="utf-8")
            (root / "a.json").write_text("{}", encoding="utf-8")
            self.assertEqual(_scenario_paths(root), [root / "a.json", root / "z.json"])

    def test_unsafe_scenario_ids_are_rejected(self):
        for scenario_id in ("../escape", "nested/name", "", "."):
            with self.subTest(scenario_id=scenario_id):
                with self.assertRaises(ValueError):
                    _validated_scenario_id(scenario_id)

    def test_permuted_scenario_reassigns_ball_indices(self):
        source = {
            "id": "case",
            "balls": [{"index": 0, "velocity_cm_s": [1, 0, 0]},
                      {"index": 2, "velocity_cm_s": [0, 0, 0]}],
            "expectations": [{
                "metric": "permutation_invariance",
                "value": {"index_map": {"0": 2, "2": 0}},
            }],
        }
        result = _permuted_scenario(source)
        self.assertEqual(result["id"], "case__permuted")
        self.assertEqual([ball["index"] for ball in result["balls"]], [2, 0])
        self.assertEqual(result["expectations"], source["expectations"])

    def test_report_contains_per_scenario_reproduction_metadata(self):
        result = ScenarioResult("case", True, "C", {"finite_state": True}, ())
        matching = KnownFailureMatch(set(), set(), set())
        metadata = {
            "build_id": "abc123",
            "executable": "/tmp/Billiards",
            "scenarios": {
                "case": {
                    "source_path": "/tmp/case.json",
                    "trace_path": "/tmp/traces/case.json",
                    "expectations": [{"metric": "finite_state", "absolute_tolerance": 0.0}],
                    "evidence": {"grade": "C", "source": "analytic"},
                }
            },
        }
        with tempfile.TemporaryDirectory() as directory:
            json_path, _ = write_reports([result], matching, Path(directory), metadata)
            payload = json.loads(json_path.read_text(encoding="utf-8"))
        self.assertEqual(payload["metadata"]["build_id"], "abc123")
        scenario_payload = payload["scenarios"][0]
        self.assertEqual(scenario_payload["source_path"], "/tmp/case.json")
        self.assertEqual(scenario_payload["trace_path"], "/tmp/traces/case.json")
        self.assertEqual(scenario_payload["expectations"][0]["absolute_tolerance"], 0.0)


if __name__ == "__main__":
    unittest.main()
