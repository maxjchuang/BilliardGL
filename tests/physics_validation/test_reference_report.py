import csv
import json
import tempfile
import unittest
from dataclasses import replace
from pathlib import Path

from tools.physics_validation.analyzer import Failure, ScenarioResult
from tools.physics_validation.reference_accounting import (
    ReferenceAccounting,
    ReferenceFailureKey,
)
from tools.physics_validation.reference_adapter import default_reference_registry
from tools.physics_validation.reference_package import load_reference_package
from tools.physics_validation.reference_point import read_reference_points
from tools.physics_validation.reference_report import write_reference_reports
from tools.physics_validation.reference_split import load_reference_split


FIXTURE_ROOT = Path(__file__).parent / "fixtures/reference_package_v1"


def accounting(**overrides):
    values = {
        "known_model_mismatches": frozenset(),
        "new_model_mismatches": frozenset(),
        "missing_model_mismatches": frozenset(),
        "known_limitations": frozenset(),
        "new_limitations": frozenset(),
        "missing_limitations": frozenset(),
        "unallowlistable_failures": frozenset(),
    }
    values.update(overrides)
    return ReferenceAccounting(**values)


def result_for(case, prediction=None, code=None):
    metric = case.points[0].metric
    failures = () if code is None else (
        Failure(code, metric, "comparison failed", case.points[0].expected, prediction),)
    return ScenarioResult(
        json.loads(case.scenario_json)["id"],
        not failures,
        "C",
        {} if prediction is None else {metric: prediction},
        failures,
    )


