from __future__ import annotations

import json
from pathlib import Path
import shutil
import tempfile
import unittest

from scripts import validate_phase5_rules_decision_ui_v5_pixel_visible_exact16_ready_v3 as validator


class ManifestCorrectedReadyV3Tests(unittest.TestCase):
    def test_source_contract_passes(self) -> None:
        self.assertFalse(validator.validate_source_contract()["gpu_executed"])

    def test_live_when_present(self) -> None:
        if not validator.prep.DEFAULT_OUTPUT.exists():
            self.skipTest("final Phase4-coordinated package intentionally pending")
        self.assertTrue(validator.validate()["full_runner_json_contract_parity"])

    def mutate(self, filename: str, update) -> None:
        if not validator.prep.DEFAULT_OUTPUT.exists():
            self.skipTest("final Phase4-coordinated package intentionally pending")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "ready"
            shutil.copytree(validator.prep.DEFAULT_OUTPUT, root)
            path = root / filename
            value = json.loads(path.read_text())
            update(value)
            path.write_text(json.dumps(value))
            with self.assertRaises(RuntimeError): validator.validate(root)

    def test_missing_gate_rejected_when_present(self) -> None:
        self.mutate("rules-decision-ui-v5-source-bindings-v3.json",
                    lambda value: value.pop("gate_sha256"))

    def test_wrong_gate_rejected_when_present(self) -> None:
        self.mutate("rules-decision-ui-v5-source-bindings-v3.json",
                    lambda value: value.update(gate_sha256="0" * 64))

    def test_case_promotion_rejected_when_present(self) -> None:
        self.mutate("rules-decision-ui-v5-exact16-ready-v3.json",
                    lambda value: value["case_results"][0].update(status="PASS"))

    def test_auth_promotion_rejected_when_present(self) -> None:
        self.mutate("rules-decision-ui-v5-authorization-template-not-issued-v3.json",
                    lambda value: value.update(authorized=True))

    def test_parity_claim_rejected_when_present(self) -> None:
        self.mutate("rules-decision-ui-v5-exact16-ready-v3.json",
                    lambda value: value["manifest_contract_correction"].update(
                        full_runner_json_contract_parity_tested=False))


if __name__ == "__main__":
    unittest.main()
