from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest

from scripts import prepare_phase2_v10i_formal_full_six_reentry_ready_v7 as prep
from scripts import validate_phase2_v10i_formal_full_six_reentry_ready_v7 as validator
from scripts import validate_phase2_v10i_formal_six_scene_ready_v2 as base


class Phase2FullSixReentryReadinessV7Tests(unittest.TestCase):
    def generate(self, root: Path):
        result = prep.prepare(root, executable=prep.DEFAULT_EXECUTABLE)
        self.assertEqual(validator.validate(root)["status"],
                         "PASS_CPU_ONLY_FULL_SIX_REENTRY_READINESS_V7")
        return result

    def test_exact_six_not_run_gpu_free_current_source(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            files = self.generate(Path(directory) / "ready")
            ready = json.loads(files["readiness_manifest"].read_text())
            plan = json.loads(files["six_scene_plan"].read_text())
            self.assertEqual(ready["integrated_phase4_source_state"],
                             "PHASE4_V8_SOURCE_STABLE")
            self.assertEqual([row["scene_id"] for row in plan["rows"]],
                             list(prep.SCENE_IDS))
            self.assertTrue(all(row["status"] == "NOT_RUN" for row in plan["rows"]))
            self.assertFalse(any(Path(directory).rglob("*.ppm")))

    def test_all_rows_have_exact_instant_semantic_query_and_roi(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            rows = json.loads(self.generate(Path(directory) / "ready")[
                "six_scene_plan"].read_text())["rows"]
            for row, scene in zip(rows, prep.FORMAL_MODERN_VISUAL_SCENES):
                self.assertEqual(row["recipe"], prep.recipe(scene))
                self.assertTrue(any(command["command"] == "set_camera_preset" and
                                    command["params"].get("instant") is True
                                    for command in row["recipe"]))
                self.assertEqual(row["presentation_query"], "get_presentation_state")
                self.assertFalse(row["expected_presentation"]["transitioning"])
                self.assertEqual(row["interest_region"], prep.region(scene))

    def test_thresholds_are_frozen(self) -> None:
        contract = prep.capture_contract()
        self.assertEqual(contract["resolution"], {"width": 1920, "height": 1080})
        self.assertEqual(contract["msaa_samples"], 4)
        self.assertEqual(contract["performance"]["warmup_frames_min"], 120)
        self.assertEqual(contract["performance"]["sample_frames_min"], 300)
        self.assertEqual(contract["performance"]["p95_total_frame_ms_max"], 16.7)
        self.assertEqual(contract["performance"]["draw_calls_complete_scene_max"], 100)
        self.assertEqual(contract["performance"]["texture_resident_mib_max"], 256)

    def test_auth_template_is_not_issued_and_output_absent(self) -> None:
        value = prep.authorization_template()
        self.assertEqual(value["status"], "NOT_ISSUED")
        self.assertEqual(value["authorized_capture_count"], 0)
        self.assertFalse(value["gpu_capture_authorized"])
        self.assertFalse(prep.FUTURE_OUTPUT_ROOT.exists())

    def test_validator_rejects_order_threshold_and_claim_mutations(self) -> None:
        mutations = ("order", "threshold", "claim")
        for mutation in mutations:
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory() as directory:
                root = Path(directory) / "ready"
                files = self.generate(root)
                if mutation == "order":
                    path = files["six_scene_plan"]
                    value = json.loads(path.read_text())
                    value["rows"][0], value["rows"][1] = value["rows"][1], value["rows"][0]
                elif mutation == "threshold":
                    path = files["six_scene_plan"]
                    value = json.loads(path.read_text())
                    value["capture_contract"]["minimum_interest_region_visible_pixels"] = 0
                else:
                    path = files["readiness_manifest"]
                    value = json.loads(path.read_text())
                    value["formal_pass"] = True
                path.write_text(json.dumps(value))
                with self.assertRaises(base.ValidationError):
                    validator.validate(root)


if __name__ == "__main__":
    unittest.main()
