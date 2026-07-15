import hashlib
import io
import json
import shutil
import tempfile
import unittest
from contextlib import redirect_stderr
from pathlib import Path
from unittest.mock import patch

from tools.physics_validation.data_lifecycle import load_data_lifecycle
from tools.physics_validation.model_candidate import write_candidate_freeze
from tools.physics_validation.validation_run import main, run_candidate_validation
from tests.physics_validation.test_reference_run import trace_for_scenario


FIXTURE_ROOT = Path(__file__).parent / "fixtures/reference_package_v1"
CANDIDATE_FIXTURE = Path(__file__).parent / "fixtures/model_candidate_v1"
COMMITTED_LIFECYCLE = Path(__file__).parent / "validation_data_status.json"


def _canonical(document):
    return json.dumps(document, indent=2, sort_keys=True) + "\n"


class ValidationRunTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.candidate = self.root / "candidate"
        (self.candidate / "calibration").mkdir(parents=True)
        self.profile = self.candidate / "profile.json"
        self.report = self.candidate / "calibration/reference_report.json"
        shutil.copyfile(CANDIDATE_FIXTURE / "profile.json", self.profile)
        shutil.copyfile(CANDIDATE_FIXTURE / "calibration_report.json", self.report)
        self.executable = self.root / "Billiards"
        self.executable.write_bytes(b"synthetic executable")
        self.freeze = write_candidate_freeze(
            candidate_id="surface_motion_v1",
            formula_version="legacy_v1",
            source_revision="0123456789abcdef0123456789abcdef01234567",
            profile=self.profile,
            executable=self.executable,
            calibration_report=self.report,
            dataset_manifests=(FIXTURE_ROOT / "manifest.json",),
            created_at="2026-07-14T00:00:00Z",
            output=self.candidate / "freeze.json",
        )
        self.lifecycle = self.root / "lifecycle.json"
        self._write_lifecycle("validation")
        self.output = self.root / "validation"

    def _write_lifecycle(self, holdout_status):
        self.lifecycle.write_text(_canonical({
            "schema_version": 1,
            "datasets": [{
                "dataset_id": "synthetic_reference",
                "dataset_version": "1.0.0",
                "calibration_status": "calibration",
                "holdout_status": holdout_status,
            }],
        }), encoding="utf-8")

    def _make_v2_package(self, dataset_id):
        package = self.root / dataset_id
        shutil.copytree(FIXTURE_ROOT, package)
        if dataset_id != "synthetic_reference":
            for name in ("raw_extracted.csv", "normalized.csv", "split.json"):
                path = package / name
                path.write_text(
                    path.read_text(encoding="utf-8").replace(
                        "synthetic_reference", dataset_id),
                    encoding="utf-8",
                )
            extraction_path = package / "extraction.json"
            extraction = json.loads(
                extraction_path.read_text(encoding="utf-8"))
            extraction["inputs"][0]["sha256"] = "sha256:" + hashlib.sha256(
                (package / "raw_extracted.csv").read_bytes()).hexdigest()
            extraction_path.write_text(_canonical(extraction), encoding="utf-8")
        manifest_path = package / "manifest.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["dataset_id"] = dataset_id
        for item in manifest["files"]:
            item["sha256"] = "sha256:" + hashlib.sha256(
                (package / item["path"]).read_bytes()).hexdigest()
        manifest_path.write_text(_canonical(manifest), encoding="utf-8")
        return package, manifest

    def _write_v2_lifecycle(self):
        self.lifecycle.write_text(_canonical({
            "schema_version": 1,
            "datasets": [{
                "dataset_id": dataset_id,
                "dataset_version": "1.0.0",
                "calibration_status": "calibration",
                "holdout_status": "validation",
            } for dataset_id in (
                "synthetic_reference", "synthetic_reference_two")],
        }), encoding="utf-8")

    def _run(self, execute_once, package=FIXTURE_ROOT):
        with patch(
            "tools.physics_validation.validation_run.DEFAULT_LIFECYCLE_PATH",
            self.lifecycle,
        ):
            return run_candidate_validation(
                self.freeze,
                self.executable,
                package,
                self.profile,
                self.output,
                execute_once=execute_once,
            )

    def test_executes_only_holdout_and_writes_bound_receipt(self):
        seen = []

        def execute_once(executable, scenario):
            seen.append(scenario["id"])
            return trace_for_scenario(scenario)

        self.assertEqual(self._run(execute_once), 0)
        self.assertEqual(
            seen,
            ["synthetic_reference__free_roll_holdout"] * 2,
        )
        report_path = self.output / "reference_report.json"
        report = json.loads(report_path.read_text(encoding="utf-8"))
        freeze_hash = hashlib.sha256(self.freeze.read_bytes()).hexdigest()
        self.assertEqual(report["metadata"]["candidate_id"], "surface_motion_v1")
        self.assertEqual(report["metadata"]["freeze_sha256"], freeze_hash)
        self.assertEqual(report["partitions"]["CALIBRATION"]["summary"]["points"], 0)
        self.assertEqual(report["partitions"]["HOLDOUT"]["summary"]["points"], 1)

        receipt_path = self.output / "validation_receipt.json"
        receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
        self.assertEqual(receipt, {
            "schema_version": 1,
            "candidate_id": "surface_motion_v1",
            "freeze_sha256": freeze_hash,
            "dataset_id": "synthetic_reference",
            "dataset_version": "1.0.0",
            "partition": "HOLDOUT",
            "report_sha256": hashlib.sha256(report_path.read_bytes()).hexdigest(),
            "result": "PASSED_OR_ACCOUNTED",
        })
        self.assertEqual(
            receipt_path.read_text(encoding="utf-8"), _canonical(receipt))

    def test_hash_mismatches_fail_before_process_execution(self):
        mutations = {
            "executable": lambda: self.executable.write_bytes(b"changed executable"),
            "profile": lambda: self.profile.write_text(
                self.profile.read_text(encoding="utf-8") + " ", encoding="utf-8"),
            "calibration": lambda: self.report.write_text(
                self.report.read_text(encoding="utf-8") + " ", encoding="utf-8"),
        }
        for name, mutate in mutations.items():
            with self.subTest(name=name):
                self.setUp()
                seen = []
                mutate()
                with self.assertRaisesRegex(ValueError, "sha256"):
                    self._run(lambda executable, scenario: seen.append(scenario))
                self.assertEqual(seen, [])

    def test_package_mismatch_fails_before_process_execution(self):
        package = self.root / "changed_package"
        shutil.copytree(FIXTURE_ROOT, package)
        manifest = package / "manifest.json"
        manifest.write_text(
            manifest.read_text(encoding="utf-8") + " ", encoding="utf-8")
        seen = []
        with self.assertRaisesRegex(ValueError, "dataset manifest"):
            self._run(
                lambda executable, scenario: seen.append(scenario),
                package=package,
            )
        self.assertEqual(seen, [])

    def test_schema_v2_verifies_complete_freeze_and_validates_either_package(self):
        first_package, first_manifest = self._make_v2_package(
            "synthetic_reference")
        second_package, second_manifest = self._make_v2_package(
            "synthetic_reference_two")
        reports = []
        for package, manifest in (
                (first_package, first_manifest),
                (second_package, second_manifest)):
            report = json.loads(
                CANDIDATE_FIXTURE.joinpath(
                    "calibration_report.json").read_text(encoding="utf-8"))
            report["metadata"]["dataset_id"] = manifest["dataset_id"]
            report["metadata"]["package_hashes"] = {
                item["id"]: item["sha256"] for item in manifest["files"]}
            path = self.candidate / "calibration" / (
                manifest["dataset_id"] + ".json")
            path.write_text(_canonical(report), encoding="utf-8")
            reports.append(path)
        fit = self.root / "material_fit.json"
        fit.write_text('{"full_precision":true}\n', encoding="utf-8")
        self.freeze = write_candidate_freeze(
            candidate_id="surface_motion_v1",
            formula_version="legacy_v1",
            source_revision="0123456789abcdef0123456789abcdef01234567",
            profile=self.profile,
            executable=self.executable,
            calibration_report=None,
            calibration_reports=tuple(reports),
            dataset_manifests=(
                first_package / "manifest.json",
                second_package / "manifest.json",
            ),
            supplemental_artifacts=(fit,),
            repository_root=self.root,
            created_at="2026-07-14T00:00:00Z",
            output=self.candidate / "freeze.json",
        )
        self._write_v2_lifecycle()

        for package in (first_package, second_package):
            with self.subTest(package=package.name), patch(
                "tools.physics_validation.validation_run.DEFAULT_LIFECYCLE_PATH",
                self.lifecycle,
            ):
                seen = []
                exit_code = run_candidate_validation(
                    self.freeze,
                    self.executable,
                    package,
                    self.profile,
                    self.root / ("validation_" + package.name),
                    execute_once=lambda executable, scenario: (
                        seen.append(scenario["id"]),
                        trace_for_scenario(scenario),
                    )[1],
                    repository_root=self.root,
                )
                self.assertEqual(exit_code, 0)
                self.assertTrue(seen)

    def test_nonvalidation_lifecycle_states_fail_before_execution(self):
        for status in ("spent", "calibration"):
            with self.subTest(status=status):
                self._write_lifecycle(status)
                seen = []
                with self.assertRaisesRegex(ValueError, "HOLDOUT.*validation"):
                    self._run(lambda executable, scenario: seen.append(scenario))
                self.assertEqual(seen, [])

    def test_accounted_physical_mismatch_still_writes_receipt(self):
        package = self.root / "known_mismatch_package"
        shutil.copytree(FIXTURE_ROOT, package)
        mismatch_path = package / "expected_model_mismatches.json"
        mismatch_path.write_text(_canonical({
            "schema_version": 1,
            "failures": [{
                "dataset_id": "synthetic_reference",
                "case_id": "free_roll_holdout",
                "code": "MODEL_MISMATCH",
                "metric": "stopping_distance_cm",
                "rationale": "Deliberate validation fixture mismatch",
            }],
        }), encoding="utf-8")
        mismatch_hash = hashlib.sha256(mismatch_path.read_bytes()).hexdigest()
        manifest_path = package / "manifest.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        for item in manifest["files"]:
            if item["id"] == "expected_model_mismatches":
                item["sha256"] = "sha256:" + mismatch_hash
        manifest_path.write_text(_canonical(manifest), encoding="utf-8")

        report = json.loads(self.report.read_text(encoding="utf-8"))
        report["metadata"]["package_hashes"][
            "expected_model_mismatches"] = "sha256:" + mismatch_hash
        self.report.write_text(_canonical(report), encoding="utf-8")
        self.freeze = write_candidate_freeze(
            candidate_id="surface_motion_v1",
            formula_version="legacy_v1",
            source_revision="0123456789abcdef0123456789abcdef01234567",
            profile=self.profile,
            executable=self.executable,
            calibration_report=self.report,
            dataset_manifests=(manifest_path,),
            created_at="2026-07-14T00:00:00Z",
            output=self.candidate / "freeze.json",
        )

        exit_code = self._run(
            lambda executable, scenario: trace_for_scenario(
                scenario, final_offset=1.0),
            package=package,
        )
        self.assertEqual(exit_code, 0)
        validation_report = json.loads(
            (self.output / "reference_report.json").read_text(encoding="utf-8"))
        self.assertEqual(
            validation_report["partitions"]["HOLDOUT"]["points"][0]["status"],
            "MODEL_MISMATCH_KNOWN",
        )
        receipt = json.loads(
            (self.output / "validation_receipt.json").read_text(encoding="utf-8"))
        self.assertEqual(receipt["result"], "PASSED_OR_ACCOUNTED")

    def test_lifecycle_schema_and_committed_registry_are_strict(self):
        registry = load_data_lifecycle(COMMITTED_LIFECYCLE)
        self.assertEqual(len(registry.datasets), 12)
        self.assertEqual(
            registry.entry(
                "alciatore_2005_tp_a15", "1.0.0").holdout_status,
            "confirmation")
        self.assertEqual(
            registry.entry("sudo_2002", "1.0.0").holdout_status,
            "spent")
        self.assertEqual(
            registry.entry("derby_fuller_1999", "1.0.0").holdout_status,
            "spent")
        self.assertEqual(
            registry.entry("han_2005", "1.0.0").holdout_status,
            "confirmation")
        self.assertEqual(
            registry.entry(
                "mathavan_2010_cushion", "1.0.0").holdout_status,
            "spent")
        self.assertEqual(
            registry.entry("cue_contact_analytic_contract", "1.0.0").holdout_status,
            "validation")
        self.assertEqual(
            registry.entry(
                "pocket_geometry_analytic_contract", "1.0.0").holdout_status,
            "validation")
        self.assertEqual(
            registry.entry(
                "multi_contact_solver_analytic_contract", "1.0.0").holdout_status,
            "validation")
        shimamura = registry.entry("shimamura_2006_cue_contact", "1.0.0")
        self.assertEqual(
            (shimamura.calibration_status, shimamura.holdout_status),
            ("calibration", "spent"))
        document = json.loads(self.lifecycle.read_text(encoding="utf-8"))
        document["datasets"][0]["holdout_status"] = "secret"
        self.lifecycle.write_text(_canonical(document), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "lifecycle state"):
            load_data_lifecycle(self.lifecycle)

    def test_cli_exposes_no_partition_or_parameter_escape_hatches(self):
        for argument in (
                "--calibration", "--case", "--split", "--partition", "--parameter"):
            with self.subTest(argument=argument), redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as raised:
                    main([
                        "--freeze", str(self.freeze),
                        "--executable", str(self.executable),
                        "--package", str(FIXTURE_ROOT),
                        "--profile", str(self.profile),
                        "--output", str(self.output),
                        argument, "anything",
                    ])
            self.assertEqual(raised.exception.code, 2)


if __name__ == "__main__":
    unittest.main()
