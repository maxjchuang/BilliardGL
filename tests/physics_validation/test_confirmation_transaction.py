import hashlib
import json
import shutil
import tempfile
import threading
import unittest
from pathlib import Path

from tools.physics_validation.confirmation_transaction import (
    ConfirmationAccessError,
    consume_confirmation,
    finalize_interrupted,
    reserve_from_freeze,
    validate_confirmation_access_from_freeze,
)


FIXTURE = Path(__file__).parent / "fixtures/confirmation_transaction_v1"
PACKAGE_KEY = "fixture_confirmation"


def canonical(document):
    return json.dumps(document, indent=2, sort_keys=True) + "\n"


class ConfirmationTransactionTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.package = self.root / PACKAGE_KEY
        shutil.copytree(FIXTURE, self.package)
        manifest = self.package / "manifest.json"
        self.freeze = self.root / "freeze.json"
        self.freeze.write_text(canonical({
            "artifacts": [{
                "path": f"{PACKAGE_KEY}/manifest.json",
                "role": "confirmation_package_manifest",
                "sha256": hashlib.sha256(manifest.read_bytes()).hexdigest(),
            }],
            "candidate_id": "synthetic_candidate_v3",
            "schema_version": 2,
        }), encoding="utf-8")
        self.lifecycle = self.root / "lifecycle.json"
        self.lifecycle.write_text(canonical({
            "datasets": [{
                "calibration_status": "confirmation",
                "dataset_id": PACKAGE_KEY,
                "dataset_version": "1.0.0",
                "holdout_status": "confirmation",
            }],
            "schema_version": 1,
        }), encoding="utf-8")
        self.ledger = self.root / "ledger.json"
        self.output = self.root / "output"

    def consume(self, evaluator, output=None):
        return consume_confirmation(
            self.freeze,
            PACKAGE_KEY,
            output or self.output,
            self.ledger,
            evaluator,
            repository_root=self.root,
            lifecycle_path=self.lifecycle,
        )

    def test_reservation_precedes_package_read_and_runner_launch(self):
        events = []

        def evaluator(package):
            ledger = json.loads(self.ledger.read_text(encoding="utf-8"))
            events.append(("open", ledger["attempts"][0]["state"] == "STARTED"))
            events.append(("run", package.manifest["dataset_id"] ==
                           PACKAGE_KEY))
            return {"result": "PASSED", "files": {"metrics.csv": b"ok\n"}}

        receipt = self.consume(evaluator)
        self.assertEqual(events, [("open", True), ("run", True)])
        self.assertEqual(receipt["candidate_id"], "synthetic_candidate_v3")
        self.assertEqual(receipt["result"], "PASSED")

    def test_concurrent_consumers_launch_exactly_one_runner(self):
        barrier = threading.Barrier(2)
        launches = []
        errors = []

        def worker(index):
            barrier.wait()
            try:
                self.consume(
                    lambda package: (
                        launches.append(index),
                        {"result": "PASSED", "files": {"result.txt": b"ok\n"}},
                    )[1],
                    self.root / f"output-{index}",
                )
            except ConfirmationAccessError as error:
                errors.append(str(error))

        threads = [threading.Thread(target=worker, args=(index,))
                   for index in range(2)]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()
        self.assertEqual(len(launches), 1)
        self.assertEqual(len(errors), 1)

    def test_interrupted_attempt_can_only_be_finalized_failed(self):
        attempt = reserve_from_freeze(
            self.root, self.freeze, PACKAGE_KEY, self.ledger,
            self.output, self.lifecycle)
        receipt = finalize_interrupted(attempt)
        self.assertEqual(receipt["result"], "FAILED")
        self.assertEqual(receipt["failure_code"], "FAILED_INTERRUPTED")
        with self.assertRaises(ConfirmationAccessError):
            reserve_from_freeze(
                self.root, self.freeze, PACKAGE_KEY, self.ledger,
                self.root / "replay", self.lifecycle)

    def test_access_check_does_not_open_declared_manifest(self):
        self.package.rename(self.root / "package-hidden")
        self.assertEqual(validate_confirmation_access_from_freeze(
            self.root, self.freeze, PACKAGE_KEY, self.ledger,
            self.lifecycle), [])

    def test_package_fault_after_reservation_is_failed_closed(self):
        (self.package / "manifest.json").write_text("{}\n", encoding="utf-8")
        receipt = self.consume(lambda package: self.fail("must not launch"))
        self.assertEqual(receipt["result"], "FAILED")
        self.assertEqual(receipt["failure_code"], "FAILED_EVALUATOR_EXCEPTION")
        failures = validate_confirmation_access_from_freeze(
            self.root, self.freeze, PACKAGE_KEY, self.ledger,
            self.lifecycle)
        self.assertIn("confirmation partition is already consumed", failures)

    def test_invalid_evaluator_result_is_finalized_failed(self):
        receipt = self.consume(lambda package: {
            "result": "MAYBE",
            "files": {},
        })
        self.assertEqual(receipt["result"], "FAILED")
        self.assertEqual(receipt["failure_code"],
                         "FAILED_EVALUATOR_EXCEPTION")
        ledger = json.loads(self.ledger.read_text(encoding="utf-8"))
        self.assertEqual(ledger["records"], [receipt])


if __name__ == "__main__":
    unittest.main()
