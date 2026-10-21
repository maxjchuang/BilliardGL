from __future__ import annotations

import copy
import json
import subprocess
import sys
import unittest
from pathlib import Path

from scripts.validate_phase5_wpa_heyball_completeness_audit import (
    EXPECTED_M4_ROWS,
    EXPECTED_STATUS,
    OFFICIAL_SHA256,
    OFFICIAL_URL,
    REQUIRED_MAPPING_KEYS,
    validate_audit,
)


ROOT = Path(__file__).resolve().parents[2]
AUDIT = ROOT / "docs/rules/phase5-wpa-heyball-completeness-audit.json"
SCHEMA = ROOT / "docs/rules/phase5-wpa-heyball-completeness-audit.schema.json"


class Phase5WpaHeyballCompletenessAuditTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.value = json.loads(AUDIT.read_text(encoding="utf-8"))

    def mutate(self):
        return copy.deepcopy(self.value)

    def test_committed_audit_is_valid_and_explicitly_incomplete(self):
        self.assertEqual(validate_audit(self.value), [])
        self.assertFalse(self.value["profile_complete"])
        self.assertFalse(self.value["complete_profile_product_exposed"])
        self.assertEqual(len(self.value["matrix"]), 40)
        self.assertEqual(self.value["status_summary"], {
            "IMPLEMENTED": 4,
            "PARTIAL": 12,
            "MISSING": 17,
            "PROCEDURAL_ONLY": 7,
            "total": 40,
        })

    def test_official_pdf_identity_and_page_citations_are_frozen(self):
        source = self.value["normative_source"]
        self.assertEqual(source["url"], OFFICIAL_URL)
        self.assertEqual(source["sha256"], OFFICIAL_SHA256)
        self.assertEqual(source["page_count"], 21)
        self.assertEqual(source["retrieval_status"], "VERIFIED")
        for row in self.value["matrix"]:
            self.assertTrue(row["source"]["chapter"])
            self.assertTrue(row["source"]["clause"])
            self.assertTrue(row["source"]["locator"])
            self.assertTrue(all(1 <= page <= 21 for page in row["source"]["pages"]))

    def test_every_clause_maps_all_six_product_layers(self):
        self.assertEqual({row["id"] for row in self.value["matrix"]},
                         set(EXPECTED_STATUS))
        for row in self.value["matrix"]:
            self.assertTrue(REQUIRED_MAPPING_KEYS.issubset(row))
            self.assertIn("coverage", row["shot_facts"])
            self.assertIn("coverage", row["rules_logic"])
            self.assertIn("coverage", row["runtime_action"])
            self.assertIn("reason_codes", row["reason_text"])
            self.assertIn("text_keys", row["reason_text"])
            self.assertIn("interactive_actions_product_exposed", row["ui_action"])
            self.assertIn("coverage", row["tests"])

    def test_pending_capabilities_are_reciprocal_and_fail_closed(self):
        by_id = {item["id"]: item for item in self.value["pending_capabilities"]}
        row_refs = {capability: set() for capability in by_id}
        for row in self.value["matrix"]:
            for capability in row["ui_action"]["pending_capabilities"]:
                row_refs[capability].add(row["id"])
                self.assertFalse(row["ui_action"]["interactive_actions_product_exposed"])
        for capability, item in by_id.items():
            self.assertEqual(item["state"], "pending")
            self.assertFalse(item["product_exposed"])
            self.assertEqual(set(item["source_rows"]), row_refs[capability])

    def test_m4_minimum_is_exact_and_covers_requested_high_risk_rules(self):
        closure = self.value["m4_minimum_closure"]
        self.assertEqual(set(closure["required_rows"]), EXPECTED_M4_ROWS)
        self.assertTrue(closure["profile_must_remain_incomplete_until_gate_passes"])
        for row_id in (
            "WPA-II-06-E", "WPA-II-06-F", "WPA-II-08", "WPA-II-10-4",
            "WPA-II-12-5-6", "WPA-II-12-7", "WPA-II-13",
            "WPA-II-15-1", "WPA-II-15-2", "WPA-II-16",
            "WPA-II-20", "WPA-II-21",
        ):
            self.assertIn(row_id, EXPECTED_M4_ROWS)

    def test_profile_completion_claim_is_rejected(self):
        value = self.mutate()
        value["profile_complete"] = True
        value["complete_profile_product_exposed"] = True
        errors = validate_audit(value)
        self.assertTrue(any("must remain false" in error for error in errors))

    def test_missing_official_clause_is_rejected(self):
        value = self.mutate()
        value["matrix"].pop()
        errors = validate_audit(value)
        self.assertTrue(any("clause inventory differs" in error for error in errors))

    def test_bad_page_or_pdf_fingerprint_is_rejected(self):
        value = self.mutate()
        value["normative_source"]["sha256"] = "0" * 64
        value["matrix"][0]["source"]["pages"] = [22]
        errors = validate_audit(value)
        self.assertTrue(any("PDF identity" in error for error in errors))
        self.assertTrue(any("page citation invalid" in error for error in errors))

    def test_status_upgrade_without_implementation_audit_is_rejected(self):
        value = self.mutate()
        row = next(row for row in value["matrix"] if row["id"] == "WPA-II-16")
        row["status"] = "IMPLEMENTED"
        errors = validate_audit(value)
        self.assertTrue(any("audited status differs: WPA-II-16" in error
                            for error in errors))
        self.assertTrue(any("status summary" in error for error in errors))

    def test_pending_action_exposure_is_rejected(self):
        value = self.mutate()
        value["pending_capabilities"][0]["product_exposed"] = True
        row = next(row for row in value["matrix"]
                   if row["ui_action"]["pending_capabilities"])
        row["ui_action"]["interactive_actions_product_exposed"] = True
        errors = validate_audit(value)
        self.assertTrue(any("must remain fail closed" in error for error in errors))
        self.assertTrue(any("pending interactive UI/action" in error for error in errors))

    def test_pending_row_reference_drift_is_rejected(self):
        value = self.mutate()
        value["pending_capabilities"][0]["source_rows"].pop()
        errors = validate_audit(value)
        self.assertTrue(any("row references differ" in error for error in errors))

    def test_missing_mapping_layer_is_rejected(self):
        value = self.mutate()
        del value["matrix"][0]["shot_facts"]
        errors = validate_audit(value)
        self.assertTrue(any("mapping incomplete" in error for error in errors))

    def test_off_table_source_conflict_cannot_be_silently_resolved(self):
        value = self.mutate()
        row = next(row for row in value["matrix"] if row["id"] == "WPA-II-12-3")
        row["policy_decision_required"] = False
        row["source"]["pages"] = [11]
        row["gaps"] = ["Resolved automatically."]
        errors = validate_audit(value)
        self.assertTrue(any("contradiction must remain explicit" in error
                            for error in errors))

    def test_schema_is_valid_json_and_preserves_fail_closed_consts(self):
        schema = json.loads(SCHEMA.read_text(encoding="utf-8"))
        self.assertEqual(schema["properties"]["profile_complete"]["const"], False)
        self.assertEqual(schema["properties"]["complete_profile_product_exposed"]["const"], False)
        pending = schema["properties"]["pending_capabilities"]["items"]["properties"]
        self.assertEqual(pending["state"]["const"], "pending")
        self.assertEqual(pending["product_exposed"]["const"], False)

    def test_validator_cli_reports_incomplete_fail_closed_boundary(self):
        result = subprocess.run(
            [sys.executable,
             str(ROOT / "scripts/validate_phase5_wpa_heyball_completeness_audit.py"),
             str(AUDIT)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("rows=40 implemented=4 partial=12 missing=17 procedural=7", result.stdout)
        self.assertIn("m4_required=24", result.stdout)
        self.assertIn("profile_complete=false pending_fail_closed=true", result.stdout)


if __name__ == "__main__":
    unittest.main()
