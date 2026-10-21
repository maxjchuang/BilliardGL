import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from scripts import capture_visual_baselines as capture
from scripts import prepare_phase2_v10i_formal_six_scene_ready_v3 as ready
from scripts import validate_phase2_v10i_formal_six_scene_ready_v3 as validator


class Phase2V10iFormalSixSceneReadinessV3Tests(unittest.TestCase):
    def executable_authorization(self, readiness_sha="a" * 64):
        return {
            "schema": "phase2_v10i_formal_six_scene_one_shot_authorization_v3",
            "status": "AUTHORIZED_FOR_EXACTLY_SIX_FRESH_PROCESS_CAPTURES",
            "authorization_id": "test-only",
            "authorized_by": "test-only",
            "authorized_at_utc": "2026-08-11T00:00:00Z",
            "authorized_output_canonical_path":
                str(Path("/tmp/formal-v3-capture").resolve()),
            "gpu_capture_authorized": True,
            "formal_six_scene_evidence_authorized": True,
            "authorized_capture_count": 6,
            "consumed_capture_count": 0,
            "scene_ids_in_order": list(ready.SCENE_IDS),
            "required_readiness_manifest_sha256": readiness_sha,
            "formal_pass": False,
            "golden_promotion": False,
            "m2_promotion": False,
        }

    def test_v2_lineage_is_exact(self):
        self.assertEqual(ready.v2.sha256(ready.V2_READINESS),
                         ready.V2_READINESS_SHA256)
        self.assertEqual(ready.V2_READINESS_SHA256,
                         "8e3bd17da6aba3d3fb56678cf220dcabe0f394cc5b358c6d934a9d6dfadb5a61")

    def test_formal_runner_explicitly_selects_modern_renderer(self):
        source = Path(capture.__file__).read_text(encoding="utf-8")
        self.assertIn('["--renderer", "modern"]', source)
        self.assertIn("extra_args=automation_extra_args", source)

    def test_v3_authorization_schema_is_accepted_but_bad_sha_rejected(self):
        value = self.executable_authorization()
        capture.validate_formal_authorization(
            value, "a" * 64, Path("/tmp/formal-v3-capture"))
        value["required_readiness_manifest_sha256"] = "b" * 64
        with self.assertRaisesRegex(RuntimeError, "readiness SHA"):
            capture.validate_formal_authorization(
                value, "a" * 64, Path("/tmp/formal-v3-capture"))
        value["required_readiness_manifest_sha256"] = "a" * 64
        with self.assertRaisesRegex(RuntimeError, "output path mismatch"):
            capture.validate_formal_authorization(
                value, "a" * 64, Path("/tmp/different-formal-v3-capture"))
        value["authorization_id"] = None
        with self.assertRaisesRegex(RuntimeError, "identity/time"):
            capture.validate_formal_authorization(
                value, "a" * 64, Path("/tmp/formal-v3-capture"))

    def test_missing_authorization_rejects_before_output_and_process(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "capture"
            with mock.patch.object(capture, "AutomationClient") as client:
                with self.assertRaisesRegex(RuntimeError, "authorization is required"):
                    capture.capture_all(
                        Path("unused"), output,
                        formal_asset_package=ready.v2.PACKAGE_ROOT)
            client.assert_not_called()
            self.assertFalse(output.exists())

    def test_generated_v3_validates_and_remains_not_run(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            executable = root / "Billiards"
            executable.write_bytes(b"fresh-release-v3-test-binding")
            files = ready.prepare(root / "ready", root / "package", executable)
            result = validator.validate(root / "ready")
            self.assertEqual(result["status"], "PASS_CPU_ONLY_READINESS_V3")
            self.assertEqual(result["closed_v2_drift_count"], 1)
            plan = json.loads(files["six_scene_plan"].read_text())
            self.assertEqual(plan["capture_contract"]["automation_extra_args"],
                             ["--renderer", "modern"])
            self.assertEqual([row["status"] for row in plan["rows"]],
                             ["NOT_RUN"] * 6)
            authorization = json.loads(files["authorization_template"].read_text())
            self.assertEqual(authorization["status"], "NOT_ISSUED")
            self.assertFalse(authorization["gpu_capture_authorized"])

    def test_preparer_cannot_start_process_or_gpu(self):
        source = Path(ready.__file__).read_text(encoding="utf-8")
        for forbidden in ("import subprocess", "Popen(", "AutomationClient",
                          "Cocoa", "OpenGL", "screenshot("):
            self.assertNotIn(forbidden, source)


if __name__ == "__main__":
    unittest.main()
