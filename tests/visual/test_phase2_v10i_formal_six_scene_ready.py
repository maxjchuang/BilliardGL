import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

from scripts import prepare_phase2_v10i_formal_six_scene_ready as ready


class Phase2V10iFormalSixSceneReadinessTests(unittest.TestCase):
    def exact_hypothetical_authorization(self):
        return {
            "schema": "phase2_v10i_formal_six_scene_one_shot_authorization_v1",
            "status": "AUTHORIZED_FOR_EXACTLY_SIX_FRESH_PROCESS_CAPTURES",
            "gpu_capture_authorized": True,
            "formal_six_scene_evidence_authorized": True,
            "authorized_capture_count": 6,
            "consumed_capture_count": 0,
            "scene_ids_in_order": list(ready.SCENE_IDS),
        }

    def test_approved_scene_order_is_exact(self):
        self.assertEqual(ready.SCENE_IDS, (
            "break", "low_aim", "ball_closeup", "pocket_closeup",
            "tactical_overhead", "post_shot_observe"))

    def test_frozen_single_scene_prerequisites_are_exact(self):
        bindings = ready.validate_frozen_inputs()
        self.assertEqual(bindings["single_capture_png"]["sha256"],
                         "sha256:" + ready.EXPECTED_FROZEN_SHA256["single_capture_png"])

    def test_template_is_not_an_executable_authorization(self):
        value = ready.exact_authorization_template()
        self.assertEqual(value["status"], "BLOCKED_NOT_AUTHORIZABLE")
        self.assertFalse(value["gpu_capture_authorized"])
        self.assertFalse(value["formal_six_scene_evidence_authorized"])

    def test_missing_authorization_rejects_before_process_and_output(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "capture"
            with mock.patch.object(subprocess, "Popen") as popen:
                with self.assertRaisesRegex(ready.ReadinessError, "authorization status"):
                    ready.pretrigger_capture(ready.exact_authorization_template(), output)
            popen.assert_not_called()
            self.assertFalse(output.exists())

    def test_bad_schema_rejects_before_process_and_output(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "capture"
            value = self.exact_hypothetical_authorization()
            value["schema"] = "wrong"
            with mock.patch.object(subprocess, "Popen") as popen:
                with self.assertRaisesRegex(ready.ReadinessError, "schema mismatch"):
                    ready.pretrigger_capture(value, output)
            popen.assert_not_called()
            self.assertFalse(output.exists())

    def test_bad_scene_order_rejects_before_process_and_output(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "capture"
            value = self.exact_hypothetical_authorization()
            value["scene_ids_in_order"] = list(reversed(value["scene_ids_in_order"]))
            with mock.patch.object(subprocess, "Popen") as popen:
                with self.assertRaisesRegex(ready.ReadinessError, "scene order"):
                    ready.pretrigger_capture(value, output)
            popen.assert_not_called()
            self.assertFalse(output.exists())

    def test_bad_capture_count_rejects_before_process_and_output(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "capture"
            value = self.exact_hypothetical_authorization()
            value["authorized_capture_count"] = 1
            with mock.patch.object(subprocess, "Popen") as popen:
                with self.assertRaisesRegex(ready.ReadinessError, "capture count"):
                    ready.pretrigger_capture(value, output)
            popen.assert_not_called()
            self.assertFalse(output.exists())

    def test_even_hypothetical_exact_authorization_is_blocked_pre_cocoa(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "capture"
            with mock.patch.object(subprocess, "Popen") as popen:
                with self.assertRaisesRegex(ready.ReadinessError, "exact per-scene v10i"):
                    ready.pretrigger_capture(self.exact_hypothetical_authorization(), output)
            popen.assert_not_called()
            self.assertFalse(output.exists())

    def test_existing_capture_output_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "capture"
            output.mkdir()
            with self.assertRaisesRegex(ready.ReadinessError, "must be absent"):
                ready.pretrigger_capture(self.exact_hypothetical_authorization(), output)

    def test_generated_plan_has_exact_six_not_run_rows(self):
        with tempfile.TemporaryDirectory() as directory:
            files = ready.prepare(Path(directory) / "ready")
            plan = json.loads(files["six_scene_plan"].read_text())
            self.assertEqual([row["scene_id"] for row in plan["rows"]], list(ready.SCENE_IDS))
            self.assertTrue(all(row["status"] == "NOT_RUN" for row in plan["rows"]))
            self.assertTrue(all(row["actual_gpu"] is False for row in plan["rows"]))

    def test_generated_manual_template_promotes_full_table_blockers(self):
        with tempfile.TemporaryDirectory() as directory:
            files = ready.prepare(Path(directory) / "ready")
            manual = json.loads(files["manual_template"].read_text())
            self.assertEqual(len(manual["formal_blocking_axes_promoted_from_single_scene_outside_scope"]), 3)
            for scene in manual["scenes"]:
                axes = {axis["axis"]: axis for axis in scene["technical_art"]["axes"]}
                for axis in ready.TA_AXES[-3:]:
                    self.assertIn("blocking_policy", axes[axis])
                    self.assertEqual(axes[axis]["status"], "NOT_RUN")

    def test_generated_machine_template_contains_medium_1080_gates(self):
        with tempfile.TemporaryDirectory() as directory:
            files = ready.prepare(Path(directory) / "ready")
            machine = json.loads(files["machine_template"].read_text())
            gates = machine["performance_gates"]
            self.assertEqual(gates["profile"], "1920x1080_medium")
            self.assertEqual(gates["warmup_frames_min"], 120)
            self.assertEqual(gates["sample_frames_min"], 300)
            self.assertEqual(gates["p95_total_frame_ms_max"], 16.7)

    def test_generated_package_keeps_every_promotion_flag_false(self):
        with tempfile.TemporaryDirectory() as directory:
            files = ready.prepare(Path(directory) / "ready")
            for name in ("six_scene_plan", "machine_template", "manual_template",
                         "authorization_template", "readiness_manifest", "readiness_receipt"):
                value = json.loads(files[name].read_text())
                for field in ("actual_gpu_used", "gpu_capture_authorized", "formal_six_scene_evidence",
                              "formal_pass", "golden_promotion", "m2_promotion"):
                    self.assertIs(value[field], False, name + ":" + field)


if __name__ == "__main__":
    unittest.main()
