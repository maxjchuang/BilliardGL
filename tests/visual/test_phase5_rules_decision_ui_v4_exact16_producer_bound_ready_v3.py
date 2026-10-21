from __future__ import annotations

import json
from pathlib import Path
import shutil
import tempfile
import unittest

from scripts import validate_phase5_rules_decision_ui_v4_exact16_producer_bound_ready_v3 as validator


class ProducerBoundReadyV3Tests(unittest.TestCase):
    def test_live_readiness_passes(self) -> None:
        self.assertEqual(validator.validate()["case_count"], 16)

    def mutate(self, filename: str, mutate) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "ready"; shutil.copytree(validator.prep.OUTPUT, root)
            path = root / filename; value = json.loads(path.read_text())
            mutate(value); path.write_text(json.dumps(value))
            with self.assertRaises(RuntimeError): validator.validate(root)

    def test_rejects_case_promotion(self) -> None:
        self.mutate("rules-decision-ui-v4-exact16-ready.json",
                    lambda v: v["case_results"][0].update(status="PASS"))

    def test_rejects_producer_unbound(self) -> None:
        self.mutate("rules-decision-ui-v4-exact16-ready.json",
                    lambda v: v["dispatch_gate"].update(compatible_capture_producer_bound=False))

    def test_rejects_target_or_definition_drift(self) -> None:
        self.mutate("rules-decision-ui-v4-exact16-ready.json",
                    lambda v: v["capture_producer"].update(target="wrong"))

    def test_rejects_auth_issuance(self) -> None:
        self.mutate("rules-decision-ui-v4-authorization-template-not-issued.json",
                    lambda v: v.update(authorized=True))

    def test_rejects_runner_binary_sha_drift(self) -> None:
        self.mutate("rules-decision-ui-v4-exact16-ready.json",
                    lambda v: v["frozen_bindings"].update(runner_binary_sha256="0" * 64))


if __name__ == "__main__": unittest.main()
