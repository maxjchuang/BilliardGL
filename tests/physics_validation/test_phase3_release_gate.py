import json
import inspect
import tempfile
import unittest
from pathlib import Path

from tools.physics_validation.holdout_access import validate_candidate_holdout_access
from tools.physics_validation.phase3_release_gate import validate_phase3_release
from tools.physics_validation.promotion import validate_release_manifest


ROOT = Path(__file__).resolve().parents[2]
RELEASE = ROOT / "physics_models/promotion/phase3_release_v1.json"
RELEASE_V2 = ROOT / "tests/physics_validation/fixtures/phase3_release_v2.json"


class Phase3ReleaseGateTests(unittest.TestCase):
    def call_v2_manifest(self, **arguments):
        required = {"head_revision", "is_ancestor", "executable_profile_id"}
        self.assertTrue(
            required <= set(inspect.signature(validate_release_manifest).parameters),
            "schema-v2 release verification inputs are not implemented",
        )
        defaults = {
            "head_revision": "a" * 40,
            "is_ancestor": lambda source, head: True,
            "executable_profile_id": "chinese_pool_full_game_v2",
        }
        defaults.update(arguments)
        return validate_release_manifest(RELEASE_V2, ROOT, **defaults)

    def test_v2_release_accepts_only_passed(self):
        document = json.loads(RELEASE_V2.read_text(encoding="utf-8"))
        document["status"] = "PASSED_WITH_DECLARED_LIMITATIONS"
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "release.json"
            path.write_text(json.dumps(document), encoding="utf-8")
            required = {"head_revision", "is_ancestor", "executable_profile_id"}
            self.assertTrue(
                required <= set(inspect.signature(validate_release_manifest).parameters),
                "schema-v2 release verification inputs are not implemented",
            )
            failures = validate_release_manifest(
                path, ROOT, head_revision="a" * 40,
                is_ancestor=lambda source, head: True,
                executable_profile_id="chinese_pool_full_game_v2",
            )
        self.assertIn("v2 release status must be PASSED", failures)

    def test_release_source_must_be_an_ancestor(self):
        failures = self.call_v2_manifest(
            head_revision="f" * 40,
            is_ancestor=lambda source, head: False,
        )
        self.assertIn("release source revision is not an ancestor of HEAD", failures)

    def test_executable_default_profile_must_match_freeze(self):
        failures = self.call_v2_manifest(
            executable_profile_id="chinese_pool_full_game_v1",
        )
        self.assertIn("production default profile differs from frozen profile", failures)

    def test_valid_v2_release_identity_contract_passes(self):
        self.assertEqual(self.call_v2_manifest(), [])

    def test_phase3_gate_forwards_v2_source_and_profile_identity(self):
        required = {"head_revision", "is_ancestor", "executable_profile_id"}
        self.assertTrue(
            required <= set(inspect.signature(validate_phase3_release).parameters),
            "phase3 gate does not expose strict v2 identity inputs",
        )
        self.assertEqual(
            validate_phase3_release(
                ROOT,
                release_path=RELEASE_V2,
                head_revision="a" * 40,
                is_ancestor=lambda source, head: True,
                executable_profile_id="chinese_pool_full_game_v2",
            ),
            [],
        )

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
