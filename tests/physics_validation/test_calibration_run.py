import inspect
import io
import json
import tempfile
import unittest
from contextlib import redirect_stderr
from pathlib import Path
from unittest.mock import patch

from tools.physics_validation.calibration_run import main, run_calibration
from tools.physics_validation.partition_run import (
    LoadedReferenceInputs,
    case_ids_for_partition,
)
from tools.physics_validation.reference_adapter import ReferenceAdaptation
from tests.physics_validation.test_reference_run import trace_for_scenario


FIXTURE_ROOT = Path(__file__).parent / "fixtures/reference_package_v1"


class CalibrationRunTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.executable = self.root / "Billiards"
        self.executable.write_bytes(b"synthetic executable")
        self.output = self.root / "output"

    def test_executes_only_committed_calibration_cases(self):
        seen = []

        def execute_once(executable, scenario):
            seen.append(scenario["id"])
            return trace_for_scenario(scenario)

        self.assertEqual(
            run_calibration(
                self.executable,
                FIXTURE_ROOT,
                self.output,
                execute_once=execute_once,
            ),
            0,
        )
        self.assertEqual(
            seen,
            ["synthetic_reference__free_roll_calibration"] * 2,
        )
        report = json.loads(
            (self.output / "reference_report.json").read_text(encoding="utf-8"))
        self.assertEqual(report["partitions"]["CALIBRATION"]["summary"]["points"], 1)
        self.assertEqual(report["partitions"]["HOLDOUT"]["summary"]["points"], 0)

    def test_function_and_cli_offer_no_partition_escape_hatches(self):
        parameters = inspect.signature(run_calibration).parameters
        for forbidden in ("case", "case_ids", "partition", "split", "holdout"):
            self.assertNotIn(forbidden, parameters)

        for argument in ("--case", "--partition", "--split", "--holdout"):
            with self.subTest(argument=argument), redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as raised:
                    main([
                        "--executable", str(self.executable),
                        "--package", str(FIXTURE_ROOT),
                        "--output", str(self.output),
                        argument, "anything",
                    ])
            self.assertEqual(raised.exception.code, 2)

    def test_package_without_calibration_cases_fails_before_execution(self):
        executed = []
        empty = LoadedReferenceInputs(
            reference_package=None,
            dataset_id="synthetic_reference",
            dataset_version="1.0.0",
            points=(),
            split=None,
            adaptation=ReferenceAdaptation(()),
        )
        with patch(
            "tools.physics_validation.calibration_run.load_reference_inputs",
            return_value=empty,
        ):
            with self.assertRaisesRegex(ValueError, "no executable CALIBRATION cases"):
                run_calibration(
                    self.executable,
                    FIXTURE_ROOT,
                    self.output,
                    execute_once=lambda executable, scenario: executed.append(scenario),
                )
        self.assertEqual(executed, [])

    def test_partition_selector_rejects_arbitrary_partitions(self):
        with self.assertRaisesRegex(ValueError, "committed reference partition"):
            case_ids_for_partition(ReferenceAdaptation(()), "ALL")


if __name__ == "__main__":
    unittest.main()
