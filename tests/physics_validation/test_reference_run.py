import hashlib
import inspect
import io
import json
import tempfile
import unittest
from contextlib import redirect_stderr
from pathlib import Path

from tools.physics_validation.reference_run import (
    main,
    run_reference_validation,
)


FIXTURE_ROOT = Path(__file__).parent / "fixtures/reference_package_v1"


def trace_for_scenario(scenario, final_offset=0.0):
    expectation = scenario["expectations"][0]["value"]
    distance = expectation["expected"] + final_offset
    start = scenario["balls"][0]["position_cm"]
    ticks = scenario["simulation"]["ticks"]
    frames = []
    for tick in range(1, ticks + 1):
        fraction = tick / ticks
        frames.append({
            "tick": tick,
            "translational_kinetic_energy_j": 1.0 - 0.5 * fraction,
            "maximum_penetration_cm": 0.0,
            "balls": [{
                "index": 0,
                "position_cm": {
                    "x": start[0] + distance * fraction,
                    "y": start[1],
                    "z": start[2],
                },
                "velocity_cm_s": {"x": 1.0, "y": 0.0, "z": 0.0},
                "acceleration_cm_s2": {"x": 0.0, "y": 0.0, "z": 0.0},
                "angular_velocity_rad_s": {"x": 0.0, "y": 0.0, "z": 0.0},
                "speed_cm_s": 1.0,
                "pocketed": False,
            }],
            "contacts": [],
        })
    return frames


class ReferenceRunTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.executable = self.root / "Billiards"
        self.executable.write_bytes(b"synthetic executable")

    def _output(self, name):
        return self.root / name

    def test_runs_all_cases_twice_and_writes_auditable_artifacts(self):
        output = self._output("success")

        exit_code = run_reference_validation(
            self.executable,
            FIXTURE_ROOT,
            output,
            execute_once=lambda executable, scenario: trace_for_scenario(scenario),
        )

        self.assertEqual(exit_code, 0)
        payload = json.loads(
            (output / "reference_report.json").read_text(encoding="utf-8"))
        self.assertEqual(payload["partitions"]["CALIBRATION"]["summary"]["passed"], 1)
        self.assertEqual(payload["partitions"]["HOLDOUT"]["summary"]["passed"], 1)
        self.assertEqual(
            payload["metadata"]["build_id"],
            "sha256:" + hashlib.sha256(b"synthetic executable").hexdigest())
        self.assertEqual(len(list((output / "traces").glob("*.json"))), 2)
        self.assertEqual(len(list((output / "provenance").glob("*.json"))), 2)
        self.assertTrue((output / "reference_points.csv").is_file())
        self.assertTrue((output / "reference_report.md").is_file())
        for trace_path in (output / "traces").glob("*.json"):
            frames = json.loads(trace_path.read_text(encoding="utf-8"))
            self.assertEqual(len(frames), 10)
            self.assertEqual([frame["tick"] for frame in frames], list(range(1, 11)))

    def test_detects_nondeterminism_between_fresh_runs(self):
        invocation = {}

        def changing_executor(executable, scenario):
            scenario_id = scenario["id"]
            invocation[scenario_id] = invocation.get(scenario_id, 0) + 1
            offset = 1.0 if invocation[scenario_id] == 2 else 0.0
            return trace_for_scenario(scenario, final_offset=offset)

        output = self._output("nondeterministic")
        exit_code = run_reference_validation(
            self.executable, FIXTURE_ROOT, output, execute_once=changing_executor)

        self.assertEqual(exit_code, 1)
        payload = json.loads(
            (output / "reference_report.json").read_text(encoding="utf-8"))
        statuses = {
            row["status"]
            for partition in payload["partitions"].values()
            for row in partition["points"]
        }
        self.assertEqual(statuses, {"NON_DETERMINISTIC"})
        self.assertEqual(
            len(payload["accounting"]["unallowlistable_failures"]), 2)

    def test_execution_error_becomes_integration_failure_and_remaining_case_runs(self):
        def partly_failing_executor(executable, scenario):
            if scenario["id"].endswith("free_roll_calibration"):
                raise RuntimeError("process failed")
            return trace_for_scenario(scenario)

        output = self._output("partial")
        exit_code = run_reference_validation(
            self.executable, FIXTURE_ROOT, output,
            execute_once=partly_failing_executor)

        self.assertEqual(exit_code, 1)
        payload = json.loads(
            (output / "reference_report.json").read_text(encoding="utf-8"))
        calibration = payload["partitions"]["CALIBRATION"]["points"][0]
        holdout = payload["partitions"]["HOLDOUT"]["points"][0]
        self.assertEqual(calibration["status"], "INTEGRATION_MISMATCH")
        self.assertEqual(holdout["status"], "PASSED")
        self.assertEqual(len(list((output / "traces").glob("*.json"))), 1)
        self.assertEqual(len(list((output / "provenance").glob("*.json"))), 2)

    def test_nonfinite_trace_remains_numerical_failure(self):
        def nonfinite_executor(executable, scenario):
            frames = trace_for_scenario(scenario)
            frames[-1]["balls"][0]["position_cm"]["x"] = float("nan")
            return frames

        output = self._output("nonfinite")
        exit_code = run_reference_validation(
            self.executable, FIXTURE_ROOT, output,
            execute_once=nonfinite_executor,
            case_ids=("free_roll_calibration",),
        )

        self.assertEqual(exit_code, 1)
        payload = json.loads(
            (output / "reference_report.json").read_text(encoding="utf-8"))
        row = payload["partitions"]["CALIBRATION"]["points"][0]
        self.assertEqual(row["status"], "NUMERICAL_FAILURE")
        self.assertIsNone(row["prediction"])
        self.assertEqual(row["prediction_nonfinite"], "NaN")
        self.assertEqual(
            payload["accounting"]["unallowlistable_failures"][0]["code"],
            "NUMERICAL_FAILURE",
        )

    def test_case_filter_selects_without_changing_committed_partition(self):
        output = self._output("filtered")
        exit_code = run_reference_validation(
            self.executable,
            FIXTURE_ROOT,
            output,
            execute_once=lambda executable, scenario: trace_for_scenario(scenario),
            case_ids=("free_roll_holdout",),
        )

        self.assertEqual(exit_code, 0)
        payload = json.loads(
            (output / "reference_report.json").read_text(encoding="utf-8"))
        self.assertEqual(payload["partitions"]["CALIBRATION"]["summary"]["points"], 0)
        self.assertEqual(payload["partitions"]["HOLDOUT"]["summary"]["points"], 1)
        self.assertEqual(
            payload["partitions"]["HOLDOUT"]["points"][0]["partition"], "HOLDOUT")

    def test_unknown_case_filter_is_rejected_before_execution(self):
        with self.assertRaisesRegex(ValueError, "unknown case"):
            run_reference_validation(
                self.executable,
                FIXTURE_ROOT,
                self._output("unknown"),
                execute_once=lambda executable, scenario: trace_for_scenario(scenario),
                case_ids=("not_a_case",),
            )

    def test_function_and_cli_expose_no_split_override(self):
        self.assertNotIn("split", inspect.signature(run_reference_validation).parameters)
        with redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as raised:
                main([
                    "--executable", str(self.executable),
                    "--package", str(FIXTURE_ROOT),
                    "--output", str(self._output("cli")),
                    "--split", "other.json",
                ])
        self.assertEqual(raised.exception.code, 2)


if __name__ == "__main__":
    unittest.main()
