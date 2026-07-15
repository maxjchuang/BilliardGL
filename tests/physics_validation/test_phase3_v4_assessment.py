import hashlib
import json
import tempfile
import unittest
from pathlib import Path

from tools.physics_validation.phase3_v4_assessment import build_final_assessment


LIVE_ROOT = Path(__file__).resolve().parents[2]
CANDIDATE_ID = "phase3_integrated_v4"
PACKAGES = ("alciatore_2005_tp_a15", "han_2005")


def canonical(document):
    return (json.dumps(document, indent=2, sort_keys=True) + "\n").encode()


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def tree_digest(directory):
    directory = Path(directory)
    entries = []
    for path in sorted(item for item in directory.rglob("*") if item.is_file()):
        entries.append(
            f"{path.relative_to(directory).as_posix()}\0{digest(path)}\n")
    return hashlib.sha256("".join(entries).encode()).hexdigest()


def write_json(path, document):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(canonical(document))


def synthetic_repository(root, results):
    candidate = root / "physics_models/candidates" / CANDIDATE_ID
    freeze = {
        "candidate_id": CANDIDATE_ID,
        "executable_sha256": "1" * 64,
        "schema_version": 2,
        "source_revision": "2" * 40,
    }
    write_json(candidate / "freeze.json", freeze)
    full_game = candidate / "full_game"
    write_json(full_game / "matrix_summary.json", {
        "cases": [{"case_id": str(index), "passed": True}
                  for index in range(12)],
        "passed": True,
        "schema_version": 2,
    })
    write_json(candidate / "confirmation_readiness.json", {
        "candidate_id": CANDIDATE_ID,
        "checks": {"all": True},
        "confirmation_packages": {
            package: {"attempt": "UNOPENED", "ready": True}
            for package in PACKAGES
        },
        "failures": [],
        "freeze_sha256": digest(candidate / "freeze.json"),
        "full_game_tree_sha256": tree_digest(full_game),
        "schema_version": 1,
        "status": "READY",
    })
    attempts = []
    records = []
    for index, (package, result) in enumerate(results.items(), start=1):
        output = candidate / "confirmation" / package
        write_json(output / "reference_report.json", {
            "dataset_id": package, "result": result, "schema_version": 1})
        write_json(output / "provenance/scenario.json", {
            "candidate_id": CANDIDATE_ID,
            "dataset_id": package,
            "executable_sha256": freeze["executable_sha256"],
            "freeze_sha256": digest(candidate / "freeze.json"),
            "schema_version": 1,
            "source_revision": freeze["source_revision"],
        })
        attempt_id = hashlib.sha256(f"{package}-{index}".encode()).hexdigest()
        attempt = {
            "attempt_id": attempt_id,
            "candidate_id": CANDIDATE_ID,
            "dataset_id": package,
            "freeze_sha256": digest(candidate / "freeze.json"),
            "package_key": package,
            "partition": "CONFIRMATION",
            "schema_version": 2,
            "state": "STARTED",
        }
        receipt = {
            "attempt_id": attempt_id,
            "candidate_id": CANDIDATE_ID,
            "dataset_id": package,
            "files": {
                "provenance/scenario.json": digest(
                    output / "provenance/scenario.json"),
                "reference_report.json": digest(output / "reference_report.json"),
            },
            "freeze_sha256": digest(candidate / "freeze.json"),
            "package_key": package,
            "partition": "CONFIRMATION",
            "result": result,
            "schema_version": 3,
        }
        write_json(output / "validation_receipt.json", receipt)
        attempts.append(attempt)
        records.append(receipt)
    if results:
        write_json(candidate / "confirmation_consumption.json", {
            "attempts": attempts, "records": records, "schema_version": 1})
    return candidate


class Phase3V4AssessmentTests(unittest.TestCase):
    def test_live_failed_alciatore_tree_is_rejected_and_hash_bound(self):
        candidate = LIVE_ROOT / "physics_models/candidates" / CANDIDATE_ID
        assessment_path = candidate / "final_assessment.json"
        assessment = build_final_assessment(LIVE_ROOT)
        committed = json.loads(
            assessment_path.read_text(encoding="utf-8"))
        self.assertEqual(assessment, committed)
        self.assertEqual(assessment["disposition"], "REJECTED")
        self.assertEqual(set(assessment["confirmations"]),
                         {"alciatore_2005_tp_a15"})
        evidence = assessment["confirmations"]["alciatore_2005_tp_a15"]
        self.assertEqual(evidence["receipt"]["result"], "FAILED")
        self.assertEqual(
            evidence["tree_sha256"],
            tree_digest(candidate / "confirmation/alciatore_2005_tp_a15"))
        rejection = json.loads((
            LIVE_ROOT / "physics_models/promotion/"
            "phase3_integrated_v4_rejection.json"
        ).read_text(encoding="utf-8"))
        self.assertEqual(rejection["han_2005"], "NOT_EXECUTED")
        self.assertEqual(rejection["assessment_sha256"], digest(assessment_path))

    def test_passing_alciatore_requires_han(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            synthetic_repository(root, {"alciatore_2005_tp_a15": "PASSED"})
            with self.assertRaisesRegex(ValueError,
                                        "Han confirmation is absent"):
                build_final_assessment(root)

    def test_alciatore_failure_rejects_without_opening_han(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            candidate = synthetic_repository(
                root, {"alciatore_2005_tp_a15": "FAILED"})
            assessment = build_final_assessment(root)
            self.assertEqual(assessment["disposition"], "REJECTED")
            self.assertNotIn("han_2005", assessment["confirmations"])
            evidence = assessment["confirmations"]["alciatore_2005_tp_a15"]
            self.assertEqual(
                evidence["tree_sha256"],
                tree_digest(candidate / "confirmation/alciatore_2005_tp_a15"))

    def test_han_failure_rejects_and_binds_both_trees(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            candidate = synthetic_repository(root, {
                "alciatore_2005_tp_a15": "PASSED_OR_ACCOUNTED",
                "han_2005": "FAILED",
            })
            assessment = build_final_assessment(root)
            self.assertEqual(assessment["disposition"], "REJECTED")
            for package in PACKAGES:
                self.assertEqual(
                    assessment["confirmations"][package]["tree_sha256"],
                    tree_digest(candidate / "confirmation" / package))

    def test_two_passes_accept_with_distinct_attempts(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            synthetic_repository(root, {
                "alciatore_2005_tp_a15": "PASSED_OR_ACCOUNTED",
                "han_2005": "PASSED",
            })
            assessment = build_final_assessment(root)
            self.assertEqual(assessment["disposition"], "ACCEPTED")
            self.assertEqual(set(assessment["confirmations"]), set(PACKAGES))
            self.assertNotEqual(
                assessment["confirmations"][PACKAGES[0]]["attempt_id"],
                assessment["confirmations"][PACKAGES[1]]["attempt_id"])


if __name__ == "__main__":
    unittest.main()
