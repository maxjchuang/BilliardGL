import csv
import json
import math
import unittest
from pathlib import Path

from tools.physics_validation.confirmation_adapters import (
    base_scenario,
    confirmation_adapter,
)
from tools.physics_validation.cross_2016_confirmation import (
    build_cross_scenarios,
    evaluate_cross,
)
from tools.physics_validation.data_lifecycle import load_data_lifecycle
from tools.physics_validation.extract_cross_2016_newtons_cradle import (
    DATASET_ID,
    DATASET_VERSION,
    MEDIA_SHA256,
    generated_files,
    verify_package,
)
from tools.physics_validation.reference_package import load_reference_package


ROOT = Path(__file__).resolve().parents[2]
PACKAGE = ROOT / "tests/physics_validation/reference_data" / DATASET_ID
STATUS = ROOT / "tests/physics_validation/validation_data_status.json"
PROFILE = json.loads((
    ROOT / "physics_models/profiles/chinese_pool_full_game_v4.json"
).read_text(encoding="utf-8"))["runtime_profile"]


def _vector(x):
    return {"x": x, "y": 0.0, "z": 0.0}


def _passing_trace(back_speed=98.7, front_speed=100.0):
    contact = {
        "kind": 0,
        "first_ball": 0,
        "second_ball": 1,
        "normal_impulse_n_s": 0.1,
    }
    microstep = {
        "balls": [
            {"index": 0, "velocity_cm_s": _vector(back_speed)},
            {"index": 1, "velocity_cm_s": _vector(front_speed)},
        ],
        "contacts": [contact],
        "energy_residual_j": -1e-9,
        "index": 0,
        "regime": "stick",
    }
    frames = []
    for tick in range(1, 9):
        frame = {
            "balls": [
                {"index": 0, "position_cm": _vector(tick * back_speed / 120),
                 "velocity_cm_s": _vector(back_speed)},
                {"index": 1, "position_cm": _vector(
                    2 * PROFILE["ball"]["radius_cm"] +
                    tick * front_speed / 120),
                 "velocity_cm_s": _vector(front_speed)},
            ],
            "contacts": [],
            "tick": tick,
            "total_kinetic_energy_j": 1.0,
        }
        if tick == 1:
            frame["cue_contact"] = {
                "applied": True,
                "error_code": "",
                "microsteps": [microstep],
                "microtrace_schema_version": 1,
                "regime": "released",
            }
        frames.append(frame)
    return frames


class Cross2016PackageTests(unittest.TestCase):
    def test_primary_source_and_video_provenance_are_complete(self):
        manifest = json.loads(
            (PACKAGE / "manifest.json").read_text(encoding="utf-8"))
        audit = json.loads((PACKAGE / "source_access_audit.json").read_text(
            encoding="utf-8"))
        self.assertEqual(manifest["dataset_id"], DATASET_ID)
        self.assertEqual(manifest["dataset_version"], DATASET_VERSION)
        self.assertEqual(manifest["source"]["doi"],
                         "10.1088/0031-9120/51/6/065020")
        self.assertEqual(audit["author_manuscript_url"],
                         "https://www.oxfordcroquet.com/tech/cross2/")
        self.assertEqual(audit["supplementary_video_identifier"], "4582")
        self.assertEqual(audit["media_sha256"], MEDIA_SHA256)
        self.assertFalse(audit["source_media_committed"])
        self.assertIn("two billiard balls", audit["apparatus_statement"])
        self.assertIn("same speed", audit["published_observation"])

    def test_complete_stable_window_tracks_both_balls(self):
        with (PACKAGE / "raw_frame_tracks.csv").open(
                encoding="utf-8", newline="") as stream:
            rows = list(csv.DictReader(stream))
        self.assertEqual(len(rows), 32)
        self.assertEqual({row["ball_id"] for row in rows}, {"back", "front"})
        for ball_id in ("back", "front"):
            ball_rows = [row for row in rows if row["ball_id"] == ball_id]
            self.assertEqual([int(row["source_frame_number"])
                              for row in ball_rows], list(range(80, 96)))
        required = {
            "source_frame_number", "source_time_seconds", "ball_id",
            "center_x_px", "center_y_px", "pixels_per_cm",
            "velocity_x_cm_s", "velocity_standard_uncertainty_cm_s",
            "coordinate_standard_uncertainty_px",
            "time_standard_uncertainty_seconds",
        }
        self.assertTrue(required <= set(rows[0]))
        self.assertTrue(all(math.isfinite(float(row[field])) for row in rows
                            for field in required - {
                                "source_frame_number", "ball_id"}))

    def test_normalization_and_uncertainty_contract(self):
        with (PACKAGE / "scalars.csv").open(
                encoding="utf-8", newline="") as stream:
            scalar_rows = list(csv.DictReader(stream))
        scalars = {row["metric"]: row for row in scalar_rows}
        ratio = float(scalars["back_to_front_speed_ratio"]["source_value"])
        uncertainty = float(scalars[
            "back_to_front_speed_ratio"]["measurement_uncertainty"])
        self.assertAlmostEqual(ratio, 0.9874325803526451)
        self.assertAlmostEqual(uncertainty, 0.02610928824969865)
        scenario = json.loads((PACKAGE / "scenario_template.json").read_text(
            encoding="utf-8"))
        self.assertEqual(scenario["ratio_absolute_floor"], 0.05)
        self.assertEqual(scenario["ratio_uncertainty_multiplier"], 2)
        self.assertEqual(scenario["stable_source_frame_window"], [80, 95])
        residuals = [row for row in scalar_rows
                     if row["metric"] == "ols_residual"]
        self.assertEqual(len(residuals), 32)
        self.assertEqual({row["point_id"].split("_")[1]
                          for row in residuals}, {"back", "front"})
        self.assertEqual(
            sum(row["metric"] == "ols_slope" for row in scalar_rows), 2)
        self.assertEqual(float(scalars[
            "ratio_common_time_offset_uncertainty"]["source_value"]), 0.0)

    def test_package_is_byte_reproducible_and_unopened(self):
        self.assertEqual(verify_package(PACKAGE), [])
        self.assertEqual(set(generated_files()), {
            "raw_frame_tracks.csv", "normalized.csv", "scalars.csv",
            "split.json", "extraction.json", "scenario_template.json",
            "expected_model_mismatches.json",
            "expected_reference_limitations.json",
            "source_access_audit.json", "manifest.json",
        })
        package = load_reference_package(PACKAGE)
        self.assertTrue(package.manifest["evidence"]["confirmation_only"])
        lifecycle = load_data_lifecycle(STATUS).entry(DATASET_ID, DATASET_VERSION)
        self.assertEqual(
            (lifecycle.calibration_status, lifecycle.holdout_status),
            ("confirmation", "confirmation"),
        )
        self.assertFalse((PACKAGE / "candidate_prediction.csv").exists())
        self.assertFalse((PACKAGE / "residuals.csv").exists())


