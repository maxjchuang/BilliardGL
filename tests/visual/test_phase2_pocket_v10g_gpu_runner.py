import json
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

from run_phase2_pocket_v10g_gpu_preflight import (  # noqa: E402
    ASSET_ROOT, EVIDENCE_SCOPE, EXPECTED, OFFLINE_FREEZE,
    _verify_freeze_receipt, canonicalize_metrics, pretrigger_checks, sha256,
)


class Phase2PocketV10gGpuRunnerTests(unittest.TestCase):
    def test_all_root_authorized_frozen_bindings_are_exact(self):
        paths = {
            "gate": ROOT / "docs/art/phase2-pocket-v10g-offline-correction-design-gate-v2.json",
            "offline_report": ROOT / "docs/art/phase2-pocket-v10g-offline-preflight-v2/phase2-pocket-v10g-offline-report-v2.json",
            "offline_freeze": OFFLINE_FREEZE,
            "gltf": ASSET_ROOT / "focused_practice_hall_modular_v1.gltf",
            "asset_manifest": ASSET_ROOT / "focused_practice_hall_modular_v1.asset.json",
            "renderer_source": ROOT / "src/Billiards/render/modern/modern_renderer.cpp",
            "smoke_source": ROOT / "spikes/render_core_profile/modern_renderer_slice_smoke.mm",
        }
        for name, path in paths.items():
            self.assertEqual(sha256(path), "sha256:" + EXPECTED[name], name)

    def test_offline_freeze_is_still_11_of_11_byte_identical(self):
        receipt = _verify_freeze_receipt(OFFLINE_FREEZE)
        self.assertEqual(receipt["binding_count"], 11)
        self.assertTrue(receipt["all_original_and_snapshot_sha256_match"])

    def test_canonical_metrics_discard_stale_smoke_metadata(self):
        raw = {
            "evidence_scope": "phase2_pocket_v10c_single_scene_non_formal",
            "visual_review_status": "PENDING_TA",
            "renderer": "modern", "backend": "OpenGL 4.1 Core",
            "width": 1920, "height": 1080, "quality": "medium",
            "msaa_samples": 4,
            "camera": {"eye_cm": [115, 215, 175], "target_cm": [50, 89, 95],
                       "vertical_fov_degrees": 30},
            "warmup_frames": 120, "sample_frames": 300,
            "pocket_only_ui_pass_draw_calls": 0,
            "full_frame_ui_delta_nonzero_pixels": 0,
            "p95_total_frame_ms": 2.5, "draw_calls_max": 10,
            "triangles_max": 100, "texture_resident_mib": 20,
            "measurement_method": "gpu_timer_query_plus_cpu_frame",
        }
        result = canonicalize_metrics(raw)
        serialized = json.dumps(result)
        self.assertEqual(result["evidence_scope"], EVIDENCE_SCOPE)
        self.assertEqual(result["visual_review_status"], "NOT_RUN")
        self.assertEqual(result["actual_gpu_capture_count"], 1)
        self.assertFalse(result["ta_review_authorized"])
        self.assertNotIn("v10c", serialized)
        self.assertNotIn("PENDING_TA", serialized)

    def test_canonical_metrics_fail_closed_on_missing_runtime_field(self):
        with self.assertRaisesRegex(RuntimeError, "raw smoke metrics are incomplete"):
            canonicalize_metrics({})

    def test_existing_output_consumes_single_use_path_before_gpu(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-v10g-runner-guard-") as directory:
            temporary = Path(directory)
            output = temporary / "already_exists"
            output.mkdir()
            with self.assertRaisesRegex(RuntimeError, "single-use GPU evidence"):
                pretrigger_checks(ASSET_ROOT, temporary / "missing-executable",
                                  output, temporary / "freeze")

    def test_runner_source_has_no_resume_or_formal_capture_path(self):
        source = (ROOT / "scripts/run_phase2_pocket_v10g_gpu_preflight.py").read_text()
        self.assertNotIn("resume-existing-capture", source)
        self.assertNotIn("tactical_overhead", source)
        self.assertNotIn("post_shot_observe", source)
        self.assertIn('status = "MACHINE_PASS_PENDING_ROOT_AND_TA"', source)
        self.assertIn('else "REJECTED_MACHINE"', source)


if __name__ == "__main__":
    unittest.main()
