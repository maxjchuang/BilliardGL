import hashlib
import json
import shutil
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools.physics_validation.confirmation_run import build_confirmation_result
from tools.physics_validation.holdout_access import validate_confirmation_access
from tools.physics_validation.run import ExecutionEvidence
from tools.physics_validation.validation_run import (
    ConfirmationAccessError,
    consume_confirmation,
)


ROOT = Path(__file__).resolve().parents[2]
FREEZE = ROOT / "physics_models/candidates/phase3_integrated_v2/freeze.json"
SUDO = ROOT / "tests/physics_validation/reference_data/sudo_2002"
DERBY = ROOT / "tests/physics_validation/reference_data/derby_fuller_1999"
TRANSACTION_FIXTURE = (
    ROOT / "tests/physics_validation/fixtures/confirmation_transaction_v1")
REAL_LEDGER = (
    ROOT / "physics_models/candidates/phase3_integrated_v2/confirmation_consumption.json")
SUDO_RESULT = (
    ROOT / "physics_models/candidates/phase3_integrated_v2/confirmation/sudo_2002")
FIT_SURFACE = ROOT / "tools/physics_validation/fit_surface.py"
FIT_BALL = ROOT / "tools/physics_validation/fit_ball_collision.py"
FIT_CUSHION = ROOT / "tools/physics_validation/fit_cushion.py"


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


