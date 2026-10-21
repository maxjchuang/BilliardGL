from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest

from scripts import validate_phase5_rules_decision_ui_v5_exact16_producer_contract_correction_gate as validator


class ProducerContractCorrectionGateTests(unittest.TestCase):
    def test_live_gate_passes(self) -> None:
        self.assertFalse(validator.validate()["gpu_executed"])

    def reject(self, update) -> None:
        value = json.loads(validator.DEFAULT.read_text(encoding="utf-8"))
        update(value)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            with self.assertRaises(RuntimeError):
                validator.validate(path)

    def test_extra_allowed_source_rejected(self) -> None:
        self.reject(lambda value: value["allowed_source_changes"].append(
            {"path": "src/Billiards/render/modern/modern_renderer.cpp",
             "scope": "bad"}))

    def test_product_change_rejected(self) -> None:
        self.reject(lambda value: value["diagnosis"].update(
            production_visual_bytes_need_change=True))

    def test_dispatcher_change_rejected(self) -> None:
        self.reject(lambda value: value["diagnosis"].update(
            dispatcher_single_use_protocol_need_change=True))

    def test_gate_sha_drift_rejected(self) -> None:
        self.reject(lambda value: value["required_corrected_contract"].update(
            manifest_gate_sha256="0" * 64))

    def test_readiness_schema_drift_rejected(self) -> None:
        self.reject(lambda value: value["required_corrected_contract"].update(
            readiness_schema="v4"))

    def test_gpu_acceptance_rejected(self) -> None:
        self.reject(lambda value: value["acceptance"].update(gpu_executed=True))


if __name__ == "__main__":
    unittest.main()
