import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

import run_phase2_pocket_v10i_gpu_preflight as runner  # noqa: E402


class Phase2PocketV10iGpuRunnerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.executable = runner.DEFAULT_BUILD / "BilliardsModernRendererSliceSmoke"

    def authorization_document(self):
        return {
            "schema": "phase2_pocket_v10i_root_one_shot_gpu_authorization_v1",
            "status": "AUTHORIZED_FOR_EXACTLY_ONE_GPU_CAPTURE",
            "authorization_id": "unit-test-not-executed",
            "authorized_by": "root", "gpu_capture_authorized": True,
            "authorized_capture_count": 1, "consumed_capture_count": 0,
            "scene_id": "pocket_closeup", "width": 1920, "height": 1080,
            "quality": "medium", "msaa_samples": 4,
            "evidence_scope": runner.EVIDENCE_SCOPE,
            "additional_capture_authorized": False,
            "ta_review_authorized": False,
            "formal_six_scene_evidence_authorized": False,
            "formal_pass_authorized": False, "golden_promotion_authorized": False,
            "bindings": {
                "ready_manifest": runner.sha256(runner.READY_MANIFEST),
                "ready_receipt": runner.sha256(runner.READY_RECEIPT),
                "runner_source": runner.sha256(Path(runner.__file__).resolve()),
                "renderer_executable": runner.sha256(self.executable),
                "gate": "sha256:" + runner.EXPECTED["gate"],
                "offline_report": "sha256:" + runner.EXPECTED["offline_report"],
                "offline_freeze": "sha256:" + runner.EXPECTED["offline_freeze"],
                "gltf": "sha256:" + runner.EXPECTED["gltf"],
                "asset_manifest": "sha256:" + runner.EXPECTED["asset_manifest"],
            },
        }

    def test_fresh_binary_exists_and_is_newer_than_sources(self):
        self.assertTrue(self.executable.is_file())
        self.assertTrue(os.access(self.executable, os.X_OK))
        source_mtime = max((ROOT / "src/Billiards/render/modern/modern_renderer.cpp").stat().st_mtime_ns,
                           (ROOT / "spikes/render_core_profile/modern_renderer_slice_smoke.mm").stat().st_mtime_ns)
        self.assertGreaterEqual(self.executable.stat().st_mtime_ns, source_mtime)

    def test_readiness_package_is_gpu_false_and_all_checks_pass(self):
        manifest = json.loads(runner.READY_MANIFEST.read_text())
        receipt = json.loads(runner.READY_RECEIPT.read_text())
        self.assertEqual("READY_FOR_ROOT_ONE_SHOT_AUTHORIZATION_GPU_NOT_AUTHORIZED",
                         manifest["status"])
        self.assertTrue(all(manifest["readiness_checks"].values()))
        self.assertFalse(manifest["actual_gpu_used"])
        self.assertFalse(manifest["gpu_capture_authorized"])
        self.assertEqual(runner.sha256(runner.READY_MANIFEST), receipt["ready_manifest_sha256"])

    def test_missing_authorization_fails_before_mock_renderer_start(self):
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            with mock.patch.object(runner.subprocess, "run") as run_mock:
                with self.assertRaisesRegex(RuntimeError, "authorization file is missing"):
                    runner.pretrigger_checks(runner.ASSET_ROOT, self.executable,
                                             temporary / "output", temporary / "freeze",
                                             temporary / "missing-authorization.json")
                run_mock.assert_not_called()

    def test_bad_authorization_json_fails_closed(self):
        manifest = json.loads(runner.READY_MANIFEST.read_text())
        receipt = json.loads(runner.READY_RECEIPT.read_text())
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad.json"
            bad = self.authorization_document(); bad["authorized_capture_count"] = 2
            path.write_text(json.dumps(bad))
            with self.assertRaisesRegex(RuntimeError, "field mismatch"):
                runner._verify_authorization(path, manifest, receipt, self.executable)

    def test_exact_mock_authorization_parses_without_starting_gpu(self):
        manifest = json.loads(runner.READY_MANIFEST.read_text())
        receipt = json.loads(runner.READY_RECEIPT.read_text())
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "mock-valid-not-consumed.json"
            path.write_text(json.dumps(self.authorization_document()))
            with mock.patch.object(runner.subprocess, "run") as run_mock:
                parsed = runner._verify_authorization(path, manifest, receipt, self.executable)
                self.assertEqual(1, parsed["authorized_capture_count"])
                run_mock.assert_not_called()

    def test_existing_single_use_output_fails_before_authorization(self):
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory); output = temporary / "output"; output.mkdir()
            with self.assertRaisesRegex(RuntimeError, "single-use GPU evidence"):
                runner.pretrigger_checks(runner.ASSET_ROOT, self.executable, output,
                                         temporary / "freeze", temporary / "missing.json")

    def test_canonical_json_metadata_discards_stale_claims(self):
        raw = {"renderer":"modern","backend":"OpenGL 4.1 Core","width":1920,
               "height":1080,"quality":"medium","msaa_samples":4,
               "camera":{"eye_cm":[115,215,175],"target_cm":[50,89,95],"vertical_fov_degrees":30},
               "warmup_frames":120,"sample_frames":300,"pocket_only_ui_pass_draw_calls":0,
               "full_frame_ui_delta_nonzero_pixels":0,"p95_total_frame_ms":2.5,
               "draw_calls_max":62,"triangles_max":14030,"texture_resident_mib":27.0,
               "measurement_method":"gpu_timer_query_plus_cpu_frame",
               "visual_review_status":"PENDING_TA","evidence_scope":"v10c"}
        result = runner.canonicalize_metrics(raw)
        serialized = json.dumps(result)
        self.assertEqual("NOT_RUN", result["visual_review_status"])
        self.assertFalse(result["ta_review_authorized"])
        self.assertNotIn("PENDING_TA", serialized)
        self.assertNotIn("v10c", serialized)

    def test_canonical_json_rejects_missing_fields(self):
        with self.assertRaisesRegex(RuntimeError, "incomplete"):
            runner.canonicalize_metrics({})

    def test_runner_has_no_resume_or_six_scene_path(self):
        source = Path(runner.__file__).read_text()
        self.assertNotIn("resume-existing-capture", source)
        self.assertNotIn("tactical_overhead", source)
        self.assertNotIn("post_shot_observe", source)
        self.assertIn("root one-shot authorization file is missing", source)


if __name__ == "__main__":
    unittest.main()
