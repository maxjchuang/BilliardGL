#!/usr/bin/env python3
"""Mutation tests for the Phase4 targeted11 NOT_RUN readiness boundary."""

from __future__ import annotations

import copy
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
VALIDATOR = ROOT / "scripts/validate_phase4_final_ownership_targeted_readiness.py"
PREPARER = ROOT / "scripts/prepare_phase4_final_ownership_targeted_readiness.py"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class ReadinessMutationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        value = os.environ.get("PHASE4_READINESS_MANIFEST")
        if not value:
            raise unittest.SkipTest("PHASE4_READINESS_MANIFEST is required")
        cls.manifest_path = Path(value).resolve()
        cls.baseline = json.loads(cls.manifest_path.read_text())
        baseline = subprocess.run(
            ["python3", str(VALIDATOR), str(cls.manifest_path)],
            cwd=ROOT, text=True, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, check=False)
        if baseline.returncode != 0:
            raise AssertionError(f"baseline readiness is invalid:\n{baseline.stdout}")

    def validate_mutation(self, mutate, expected_error: str) -> None:
        with tempfile.TemporaryDirectory(prefix="phase4-readiness-mutation-") as temp:
            root = Path(temp)
            manifest = copy.deepcopy(self.baseline)
            mutate(manifest, root)
            path = root / "targeted11-readiness-mutated.json"
            path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
            result = subprocess.run(
                ["python3", str(VALIDATOR), str(path)], cwd=ROOT, text=True,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertIn(expected_error, result.stdout)

    @staticmethod
    def mutate_bound_json(manifest: dict, root: Path, path_key: str,
                          sha_key: str, mutation) -> None:
        value = json.loads(Path(manifest[path_key]).read_text())
        mutation(value)
        path = root / Path(manifest[path_key]).name
        path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")
        manifest[path_key] = str(path)
        manifest[sha_key] = sha(path)

    def test_gate_binding_mutation_fails(self) -> None:
        self.validate_mutation(
            lambda manifest, _: manifest["gate_bindings"].update(
                {"ta_gate_sha256": "0" * 64}),
            "gate_bindings_invalid")

    def test_ordinal2_probe_inventory_binding_mutation_fails(self) -> None:
        self.validate_mutation(
            lambda manifest, _: manifest["immutable_invariants"].update(
                {"ordinal2_source_probe_denominator": 82}),
            "immutable_invariants_drift")

    def test_expected_artifact_mutation_fails(self) -> None:
        self.validate_mutation(
            lambda manifest, _: manifest[
                "required_expected_release_artifact_sha256"].update(
                    {"runner": "0" * 64}),
            "required_expected_release_artifact_sha_mismatch")

    def test_source_aggregate_mutation_fails(self) -> None:
        def mutate(manifest: dict, root: Path) -> None:
            self.mutate_bound_json(
                manifest, root, "source_binding_manifest_path",
                "source_binding_manifest_sha256",
                lambda value: value.update(
                    {"implementation_source_aggregate_sha256": "0" * 64}))
        self.validate_mutation(mutate, "source_binding_semantics_invalid")

    def test_cpu_receipt_semantic_mutation_fails(self) -> None:
        def mutate(manifest: dict, root: Path) -> None:
            self.mutate_bound_json(
                manifest, root, "cpu_fixture_receipt_path",
                "cpu_fixture_receipt_sha256",
                lambda value: value.update({"fixture_count": 11}))
        self.validate_mutation(mutate, "cpu_fixture_receipt_semantics_invalid")

    def test_frozen_receipt_semantic_mutation_fails(self) -> None:
        def mutate(manifest: dict, root: Path) -> None:
            self.mutate_bound_json(
                manifest, root, "frozen_replay_receipt_path",
                "frozen_replay_receipt_sha256",
                lambda value: value.update({"integrated_ownership_claimed": True}))
        self.validate_mutation(mutate, "frozen_replay_receipt_semantics_invalid")

    def test_static_receipt_semantic_mutation_fails(self) -> None:
        def mutate(manifest: dict, root: Path) -> None:
            self.mutate_bound_json(
                manifest, root, "static_conformance_receipt_path",
                "static_conformance_receipt_sha256",
                lambda value: value["checks"].update(
                    {"default_off_and_color_attachment_zero_invariance": False}))
        self.validate_mutation(mutate, "static_conformance_receipt_semantics_invalid")

    def test_plan_self_rehash_mutation_still_fails(self) -> None:
        def mutate(manifest: dict, root: Path) -> None:
            plan = root / "mutated-plan.tsv"
            plan.write_bytes(Path(manifest["plan_path"]).read_bytes() + b"\n")
            manifest["plan_path"] = str(plan)
            manifest["plan_sha256"] = sha(plan)
        self.validate_mutation(mutate, "canonical_plan_binding_drift")

    def test_duplicate_capture_ordinal_and_id_fail(self) -> None:
        def mutate(manifest: dict, _: Path) -> None:
            manifest["captures"][1]["ordinal_one_based"] = 1
            manifest["captures"][1]["capture_id"] = manifest["captures"][0]["capture_id"]
        self.validate_mutation(mutate, "capture_ordinal_or_id_not_unique")

    def test_preparer_rejects_wrong_required_expected_sha_before_writes(self) -> None:
        build = Path(self.baseline["runner_path"]).parent
        cpu = Path(json.loads(Path(
            self.baseline["cpu_fixture_receipt_path"]).read_text())[
                "telemetry_test_binary"]).parent
        with tempfile.TemporaryDirectory(prefix="phase4-preparer-reject-") as temp:
            root = Path(temp)
            ready = root / "ready-must-not-exist"
            output = root / "output-must-not-exist"
            expected = self.baseline["required_expected_release_artifact_sha256"]
            result = subprocess.run([
                "python3", str(PREPARER),
                "--build-dir", str(build),
                "--cpu-build-dir", str(cpu),
                "--ready-dir", str(ready),
                "--output-root", str(output),
                "--expected-billiards-sha256", expected["billiards"],
                "--expected-runner-sha256", "0" * 64,
                "--expected-runner-source-sha256", expected["runner_source"],
                "--lineage-ready-manifest",
                self.baseline["lineage_v1_ready_manifest_path"],
                "--superseded-v2-ready-manifest",
                self.baseline["supersedes_v2_ready_manifest_path"],
                "--failed-v3-ready-manifest",
                self.baseline["failed_v3_ready_manifest_path"],
                "--failed-v4-ready-manifest",
                self.baseline["failed_v4_ready_manifest_path"],
                "--superseded-v5-ready-manifest",
                self.baseline["superseded_v5_ready_manifest_path"],
                "--superseded-v6-ready-manifest",
                self.baseline["superseded_v6_ready_manifest_path"],
                "--failed-v7-ready-manifest",
                self.baseline["failed_v7_ready_manifest_path"],
                "--failed-v8-ready-manifest",
                self.baseline["failed_v8_ready_manifest_path"],
                "--failed-v9-ready-manifest",
                self.baseline["failed_v9_ready_manifest_path"],
                "--failed-v10-ready-manifest",
                self.baseline["failed_v10_ready_manifest_path"],
            ], cwd=ROOT, text=True, stdout=subprocess.PIPE,
               stderr=subprocess.STDOUT, check=False)
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertIn("does not match required expected SHA", result.stdout)
            self.assertFalse(ready.exists())
            self.assertFalse(output.exists())

    def test_failed_v3_lineage_mutation_fails(self) -> None:
        self.validate_mutation(
            lambda manifest, _: manifest.update(
                {"failed_v3_retry_authorized": True}),
            "failed_v3_lineage_invalid")

    def test_failed_v4_lineage_mutation_fails(self) -> None:
        self.validate_mutation(
            lambda manifest, _: manifest.update(
                {"failed_v4_retry_authorized": True}),
            "failed_v4_lineage_invalid")

    def test_superseded_v5_lineage_mutation_fails(self) -> None:
        self.validate_mutation(
            lambda manifest, _: manifest.update(
                {"superseded_v5_capture_authorized": True}),
            "superseded_v5_readiness_invalid")

    def test_superseded_v6_lineage_mutation_fails(self) -> None:
        self.validate_mutation(
            lambda manifest, _: manifest.update(
                {"superseded_v6_capture_authorized": True}),
            "superseded_v6_readiness_invalid")

    def test_failed_v7_lineage_mutation_fails(self) -> None:
        self.validate_mutation(
            lambda manifest, _: manifest.update(
                {"failed_v7_retry_authorized": True}),
            "failed_v7_lineage_invalid")

    def test_failed_v8_lineage_mutation_fails(self) -> None:
        self.validate_mutation(
            lambda manifest, _: manifest.update(
                {"failed_v8_retry_authorized": True}),
            "failed_v8_lineage_invalid")

    def test_failed_v9_lineage_mutation_fails(self) -> None:
        self.validate_mutation(
            lambda manifest, _: manifest.update(
                {"failed_v9_retry_authorized": True}),
            "failed_v9_lineage_invalid")

    def test_failed_v10_lineage_mutation_fails(self) -> None:
        self.validate_mutation(
            lambda manifest, _: manifest.update(
                {"failed_v10_retry_authorized": True}),
            "failed_v10_lineage_invalid")

    def test_frozen_v7_counterfactual_cannot_become_machine_pass(self) -> None:
        def mutate(manifest, root):
            self.mutate_bound_json(manifest, root,
                "frozen_v7_association_counterfactual_receipt_path",
                "frozen_v7_association_counterfactual_receipt_sha256",
                lambda receipt: receipt.update(
                    {"corrected_capture_claimed": True}))
        self.validate_mutation(mutate,
            "frozen_v7_counterfactual_semantics_invalid")

    def test_frozen_v8_failure_cannot_become_machine_pass(self) -> None:
        def mutate(manifest, root):
            self.mutate_bound_json(manifest, root,
                "frozen_v8_visual_failure_receipt_path",
                "frozen_v8_visual_failure_receipt_sha256",
                lambda receipt: receipt.update({"machine_pass": True}))
        self.validate_mutation(mutate,
            "frozen_v8_visual_failure_semantics_invalid")

    def test_frozen_v9_failure_cannot_become_machine_pass(self) -> None:
        def mutate(manifest, root):
            self.mutate_bound_json(manifest, root,
                "frozen_v9_compositing_failure_receipt_path",
                "frozen_v9_compositing_failure_receipt_sha256",
                lambda receipt: receipt["authority_separation"][
                    "corrected_integrated"].update({"visible": 95}))
        self.validate_mutation(mutate,
            "frozen_v9_compositing_failure_semantics_invalid")

    def test_frozen_v9_old_validator_is_only_expected_superseded_drift(self) -> None:
        def mutate(manifest, root):
            self.mutate_bound_json(manifest, root,
                "frozen_v9_superseded_validator_lineage_receipt_path",
                "frozen_v9_superseded_validator_lineage_receipt_sha256",
                lambda receipt: receipt["superseded_validator_output"].update(
                    {"status": "PASS"}))
        self.validate_mutation(mutate,
            "frozen_v9_superseded_lineage_semantics_invalid")

    def test_frozen_v9_superseded_lineage_binds_successor_aggregate(self) -> None:
        def mutate(manifest, root):
            self.mutate_bound_json(manifest, root,
                "frozen_v9_superseded_validator_lineage_receipt_path",
                "frozen_v9_superseded_validator_lineage_receipt_sha256",
                lambda receipt: receipt.update({
                    "successor_implementation_source_aggregate_sha256":
                        "0" * 64}))
        self.validate_mutation(mutate,
            "frozen_v9_superseded_lineage_semantics_invalid")

    def test_frozen_v10_failure_cannot_claim_exclusive_predicate(self) -> None:
        def mutate(manifest, root):
            self.mutate_bound_json(manifest, root,
                "frozen_v10_machine_contract_failure_receipt_path",
                "frozen_v10_machine_contract_failure_receipt_sha256",
                lambda receipt: receipt["root_cause"].update({
                    "exclusivity_of_this_single_failure_proven": True}))
        self.validate_mutation(mutate,
            "frozen_v10_machine_contract_failure_semantics_invalid")

    def test_frozen_v10_failure_predicate_contract_is_pinned(self) -> None:
        def mutate(manifest, root):
            self.mutate_bound_json(manifest, root,
                "frozen_v10_machine_contract_failure_receipt_path",
                "frozen_v10_machine_contract_failure_receipt_sha256",
                lambda receipt: receipt["static_machine_predicate_evidence"].update({
                    "frozen_runner_required_exact_draw_submissions": 2}))
        self.validate_mutation(mutate,
            "frozen_v10_machine_contract_failure_semantics_invalid")

    def test_frozen_v10_old_validator_is_only_expected_superseded_drift(self) -> None:
        def mutate(manifest, root):
            self.mutate_bound_json(manifest, root,
                "frozen_v10_superseded_validator_lineage_receipt_path",
                "frozen_v10_superseded_validator_lineage_receipt_sha256",
                lambda receipt: receipt["superseded_validator_output"].update(
                    {"status": "PASS"}))
        self.validate_mutation(mutate,
            "frozen_v10_superseded_lineage_semantics_invalid")

    def test_frozen_v10_superseded_lineage_binds_successor_aggregate(self) -> None:
        def mutate(manifest, root):
            self.mutate_bound_json(manifest, root,
                "frozen_v10_superseded_validator_lineage_receipt_path",
                "frozen_v10_superseded_validator_lineage_receipt_sha256",
                lambda receipt: receipt.update({
                    "successor_all_source_test_tooling_aggregate_sha256":
                        "0" * 64}))
        self.validate_mutation(mutate,
            "frozen_v10_superseded_lineage_semantics_invalid")


if __name__ == "__main__":
    unittest.main()
