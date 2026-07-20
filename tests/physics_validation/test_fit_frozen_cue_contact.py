import csv
import io
import json
import math
import unittest
from pathlib import Path

from tools.physics_validation.fit_frozen_cue_contact import (
    DEFAULT_BOUNDS, DEFAULT_FIXED, build_fit_artifacts, fit_frozen_contact,
    hunt_crossley_force, load_points,
)


ROOT = Path(__file__).resolve().parents[2]
PACKAGE = ROOT / "tests/physics_validation/reference_data/shimamura_2006_cue_contact"
CALIBRATION = ROOT / "physics_models/calibration"


class FrozenCueContactFitTests(unittest.TestCase):
    def test_fit_is_bounded_passive_complete_and_within_timing_uncertainty(self):
        points = load_points(PACKAGE)
        fit = fit_frozen_contact(points)
        winner = fit["winner"]
        self.assertEqual(fit["fixed"]["normal_exponent"], 1.5)
        self.assertLessEqual(abs(winner["duration_residual_s"]),
                             DEFAULT_FIXED["timing_uncertainty_s"])
        self.assertTrue(winner["passive"])
        self.assertTrue(all(math.isfinite(row["objective"])
                            and row["passive"] for row in fit["residuals"]))
        self.assertEqual({row["dissipation_s_per_m"]
                          for row in fit["residuals"]},
                         set(DEFAULT_BOUNDS["dissipation_s_per_m"]))
        self.assertEqual(len(fit["residuals"]),
                         DEFAULT_BOUNDS["stiffness_grid_points"] * 5 + 5)
        refined = [row for row in fit["residuals"]
                   if row["stage"] == "refined"]
        self.assertEqual(winner, min(refined, key=lambda row: (
            row["objective"], row["stiffness_n_per_m32"],
            row["dissipation_s_per_m"])))
        self.assertEqual({(row["parameter"], row["scale"])
                          for row in fit["sensitivity"]}, {
            (parameter, scale)
            for parameter in ("stiffness_n_per_m32", "dissipation_s_per_m")
            for scale in (0.5, 1.0, 2.0)})

    def test_force_formula_matches_the_cpp_analytic_fixture(self):
        expected = 1.25e7 * 0.001 ** 1.5 * 1.01
        self.assertAlmostEqual(
            hunt_crossley_force(0.001, 0.2, 1.25e7, 0.05), expected, places=12)
        self.assertEqual(
            hunt_crossley_force(0.001, -1000.0, 1.25e7, 0.05), 0.0)

    def test_all_committed_fit_artifacts_are_byte_reproducible(self):
        generated = build_fit_artifacts(PACKAGE)
        self.assertEqual(set(generated), {
            "frozen_cue_contact_v1_inputs.csv",
            "frozen_cue_contact_v1_fit.json",
            "frozen_cue_contact_v1_residuals.csv",
            "frozen_cue_contact_v1_sensitivity.csv",
        })
        for name, data in generated.items():
            self.assertEqual((CALIBRATION / name).read_bytes(), data, name)
        report = json.loads(generated["frozen_cue_contact_v1_fit.json"])
        self.assertEqual(report["fit_version"], "frozen_cue_contact_v1")
        residuals = list(csv.DictReader(io.StringIO(generated[
            "frozen_cue_contact_v1_residuals.csv"].decode("utf-8"))))
        self.assertEqual(len(residuals), 810)


if __name__ == "__main__":
    unittest.main()
