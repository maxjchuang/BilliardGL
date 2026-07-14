import json
import tempfile
import unittest
from pathlib import Path

from tools.physics_validation.analyzer import Failure, ScenarioResult
from tools.physics_validation.reference_accounting import (
    ReferenceFailureKey,
    reconcile_reference_failures,
)


def result(case_id, *failures):
    return ScenarioResult(
        f"synthetic_reference__{case_id}",
        not failures,
        "A",
        {},
        tuple(failures),
    )


def failure(code, metric):
    return Failure(code, metric, "message", 1.0, 2.0)


def model_item(case_id="case_model", code="MODEL_MISMATCH"):
    return {
        "dataset_id": "synthetic_reference",
        "case_id": case_id,
        "code": code,
        "metric": "speed",
        "rationale": "Current model does not represent the measured response.",
    }


def limitation_item(case_id="case_limited", code="REFERENCE_LIMITATION"):
    return {
        "dataset_id": "synthetic_reference",
        "case_id": case_id,
        "code": code,
        "metric": "angle",
        "missing_evidence": "Exact apparatus condition is unavailable.",
        "resolution_condition": "Obtain the complete experimental apparatus record.",
    }


class ReferenceAccountingTests(unittest.TestCase):
    def _manifest(self, failures):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        path = Path(temporary.name) / "manifest.json"
        path.write_text(
            json.dumps({"schema_version": 1, "failures": failures},
                       ensure_ascii=False, indent=2, sort_keys=True,
                       allow_nan=False) + "\n",
            encoding="utf-8")
        return path

    def test_exact_expected_sets_reconcile_without_hiding_failures(self):
        results = (
            result("case_model", failure("MODEL_MISMATCH", "speed")),
            result("case_limited", failure("REFERENCE_LIMITATION", "angle")),
            result("case_passed"),
        )

        accounting = reconcile_reference_failures(
            results,
            self._manifest([model_item()]),
            self._manifest([limitation_item()]),
            "synthetic_reference",
        )

        self.assertTrue(accounting.ci_passed)
        self.assertEqual(accounting.known_model_mismatches, frozenset({
            ReferenceFailureKey(
                "synthetic_reference", "case_model", "MODEL_MISMATCH", "speed")}))
        self.assertEqual(accounting.known_limitations, frozenset({
            ReferenceFailureKey(
                "synthetic_reference", "case_limited", "REFERENCE_LIMITATION", "angle")}))
        self.assertFalse(results[0].passed)
        self.assertFalse(results[1].passed)

    def test_new_or_missing_expected_failures_fail_ci(self):
        new = reconcile_reference_failures(
            [result("unexpected", failure("MODEL_MISMATCH", "speed"))],
            self._manifest([]), self._manifest([]), "synthetic_reference")
        missing = reconcile_reference_failures(
            [], self._manifest([model_item()]),
            self._manifest([limitation_item()]), "synthetic_reference")

        self.assertFalse(new.ci_passed)
        self.assertEqual(len(new.new_model_mismatches), 1)
        self.assertFalse(missing.ci_passed)
        self.assertEqual(len(missing.missing_model_mismatches), 1)
        self.assertEqual(len(missing.missing_limitations), 1)

    def test_versioned_scenario_id_reconciles_to_stable_reference_case(self):
        versioned = ScenarioResult(
            "synthetic_reference__case_model_v2", False, "B", {},
            (failure("MODEL_MISMATCH", "speed"),))

        accounting = reconcile_reference_failures(
            [versioned], self._manifest([model_item()]), self._manifest([]),
            "synthetic_reference",
            {versioned.scenario_id: "case_model"})

        self.assertTrue(accounting.ci_passed)

    def test_actual_numerical_integration_and_nondeterministic_failures_are_unallowlistable(self):
        results = [
            result("numeric", failure("NUMERICAL_FAILURE", "finite_state")),
            result("integration", failure("INTEGRATION_MISMATCH", "trace")),
            result("nondeterministic", failure("NON_DETERMINISTIC", "trace_equal")),
        ]

        accounting = reconcile_reference_failures(
            results, self._manifest([]), self._manifest([]), "synthetic_reference")

        self.assertFalse(accounting.ci_passed)
        self.assertEqual(
            {item.code for item in accounting.unallowlistable_failures},
            {"NUMERICAL_FAILURE", "INTEGRATION_MISMATCH", "NON_DETERMINISTIC"},
        )

    def test_rejects_duplicate_manifest_entries(self):
        for items, is_model in (([model_item(), model_item()], True),
                                ([limitation_item(), limitation_item()], False)):
            with self.subTest(is_model=is_model), self.assertRaisesRegex(
                    ValueError, "duplicate"):
                reconcile_reference_failures(
                    [],
                    self._manifest(items if is_model else []),
                    self._manifest([] if is_model else items),
                    "synthetic_reference",
                )

    def test_rejects_cross_dataset_manifest_entry(self):
        item = model_item()
        item["dataset_id"] = "other_dataset"

        with self.assertRaisesRegex(ValueError, "dataset_id"):
            reconcile_reference_failures(
                [], self._manifest([item]), self._manifest([]),
                "synthetic_reference")

    def test_rejects_wrong_failure_code_in_each_manifest(self):
        invalid_manifests = (
            (self._manifest([model_item(code="NUMERICAL_FAILURE")]), self._manifest([])),
            (self._manifest([]), self._manifest([
                limitation_item(code="INTEGRATION_MISMATCH")])),
        )
        for model_manifest, limitation_manifest in invalid_manifests:
            with self.subTest(model=model_manifest), self.assertRaisesRegex(
                    ValueError, "code"):
                reconcile_reference_failures(
                    [], model_manifest, limitation_manifest, "synthetic_reference")

    def test_rejects_missing_audit_fields_and_extra_schema_keys(self):
        model = model_item()
        del model["rationale"]
        limitation = limitation_item()
        limitation["notes"] = "unregistered field"

        for model_items, limitation_items in (([model], []), ([], [limitation])):
            with self.subTest(model_items=model_items), self.assertRaisesRegex(
                    ValueError, "schema"):
                reconcile_reference_failures(
                    [], self._manifest(model_items), self._manifest(limitation_items),
                    "synthetic_reference")

    def test_rejects_unsafe_case_or_metric_ids(self):
        for field, value in (("case_id", "../case"), ("metric", "bad metric")):
            item = model_item()
            item[field] = value
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, field):
                reconcile_reference_failures(
                    [], self._manifest([item]), self._manifest([]),
                    "synthetic_reference")


if __name__ == "__main__":
    unittest.main()