class ReferenceReportTests(unittest.TestCase):
    def setUp(self):
        package = load_reference_package(FIXTURE_ROOT)
        points = read_reference_points(
            package.files["normalized"], package.manifest["dataset_id"])
        split = load_reference_split(
            package.files["split"], points,
            package.manifest["dataset_id"], package.manifest["dataset_version"])
        self.cases = default_reference_registry().adapt(package, split, points)
        self.metadata = {
            "build_id": "sha256:" + "a" * 64,
            "executable": "/tmp/Billiards",
            "scenarios": {
                json.loads(case.scenario_json)["id"]: {
                    "trace_path": f"/tmp/traces/{case.case_id}.json",
                    "replay_command": (
                        "python3 -m tools.physics_validation.reference_run "
                        f"--case {case.case_id}"
                    ),
                }
                for case in self.cases
            },
        }

    def _write(self, cases, results, reconciliation):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        return write_reference_reports(
            cases, results, reconciliation, Path(temporary.name), self.metadata)

    def test_json_and_csv_contain_complete_partitioned_point_records(self):
        known_key = ReferenceFailureKey(
            "synthetic_reference", "free_roll_holdout",
            "MODEL_MISMATCH", "stopping_distance_cm")
        paths = self._write(
            self.cases,
            [
                result_for(self.cases[0], self.cases[0].points[0].expected),
                result_for(
                    self.cases[1], self.cases[1].points[0].expected + 1.0,
                    "MODEL_MISMATCH"),
            ],
            accounting(known_model_mismatches=frozenset({known_key})),
        )
        json_path, csv_path, markdown_path = paths
        payload = json.loads(json_path.read_text(encoding="utf-8"))
        with csv_path.open("r", encoding="utf-8", newline="") as source:
            csv_rows = list(csv.DictReader(source))

        calibration = payload["partitions"]["CALIBRATION"]
        holdout = payload["partitions"]["HOLDOUT"]
        self.assertEqual(calibration["summary"]["points"], 1)
        self.assertEqual(calibration["summary"]["passed"], 1)
        self.assertEqual(holdout["summary"]["points"], 1)
        self.assertEqual(holdout["summary"]["passed"], 0)
        self.assertEqual(calibration["series"][0], {
            "count": 1,
            "maximum_absolute_error": 0.0,
            "pass_rate": 1.0,
            "rmse": 0.0,
            "series_id": "synthetic_free_roll",
        })
        row = holdout["points"][0]
        self.assertEqual(row["prediction"], self.cases[1].points[0].expected + 1.0)
        self.assertEqual(row["experimental_value"], self.cases[1].points[0].expected)
        self.assertEqual(row["signed_error"], 1.0)
        self.assertEqual(
            row["acceptance_interval"],
            list(self.cases[1].points[0].acceptance_interval))
        self.assertEqual(row["status"], "MODEL_MISMATCH_KNOWN")
        self.assertEqual(row["dataset_version"], "1.0.0")
        self.assertEqual(row["source_locator"], "synthetic:holdout:row-1")
        self.assertEqual(row["trace_path"], "/tmp/traces/free_roll_holdout.json")
        self.assertEqual(row["build_id"], self.metadata["build_id"])
        self.assertIn("--case free_roll_holdout", row["replay_command"])
        self.assertEqual(row["measurement_uncertainty"], 0.1)
        self.assertEqual(row["combined_standard_uncertainty"], 0.1)
        self.assertEqual(len(csv_rows), 2)
        self.assertEqual(
            csv_rows[1]["prediction"], str(self.cases[1].points[0].expected + 1.0))
        self.assertIn("## CALIBRATION", markdown_path.read_text(encoding="utf-8"))
        self.assertIn("## HOLDOUT", markdown_path.read_text(encoding="utf-8"))

    def test_every_failure_class_has_an_explicit_visible_status(self):
        case = self.cases[0]
        key = lambda code: ReferenceFailureKey(
            "synthetic_reference", case.case_id, code, case.points[0].metric)
        examples = (
            (None, accounting(), "PASSED"),
            ("MODEL_MISMATCH",
             accounting(known_model_mismatches=frozenset({key("MODEL_MISMATCH")})),
             "MODEL_MISMATCH_KNOWN"),
            ("MODEL_MISMATCH",
             accounting(new_model_mismatches=frozenset({key("MODEL_MISMATCH")})),
             "MODEL_MISMATCH_NEW"),
            ("REFERENCE_LIMITATION",
             accounting(known_limitations=frozenset({key("REFERENCE_LIMITATION")})),
             "REFERENCE_LIMITATION_KNOWN"),
            ("REFERENCE_LIMITATION",
             accounting(new_limitations=frozenset({key("REFERENCE_LIMITATION")})),
             "REFERENCE_LIMITATION_NEW"),
            ("INTEGRATION_MISMATCH",
             accounting(unallowlistable_failures=frozenset({key("INTEGRATION_MISMATCH")})),
             "INTEGRATION_MISMATCH"),
            ("NUMERICAL_FAILURE",
             accounting(unallowlistable_failures=frozenset({key("NUMERICAL_FAILURE")})),
             "NUMERICAL_FAILURE"),
            ("NON_DETERMINISTIC",
             accounting(unallowlistable_failures=frozenset({key("NON_DETERMINISTIC")})),
             "NON_DETERMINISTIC"),
        )
        observed = set()
        for code, reconciliation, expected_status in examples:
            with self.subTest(code=code):
                prediction = case.points[0].expected if code is None else None
                json_path, _, _ = self._write(
                    [case], [result_for(case, prediction, code)], reconciliation)
                payload = json.loads(json_path.read_text(encoding="utf-8"))
                status = payload["partitions"]["CALIBRATION"]["points"][0]["status"]
                self.assertEqual(status, expected_status)
                observed.add(status)
        self.assertEqual(observed, {
            "PASSED", "MODEL_MISMATCH_KNOWN", "MODEL_MISMATCH_NEW",
            "REFERENCE_LIMITATION_KNOWN", "REFERENCE_LIMITATION_NEW",
            "INTEGRATION_MISMATCH", "NUMERICAL_FAILURE", "NON_DETERMINISTIC",
        })

    def test_known_failures_are_never_reported_as_passed_or_skipped(self):
        case = self.cases[0]
        key = ReferenceFailureKey(
            "synthetic_reference", case.case_id,
            "REFERENCE_LIMITATION", case.points[0].metric)
        json_path, _, markdown_path = self._write(
            [case], [result_for(case, None, "REFERENCE_LIMITATION")],
            accounting(known_limitations=frozenset({key})))

        serialized = json_path.read_text(encoding="utf-8")
        markdown = markdown_path.read_text(encoding="utf-8")
        self.assertIn("REFERENCE_LIMITATION_KNOWN", serialized)
        self.assertIn("REFERENCE_LIMITATION_KNOWN", markdown)
        self.assertNotIn("SKIPPED", serialized + markdown)

    def test_outputs_are_byte_deterministic(self):
        results = [result_for(case, case.points[0].expected) for case in self.cases]
        first = self._write(self.cases, results, accounting())
        second = self._write(tuple(reversed(self.cases)), tuple(reversed(results)), accounting())

        self.assertEqual(
            [path.read_bytes() for path in first],
            [path.read_bytes() for path in second],
        )

    def test_csv_neutralizes_formula_prefixes_only_for_text(self):
        original = self.cases[0]
        unsafe_point = replace(original.points[0], source_locator="=cmd|' /C calc'!A0")
        case = replace(original, points=(unsafe_point,))
        _, csv_path, _ = self._write(
            [case], [result_for(case, -1.0)], accounting())
        with csv_path.open("r", encoding="utf-8", newline="") as source:
            row = next(csv.DictReader(source))

        self.assertEqual(row["source_locator"], "'=cmd|' /C calc'!A0")
        self.assertEqual(row["prediction"], "-1.0")

    def test_missing_result_is_an_integration_mismatch_not_a_skip(self):
        json_path, _, _ = self._write([self.cases[0]], [], accounting())
        payload = json.loads(json_path.read_text(encoding="utf-8"))
        row = payload["partitions"]["CALIBRATION"]["points"][0]

        self.assertEqual(row["status"], "INTEGRATION_MISMATCH")
        self.assertIsNone(row["prediction"])

    def test_markdown_lists_every_accounting_bucket(self):
        key = ReferenceFailureKey(
            "synthetic_reference", "case", "MODEL_MISMATCH", "speed")
        reconciliation = accounting(
            known_model_mismatches=frozenset({key}),
            new_model_mismatches=frozenset({key}),
            missing_model_mismatches=frozenset({key}),
            known_limitations=frozenset({replace(key, code="REFERENCE_LIMITATION")}),
            new_limitations=frozenset({replace(key, code="REFERENCE_LIMITATION")}),
            missing_limitations=frozenset({replace(key, code="REFERENCE_LIMITATION")}),
            unallowlistable_failures=frozenset({replace(key, code="NUMERICAL_FAILURE")}),
        )
        _, _, markdown_path = self._write([], [], reconciliation)
        markdown = markdown_path.read_text(encoding="utf-8")

        for heading in (
                "Known model mismatches", "New model mismatches",
                "Missing model mismatches", "Known reference limitations",
                "New reference limitations", "Missing reference limitations",
                "Unallowlistable failures"):
            self.assertIn(heading, markdown)


if __name__ == "__main__":
    unittest.main()
