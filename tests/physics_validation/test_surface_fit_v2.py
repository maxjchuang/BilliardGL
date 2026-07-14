import hashlib
import tempfile
import unittest
from pathlib import Path

from tools.physics_validation.fit_surface import (
    fit_surface_parameters,
    read_surface_inputs,
    write_fit_artifacts,
)


ROOT = Path(__file__).resolve().parents[2]
INPUTS = ROOT / "physics_models/calibration/surface_fit_v2_inputs.csv"


class SurfaceFitV2Tests(unittest.TestCase):
    def test_surface_fit_uses_only_spent_calibration_rows(self):
        points = read_surface_inputs(INPUTS)

        self.assertTrue(points)
        self.assertEqual({point.lifecycle for point in points}, {"spent"})
        self.assertFalse(any(
            name in point.dataset_id
            for point in points
            for name in ("derby", "sudo")
        ))
        self.assertEqual({point.phase for point in points}, {"sliding", "rolling"})
        self.assertTrue(all(point.contiguous_sample_count >= 1 for point in points))

    def test_surface_fit_is_deterministic_and_reports_every_residual(self):
        points = read_surface_inputs(INPUTS)

        first = fit_surface_parameters(points)
        second = fit_surface_parameters(read_surface_inputs(INPUTS))

        self.assertEqual(first, second)
        self.assertEqual(len(first.residuals), len(points))
        self.assertAlmostEqual(
            first.sliding_acceleration_cm_s2, 196.13296508789062)
        self.assertAlmostEqual(
            first.rolling_resistance_acceleration_cm_s2,
            12.499999813735489,
        )
        self.assertAlmostEqual(
            first.sliding_friction_coefficient,
            196.13296508789062 / 980.665,
        )

    def test_every_input_is_bound_to_committed_evidence(self):
        for point in read_surface_inputs(INPUTS):
            evidence = ROOT / point.evidence_path
            self.assertTrue(evidence.is_file())
            digest = "sha256:" + hashlib.sha256(evidence.read_bytes()).hexdigest()
            self.assertEqual(digest, point.evidence_sha256)

    def test_artifacts_are_canonical_and_reproducible(self):
        points = read_surface_inputs(INPUTS)
        with tempfile.TemporaryDirectory() as first_dir, tempfile.TemporaryDirectory() as second_dir:
            first_json = Path(first_dir) / "fit.json"
            first_csv = Path(first_dir) / "residuals.csv"
            second_json = Path(second_dir) / "fit.json"
            second_csv = Path(second_dir) / "residuals.csv"

            write_fit_artifacts(points, first_json, first_csv)
            write_fit_artifacts(points, second_json, second_csv)

            self.assertEqual(first_json.read_bytes(), second_json.read_bytes())
            self.assertEqual(first_csv.read_bytes(), second_csv.read_bytes())

    def test_input_contract_rejects_confirmation_and_noncontiguous_rows(self):
        header = (
            "point_id,dataset_id,dataset_version,lifecycle,phase,selection,"
            "contiguous_sample_count,observed_acceleration_cm_s2,"
            "standard_uncertainty_cm_s2,source_expected_cm_s2,"
            "source_lower_cm_s2,source_upper_cm_s2,source_locator,"
            "evidence_path,evidence_sha256\n"
        )
        rows = (
            "bad,derby_fuller_1999,1.0.0,confirmation,sliding,"
            "maximal_contiguous_phase,2,196,1,207.5,175,240,p.4,trace.json,"
            "sha256:0000000000000000000000000000000000000000000000000000000000000000\n"
        )
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "inputs.csv"
            path.write_text(header + rows, encoding="utf-8")
            with self.assertRaises(ValueError):
                read_surface_inputs(path)


if __name__ == "__main__":
    unittest.main()
