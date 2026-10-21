from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest

from scripts import validate_phase5_rules_decision_ui_v5v2_exact16_preordinal_missing_gate_failure as validator


class FrozenV5V2MissingGateFailureTests(unittest.TestCase):
    def test_live_passes(self) -> None:
        result = validator.validate()
        self.assertEqual(result["missing_field"], "gate_sha256")
        self.assertTrue(result["authorization_reuse_forbidden"])

    def reject(self, update) -> None:
        value = json.loads(validator.DEFAULT.read_text())
        update(value)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad.json"
            path.write_text(json.dumps(value))
            with self.assertRaises(RuntimeError):
                validator.validate(path)

    def test_case_promotion_rejected(self) -> None:
        self.reject(lambda value: value["attempt_scope"].update(completed_count=1))

    def test_gpu_claim_rejected(self) -> None:
        self.reject(lambda value: value["attempt_scope"].update(gpu_work_submitted=True))

    def test_missing_field_drift_rejected(self) -> None:
        self.reject(lambda value: value["failure"].update(missing_field="other"))

    def test_manifest_presence_claim_rejected(self) -> None:
        self.reject(lambda value: value["frozen_artifacts"][
            "source_binding_manifest"].update(gate_sha256_present=True))

    def test_reuse_rejected(self) -> None:
        self.reject(lambda value: value["single_use_boundary"].update(
            authorization_reuse_forbidden=False))

    def test_root_cause_drift_rejected(self) -> None:
        self.reject(lambda value: value["root_cause"].update(
            exact_missing_field="visual_design_gate_sha256"))

    def test_frozen_sha_drift_rejected(self) -> None:
        self.reject(lambda value: value["frozen_artifacts"]["authorization"].update(
            sha256="0" * 64))

    def test_claim_promotion_rejected(self) -> None:
        self.reject(lambda value: value["claims"].update(targeted_machine16_pass=True))


if __name__ == "__main__":
    unittest.main()
