from __future__ import annotations

import copy
import unittest

from scripts import phase5_rules_decision_ui_v5_exact16_runner_json_contract as contract


class V5Exact16RunnerJsonContractTests(unittest.TestCase):
    def setUp(self) -> None:
        self.fixture = contract.corrected_frozen_fixture()
        self.now = self.fixture[0]["expires_unix_seconds"] - 1

    def validate(self, fixture=None) -> dict:
        return contract.validate_contract(*(fixture or self.fixture), now_unix=self.now)

    def test_real_full_frozen_objects_with_only_gate_overlay_pass(self) -> None:
        result = contract.validate_real_frozen_with_approved_overlay()
        self.assertEqual(result["case_count"], 16)
        self.assertFalse(result["gpu_executed"])

    def test_every_auth_field_is_required(self) -> None:
        for field in contract.AUTH_REQUIRED:
            with self.subTest(field=field):
                fixture = copy.deepcopy(self.fixture)
                fixture[0].pop(field)
                with self.assertRaises(RuntimeError): self.validate(fixture)

    def test_every_manifest_field_is_required(self) -> None:
        for field in contract.MANIFEST_REQUIRED:
            with self.subTest(field=field):
                fixture = copy.deepcopy(self.fixture)
                fixture[1].pop(field)
                with self.assertRaises(RuntimeError): self.validate(fixture)

    def test_every_readiness_field_is_required(self) -> None:
        for field in contract.READY_REQUIRED:
            with self.subTest(field=field):
                fixture = copy.deepcopy(self.fixture)
                fixture[2].pop(field)
                with self.assertRaises(RuntimeError): self.validate(fixture)
        for field in contract.READY_FROZEN_REQUIRED:
            with self.subTest(field=field):
                fixture = copy.deepcopy(self.fixture)
                fixture[2]["frozen_bindings"].pop(field)
                with self.assertRaises(RuntimeError): self.validate(fixture)

    def test_every_marker_field_is_required(self) -> None:
        for field in contract.MARKER_REQUIRED:
            with self.subTest(field=field):
                fixture = copy.deepcopy(self.fixture)
                fixture[3].pop(field)
                with self.assertRaises(RuntimeError): self.validate(fixture)

    def test_wrong_gate_rejected(self) -> None:
        fixture = copy.deepcopy(self.fixture)
        fixture[1]["gate_sha256"] = "0" * 64
        with self.assertRaises(RuntimeError): self.validate(fixture)

    def test_wrong_aggregate_rejected(self) -> None:
        fixture = copy.deepcopy(self.fixture)
        fixture[1]["aggregate_sha256"] = "0" * 64
        with self.assertRaises(RuntimeError): self.validate(fixture)

    def test_wrong_readiness_schema_rejected(self) -> None:
        fixture = copy.deepcopy(self.fixture)
        fixture[2]["schema"] = "wrong"
        with self.assertRaises(RuntimeError): self.validate(fixture)

    def test_wrong_scene_order_rejected(self) -> None:
        fixture = copy.deepcopy(self.fixture)
        fixture[0]["scene_ids_in_order"][0:2] = reversed(
            fixture[0]["scene_ids_in_order"][0:2])
        with self.assertRaises(RuntimeError): self.validate(fixture)

    def test_expired_rejected(self) -> None:
        with self.assertRaises(RuntimeError):
            contract.validate_contract(*self.fixture,
                now_unix=self.fixture[0]["expires_unix_seconds"])

    def test_runner_access_inventory_rejects_removed_field(self) -> None:
        source = contract.RUNNER_SOURCE.read_text().replace(
            '"gate_sha256"', '"removed"', 1)
        with self.assertRaises(RuntimeError):
            contract.validate_runner_source_access_inventory(source)


if __name__ == "__main__":
    unittest.main()
