import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
DOCS = ROOT / "docs/art"
CAPTURE_TEMPLATE = DOCS / "phase3-gpu-capture-matrix-v1.pending.json"
MANUAL_TEMPLATE = DOCS / "phase3-gpu-manual-review-v1.pending.json"
CAPTURE_SCHEMA = DOCS / "phase3-gpu-capture-manifest-v1.schema.json"
MANUAL_SCHEMA = DOCS / "phase3-gpu-manual-review-v1.schema.json"
GENERATOR = ROOT / "scripts/generate_phase3_gpu_capture_templates.py"
VALIDATOR = ROOT / "scripts/validate_phase3_gpu_capture_evidence.py"

sys.path.insert(0, str(ROOT / "scripts"))
from generate_phase3_gpu_capture_templates import MANUAL_AXES, SHELL_SCENARIOS  # noqa: E402
from run_phase3_gpu_capture import (  # noqa: E402
    _asset_binding_from_rows,
    _path_for_report,
)
from validate_phase3_gpu_capture_evidence import (  # noqa: E402
    manual_acceptance_errors,
    validate_capture_manifest,
    validate_manual_review,
    validate_pending_capture_template,
    validate_pending_manual_template,
)


def passing_capture_without_files():
    value = json.loads(CAPTURE_TEMPLATE.read_text())
    value["status"] = "PASS"
    value["runtime_attestation"] = {
        "git_revision": "0123456789abcdef", "executable_sha256": "sha256:" + "1" * 64,
        "renderer": "modern_opengl_core", "gpu_vendor": "Fixture GPU Vendor",
        "gpu_device": "Fixture GPU Device", "driver": "fixture-driver-1",
        "operating_system": "fixture-os", "capture_utc": "2026-07-27T15:00:00Z",
    }
    for entry in value["captures"]:
        digest = hashlib.sha256(entry["capture_id"].encode()).hexdigest()
        entry["artifact"] = {
            "path": "captures/" + entry["capture_id"] + ".png",
            "sha256": "sha256:" + digest, "actual_gpu_capture": True,
            "capture_method": "rendered_automation_gpu_readback",
        }
        entry["machine_status"] = "PASS"
        entry["machine_metrics"] = {
            "safe_area_violations": 0, "truncated_text_runs": 0, "overlap_pairs": 0,
            "minimum_rendered_text_px": entry["minimum_text_physical_px"],
            "expanded_icon_count": entry["minimum_expanded_icons"],
            "opaque_icon_count": 0, "unexpanded_icon_count": 0,
            "focus_indicator_count": entry["minimum_focus"],
            "selected_indicator_count": entry["minimum_selected"],
            "error_indicator_count": entry["minimum_error"],
            "disabled_shape_count": entry["minimum_disabled"],
            "debug_node_count": 0, "glyph_rasterizations_during_frame": 0,
        }
        entry["performance"] = {
            "warmup_frames": 120, "sample_frames": 300,
            "p95_total_frame_ms": 16.7, "draw_calls_complete_scene": 100,
            "texture_resident_mib": 256,
            "measurement_method": "gpu_timer_query_plus_cpu_frame",
        }
    return value


def completed_manual(capture):
    value = json.loads(MANUAL_TEMPLATE.read_text())
    value["status"] = "PASS"
    capture_bytes = (json.dumps(capture, indent=2, sort_keys=True) + "\n").encode()
    value["capture_binding"] = {
        "manifest_path": "fixture-capture.json",
        "manifest_sha256": "sha256:" + hashlib.sha256(capture_bytes).hexdigest(),
        "machine_status_required": "PASS_ALL_594",
    }
    hashes = {entry["capture_id"]: entry["artifact"]["sha256"]
              for entry in capture["captures"]}
    for review in value["capture_reviews"]:
        capture_id = review["capture_id"]
        review["screenshot_sha256"] = hashes[capture_id]
        surface = capture_id.split("__", 1)[0]
        for axis in MANUAL_AXES:
            status = "PASS"
            if surface == "hud" and axis == "focus_selected_error_disabled_clarity":
                status = "N/A"
            if surface == "shell" and axis == "camera_preset_consistency":
                status = "N/A"
            review["axes"][axis] = {
                "status": status,
                "evidence": "Inspected the exact SHA-bound GPU capture at native resolution.",
            }
    return value


