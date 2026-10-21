from __future__ import annotations

import copy
import json
from pathlib import Path
import unittest

from scripts.validate_phase5_wpa_heyball_as_built_audit_v3 import validate


ROOT = Path(__file__).resolve().parents[2]
AUDIT = ROOT / "docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v3.json"


class WpaHeyballAuditV3Tests(unittest.TestCase):
    def setUp(self) -> None:
        self.value = json.loads(AUDIT.read_text(encoding="utf-8"))

    def row(self, value: dict, row_id: str) -> dict:
        return next(row for row in value["matrix"] if row["id"] == row_id)

    def test_generated_audit_passes(self) -> None:
        self.assertEqual([], validate(self.value))

    def test_rejects_target_without_product_reachability(self) -> None:
        for row_id in ("WPA-II-04", "WPA-II-07"):
            with self.subTest(row=row_id):
                bad = copy.deepcopy(self.value)
                self.row(bad, row_id)["product_reachable"] = False
                self.assertTrue(validate(bad, check_files=False))

    def test_rejects_target_gap_or_partial_status(self) -> None:
        bad = copy.deepcopy(self.value)
        self.row(bad, "WPA-II-04")["status"] = "PARTIAL"
        self.assertTrue(validate(bad, check_files=False))
        bad = copy.deepcopy(self.value)
        self.row(bad, "WPA-II-07")["gaps"] = ["foundation exposed"]
        self.assertTrue(validate(bad, check_files=False))

    def test_rejects_non_target_promotion(self) -> None:
        bad = copy.deepcopy(self.value)
        self.row(bad, "WPA-II-06-C")["status"] = "IMPLEMENTED"
        self.assertTrue(validate(bad, check_files=False))

    def test_rejects_foundation_or_global_ui_exposure(self) -> None:
        for key in ("foundation_actions_product_exposed",
                    "rules_decision_ui_product_exposed"):
            with self.subTest(key=key):
                bad = copy.deepcopy(self.value)
                bad["targeted_product_closure"][key] = True
                self.assertTrue(validate(bad, check_files=False))

    def test_rejects_normative_seeded_order_or_old_visual_inheritance(self) -> None:
        bad = copy.deepcopy(self.value)
        bad["targeted_product_closure"]["seeded_numbered_ball_order_normative"] = True
        self.assertTrue(validate(bad, check_files=False))
        bad = copy.deepcopy(self.value)
        bad["targeted_product_closure"]["historical_phase2_phase4_phase5_visual_evidence_inheritable"] = True
        self.assertTrue(validate(bad, check_files=False))

    def test_rejects_missing_product_source_or_test_evidence(self) -> None:
        for row_id, path in (
            ("WPA-II-04", "src/Billiards/product_shell_controller.cpp"),
            ("WPA-II-07", "tests/rules_decision_session_integration_tests.cpp"),
        ):
            with self.subTest(row=row_id):
                bad = copy.deepcopy(self.value)
                row = self.row(bad, row_id)
                row["evidence"] = [item for item in row["evidence"]
                                   if item["path"] != path]
                self.assertTrue(validate(bad, check_files=False))

    def test_rejects_bad_evidence_path_or_hash(self) -> None:
        bad = copy.deepcopy(self.value)
        self.row(bad, "WPA-II-04")["evidence"][0]["path"] = "../escape"
        self.assertTrue(validate(bad, check_files=False))
        bad = copy.deepcopy(self.value)
        self.row(bad, "WPA-II-07")["evidence"][0]["sha256"] = "0" * 64
        self.assertTrue(validate(bad))

    def test_rejects_wrong_counts_or_predecessor(self) -> None:
        bad = copy.deepcopy(self.value)
        bad["summary"]["implemented"] = 11
        self.assertTrue(validate(bad, check_files=False))
        bad = copy.deepcopy(self.value)
        bad["frozen_v2_predecessor"]["sha256"] = "0" * 64
        self.assertTrue(validate(bad, check_files=False))

    def test_rejects_inherited_or_falsely_recomputed_downstream_evidence(self) -> None:
        bad = copy.deepcopy(self.value)
        bad["downstream_binding_impact"]["phase4_probe_digest"] = "UNCHANGED"
        self.assertTrue(validate(bad, check_files=False))
        bad = copy.deepcopy(self.value)
        bad["downstream_binding_impact"]["old_visual_or_machine_evidence_modified"] = True
        self.assertTrue(validate(bad, check_files=False))


if __name__ == "__main__":
    unittest.main()
