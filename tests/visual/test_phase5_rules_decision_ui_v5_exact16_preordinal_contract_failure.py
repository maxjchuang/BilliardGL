from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest

from scripts import validate_phase5_rules_decision_ui_v5_exact16_preordinal_contract_failure as validator


class FrozenPreordinalContractFailureTests(unittest.TestCase):
    def test_live_receipt_passes(self) -> None:
        result = validator.validate()
        self.assertEqual(result["completed_count"], 0)
        self.assertTrue(result["authorization_reuse_forbidden"])

    def reject(self, update) -> None:
        value = json.loads(validator.DEFAULT.read_text(encoding="utf-8"))
        update(value)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            with self.assertRaises(RuntimeError):
                validator.validate(path)

    def test_case_promotion_rejected(self) -> None:
        self.reject(lambda value: value["attempt_scope"].update(completed_count=1))

    def test_gpu_claim_rejected(self) -> None:
        self.reject(lambda value: value["attempt_scope"].update(gpu_work_submitted=True))

    def test_reuse_rejected(self) -> None:
        self.reject(lambda value: value["single_use_boundary"].update(
            authorization_reuse_forbidden=False))

    def test_root_cause_drift_rejected(self) -> None:
        self.reject(lambda value: value["root_cause"].update(
            first_mismatch="dispatcher"))

    def test_frozen_sha_drift_rejected(self) -> None:
        self.reject(lambda value: value["frozen_artifacts"]["authorization"].update(
            sha256="0" * 64))

    def test_output_count_drift_rejected(self) -> None:
        self.reject(lambda value: value["frozen_artifacts"]["output_root"].update(
            file_count=2))

    def test_claim_promotion_rejected(self) -> None:
        self.reject(lambda value: value["claims"].update(targeted_machine16_pass=True))


if __name__ == "__main__":
    unittest.main()
