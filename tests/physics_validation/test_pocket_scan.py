import csv
import tempfile
import unittest
from pathlib import Path

from tools.physics_validation.analyzer import analyze_pocket_scan
from tools.physics_validation.pocket_scan import FIELDS, rows, write


class PocketScanTests(unittest.TestCase):
    def test_matrix_covers_every_declared_axis_and_is_finite(self):
        matrix = rows()
        self.assertEqual(len(matrix), 2800)
        self.assertEqual({row["pocket_kind"] for row in matrix}, {"corner", "side"})
        self.assertEqual({row["speed_cm_s"] for row in matrix}, {25.0, 100.0, 250.0, 500.0})
        self.assertEqual(analyze_pocket_scan(matrix), [])

    def test_committed_csv_is_reproducible_at_full_precision(self):
        committed = Path("physics_models/scans/pocket_boundary_v1.csv").read_bytes()
        with tempfile.TemporaryDirectory() as directory:
            generated = Path(directory) / "scan.csv"
            write(generated)
            self.assertEqual(generated.read_bytes(), committed)
            with generated.open(encoding="utf-8", newline="") as stream:
                self.assertEqual(tuple(next(csv.reader(stream))), FIELDS)

    def test_analyzer_detects_duplicate_capture_and_broken_mirror(self):
        matrix = rows()
        duplicate = dict(matrix[0])
        duplicate["case_id"] = matrix[0]["case_id"]
        duplicate["throat_passable"] = not duplicate["throat_passable"]
        failures = analyze_pocket_scan(matrix + [duplicate])
        self.assertTrue(any("duplicate" in failure for failure in failures))
        self.assertTrue(any("mirror" in failure for failure in failures))


if __name__ == "__main__":
    unittest.main()
