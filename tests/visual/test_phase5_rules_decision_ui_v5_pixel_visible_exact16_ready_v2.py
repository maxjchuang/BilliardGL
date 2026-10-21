from __future__ import annotations

import json
from pathlib import Path
import shutil
import tempfile
import unittest

from scripts import validate_phase5_rules_decision_ui_v5_pixel_visible_exact16_ready_v2 as validator


class CorrectedExact16ReadyV2Tests(unittest.TestCase):
    def test_source_contract_passes(self) -> None:
        self.assertFalse(validator.validate_source_contract()["gpu_executed"])

    def contract_rejects(self, *, cmake=None, runner=None, dispatcher=None) -> None:
        current_cmake = (validator.ROOT / "CMakeLists.txt").read_text()
        current_runner = validator.prep.RUNNER_SOURCE.read_text()
        current_dispatcher = validator.prep.DISPATCHER.read_text()
        with self.assertRaises(RuntimeError):
            validator.validate_contract_text(cmake or current_cmake,
                runner or current_runner, dispatcher or current_dispatcher)

    def test_missing_v5_definition_rejected(self) -> None:
        text = (validator.ROOT / "CMakeLists.txt").read_text().replace(
            "BILLIARDGL_PHASE5_RULES_UI_V5_EXACT16=1", "REMOVED", 1)
        self.contract_rejects(cmake=text)

    def test_wrong_gate_rejected(self) -> None:
        text = validator.prep.RUNNER_SOURCE.read_text().replace(
            validator.prep.v1.GATE_SHA, "0" * 64, 1)
        self.contract_rejects(runner=text)

    def test_wrong_readiness_schema_rejected(self) -> None:
        text = validator.prep.RUNNER_SOURCE.read_text().replace(
            "phase5_rules_decision_ui_v5_exact16_readiness_v2", "wrong", 1)
        self.contract_rejects(runner=text)

    def test_validation_moved_after_output_rejected(self) -> None:
        text = validator.prep.RUNNER_SOURCE.read_text()
        validation = "validateAndConsumeAuthorization(receipt, plan"
        text = text.replace(validation, "removedValidation", 1)
        self.contract_rejects(runner=text)

    def test_dispatcher_identity_removed_rejected(self) -> None:
        text = validator.prep.DISPATCHER.read_text().replace(
            "phase5_rules_decision_ui_v5_exact16_readiness_v2", "removed")
        self.contract_rejects(dispatcher=text)

    def test_durable_consumption_removed_rejected(self) -> None:
        text = validator.prep.DISPATCHER.read_text().replace("os.fsync", "removed")
        self.contract_rejects(dispatcher=text)

    def test_live_readiness_when_present(self) -> None:
        if not validator.prep.DEFAULT_OUTPUT.exists():
            self.skipTest("final shared build/readiness intentionally pending")
        self.assertEqual(validator.validate(validator.prep.DEFAULT_OUTPUT)["case_count"], 16)

    def mutate_readiness(self, filename: str, update) -> None:
        if not validator.prep.DEFAULT_OUTPUT.exists():
            self.skipTest("final shared build/readiness intentionally pending")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "ready"
            shutil.copytree(validator.prep.DEFAULT_OUTPUT, root)
            path = root / filename
            value = json.loads(path.read_text())
            update(value)
            path.write_text(json.dumps(value))
            with self.assertRaises(RuntimeError):
                validator.validate(root)

    def test_case_promotion_rejected_when_present(self) -> None:
        self.mutate_readiness("rules-decision-ui-v5-exact16-ready-v2.json",
            lambda value: value["case_results"][0].update(status="PASS"))

    def test_auth_promotion_rejected_when_present(self) -> None:
        self.mutate_readiness(
            "rules-decision-ui-v5-authorization-template-not-issued-v2.json",
            lambda value: value.update(authorized=True))


if __name__ == "__main__":
    unittest.main()
