from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest

from scripts import validate_phase5_rules_decision_ui_v5v2_exact16_manifest_runner_contract_correction_gate as validator


class ManifestRunnerContractCorrectionGateTests(unittest.TestCase):
    def test_live_passes(self) -> None:
        self.assertFalse(validator.validate()["gpu_executed"])

    def reject(self, update) -> None:
        value = json.loads(validator.DEFAULT.read_text())
        update(value)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad.json"
            path.write_text(json.dumps(value))
            with self.assertRaises(RuntimeError):
                validator.validate(path)

    def test_runner_change_rejected(self) -> None:
        self.reject(lambda value: value["diagnosis"].update(
            runner_source_change_required=True))

    def test_wrong_field_rejected(self) -> None:
        self.reject(lambda value: value["required_correction"].update(
            source_manifest_add_top_level_field="visual_design_gate_sha256"))

    def test_wrong_gate_rejected(self) -> None:
        self.reject(lambda value: value["required_correction"].update(
            source_manifest_gate_sha256="0" * 64))

    def test_extra_allowed_file_rejected(self) -> None:
        self.reject(lambda value: value["allowed_new_files"].append("CMakeLists.txt"))

    def test_parity_relaxation_rejected(self) -> None:
        self.reject(lambda value: value["parity_test_requirements"].update(
            parse_real_frozen_v2_signed_authorization=False))

    def test_gpu_acceptance_rejected(self) -> None:
        self.reject(lambda value: value["acceptance"].update(gpu_executed=True))


if __name__ == "__main__":
    unittest.main()
