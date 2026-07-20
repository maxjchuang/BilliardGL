import hashlib
import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CANDIDATE = ROOT / "physics_models/candidates/multi_contact_solver_v1"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class MultiContactSolverValidationArtifactTests(unittest.TestCase):
    def test_frozen_holdout_artifacts_are_immutable_and_complete(self):
        validation = CANDIDATE / "validation"
        self.assertEqual(len([path for path in validation.rglob("*") if path.is_file()]), 12)
        self.assertEqual(digest(CANDIDATE / "freeze.json"),
                         "360c85919e3167c3a86351068cdb147b02974ff336b019a3ffa18ac82eaf4969")
        self.assertEqual(digest(validation / "reference_report.json"),
                         "86c2dc17b5e504b0068bae57e3a95a8bfbc88b212715a86661ed97afbbce2eb1")
        self.assertEqual(digest(validation / "validation_receipt.json"),
                         "0e7bea39012d5963f00c9d0e9f72ade573b7c58cf16a6adcd719b7b7334c8732")
        report = json.loads((validation / "reference_report.json").read_text(encoding="utf-8"))
        summary = report["partitions"]["HOLDOUT"]["summary"]
        self.assertEqual(summary["points"], 4)
        self.assertEqual(summary["passed"], 4)
        self.assertEqual(summary["statuses"], {"PASSED": 4})
        receipt = json.loads((validation / "validation_receipt.json").read_text(encoding="utf-8"))
        self.assertEqual(receipt["partition"], "HOLDOUT")
        self.assertEqual(receipt["result"], "PASSED_OR_ACCOUNTED")


if __name__ == "__main__":
    unittest.main()
