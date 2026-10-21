from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest

from scripts import validate_phase2_v10i_formal_full_six_v8_dispatcher_cli_correction_gate as validator


class V8DispatcherCorrectionGateTests(unittest.TestCase):
    def test_live_gate_passes(self) -> None:
        self.assertTrue(validator.validate()["gate_sha256"])

    def reject(self, mutate) -> None:
        value = json.loads(validator.GATE.read_text())
        mutate(value)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "gate.json"
            path.write_text(json.dumps(value))
            with self.assertRaises(RuntimeError):
                validator.validate(path)

    def test_rejects_receipt_drift(self) -> None:
        self.reject(lambda value: value["frozen_v7_failure_receipt"].__setitem__(
            "sha256", "0" * 64))

    def test_rejects_wrong_replacement(self) -> None:
        self.reject(lambda value: value["root_cause"].__setitem__(
            "capture_driver_parser_accepts", "--formal-asset-package"))

    def test_rejects_capture_source_permission(self) -> None:
        self.reject(lambda value: value["forbidden_changes"].remove(
            "scripts/capture_visual_baselines.py"))

    def test_rejects_gpu_promotion(self) -> None:
        self.reject(lambda value: value["required_v8_boundary"].__setitem__(
            "gpu_executed", True))


if __name__ == "__main__":
    unittest.main()
