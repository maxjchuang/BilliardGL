import hashlib
import json
import tempfile
import unittest
from pathlib import Path

from tools.physics_validation.holdout_access import validate_confirmation_access
from tools.physics_validation.validation_run import (
    ConfirmationAccessError,
    consume_confirmation,
)


ROOT = Path(__file__).resolve().parents[2]
FREEZE = ROOT / "physics_models/candidates/phase3_integrated_v2/freeze.json"
SUDO = ROOT / "tests/physics_validation/reference_data/sudo_2002"
DERBY = ROOT / "tests/physics_validation/reference_data/derby_fuller_1999"
FIT_SURFACE = ROOT / "tools/physics_validation/fit_surface.py"
FIT_BALL = ROOT / "tools/physics_validation/fit_ball_collision.py"
FIT_CUSHION = ROOT / "tools/physics_validation/fit_cushion.py"


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


class Phase3V2ConfirmationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.scratch = Path(self.temporary.name)
        self.ledger = self.scratch / "confirmation_consumption.json"
        self.output = self.scratch / "confirmation" / "sudo_2002"

    def test_confirmation_runner_requires_frozen_unopened_candidate(self):
        self.assertEqual(
            validate_confirmation_access(ROOT, FREEZE, SUDO, self.ledger), [])
        self.assertEqual(
            validate_confirmation_access(ROOT, FREEZE, DERBY, self.ledger), [])
        self.assertFalse(self.ledger.exists())

    def test_fitters_do_not_import_confirmation_packages(self):
        for path in (FIT_SURFACE, FIT_BALL, FIT_CUSHION):
            text = path.read_text(encoding="utf-8")
            self.assertNotIn("sudo_2002", text)
            self.assertNotIn("derby_fuller_1999", text)

    def test_second_confirmation_execution_is_rejected(self):
        calls = []
        consume_confirmation(
            FREEZE, SUDO, self.output, self.ledger,
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
                FREEZE, SUDO, self.scratch / "second", self.ledger,
                lambda: calls.append("executed"), repository_root=ROOT)
        self.assertEqual(calls, [])

    def test_atomic_output_receipt_and_ledger_hash_every_result(self):
        receipt = consume_confirmation(
            FREEZE, SUDO, self.output, self.ledger,
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
        self.assertEqual(receipt["freeze_sha256"], digest(FREEZE))
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
                FREEZE, SUDO, self.output, self.ledger,
                lambda: self.fail("runner must not execute"),
                repository_root=ROOT)
        self.output.rmdir()
        receipt = consume_confirmation(
            FREEZE, SUDO, self.output, self.ledger,
            lambda: {"result": "FAILED", "files": {"failure.json": b'{}\n'}},
            repository_root=ROOT,
        )
        self.assertEqual(receipt["result"], "FAILED")
        self.assertIn("confirmation partition is already consumed",
                      validate_confirmation_access(
                          ROOT, FREEZE, SUDO, self.ledger))

    def test_malformed_ledger_and_runner_supplied_receipt_fail_closed(self):
        self.ledger.write_text("{}\n", encoding="utf-8")
        failures = validate_confirmation_access(
            ROOT, FREEZE, SUDO, self.ledger)
        self.assertIn("confirmation ledger is invalid", failures)
        self.ledger.unlink()
        with self.assertRaisesRegex(ConfirmationAccessError,
                                    "cannot provide its own receipt"):
            consume_confirmation(
                FREEZE, SUDO, self.output, self.ledger,
                lambda: {
                    "result": "PASSED_OR_ACCOUNTED",
                    "files": {"validation_receipt.json": b"{}\n"},
                },
                repository_root=ROOT,
            )
        self.assertFalse(self.output.exists())
        self.assertFalse(self.ledger.exists())


if __name__ == "__main__":
    unittest.main()
