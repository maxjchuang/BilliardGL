import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
AUDIT = ROOT / "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v2.json"
VALIDATOR = ROOT / "scripts/validate_phase5_wpa_heyball_as_built_audit_v2.py"
spec = importlib.util.spec_from_file_location("audit_v2_validator", VALIDATOR)
validator = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(validator)


class Phase5WpaHeyballAsBuiltAuditV2Tests(unittest.TestCase):
    def setUp(self):
        self.value = json.loads(AUDIT.read_text(encoding="utf-8"))

    def errors(self, value):
        return validator.validate_audit(value, check_files=True)

    def row(self, value, row_id):
        return next(row for row in value["matrix"] if row["id"] == row_id)

    def test_committed_audit_is_valid_and_not_old_count(self):
        self.assertEqual([], self.errors(self.value))
        self.assertEqual({"total": 40, "implemented": 8, "partial": 12,
                          "missing": 13, "procedural_only": 7}, self.value["summary"])
        self.assertNotEqual((4, 12, 17, 7),
                            tuple(self.value["summary"][key] for key in
                                  ("implemented", "partial", "missing", "procedural_only")))

    def test_frozen_v1_files_match_exact_sha(self):
        for path, expected in validator.EXPECTED_PREDECESSOR.items():
            actual = hashlib.sha256((ROOT / path).read_bytes()).hexdigest()
            self.assertEqual(expected, actual, path)

    def test_foundation_or_authority_cannot_be_promoted_without_product(self):
        bad = copy.deepcopy(self.value)
        row = self.row(bad, "WPA-II-06-C")
        row["status"] = "IMPLEMENTED"
        bad["summary"]["implemented"] += 1
        bad["summary"]["partial"] -= 1
        errors = self.errors(bad)
        self.assertTrue(any("unproductized" in error for error in errors))

    def test_profile_and_product_gates_fail_closed(self):
        for key in ("profile_complete", "product_exposed",
                    "complete_profile_product_exposed"):
            bad = copy.deepcopy(self.value)
            bad[key] = True
            self.assertTrue(any("must remain false" in error for error in self.errors(bad)))

    def test_policy_conflict_cannot_be_resolved_or_declassified(self):
        bad = copy.deepcopy(self.value)
        bad["policy_conflicts"][0]["resolved"] = True
        self.assertTrue(any("policy conflict" in error for error in self.errors(bad)))
        bad = copy.deepcopy(self.value)
        self.row(bad, "WPA-II-12-3")["policy_decision_required"] = False
        self.assertTrue(any("policy conflict" in error for error in self.errors(bad)))

    def test_section_6g_must_stay_classification_pending(self):
        bad = copy.deepcopy(self.value)
        bad["classification_pending"][0]["status"] = "resolved"
        self.assertTrue(any("6(g)" in error for error in self.errors(bad)))
        bad = copy.deepcopy(self.value)
        self.row(bad, "WPA-II-06-G")["classification_error"] = "none"
        self.assertTrue(any("6(g)" in error for error in self.errors(bad)))

    def test_evidence_path_sha_and_lines_are_verified(self):
        bad = copy.deepcopy(self.value)
        self.row(bad, "WPA-II-20")["evidence"][0]["sha256"] = "0" * 64
        self.assertTrue(any("path/SHA mismatch" in error for error in self.errors(bad)))
        bad = copy.deepcopy(self.value)
        evidence = self.row(bad, "WPA-II-20")["evidence"][0]
        evidence["path"] = "../outside.cpp"
        self.assertTrue(any("metadata invalid" in error for error in self.errors(bad)))
        bad = copy.deepcopy(self.value)
        evidence = self.row(bad, "WPA-II-20")["evidence"][0]
        evidence["lines"]["end"] = 999999
        self.assertTrue(any("line range invalid" in error for error in self.errors(bad)))

    def test_implemented_row_requires_positive_source_and_test(self):
        bad = copy.deepcopy(self.value)
        row = self.row(bad, "WPA-II-13")
        row["evidence"] = [item for item in row["evidence"]
                           if item["kind"] != "test_positive"]
        self.assertTrue(any("positive source and test" in error for error in self.errors(bad)))

    def test_counts_and_m4_unclosed_are_derived(self):
        bad = copy.deepcopy(self.value)
        bad["summary"] = {"total": 40, "implemented": 4, "partial": 12,
                          "missing": 17, "procedural_only": 7}
        self.assertTrue(any("8/12/13/7" in error for error in self.errors(bad)))
        bad = copy.deepcopy(self.value)
        bad["m4_required_unclosed"].pop()
        self.assertTrue(any("M4-required" in error for error in self.errors(bad)))

    def test_deterministic_plan_rejects_policy_required_rows(self):
        bad = copy.deepcopy(self.value)
        bad["next_deterministic_implementation_plan"][0]["rows"].append("WPA-II-12-3")
        self.assertTrue(any("policy-required" in error for error in self.errors(bad)))


if __name__ == "__main__":
    unittest.main()
