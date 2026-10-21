from __future__ import annotations

import json
from pathlib import Path
import shutil
import tempfile
import unittest

from scripts import validate_phase5_rules_decision_ui_v5_pixel_visible_exact16_ready as validator


class PixelVisibleExact16ReadyTests(unittest.TestCase):
    def test_live_readiness_passes(self) -> None:
        result = validator.validate()
        self.assertEqual(result["case_count"], 16)
        self.assertFalse(result["gpu_executed"])

    def mutate(self, filename: str, update) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "ready"
            shutil.copytree(validator.prep.OUTPUT, root)
            path = root / filename
            value = json.loads(path.read_text(encoding="utf-8"))
            update(value)
            path.write_text(json.dumps(value), encoding="utf-8")
            with self.assertRaises(RuntimeError):
                validator.validate(root)

    def test_case_promotion_rejected(self) -> None:
        self.mutate("rules-decision-ui-v5-exact16-ready.json",
                    lambda value: value["case_results"][0].update(status="PASS"))

    def test_extra_lineage_allowlist_rejected(self) -> None:
        self.mutate("rules-decision-ui-v5-exact16-ready.json",
                    lambda value: value["successor_lineage"][
                        "approved_production_drift_only"].append({
                            "path": "src/Billiards/rules.cpp",
                            "pre_implementation_sha256": "0" * 64,
                            "post_implementation_sha256": "1" * 64}))

    def test_unapproved_drift_claim_rejected(self) -> None:
        self.mutate("rules-decision-ui-v5-exact16-ready.json",
                    lambda value: value["successor_lineage"].update(
                        unapproved_drift_count=1))

    def test_cpu_raster_sha_rejected(self) -> None:
        self.mutate("rules-decision-ui-v5-final-cpu-raster-manifest.json",
                    lambda value: value["files"][0].update(sha256="0" * 64))

    def test_auth_issuance_rejected(self) -> None:
        self.mutate("rules-decision-ui-v5-authorization-template-not-issued.json",
                    lambda value: value.update(authorized=True))

    def test_runner_binary_drift_rejected(self) -> None:
        self.mutate("rules-decision-ui-v5-exact16-ready.json",
                    lambda value: value["frozen_bindings"].update(
                        runner_binary_sha256="0" * 64))

    def test_dispatcher_accepts_successor_dry_run_identity(self) -> None:
        ready = validator.prep.OUTPUT / "rules-decision-ui-v5-exact16-ready.json"
        value = validator.dispatch.validate_ready(ready)
        self.assertEqual(value["case_count"], 16)

    def test_bad_successor_status_rejected_by_dispatcher(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            source = validator.prep.OUTPUT / "rules-decision-ui-v5-exact16-ready.json"
            value = json.loads(source.read_text(encoding="utf-8"))
            value["status"] = "AUTHORIZED"
            path = Path(directory) / "bad-ready.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            with self.assertRaises(RuntimeError):
                validator.dispatch.validate_ready(path)


if __name__ == "__main__":
    unittest.main()
