from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from scripts import prepare_phase2_v10i_formal_full_six_reentry_ready_v6 as prep
from scripts import validate_phase2_v10i_formal_full_six_reentry_ready_v6 as validator
from scripts import validate_phase2_v10i_formal_six_scene_ready_v2 as base


class Phase2FullSixReentryReadinessV6Tests(unittest.TestCase):
    def generate(self, root: Path) -> dict[str, Path]:
        executable = prep.ROOT / "build-integrated-phase4-ownership-pairing-v7/Billiards"
        with mock.patch.object(prep, "DEFAULT_EXECUTABLE", executable):
            result = prep.prepare(root, executable=executable)
            validated = validator.validate(root)
        self.assertEqual(validated["status"],
                         "PASS_CPU_ONLY_FULL_SIX_REENTRY_READINESS_V6")
        return result

    def test_generated_readiness_is_exact_six_not_run_and_gpu_free(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            files = self.generate(Path(directory) / "ready")
            plan = json.loads(files["six_scene_plan"].read_text())
            self.assertEqual([row["scene_id"] for row in plan["rows"]],
                             list(prep.SCENE_IDS))
            self.assertTrue(all(row["status"] == "NOT_RUN" for row in plan["rows"]))
            self.assertTrue(all(row["expected_presentation"]["transitioning"] is False
                                for row in plan["rows"]))
            self.assertFalse(any(Path(directory).rglob("*.ppm")))
            self.assertFalse(any(Path(directory).rglob("*.png")))

    def test_all_rows_bind_registry_recipe_instant_anchor_semantics_and_roi(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            files = self.generate(Path(directory) / "ready")
            rows = json.loads(files["six_scene_plan"].read_text())["rows"]
            for row, scene in zip(rows, prep.FORMAL_MODERN_VISUAL_SCENES):
                self.assertEqual(row["recipe"], prep.recipe(scene))
                self.assertTrue(any(item["command"] == "set_camera_preset" and
                                    item["params"].get("instant") is True
                                    for item in row["recipe"]))
                state, preset = prep.REGISTRY_CAMERA[scene.scene_id]
                self.assertEqual(row["expected_presentation"]["state"], state)
                self.assertEqual(row["expected_presentation"]["preset"], preset)
                self.assertEqual(row["interest_region"], prep.region(scene))

    def test_threshold_contract_is_exact_and_unrelaxed(self) -> None:
        contract = prep.capture_contract()
        self.assertEqual(contract["resolution"], {"width": 1920, "height": 1080})
        self.assertEqual(contract["msaa_samples"], 4)
        self.assertEqual(contract["minimum_whole_frame_visible_pixels"], 1)
        self.assertEqual(contract["minimum_interest_region_visible_pixels"], 1)
        self.assertEqual(contract["performance"]["warmup_frames_min"], 120)
        self.assertEqual(contract["performance"]["sample_frames_min"], 300)
        self.assertEqual(contract["performance"]["p95_total_frame_ms_max"], 16.7)
        self.assertEqual(contract["performance"]["draw_calls_complete_scene_max"], 100)
        self.assertEqual(contract["performance"]["texture_resident_mib_max"], 256)
        self.assertFalse(contract["asset_attestation"]["silent_material_fallback_allowed"])
        self.assertFalse(contract["asset_attestation"]["load_errors_allowed"])

    def test_authorization_template_is_non_executable(self) -> None:
        value = prep.authorization_template()
        self.assertEqual(value["status"], "NOT_ISSUED")
        self.assertEqual(value["authorized_capture_count"], 0)
        self.assertEqual(value["consumed_capture_count"], 0)
        self.assertFalse(value["gpu_capture_authorized"])
        self.assertFalse(value["formal_six_scene_evidence_authorized"])

    def test_validator_rejects_mutated_scene_order(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "ready"
            files = self.generate(root)
            plan = json.loads(files["six_scene_plan"].read_text())
            plan["rows"][0], plan["rows"][1] = plan["rows"][1], plan["rows"][0]
            files["six_scene_plan"].write_text(json.dumps(plan), encoding="utf-8")
            with self.assertRaises(base.ValidationError):
                validator.validate(root)

    def test_validator_rejects_threshold_relaxation(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "ready"
            files = self.generate(root)
            plan = json.loads(files["six_scene_plan"].read_text())
            plan["capture_contract"]["minimum_interest_region_visible_pixels"] = 0
            files["six_scene_plan"].write_text(json.dumps(plan), encoding="utf-8")
            with self.assertRaises(base.ValidationError):
                validator.validate(root)

    def test_validator_rejects_evidence_promotion(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "ready"
            files = self.generate(root)
            ready = json.loads(files["readiness_manifest"].read_text())
            ready["formal_pass"] = True
            files["readiness_manifest"].write_text(json.dumps(ready), encoding="utf-8")
            with self.assertRaises(base.ValidationError):
                validator.validate(root)


if __name__ == "__main__":
    unittest.main()
