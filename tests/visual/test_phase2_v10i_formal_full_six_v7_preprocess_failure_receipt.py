from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import validate_phase2_v10i_formal_full_six_v7_preprocess_failure_receipt as validator


class V7PreprocessFailureReceiptTests(unittest.TestCase):
    def test_frozen_receipt_passes(self) -> None:
        self.assertEqual(validator.validate()["not_run_count"], 6)

    def reject(self, mutate) -> None:
        value = json.loads(validator.DEFAULT_RECEIPT.read_text())
        mutate(value)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "receipt.json"
            path.write_text(json.dumps(value))
            with self.assertRaises(RuntimeError):
                validator.validate(path)

    def test_rejects_auth_sha_drift(self) -> None:
        self.reject(lambda value: value["authorization"].__setitem__("sha256", "0" * 64))

    def test_rejects_marker_reuse_promotion(self) -> None:
        self.reject(lambda value: value["consumed_marker"].__setitem__(
            "must_never_be_deleted_or_reused", False))

    def test_rejects_output_exists_claim(self) -> None:
        self.reject(lambda value: value["execution_boundary"].__setitem__(
            "output_root_exists", True))

    def test_rejects_candidate_process_promotion(self) -> None:
        self.reject(lambda value: value["execution_boundary"].__setitem__(
            "candidate_billiards_process_started", True))

    def test_rejects_scene_status_promotion(self) -> None:
        self.reject(lambda value: value["scene_results"][0].__setitem__("status", "PASS"))

    def test_rejects_formal_claim_promotion(self) -> None:
        self.reject(lambda value: value["claims"].__setitem__("formal_pass", True))

    def test_rejects_cli_root_cause_rewrite(self) -> None:
        self.reject(lambda value: value["runtime_contract"].__setitem__(
            "dispatcher_emitted_option", "--asset-package"))


if __name__ == "__main__":
    unittest.main()
