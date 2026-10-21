import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


def load(name, relative):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


fixture_generator = load(
    "generate_phase4_overlay_replay_fixtures",
    "scripts/generate_phase4_overlay_replay_fixtures.py")
generator = load(
    "generate_phase4_training_replay_overlay_capture_manifest",
    "scripts/generate_phase4_training_replay_overlay_capture_manifest.py")
dispatcher = load(
    "run_phase4_training_replay_overlay_gpu_capture",
    "scripts/run_phase4_training_replay_overlay_gpu_capture.py")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class Phase4OverlayGpuDispatcherTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.gate_path = ROOT / \
            "docs/art/phase4-training-replay-overlay-design-gate-v1.json"
        self.gate = json.loads(self.gate_path.read_text(encoding="utf-8"))
        self.runner = self.root / "runner"
        self.runner.write_bytes(b"core41 batch runner test double")
        self.executable = self.root / "Billiards"
        self.executable.write_bytes(b"headless executable test double")
        self.pack = ROOT / "assets/training/phase4/training_pack_v1.json"
        pack = json.loads(self.pack.read_text(encoding="utf-8"))
        pack_by_id = {row["id"]: row for row in pack["scenarios"]}
        fixtures = []
        for index, scenario in enumerate(self.gate["scenario_contract"]):
            replay = self.root / f"fixture-{index}.json"
            replay.write_text(json.dumps({"scenario": scenario["id"]}) + "\n",
                              encoding="utf-8")
            scenario_sha = scenario["sha256"].removeprefix("sha256:")
            fixtures.append({
                "scenario_id": scenario["id"],
                "scenario_path": str((self.pack.parent /
                    pack_by_id[scenario["id"]]["file"]).resolve()),
                "scenario_sha256": scenario_sha,
                "replay_path": str(replay),
                "replay_sha256": digest(replay),
                "executable_version": "phase4-dispatch-test-v1",
                "executable_path": str(self.executable),
                "executable_sha256": digest(self.executable),
                "content_sha256": scenario_sha,
                "physics_profile_id": "cross-2023-v1",
                "physics_profile_sha256": "1" * 64,
            })
        self.fixtures = {
            "schema": "billiardgl.phase4.overlay_replay_fixtures.v1",
            "status": "FIXTURE_VALIDATED_NOT_GPU_EVIDENCE",
            "source_sha256": fixture_generator.source_digest(),
            "training_pack_path": str(self.pack),
            "training_pack_sha256": digest(self.pack),
            "executable_sha256": digest(self.executable),
            "fixture_count": 12,
            "external_real_human_records": [],
            "fixtures": fixtures,
        }
        self.fixtures_path = self.root / "fixtures.json"
        self.fixtures_path.write_text(json.dumps(self.fixtures) + "\n",
                                      encoding="utf-8")
        self.bound = generator.bind(self.gate, self.fixtures_path,
                                    self.runner, self.root / "captures")
        self.bound_path = self.root / "bound.json"
        self.bound_path.write_text(json.dumps(self.bound) + "\n",
                                   encoding="utf-8")

    def tearDown(self):
        self.temporary.cleanup()

    def test_exact_batch_protocol_and_bindings(self):
        rows = dispatcher.plan_rows(self.bound)
        self.assertEqual(len(rows), 68)
        self.assertTrue(all(len(row.split("\t")) == 19 for row in rows))
        command = dispatcher.runner_command(self.runner,
            self.root / "plan.tsv", self.root / "gpu", self.root / "results",
            120, 300)
        self.assertEqual(len(command), 6)
        self.assertFalse(any(value.startswith("--capture") for value in command))
        gate, manifest = dispatcher.validate(self.gate_path, self.bound_path)
        dispatcher.validate_bound(gate, manifest, self.bound_path,
                                  self.fixtures_path, self.runner)
        self.assertEqual(manifest["capture_count"], 68)
        self.assertEqual(sum(row["terminal_outcome"] == "success"
                             for row in manifest["captures"]
                             if row["capture_id"].startswith("extension__")), 6)
        self.assertEqual(sum(row["terminal_outcome"] == "failure"
                             for row in manifest["captures"]
                             if row["capture_id"].startswith("extension__")), 4)

    def test_missing_fixture_fails_closed(self):
        Path(self.fixtures["fixtures"][0]["replay_path"]).unlink()
        with self.assertRaisesRegex(RuntimeError, "replay fixture drift"):
            generator.validate_fixture_manifest(self.gate, self.fixtures_path)

    def test_runner_and_executable_drift_fail_closed(self):
        self.runner.write_bytes(b"changed runner")
        with self.assertRaisesRegex(RuntimeError, "runner binding drift"):
            dispatcher.validate_bound(self.gate, self.bound, self.bound_path,
                                      self.fixtures_path, self.runner)
        self.runner.write_bytes(b"core41 batch runner test double")
        self.executable.write_bytes(b"changed executable")
        with self.assertRaisesRegex(RuntimeError, "executable binding drift"):
            dispatcher.validate_bound(self.gate, self.bound, self.bound_path,
                                      self.fixtures_path, self.runner)

    def test_incomplete_or_duplicate_results_fail_closed(self):
        incomplete = [{"capture_id": row["capture_id"]}
                      for row in self.bound["captures"][:-1]]
        with self.assertRaisesRegex(RuntimeError, "result set is incomplete"):
            dispatcher.validate_results(self.bound, incomplete,
                                        self.root / "gpu")
        duplicate = [{"capture_id": row["capture_id"]}
                     for row in self.bound["captures"]]
        duplicate.append(dict(duplicate[0]))
        duplicate.append({"record_type": "summary"})
        with self.assertRaisesRegex(RuntimeError,
                                    "formal runner result set is incomplete|duplicate"):
            dispatcher.validate_results(self.bound, duplicate,
                                        self.root / "gpu")

    def test_source_and_fixture_manifest_drift_fail_closed(self):
        drifted = json.loads(self.fixtures_path.read_text(encoding="utf-8"))
        drifted["source_sha256"] = "0" * 64
        self.fixtures_path.write_text(json.dumps(drifted) + "\n",
                                      encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "source digest drift"):
            dispatcher.validate_bound(self.gate, self.bound, self.bound_path,
                                      self.fixtures_path, self.runner)


if __name__ == "__main__":
    unittest.main()
