from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest

from scripts import prepare_phase2_v10i_formal_full_six_reentry_ready_v8 as prep
from scripts import validate_phase2_v10i_formal_full_six_reentry_ready_v8 as validator
from scripts import validate_phase2_v10i_formal_six_scene_ready_v2 as base


class Phase2FullSixReadinessV8Tests(unittest.TestCase):
    def generate(self, root: Path):
        files = prep.prepare(root)
        self.assertEqual(validator.validate(root)["not_run_scene_count"], 6)
        return files

    def test_fresh_exact_six_no_gpu_no_auth(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            files = self.generate(Path(directory) / "ready")
            ready = json.loads(files["readiness_manifest"].read_text())
            self.assertFalse(ready["gpu_capture_authorized"])
            self.assertFalse(ready["capture_output_created"])
            self.assertFalse(prep.FUTURE_OUTPUT_ROOT.exists())
            self.assertFalse(any(Path(directory).rglob("*.ppm")))

    def test_dispatch_binds_correct_flag_gate_and_failure(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            files = self.generate(Path(directory) / "ready")
            dispatch = json.loads(files["dispatch_contract"].read_text())
            self.assertEqual(dispatch["capture_driver_asset_option"], "--asset-package")
            self.assertTrue(dispatch["real_parser_contract_test_required"])
            implementation = json.loads(files["implementation_gate"].read_text())
            self.assertEqual(implementation["dispatcher_cli_correction_gate"]["sha256"],
                             prep.CORRECTION_GATE_SHA256)
            self.assertEqual(implementation["frozen_v7_failure"]["sha256"],
                             prep.V7_FAILURE_SHA256)

    def test_runtime_binary_capture_source_assets_are_unchanged(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            ready = json.loads(self.generate(Path(directory) / "ready")[
                "readiness_manifest"].read_text())
            self.assertEqual(ready["fresh_release_executable"]["sha256"],
                             "042a64496195d453f11e0a19770fa207ec448862d0a8396ce98cdaa0f0c017c1")
            self.assertEqual(ready["source_bindings"][
                "scripts/capture_visual_baselines.py"]["sha256"],
                "772ecc72a9ade0234413fbd87425fe8ddd810a85255b3f8274556eb8ecae3b89")
            self.assertEqual(ready["self_contained_asset_package"]["sha256"],
                             prep.PACKAGE_MANIFEST_SHA256)

    def test_authorization_template_not_issued(self) -> None:
        auth = prep.authorization_template()
        self.assertEqual(auth["status"], "NOT_ISSUED")
        self.assertEqual(auth["authorized_capture_count"], 0)
        self.assertFalse(auth["gpu_capture_authorized"])

    def test_validator_rejects_order_threshold_claim_and_flag_drift(self) -> None:
        for mutation in ("order", "threshold", "claim", "flag"):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory() as directory:
                root = Path(directory) / "ready"
                files = self.generate(root)
                if mutation in ("order", "threshold"):
                    path = files["six_scene_plan"]
                    value = json.loads(path.read_text())
                    if mutation == "order":
                        value["rows"][0], value["rows"][1] = value["rows"][1], value["rows"][0]
                    else:
                        value["capture_contract"]["minimum_interest_region_visible_pixels"] = 0
                elif mutation == "claim":
                    path = files["readiness_manifest"]
                    value = json.loads(path.read_text()); value["formal_pass"] = True
                else:
                    path = files["dispatch_contract"]
                    value = json.loads(path.read_text())
                    value["capture_driver_asset_option"] = "--formal-asset-package"
                path.write_text(json.dumps(value))
                with self.assertRaises(base.ValidationError):
                    validator.validate(root)


if __name__ == "__main__":
    unittest.main()