class Phase3V2ConfirmationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(dir=ROOT / "build")
        self.addCleanup(self.temporary.cleanup)
        self.scratch = Path(self.temporary.name)
        self.package = self.scratch / "fixture_confirmation"
        shutil.copytree(TRANSACTION_FIXTURE, self.package)
        manifest_sha256 = digest(self.package / "manifest.json")
        fixture_relative = self.package.relative_to(ROOT).as_posix()
        self.freeze = self.scratch / "freeze.json"
        self.freeze.write_text(json.dumps({
            "artifacts": [{
                "path": f"{fixture_relative}/manifest.json",
                "role": "confirmation_package_manifest",
                "sha256": manifest_sha256,
            }],
            "candidate_id": "confirmation_transaction_fixture",
            "schema_version": 2,
        }, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        self.lifecycle = self.scratch / "lifecycle.json"
        self.lifecycle.write_text(json.dumps({
            "datasets": [{
                "calibration_status": "confirmation",
                "dataset_id": "fixture_confirmation",
                "dataset_version": "1.0.0",
                "holdout_status": "confirmation",
            }],
            "schema_version": 1,
        }, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        lifecycle_patch = patch(
            "tools.physics_validation.validation_run.DEFAULT_LIFECYCLE_PATH",
            self.lifecycle)
        lifecycle_patch.start()
        self.addCleanup(lifecycle_patch.stop)
        self.ledger = self.scratch / "confirmation_consumption.json"
        self.output = self.scratch / "confirmation" / "fixture_confirmation"

    def test_spent_sources_are_closed_while_fixture_remains_unopened(self):
        self.assertIn(
            "reference partition is not in confirmation state",
            validate_confirmation_access(ROOT, FREEZE, SUDO, self.ledger))
        self.assertIn(
            "reference partition is not in confirmation state",
            validate_confirmation_access(ROOT, FREEZE, DERBY, self.ledger))
        self.assertEqual(
            validate_confirmation_access(
                ROOT, self.freeze, self.package, self.ledger,
                self.lifecycle), [])
        self.assertFalse(self.ledger.exists())

    def test_fitters_do_not_read_confirmation_packages(self):
        for path in (FIT_SURFACE, FIT_BALL, FIT_CUSHION):
            text = path.read_text(encoding="utf-8")
            self.assertNotIn("reference_data/sudo_2002", text)
            self.assertNotIn("derby_fuller_1999", text)
            self.assertNotIn("han_2005", text)

    def test_committed_sudo_attempt_is_rejected_fail_closed(self):
        receipt = json.loads(
            (SUDO_RESULT / "validation_receipt.json").read_text(
                encoding="utf-8"))
        ledger = json.loads(REAL_LEDGER.read_text(encoding="utf-8"))
        self.assertEqual(receipt["result"], "FAILED")
        self.assertEqual(ledger["records"], [receipt])
        self.assertEqual(ledger["attempts"][0]["state"], "STARTED")
        for relative, expected in receipt["files"].items():
            self.assertEqual(digest(SUDO_RESULT / relative), expected)
        self.assertIn("confirmation partition is already consumed",
                      validate_confirmation_access(
                          ROOT, FREEZE, SUDO, REAL_LEDGER))
        self.assertFalse(
            (ROOT / "physics_models/candidates/phase3_integrated_v2/confirmation"
             / "derby_fuller_1999").exists())

    def test_second_confirmation_execution_is_rejected(self):
        calls = []
        consume_confirmation(
            self.freeze, self.package, self.output, self.ledger,
            lambda: {
                "result": "PASSED_OR_ACCOUNTED",
                "files": {"metrics.csv": b"metric,value\ne,0.9\n"},
            },
            repository_root=ROOT,
        )
        with self.assertRaisesRegex(
                ConfirmationAccessError,
                "confirmation partition is already consumed"):
            consume_confirmation(
                self.freeze, self.package, self.scratch / "second", self.ledger,
                lambda: calls.append("executed"), repository_root=ROOT)
        self.assertEqual(calls, [])

    def test_atomic_output_receipt_and_ledger_hash_every_result(self):
        receipt = consume_confirmation(
            self.freeze, self.package, self.output, self.ledger,
            lambda: {
                "result": "PASSED_OR_ACCOUNTED",
                "files": {
                    "metrics.csv": b"metric,value\ne,0.9\n",
                    "traces/case.json": b'{"frames":[]}\n',
                },
            },
            repository_root=ROOT,
        )
        self.assertEqual(receipt["result"], "PASSED_OR_ACCOUNTED")
        self.assertEqual(receipt["freeze_sha256"], digest(self.freeze))
        self.assertEqual(
            receipt["files"], {
                "metrics.csv": digest(self.output / "metrics.csv"),
                "traces/case.json": digest(self.output / "traces/case.json"),
            })
        self.assertEqual(
            json.loads((self.output / "validation_receipt.json").read_text()),
            receipt,
        )
        ledger = json.loads(self.ledger.read_text(encoding="utf-8"))
        self.assertEqual(ledger["records"], [receipt])
        self.assertFalse(any(path.name.endswith(".tmp")
                             for path in self.scratch.rglob("*")))

    def test_failed_result_is_still_consumed_and_existing_output_fails_closed(self):
        self.output.mkdir(parents=True)
        with self.assertRaisesRegex(ConfirmationAccessError,
                                    "output path already exists"):
            consume_confirmation(
                self.freeze, self.package, self.output, self.ledger,
                lambda: self.fail("runner must not execute"),
                repository_root=ROOT)
        self.output.rmdir()
        receipt = consume_confirmation(
            self.freeze, self.package, self.output, self.ledger,
            lambda: {"result": "FAILED", "files": {"failure.json": b'{}\n'}},
            repository_root=ROOT,
        )
        self.assertEqual(receipt["result"], "FAILED")
        self.assertIn("confirmation partition is already consumed",
                      validate_confirmation_access(
                          ROOT, self.freeze, self.package, self.ledger,
                          self.lifecycle))

    def test_runner_exception_is_failed_closed_and_reserved_before_execution(self):
        observed = []

        def crashing_runner():
            ledger = json.loads(self.ledger.read_text(encoding="utf-8"))
            observed.append(ledger["attempts"][0]["state"])
            raise RuntimeError("missing expected contact")

        receipt = consume_confirmation(
            self.freeze, self.package, self.output, self.ledger, crashing_runner,
            repository_root=ROOT,
        )
        self.assertEqual(observed, ["STARTED"])
        self.assertEqual(receipt["result"], "FAILED")
        failure = json.loads((self.output / "failure.json").read_text())
        self.assertEqual(failure["exception_type"], "RuntimeError")
        self.assertEqual(failure["message"], "missing expected contact")
        self.assertIn("confirmation partition is already consumed",
                      validate_confirmation_access(
                          ROOT, self.freeze, self.package, self.ledger,
                          self.lifecycle))

    def test_malformed_ledger_and_runner_supplied_receipt_fail_closed(self):
        self.ledger.write_text("{}\n", encoding="utf-8")
        failures = validate_confirmation_access(
            ROOT, self.freeze, self.package, self.ledger, self.lifecycle)
        self.assertIn("confirmation ledger is invalid", failures)
        self.ledger.unlink()
        receipt = consume_confirmation(
            self.freeze, self.package, self.output, self.ledger,
            lambda: {
                "result": "PASSED_OR_ACCOUNTED",
                "files": {"validation_receipt.json": b"{}\n"},
            },
            repository_root=ROOT,
        )
        self.assertEqual(receipt["result"], "FAILED")
        self.assertEqual(
            receipt["failure_code"], "FAILED_EVALUATOR_EXCEPTION")
        self.assertTrue(self.output.exists())
        ledger = json.loads(self.ledger.read_text(encoding="utf-8"))
        self.assertEqual(ledger["attempts"][0]["state"], "STARTED")
        self.assertEqual(ledger["records"], [receipt])
        self.assertIn("confirmation partition is already consumed",
                      validate_confirmation_access(
                          ROOT, self.freeze, self.package, self.ledger,
                          self.lifecycle))

    def test_confirmation_metric_contract_is_fixed_before_real_execution(self):
        freeze = json.loads(FREEZE.read_text(encoding="utf-8"))

        def fake_execute(executable, scenario):
            frames = [{
                "tick": tick,
                "contacts": [],
                "balls": [],
                "surface_transitions": [],
            } for tick in range(1, scenario["simulation"]["ticks"] + 1)]
            if "cushion" in scenario["id"]:
                frames[0]["contacts"] = [{"kind": "rail", "restitution": 0.9}]
            else:
                frames[0]["contacts"] = [{
                    "kind": "ball_ball", "restitution": 0.97,
                }]
                frames[0]["balls"] = [
                    {"index": 0, "velocity_cm_s": {"x": 30.0, "z": 10.0}},
                    {"index": 1, "velocity_cm_s": {"x": 60.0, "z": -10.0}},
                ]
            return frames

        executable = self.scratch / "frozen-executable-fixture"
        executable.write_bytes(b"fixture")
        with patch(
                "tools.physics_validation.confirmation_run._sha256",
                return_value=freeze["executable_sha256"]):
            result = build_confirmation_result(
                executable, FREEZE, SUDO, ROOT, execute_once=fake_execute)
        report = json.loads(result["files"]["reference_report.json"])
        self.assertEqual(report["dataset_id"], "sudo_2002")
        self.assertEqual(report["summary"]["points"], 6)
        self.assertEqual(result["result"], "FAILED")
        scenarios = [
            json.loads(value) for path, value in result["files"].items()
            if path.startswith("scenarios/")
        ]
        self.assertTrue(scenarios)
        self.assertTrue(all(scenario["expectations"] for scenario in scenarios))
        initial_speeds = sorted(
            scenario["balls"][0]["velocity_cm_s"][0]
            for scenario in scenarios)
        self.assertEqual(initial_speeds, [98.0, 180.0, 250.0])
        execution = [
            json.loads(value) for path, value in result["files"].items()
            if path.startswith("execution/")
        ]
        self.assertEqual(len(execution), 6)
        self.assertTrue(all(item["fixture_executor"] for item in execution))
        self.assertTrue(report["execution"]["fixture_executor"])

    def test_production_executor_preserves_two_protocol_evidence_records(self):
        freeze = json.loads(FREEZE.read_text(encoding="utf-8"))

        def fake_frames(scenario):
            frames = [{
                "tick": tick,
                "contacts": [],
                "balls": [],
                "surface_transitions": [],
            } for tick in range(1, scenario["simulation"]["ticks"] + 1)]
            if "cushion" in scenario["id"]:
                frames[0]["contacts"] = [{"kind": "rail", "restitution": 0.9}]
            else:
                frames[0]["contacts"] = [{
                    "kind": "ball_ball", "restitution": 0.97,
                }]
                frames[0]["balls"] = [
                    {"index": 0, "velocity_cm_s": {"x": 30.0, "z": 10.0}},
                    {"index": 1, "velocity_cm_s": {"x": 60.0, "z": -10.0}},
                ]
            return frames

        def fake_evidence(_executable, scenario):
            return ExecutionEvidence(
                tuple(fake_frames(scenario)),
                ({"direction": "request", "scenario_id": scenario["id"]},),
                "",
                0,
            )

        executable = self.scratch / "frozen-executable-production-fixture"
        executable.write_bytes(b"fixture")
        with patch(
                "tools.physics_validation.confirmation_run._sha256",
                return_value=freeze["executable_sha256"]), patch(
                "tools.physics_validation.confirmation_run."
                "_execute_once_with_evidence",
                side_effect=fake_evidence):
            result = build_confirmation_result(
                executable, FREEZE, SUDO, ROOT)
        report = json.loads(result["files"]["reference_report.json"])
        execution = [
            json.loads(value) for path, value in result["files"].items()
            if path.startswith("execution/")
        ]
        self.assertEqual(len(execution), 6)
        self.assertTrue(all(not item["fixture_executor"] for item in execution))
        self.assertTrue(all(item["protocol_transcript"] for item in execution))
        self.assertTrue(all(item["return_code"] == 0 for item in execution))
        self.assertFalse(report["execution"]["fixture_executor"])


if __name__ == "__main__":
    unittest.main()
