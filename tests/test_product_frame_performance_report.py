"""Collection validity is not a frame-budget verdict."""

import copy
import importlib.util
from pathlib import Path
import unittest


SPEC = importlib.util.spec_from_file_location(
    "product_frame_performance_runner",
    Path(__file__).resolve().parents[1] / "scripts/run_product_frame_performance.py",
)
RUNNER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RUNNER)


def valid_report():
    series = {"sample_count": 300, "p50_ms": 20, "p95_ms": 30, "p99_ms": 40,
              "available": True, "completed_sample_frames": 300,
              "zero_elapsed_sample_frames": 0}
    return {
        "schema": "billiardgl_product_frame_performance_v1",
        "measurement_scope": "visible_product_event_loop_through_swap_completion",
        "budget_reference_series": "swap_to_swap_cadence",
        "loop_through_swap_includes_post_swap_logging": False,
        "swap_to_swap_cadence_includes_previous_post_swap_logging": True,
        "budget_verdict": "not_evaluated",
        "warmup_frames": 120,
        "sample_frames": 300,
        "drawable_width_px": 1920,
        "drawable_height_px": 1080,
        "render_quality": "medium",
        "requested_msaa_samples": 4,
        "actual_render_quality": "medium",
        "actual_msaa_samples": 4,
        "includes_product_ui": True,
        "includes_simulation": True,
        "includes_display_swap": True,
        "includes_shipping_playtest_logging": True,
        "measures_scanout": False,
        "forces_diagnostic_gl_finish": False,
        "full_frame_percentiles": {
            name: copy.deepcopy(series) for name in (
                "loop_through_swap", "swap_to_swap_cadence", "swap",
                "physics", "render_submission")
        },
        "gpu_pass_percentiles": {
            name: copy.deepcopy(series) for name in (
                "shadow_depth", "hdr_opaque", "scene_bloom", "aces_tone_map",
                "overlay", "product_ui", "product_present")
        },
        "physics_steps": 600,
        "moving_sample_frames": 150,
        "first_moving_sample_frame": 1,
        "last_moving_sample_frame": 150,
        "native_shot_requested_power": 200,
        "native_shot_dispatch_presented_frame": 120,
        "native_shot_dispatch_tick": "0",
        "first_sample_tick": "1",
        "last_sample_tick": "300",
    }


class PerformanceReportTests(unittest.TestCase):
    def test_valid_slow_capture_is_not_a_budget_pass(self):
        # A valid capture can fail 16.7 ms; the collector must not erase it.
        RUNNER.validate_report(valid_report())

    def test_missing_contract_fields_are_rejected(self):
        for key in valid_report():
            with self.subTest(key=key):
                report = valid_report()
                del report[key]
                with self.assertRaises(RuntimeError):
                    RUNNER.validate_report(report)

    def test_invalid_scope_and_stationary_window_are_rejected(self):
        mutations = {
            "measurement_scope": "render_submission_only",
            "budget_reference_series": "loop_through_swap",
            "loop_through_swap_includes_post_swap_logging": True,
            "swap_to_swap_cadence_includes_previous_post_swap_logging": False,
            "budget_verdict": "pass",
            "warmup_frames": 0,
            "sample_frames": 299,
            "drawable_width_px": 2048,
            "drawable_height_px": 1536,
            "render_quality": "low",
            "requested_msaa_samples": 0,
            "actual_render_quality": "high",
            "actual_msaa_samples": 2,
            "includes_shipping_playtest_logging": False,
            "measures_scanout": True,
            "forces_diagnostic_gl_finish": True,
            "physics_steps": 0,
            "moving_sample_frames": 0,
            "first_moving_sample_frame": 0,
            "last_moving_sample_frame": 301,
            "native_shot_requested_power": 65,
            "native_shot_dispatch_presented_frame": 119,
            "native_shot_dispatch_tick": "invalid",
            "gpu_pass_percentiles": {},
        }
        for key, value in mutations.items():
            with self.subTest(key=key):
                report = valid_report()
                report[key] = value
                with self.assertRaises(RuntimeError):
                    RUNNER.validate_report(report)

    def test_invalid_timing_series_are_rejected(self):
        for key, value in (
            ("sample_count", 299), ("p50_ms", -1),
            ("p95_ms", float("nan")), ("p99_ms", float("inf")),
            ("p95_ms", 10), ("p50_ms", True),
        ):
            with self.subTest(key=key, value=value):
                report = valid_report()
                report["full_frame_percentiles"]["loop_through_swap"][key] = value
                with self.assertRaises(RuntimeError):
                    RUNNER.validate_report(report)

    def test_gpu_query_count_must_be_positive_and_bounded(self):
        for count in (0, -1, 301, True):
            with self.subTest(count=count):
                report = valid_report()
                report["gpu_pass_percentiles"]["hdr_opaque"]["sample_count"] = count
                with self.assertRaises(RuntimeError):
                    RUNNER.validate_report(report)

    def test_zero_elapsed_gpu_pass_is_explicitly_unavailable(self):
        report = valid_report()
        report["gpu_pass_percentiles"]["product_present"] = {
            "available": False,
            "sample_count": 0,
            "completed_sample_frames": 282,
            "zero_elapsed_sample_frames": 282,
            "unavailable_reason": "completed_queries_returned_zero_elapsed",
        }
        RUNNER.validate_report(report)


if __name__ == "__main__":
    unittest.main()
