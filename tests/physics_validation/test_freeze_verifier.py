import hashlib
import importlib
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


MODULE = "tools.physics_validation.freeze_verifier"
ROLES = {
    "profile", "executable", "calibration_report", "source_manifest",
    "source_numeric_input", "split", "metric_contract", "receipt",
    "trace", "provenance", "full_game_matrix", "performance_budget",
}


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


class FreezeVerifierTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        artifact_root = self.root / "artifacts"
        artifact_root.mkdir()
        artifacts = []
        for role in sorted(ROLES):
            path = artifact_root / f"{role}.json"
            path.write_text(json.dumps({"role": role}), encoding="utf-8")
            artifacts.append({
                "path": path.relative_to(self.root).as_posix(),
                "role": role,
                "sha256": digest(path),
            })
        self.executable = artifact_root / "executable.json"
        self.freeze = self.root / "freeze.json"
        self.document = {
            "schema_version": 2,
            "candidate_id": "phase3_integrated_v2",
            "source_revision": "a" * 40,
            "executable_sha256": digest(self.executable),
            "artifact_roots": ["artifacts"],
            "artifacts": artifacts,
        }
        self.freeze.write_text(json.dumps(self.document), encoding="utf-8")

    def tearDown(self):
        self.temporary.cleanup()

    def module(self):
        self.assertIsNotNone(
            importlib.util.find_spec(MODULE),
            "freeze verifier is not implemented",
        )
        return importlib.import_module(MODULE)

    def write_freeze(self):
        self.freeze.write_text(json.dumps(self.document), encoding="utf-8")

    def test_complete_freeze_is_accepted(self):
        self.assertEqual(
            self.module().validate_freeze(self.freeze, self.root, self.executable),
            [],
        )

    def test_missing_role_fails_closed(self):
        self.document["artifacts"] = [
            value for value in self.document["artifacts"]
            if value["role"] != "source_numeric_input"
        ]
        self.write_freeze()
        failures = self.module().validate_freeze(self.freeze, self.root, self.executable)
        self.assertIn("freeze roles missing: ['source_numeric_input']", failures)

    def test_changed_executable_is_rejected(self):
        changed = self.root / "changed-executable"
        changed.write_bytes(b"changed")
        failures = self.module().validate_freeze(self.freeze, self.root, changed)
        self.assertIn("freeze executable hash mismatch", failures)

    def test_undeclared_file_duplicate_and_traversal_are_rejected(self):
        (self.root / "artifacts" / "undeclared.json").write_text("{}", encoding="utf-8")
        self.document["artifacts"][0]["path"] = "../escape.json"
        self.document["artifacts"].append(dict(self.document["artifacts"][1]))
        self.write_freeze()
        failures = self.module().validate_freeze(self.freeze, self.root, self.executable)
        self.assertTrue(any("unsafe artifact path" in value for value in failures))
        self.assertTrue(any("duplicate freeze artifact path" in value for value in failures))
        self.assertTrue(any("undeclared artifact" in value for value in failures))


if __name__ == "__main__":
    unittest.main()
