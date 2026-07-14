import json
import tempfile
import unittest
from pathlib import Path

from tools.physics_validation.holdout_access import validate_candidate_holdout_access
from tools.physics_validation.phase3_release_gate import validate_phase3_release


ROOT = Path(__file__).resolve().parents[2]
RELEASE = ROOT / "physics_models/promotion/phase3_release_v1.json"


class Phase3ReleaseGateTests(unittest.TestCase):
    def test_committed_phase3_v1_release_is_rejected_without_replaying_holdout(self):
        failures = validate_phase3_release(ROOT)
        self.assertTrue(any("receipt did not pass" in value for value in failures))

    def test_release_gate_fails_closed_on_an_unexplained_regression(self):
        document = json.loads(RELEASE.read_text(encoding="utf-8"))
        document["unexplained_regressions"] = 1
        with tempfile.TemporaryDirectory() as directory:
            mutated = Path(directory) / "release.json"
            mutated.write_text(json.dumps(document), encoding="utf-8")
            failures = validate_phase3_release(ROOT, release_path=mutated)
        self.assertIn("release has unexplained regressions", failures)

    def test_automation_never_invokes_a_validation_partition(self):
        automated = [
            ROOT / "scripts/check.sh",
            ROOT / "scripts/check_phase3_physics_release.py",
            ROOT / ".github/workflows/ci.yml",
        ]
        forbidden = (
            "tools.physics_validation.validation_run",
            "tools.physics_validation.reference_run",
        )
        for path in automated:
            contents = path.read_text(encoding="utf-8")
            for command in forbidden:
                self.assertNotIn(command, contents, f"{path} can consume HOLDOUT via {command}")

    def test_manual_candidate_validation_rejects_every_phase3_freeze(self):
        inventory = json.loads((
            ROOT / "physics_models/promotion/phase3_candidates_v1.json"
        ).read_text(encoding="utf-8"))
        for candidate in inventory["candidates"]:
            failures = validate_candidate_holdout_access(
                ROOT, ROOT / candidate["freeze"]["path"])
            self.assertTrue(failures, candidate["id"])

        workflow = (ROOT / ".github/workflows/physics-reference-full.yml").read_text(
            encoding="utf-8")
        guard = "scripts/check_candidate_holdout_access.py"
        validation = "tools.physics_validation.validation_run"
        self.assertIn(guard, workflow)
        self.assertLess(workflow.index(guard), workflow.index(validation))

    def test_spent_package_cannot_be_reused_with_an_alternate_freeze(self):
        failures = validate_candidate_holdout_access(
            ROOT,
            ROOT / "physics_models/candidates/multi_contact_solver_v1/freeze.json",
            ROOT / "tests/physics_validation/reference_data/multi_contact_solver_analytic_contract",
        )
        self.assertTrue(any("reference package HOLDOUT is already consumed" in value
                            for value in failures))


if __name__ == "__main__":
    unittest.main()
