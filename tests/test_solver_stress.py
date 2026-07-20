import csv
import tempfile
import unittest
from pathlib import Path

from tools.physics_validation.solver_stress import rows, validate, write
from tools.physics_validation.analyzer import analyze_solver_stress


class SolverStressTest(unittest.TestCase):
    def test_matrix_is_complete_and_valid(self):
        records = rows()
        self.assertEqual(len(records), 1152)
        self.assertFalse(validate(records))
        self.assertFalse(analyze_solver_stress(records))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "solver.csv"
            write(path)
            with path.open(encoding="utf-8", newline="") as stream:
                persisted = list(csv.DictReader(stream))
        self.assertEqual(len(persisted), len(records))
        self.assertFalse(validate(persisted))
        self.assertFalse(analyze_solver_stress(persisted))
        self.assertEqual(persisted[0]["state_hash"], records[0]["state_hash"])


if __name__ == "__main__":
    unittest.main()
