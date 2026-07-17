import hashlib
import json
import os
import unittest
from pathlib import Path

from tools.physics_validation.confirmation_contract_fixture import (
    build_contract_proof,
)
from tools.physics_validation.confirmation_readiness import (
    validate_contract_proof,
)


ROOT = Path(__file__).resolve().parents[2]
BUILD_DIR = Path(os.environ.get("BILLIARDGL_BUILD_DIR", ROOT / "build"))
EXECUTABLE = BUILD_DIR / "Billiards"
FIXTURE = ROOT / "tests/physics_validation/fixtures/confirmation_transaction_v1"


class ConfirmationContractFixtureTests(unittest.TestCase):
    def test_fixture_traverses_real_parser_and_runner(self):
        files = build_contract_proof(ROOT, EXECUTABLE, FIXTURE)
        proof = json.loads(files["confirmation_contract_proof.json"])
        self.assertEqual(proof["result"], "PASSED")
        self.assertTrue(proof["parse_succeeded"])
        self.assertGreater(proof["frames"], 0)
        self.assertEqual(
            proof["first_trace_sha256"], proof["second_trace_sha256"])
        self.assertEqual(proof["return_code"], 0)
        self.assertEqual(proof["stderr"], "")
        self.assertRegex(
            proof["protocol_transcript_sha256"], r"^[0-9a-f]{64}$")
        executable_sha256 = hashlib.sha256(
            EXECUTABLE.read_bytes()).hexdigest()
        self.assertEqual(
            validate_contract_proof(
                files["confirmation_contract_proof.json"],
                executable_sha256),
            [],
        )

    def test_empty_expectations_reproduces_v3_failure(self):
        def empty_expectations(scenario):
            scenario["expectations"] = []

        with self.assertRaisesRegex(
                RuntimeError,
                "invalid_scenario: expectations must be a nonempty array"):
            build_contract_proof(
                ROOT, EXECUTABLE, FIXTURE, mutate=empty_expectations)

    def test_readiness_rejects_nonmatching_or_incomplete_proof(self):
        files = build_contract_proof(ROOT, EXECUTABLE, FIXTURE)
        proof = json.loads(files["confirmation_contract_proof.json"])
        proof["stderr"] = "unexpected diagnostic"
        failures = validate_contract_proof(
            (json.dumps(proof) + "\n").encode("utf-8"), "0" * 64)
        self.assertIn(
            "confirmation contract proof executable does not match freeze",
            failures,
        )
        self.assertIn(
            "confirmation contract proof stderr is not empty",
            failures,
        )


if __name__ == "__main__":
    unittest.main()