class Phase3GpuCaptureEvidenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.pending_capture = json.loads(CAPTURE_TEMPLATE.read_text())
        cls.pending_manual = json.loads(MANUAL_TEMPLATE.read_text())
        cls.passing_capture = passing_capture_without_files()

    def test_committed_pending_templates_are_exact_and_not_acceptance(self):
        self.assertEqual(validate_pending_capture_template(self.pending_capture), [])
        self.assertEqual(validate_pending_manual_template(self.pending_manual), [])
        self.assertEqual(self.pending_capture["status"], "PENDING")
        self.assertEqual(self.pending_manual["status"], "PENDING")
        self.assertEqual(len(self.pending_capture["captures"]), 594)
        self.assertEqual(len(self.pending_manual["capture_reviews"]), 594)

    def test_matrix_counts_and_required_cross_products(self):
        contract = self.pending_capture["matrix_contract"]
        self.assertEqual(contract["hud_capture_count"], 360)
        self.assertEqual(contract["shell_capture_count"], 234)
        self.assertEqual(contract["total_capture_count"], 594)
        self.assertEqual(len(contract["hud_states"]), 5)
        self.assertEqual(len(contract["camera_presets"]), 4)
        self.assertEqual(len(contract["viewports"]), 3)
        self.assertEqual(len(contract["shell_scenarios"]), 13)

    def test_stale_match_shell_case_is_replaced_by_routable_training_ready_case(self):
        self.assertNotIn("match_default", SHELL_SCENARIOS)
        self.assertEqual(SHELL_SCENARIOS["main_menu_training_available"], {
            "screen": "main_menu", "variant": "training_pack_loaded",
            "setting_category": None, "scroll_state": "not_applicable",
            "focus_state": "training_focused", "minimum_icons": 7,
            "minimum_focus": 1, "minimum_selected": 0, "minimum_error": 0,
            "minimum_disabled": 0,
        })
        replacement = [entry for entry in self.pending_capture["captures"]
                       if entry.get("scenario") == "main_menu_training_available"]
        self.assertEqual(len(replacement), 18)
        self.assertFalse(any(entry.get("scenario") == "match_default"
                             for entry in self.pending_capture["captures"]))
        self.assertEqual({entry["screen"] for entry in replacement}, {"main_menu"})
        self.assertEqual({entry["variant"] for entry in replacement},
                         {"training_pack_loaded"})
        self.assertEqual({entry["minimum_expanded_icons"] for entry in replacement}, {7})
        self.assertEqual({entry["minimum_focus"] for entry in replacement}, {1})
        self.assertEqual({entry["minimum_selected"] for entry in replacement}, {0})
        self.assertEqual({entry["minimum_error"] for entry in replacement}, {0})
        self.assertEqual({entry["minimum_disabled"] for entry in replacement}, {0})
        corrections = self.pending_capture["matrix_contract"][
            "pre_capture_contract_corrections"]
        self.assertEqual(len(corrections), 3)
        self.assertEqual(corrections[0]["classification"],
                         "pre_capture_stale_contract_correction")
        for correction in corrections:
            self.assertEqual(correction["capture_count_delta"], 0)
            self.assertFalse(correction["machine_thresholds_changed"])
            self.assertFalse(correction["evidence_status_advanced"])

    def test_unroutable_match_shell_selector_is_rejected_as_contract_drift(self):
        invalid = copy.deepcopy(self.passing_capture)
        replacement = next(entry for entry in invalid["captures"]
                           if entry.get("scenario") == "main_menu_training_available")
        replacement["screen"] = "match"
        replacement["variant"] = "default"
        replacement["scenario"] = "match_default"
        errors = validate_capture_manifest(invalid, verify_files=False)
        self.assertTrue(any("capture IDs must exactly cover all 594" in error or
                            "selector/threshold differs" in error for error in errors), errors)

    def test_training_error_and_help_focus_scenarios_match_public_reachability(self):
        self.assertNotIn("training_catalog_empty", SHELL_SCENARIOS)
        self.assertEqual(SHELL_SCENARIOS["training_catalog_invalid_pack_error"], {
            "screen": "training_catalog", "variant": "invalid_pack_error",
            "setting_category": None, "scroll_state": "not_applicable",
            "focus_state": "none_no_enabled_item", "minimum_icons": 2,
            "minimum_focus": 0, "minimum_selected": 0, "minimum_error": 1,
            "minimum_disabled": 1,
        })
        self.assertNotIn("help_disabled_reference", SHELL_SCENARIOS)
        self.assertEqual(SHELL_SCENARIOS["help_back_focused"], {
            "screen": "help", "variant": "reference_rows_disabled",
            "setting_category": None, "scroll_state": "not_applicable",
            "focus_state": "help_back_focused", "minimum_icons": 6,
            "minimum_focus": 1, "minimum_selected": 0, "minimum_error": 0,
            "minimum_disabled": 5,
        })
        training = [entry for entry in self.pending_capture["captures"]
                    if entry.get("scenario") == "training_catalog_invalid_pack_error"]
        help_entries = [entry for entry in self.pending_capture["captures"]
                        if entry.get("scenario") == "help_back_focused"]
        self.assertEqual(len(training), 18)
        self.assertEqual(len(help_entries), 18)
        self.assertEqual({entry["minimum_error"] for entry in training}, {1})
        self.assertEqual({entry["minimum_disabled"] for entry in training}, {1})
        self.assertEqual({entry["minimum_focus"] for entry in help_entries}, {1})
        self.assertEqual({entry["minimum_disabled"] for entry in help_entries}, {5})

    def test_generator_is_byte_deterministic(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-phase3-gpu-") as directory:
            completed = subprocess.run([sys.executable, str(GENERATOR), "--output-dir", directory],
                                       cwd=ROOT, text=True, capture_output=True, timeout=30)
            self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
            for source in (CAPTURE_TEMPLATE, MANUAL_TEMPLATE, CAPTURE_SCHEMA, MANUAL_SCHEMA):
                self.assertEqual((Path(directory) / source.name).read_bytes(), source.read_bytes())

    def test_report_path_is_relative_inside_repo_and_absolute_outside(self):
        inside = ROOT / "build-product-session" / "BilliardsPhase3GpuCaptureRunner"
        self.assertEqual(_path_for_report(inside),
                         "build-product-session/BilliardsPhase3GpuCaptureRunner")
        with tempfile.TemporaryDirectory(prefix="billiardgl-external-build-") as directory:
            outside = Path(directory) / "BilliardsPhase3GpuCaptureRunner"
            serialized = _path_for_report(outside)
            self.assertTrue(Path(serialized).is_absolute())
            self.assertEqual(Path(serialized), outside.resolve())
            self.assertEqual(json.loads(json.dumps({"executable": serialized})),
                             {"executable": serialized})

        class UnexpectedFailure:
            def resolve(self):
                return self

            def relative_to(self, _root):
                raise RuntimeError("unexpected_path_failure")

        with self.assertRaisesRegex(RuntimeError, "unexpected_path_failure"):
            _path_for_report(UnexpectedFailure())

    def test_asset_binding_requires_one_exact_runtime_value(self):
        binding = {
            "asset_root": "/private/tmp/phase2-v10i-formal",
            "gltf_path": "/private/tmp/phase2-v10i-formal/scene.gltf",
            "gltf_sha256": "sha256:" + "1" * 64,
            "manifest_path": "/private/tmp/phase2-v10i-formal/scene.asset.json",
            "manifest_sha256": "sha256:" + "2" * 64,
        }
        self.assertEqual(_asset_binding_from_rows([
            {"asset_binding": binding}, {"asset_binding": copy.deepcopy(binding)}
        ]), binding)
        with self.assertRaisesRegex(RuntimeError, "missing asset binding"):
            _asset_binding_from_rows([{}])
        changed = copy.deepcopy(binding)
        changed["gltf_sha256"] = "sha256:" + "3" * 64
        with self.assertRaisesRegex(RuntimeError, "asset binding changed"):
            _asset_binding_from_rows([
                {"asset_binding": binding}, {"asset_binding": changed}
            ])

    def test_pending_validator_cli_never_reports_acceptance_pass(self):
        completed = subprocess.run([
            sys.executable, str(VALIDATOR), str(CAPTURE_TEMPLATE),
            "--manual-review", str(MANUAL_TEMPLATE), "--pending-template"],
            cwd=ROOT, text=True, capture_output=True, timeout=30)
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        self.assertIn("acceptance=NOT_RUN", completed.stdout)
        self.assertNotIn("evidence PASS", completed.stdout)

    def test_logically_complete_machine_fixture_passes_without_file_io(self):
        self.assertEqual(validate_capture_manifest(
            self.passing_capture, verify_files=False), [])

    def test_pending_or_flat_proof_artifact_is_rejected(self):
        invalid = copy.deepcopy(self.passing_capture)
        invalid["captures"][0]["artifact"] = {
            "path": "focused_practice_hud_shipping_tokens_flat_proof_v1.svg",
            "sha256": "sha256:" + "0" * 64, "actual_gpu_capture": False,
            "capture_method": "PENDING"}
        errors = validate_capture_manifest(invalid, verify_files=False)
        self.assertTrue(any("not bound to an allowed actual GPU capture" in e for e in errors), errors)

    def test_missing_matrix_case_is_rejected(self):
        invalid = copy.deepcopy(self.passing_capture)
        invalid["captures"].pop()
        errors = validate_capture_manifest(invalid, verify_files=False)
        self.assertTrue(any("exactly cover all 594" in e for e in errors), errors)

    def test_opaque_or_unexpanded_icon_is_rejected(self):
        invalid = copy.deepcopy(self.passing_capture)
        invalid["captures"][0]["machine_metrics"]["opaque_icon_count"] = 1
        invalid["captures"][0]["machine_metrics"]["unexpanded_icon_count"] = 1
        errors = validate_capture_manifest(invalid, verify_files=False)
        self.assertTrue(any("opaque_icon_count" in e for e in errors), errors)
        self.assertTrue(any("unexpanded_icon_count" in e for e in errors), errors)

    def test_debug_safe_area_overlap_and_truncation_are_rejected(self):
        invalid = copy.deepcopy(self.passing_capture)
        metrics = invalid["captures"][0]["machine_metrics"]
        metrics["debug_node_count"] = 1
        metrics["safe_area_violations"] = 1
        metrics["overlap_pairs"] = 1
        metrics["truncated_text_runs"] = 1
        errors = validate_capture_manifest(invalid, verify_files=False)
        for metric in ("debug_node_count", "safe_area_violations", "overlap_pairs",
                       "truncated_text_runs"):
            self.assertTrue(any(metric in e for e in errors), (metric, errors))

    def test_minimum_text_and_required_state_indicator_are_rejected(self):
        invalid = copy.deepcopy(self.passing_capture)
        first = invalid["captures"][0]
        first["machine_metrics"]["minimum_rendered_text_px"] = 0
        shell = next(entry for entry in invalid["captures"]
                     if entry["surface"] == "shell" and entry["minimum_focus"] > 0)
        shell["machine_metrics"]["focus_indicator_count"] = 0
        errors = validate_capture_manifest(invalid, verify_files=False)
        self.assertTrue(any("minimum rendered text" in e for e in errors), errors)
        self.assertTrue(any("focus_indicator_count" in e for e in errors), errors)

    def test_existing_performance_threshold_cannot_be_relaxed_or_exceeded(self):
        invalid = copy.deepcopy(self.passing_capture)
        invalid["machine_thresholds"]["p95_total_frame_ms_max"] = 17.0
        invalid["captures"][0]["performance"]["p95_total_frame_ms"] = 16.71
        errors = validate_capture_manifest(invalid, verify_files=False)
        self.assertTrue(any("thresholds changed" in e for e in errors), errors)
        self.assertTrue(any("p95_total_frame_ms" in e for e in errors), errors)

    def test_manual_review_is_blocked_before_machine_pass(self):
        manual = completed_manual(self.passing_capture)
        pending = copy.deepcopy(self.passing_capture)
        pending["captures"][0]["machine_status"] = "PENDING"
        errors = validate_manual_review(manual, pending, verify_files=False)
        self.assertTrue(errors[0].startswith("manual review prohibited"), errors)

    def test_manual_review_requires_manifest_and_screenshot_hash_binding(self):
        manual = completed_manual(self.passing_capture)
        manual["capture_binding"]["manifest_sha256"] = "sha256:" + "0" * 64
        manual["capture_reviews"][0]["screenshot_sha256"] = "sha256:" + "0" * 64
        errors = validate_manual_review(manual, self.passing_capture, verify_files=False)
        self.assertTrue(any("SHA-bound" in e for e in errors), errors)
        self.assertTrue(any("manual screenshot hash differs" in e for e in errors), errors)

    def test_manual_pending_axis_or_false_summary_is_rejected(self):
        manual = completed_manual(self.passing_capture)
        manual["capture_reviews"][0]["axes"]["composition_and_gameplay_occlusion"] = {
            "status": "PENDING", "evidence": "PENDING"}
        errors = validate_manual_review(manual, self.passing_capture, verify_files=False)
        self.assertTrue(any("manual result is incomplete" in e for e in errors), errors)

    def test_manual_surface_specific_na_axes_are_enforced(self):
        manual = completed_manual(self.passing_capture)
        hud_review = next(review for review in manual["capture_reviews"]
                          if review["capture_id"].startswith("hud__"))
        hud_review["axes"]["focus_selected_error_disabled_clarity"]["status"] = "PASS"
        errors = validate_manual_review(manual, self.passing_capture, verify_files=False)
        self.assertTrue(any("HUD focus/selection manual axis must be N/A" in e for e in errors), errors)

    def test_structurally_valid_failed_manual_cannot_pass_acceptance(self):
        manual = completed_manual(self.passing_capture)
        manual["status"] = "FAIL"
        first = manual["capture_reviews"][0]
        first["axes"]["composition_and_gameplay_occlusion"] = {
            "status": "FAIL",
            "evidence": "Inspected at native resolution; gameplay is occluded.",
        }
        self.assertEqual(
            validate_manual_review(manual, self.passing_capture, verify_files=False),
            [],
            "a failed review remains a valid evidence artifact")
        self.assertEqual(
            manual_acceptance_errors(manual),
            ["independent manual review outcome is FAIL"],
            "a valid failed artifact must not pass the acceptance gate")


if __name__ == "__main__":
    unittest.main()
