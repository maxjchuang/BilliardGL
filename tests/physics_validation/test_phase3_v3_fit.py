import csv
import json
import tempfile
import unittest
from pathlib import Path

from tools.physics_validation.build_v3_fit_inputs import (
    build_ball_inputs,
    build_cushion_inputs,
    write_v3_fit_inputs,
)
from tools.physics_validation.fit_ball_collision import (
    build_v3_fit_report as build_ball_v3_fit_report,
    read_impact_inputs,
    write_fit_artifacts as write_ball_fit_artifacts,
)
from tools.physics_validation.fit_cushion import (
    build_v3_fit_report as build_cushion_v3_fit_report,
    read_incident_inputs,
    write_fit_artifacts as write_cushion_fit_artifacts,
)


ROOT = Path(__file__).resolve().parents[2]
CALIBRATION = ROOT / "physics_models/calibration"
BALL_INPUTS = CALIBRATION / "ball_collision_fit_v3_inputs.csv"
CUSHION_INPUTS = CALIBRATION / "cushion_fit_v3_inputs.csv"
STRUCTURAL = CALIBRATION / "sudo_2002_structural_residuals.csv"
BALL_FIT = CALIBRATION / "ball_collision_fit_v3.json"
CUSHION_FIT = CALIBRATION / "cushion_fit_v3.json"
BALL_RESIDUALS = CALIBRATION / "ball_collision_fit_v3_residuals.csv"
CUSHION_RESIDUALS = CALIBRATION / "cushion_fit_v3_residuals.csv"


def csv_rows(path):
    with Path(path).open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream))


class Phase3V3FitTests(unittest.TestCase):
    def test_v3_inputs_include_sudo_as_spent_and_no_confirmation(self):
        rows = build_ball_inputs(ROOT) + build_cushion_inputs(ROOT)
        dataset_ids = {row["dataset_id"] for row in rows}
        self.assertIn("sudo_2002", dataset_ids)
        self.assertEqual({row["lifecycle"] for row in rows}, {"spent"})
        self.assertFalse(
            {"derby_fuller_1999", "han_2005"} & dataset_ids)

    def test_every_series_has_equal_objective_weight(self):
        ball_report, _ = build_ball_v3_fit_report(
            read_impact_inputs(BALL_INPUTS))
        self.assertEqual(
            set(ball_report["fit"]["objective_by_series"]),
            {
                "billiard_alpha1",
                "billiard_delta2",
                "mathavan_velocity",
                "sudo_ball_collision",
            },
        )
        cushion_report, _ = build_cushion_v3_fit_report(
            read_incident_inputs(CUSHION_INPUTS))
        self.assertEqual(
            set(cushion_report["fit"]["objective_by_series"]),
            {"mathavan_2009", "mathavan_2010", "sudo_cushion_restitution"},
        )
        self.assertEqual(
            cushion_report["algorithm"]["objective"],
            "mean_across_series_of_mean_squared_uncertainty_normalized_residual",
        )

    def test_contact_time_is_visible_but_not_faked(self):
        rows = csv_rows(STRUCTURAL)
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["point_id"], "cushion_contact_time_plateau")
        self.assertEqual(rows[0]["status"], "OUT_OF_MODEL_SPENT")
        self.assertEqual(rows[0]["observed"], "")
        self.assertEqual(rows[0]["expected"], "0.008")

    def test_committed_inputs_and_reports_are_reproducible(self):
        with tempfile.TemporaryDirectory() as directory:
            generated = Path(directory)
            write_v3_fit_inputs(ROOT, generated)
            for name in (
                BALL_INPUTS.name,
                CUSHION_INPUTS.name,
                STRUCTURAL.name,
            ):
                self.assertEqual(
                    (generated / name).read_bytes(),
                    (CALIBRATION / name).read_bytes(),
                )
            write_ball_fit_artifacts(
                read_impact_inputs(generated / BALL_INPUTS.name),
                generated / BALL_FIT.name,
                generated / BALL_RESIDUALS.name,
            )
            write_cushion_fit_artifacts(
                read_incident_inputs(generated / CUSHION_INPUTS.name),
                generated / CUSHION_FIT.name,
                generated / CUSHION_RESIDUALS.name,
            )
            for committed in (
                    BALL_FIT, BALL_RESIDUALS, CUSHION_FIT, CUSHION_RESIDUALS):
                self.assertEqual(
                    (generated / committed.name).read_bytes(),
                    committed.read_bytes(),
                )
        ball_report = json.loads(BALL_FIT.read_text(encoding="utf-8"))
        cushion_report = json.loads(CUSHION_FIT.read_text(encoding="utf-8"))
        self.assertEqual(ball_report["schema_version"], 3)
        self.assertEqual(cushion_report["schema_version"], 3)
        self.assertEqual(ball_report["dataset_lifecycle"], "spent")
        self.assertEqual(cushion_report["dataset_lifecycle"], "spent")


if __name__ == "__main__":
    unittest.main()
