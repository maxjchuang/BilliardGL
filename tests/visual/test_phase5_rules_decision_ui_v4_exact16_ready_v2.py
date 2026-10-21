#!/usr/bin/env python3
"""Mutation tests for Phase5 v4 exact16 dispatcher readiness v2."""

from __future__ import annotations

import copy
import json
import os
from pathlib import Path
import shutil
import tempfile
import unittest

from scripts import validate_phase5_rules_decision_ui_v4_exact16_ready_v2 as validator


class ReadyV2Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        value = os.environ.get("PHASE5_V4_EXACT16_READY_V2")
        if not value:
            raise unittest.SkipTest("PHASE5_V4_EXACT16_READY_V2 required")
        cls.root = Path(value).resolve()

    def test_live_readiness_passes(self) -> None:
        self.assertEqual(validator.validate(self.root)["case_count"], 16)

    def mutate(self, update) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / "ready"
            shutil.copytree(self.root, root)
            path = root / "rules-decision-ui-v4-exact16-ready.json"
            value = json.loads(path.read_text())
            update(value)
            path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")
            with self.assertRaises(RuntimeError):
                validator.validate(root)

    def test_rejects_case_scope_drift(self) -> None:
        self.mutate(lambda value: value.__setitem__("case_count", 15))

    def test_rejects_not_run_promotion(self) -> None:
        self.mutate(lambda value: value["case_results"][0].__setitem__("status", "PASS"))

    def test_rejects_false_producer_binding_promotion(self) -> None:
        self.mutate(lambda value: value["dispatch_gate"].__setitem__(
            "compatible_capture_producer_bound", True))

    def test_rejects_auth_template_issuance(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / "ready"
            shutil.copytree(self.root, root)
            path = root / "rules-decision-ui-v4-authorization-template-not-issued.json"
            value = json.loads(path.read_text())
            value["authorized"] = True
            path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")
            with self.assertRaises(RuntimeError):
                validator.validate(root)


if __name__ == "__main__":
    unittest.main()
