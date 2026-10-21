from __future__ import annotations

import copy
import inspect
import unittest

from scripts import validate_phase2_v10i_formal_targeted_camera_v5_machine_pass_receipt as validator


class Phase2V10iTargetedCameraV5MachinePassReceiptTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.receipt = validator.load(validator.DEFAULT_RECEIPT)

    def assert_structure_rejects(self, mutation) -> None:
        value = copy.deepcopy(self.receipt)
        mutation(value)
        with self.assertRaises(validator.MachinePassReceiptError):
            validator.validate_structure(value)

    def test_committed_receipt_passes_read_only_validator(self) -> None:
        result = validator.validate_receipt()
        self.assertEqual(result["status"],
                         "PASS_FROZEN_TARGETED_MACHINE_PASS_V1")
        self.assertFalse(result["formal_pass"])

    def test_status_cannot_promote_beyond_targeted_machine_pass(self) -> None:
        self.assert_structure_rejects(
            lambda value: value.__setitem__("status", "FORMAL_PASS"))

    def test_full_six_claims_remain_false(self) -> None:
        self.assert_structure_rejects(lambda value: value["evidence_boundary"].update(
            {"full_six_scene_evidence": True}))

    def test_ta_product_golden_and_m2_claims_remain_false(self) -> None:
        for field in ("technical_art_review_performed", "product_review_performed",
                      "golden_promotion", "m2_promotion"):
            with self.subTest(field=field):
                self.assert_structure_rejects(
                    lambda value, key=field: value["evidence_boundary"].update(
                        {key: True}))

    def test_readiness_and_manifest_hashes_are_exact(self) -> None:
        for field in ("readiness_manifest_sha256", "manifest_sha256"):
            with self.subTest(field=field):
                self.assert_structure_rejects(
                    lambda value, key=field: value["frozen_bindings"].update(
                        {key: "0" * 64}))

    def test_consumed_marker_hash_is_exact(self) -> None:
        self.assert_structure_rejects(lambda value: value["authorization"].update(
            {"consumed_marker_sha256": "0" * 64}))

    def test_ppm_hashes_and_scene_order_are_exact(self) -> None:
        self.assert_structure_rejects(lambda value: value["scenes_in_order"][0].update(
            {"ppm_sha256": "0" * 64}))
        self.assert_structure_rejects(lambda value: value.update(
            {"scenes_in_order": list(reversed(value["scenes_in_order"]))}))

    def test_permission_and_inventory_drift_are_rejected(self) -> None:
        self.assert_structure_rejects(lambda value: value["frozen_bindings"].update(
            {"output_inventory_sha256": "0" * 64}))
        self.assert_structure_rejects(lambda value: value["permission_audit"].update(
            {"consumed_marker": {"gid": 20, "mode": "0644", "uid": 501}}))
        result = validator.validate_receipt()
        self.assertEqual(result["manifest_sha256"], validator.EXPECTED_MANIFEST_SHA256)

    def test_validator_has_no_process_gpu_or_write_path(self) -> None:
        source = inspect.getsource(validator)
        for forbidden in ("subprocess", "Popen", "OpenGL", "Cocoa", "write_text(",
                          "write_bytes(", "os.system"):
            with self.subTest(forbidden=forbidden):
                self.assertNotIn(forbidden, source)


if __name__ == "__main__":
    unittest.main()