class Cross2016AdapterTests(unittest.TestCase):
    def test_adapter_is_registered_and_v5_uses_schema_12(self):
        self.assertEqual(confirmation_adapter(DATASET_ID).dataset_id, DATASET_ID)
        profile = json.loads(json.dumps(PROFILE))
        profile["frozen_cue_contact"] = {
            "normal_stiffness_n_per_m32": 1.0,
        }
        self.assertEqual(base_scenario(
            profile, "v5", [], "unbounded", 1, "schema fixture"
        )["schema_version"], 12)
        self.assertEqual(base_scenario(
            PROFILE, "v4", [], "unbounded", 1, "schema fixture"
        )["schema_version"], 11)

    def test_scenario_is_centered_horizontal_frozen_pair(self):
        scenarios = build_cross_scenarios(PROFILE, load_reference_package(PACKAGE))
        self.assertEqual(set(scenarios), {"cross_cue_frozen_pair"})
        scenario = scenarios["cross_cue_frozen_pair"]
        self.assertEqual(scenario["boundary_mode"], "unbounded")
        self.assertLessEqual(scenario["initial_contact_epsilon_cm"], 1e-5)
        self.assertEqual(scenario["cue_impact"]["direction"], [1, 0, 0])
        self.assertEqual(scenario["cue_impact"]["tip_offset_radius"], [0, 0])
        self.assertEqual(scenario["cue_impact"]["elevation_degrees"], 0)

    def test_equal_speed_and_all_hard_gates_pass(self):
        package = load_reference_package(PACKAGE)
        evaluation = evaluate_cross(
            {"cross_cue_frozen_pair": _passing_trace()}, PROFILE, package)
        self.assertTrue(all(value for key, value in
                            evaluation.summary_metrics.items()
                            if key.endswith("_passed")))
        self.assertAlmostEqual(
            evaluation.summary_metrics["back_to_front_speed_ratio"], 0.987)
        self.assertAlmostEqual(
            evaluation.summary_metrics["signed_ratio_residual"], -0.013)

    def test_missing_release_nonpassive_trace_and_recontact_fail_closed(self):
        package = load_reference_package(PACKAGE)
        trace = _passing_trace()
        trace[0]["cue_contact"]["regime"] = "stick"
        trace[0]["cue_contact"]["microsteps"][0]["energy_residual_j"] = 1e-5
        trace[2]["cue_contact"] = {"applied": True, "regime": "released",
                                    "microsteps": []}
        evaluation = evaluate_cross(
            {"cross_cue_frozen_pair": trace}, PROFILE, package)
        self.assertFalse(evaluation.summary_metrics["release_passed"])
        self.assertFalse(evaluation.summary_metrics["passive_microtrace_passed"])
        self.assertFalse(evaluation.summary_metrics["no_recontact_passed"])
        self.assertEqual(evaluation.rows[0]["status"], "FAILED")


if __name__ == "__main__":
    unittest.main()
