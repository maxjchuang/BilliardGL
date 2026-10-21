import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import (
    validate_phase5_rules_decision_ui_v4_exact16_machine_pass_freeze as freeze,
)


class Phase5RulesUiV4Exact16MachinePassFreezeTests(unittest.TestCase):
    def setUp(self) -> None:
        self.value = json.loads(
            freeze.DEFAULT_RECEIPT.read_text(encoding="utf-8"))

    def validate_mutation(self, mutate):
        value = copy.deepcopy(self.value)
        mutate(value)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "receipt.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            return freeze.validate_receipt(path, enforce_frozen_sha=False)

    def test_frozen_machine16_pass_pending_ta(self) -> None:
        result = freeze.validate_receipt()
        self.assertEqual(
            result["status"], "PASS_FROZEN_PHASE5_V4_EXACT16_MACHINE_16")
        self.assertEqual(result["machine_pass_receipt_sha256"],
                         freeze.EXPECTED_RECEIPT_SHA256)
        self.assertEqual(result["stable_png_manifest_sha256"],
                         freeze.EXPECTED_PNG_MANIFEST_SHA256)
        self.assertEqual(result["case_count"], 16)
        self.assertEqual(result["ppm_count"], 16)
        self.assertEqual(result["png_count"], 16)
        self.assertEqual(result["byte_faithful_png_count"], 16)
        self.assertEqual(result["required_independent_ta_axis_decisions"], 160)
        self.assertEqual(result["technical_art_review_completed"], 0)
        self.assertFalse(result["product_exposed"])
        self.assertFalse(result["golden_promotion"])
        self.assertFalse(result["m2_promotion"])

    def test_mutated_receipt_sha_rejects_first(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "receipt.json"
            path.write_text(json.dumps(self.value), encoding="utf-8")
            with self.assertRaisesRegex(
                    freeze.MachinePassFreezeError, "receipt SHA"):
                freeze.validate_receipt(path)

    def test_bad_readiness_binding_rejected(self) -> None:
        with self.assertRaisesRegex(
                freeze.MachinePassFreezeError, "readiness"):
            self.validate_mutation(
                lambda value: value["lineage"].update(
                    readiness_manifest_sha256="0" * 64))

    def test_bad_authorization_binding_rejected(self) -> None:
        with self.assertRaisesRegex(
                freeze.MachinePassFreezeError, "authorization"):
            self.validate_mutation(
                lambda value: value["lineage"].update(
                    authorization_sha256="0" * 64))

    def test_bad_consumed_marker_binding_rejected(self) -> None:
        with self.assertRaisesRegex(
                freeze.MachinePassFreezeError, "consumed marker"):
            self.validate_mutation(
                lambda value: value["lineage"].update(
                    consumed_marker_sha256="0" * 64))

    def test_bad_runner_claim_binding_rejected(self) -> None:
        with self.assertRaisesRegex(
                freeze.MachinePassFreezeError, "runner claim"):
            self.validate_mutation(
                lambda value: value["lineage"].update(
                    runner_claim_sha256="0" * 64))

    def test_bad_results_binding_rejected(self) -> None:
        with self.assertRaisesRegex(
                freeze.MachinePassFreezeError, "results/telemetry/dispatch"):
            self.validate_mutation(
                lambda value: value["lineage"].update(
                    results_jsonl_sha256="0" * 64))

    def test_bad_png_manifest_binding_rejected(self) -> None:
        with self.assertRaisesRegex(
                freeze.MachinePassFreezeError, "stable PNG manifest"):
            self.validate_mutation(
                lambda value: value["lineage"].update(
                    stable_png_manifest_sha256="0" * 64))

    def test_bad_counts_rejected(self) -> None:
        with self.assertRaisesRegex(
                freeze.MachinePassFreezeError, "verified machine counts"):
            self.validate_mutation(
                lambda value: value["verified_counts"].update(
                    actual_gpu_completed=15, unrun_cases=1))

    def test_bad_dimensions_rejected(self) -> None:
        with self.assertRaisesRegex(
                freeze.MachinePassFreezeError, "dimension counts"):
            self.validate_mutation(
                lambda value: value["decoded_physical_dimension_counts"].update(
                    {"1280x720": 7}))

    def test_bad_stable_aggregate_rejected(self) -> None:
        with self.assertRaisesRegex(
                freeze.MachinePassFreezeError, "stable evidence aggregates"):
            self.validate_mutation(
                lambda value: value["stable_aggregates"].update(
                    case_evidence_aggregate_sha256="0" * 64))

    def test_ta_or_promotion_claim_rejected(self) -> None:
        with self.assertRaisesRegex(
                freeze.MachinePassFreezeError, "machine-only claims"):
            self.validate_mutation(
                lambda value: value["claims"].update(
                    technical_art_review_completed=16,
                    golden_promotion=True))

    def test_ta_16x10_gate_cannot_be_precompleted(self) -> None:
        with self.assertRaisesRegex(
                freeze.MachinePassFreezeError, "independent TA 16x10"):
            self.validate_mutation(
                lambda value: value["independent_ta_review_gate"].update(
                    completed_original_image_axis_decisions=160))

    def test_bad_ppm_and_png_inputs_fail_closed(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            ppm = root / "bad.ppm"
            png = root / "bad.png"
            ppm.write_bytes(b"P3\n1 1\n255\n0 0 0\n")
            png.write_bytes(b"not-a-png")
            with self.assertRaises(freeze.MachinePassFreezeError):
                freeze.read_ppm(ppm)
            with self.assertRaises(freeze.MachinePassFreezeError):
                freeze.decode_png(png)

    def test_corrupt_png_crc_fails_closed(self) -> None:
        manifest = json.loads(
            (freeze.DEFAULT_RECEIPT.parent / "stable-png-manifest.json")
            .read_text())
        source = freeze.ROOT / manifest["cases"][0]["png_path"]
        data = bytearray(source.read_bytes())
        data[-5] ^= 1
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "corrupt.png"
            path.write_bytes(data)
            with self.assertRaisesRegex(
                    freeze.MachinePassFreezeError, "PNG CRC"):
                freeze.decode_png(path)

    def test_validator_is_cpu_only_and_read_only(self) -> None:
        source = Path(freeze.__file__).read_text(encoding="utf-8")
        for forbidden in ("import subprocess", "subprocess.run", "Popen(",
                          "Cocoa", "OpenGL", "screenshot(", "chmod(",
                          "write_bytes(", "write_text(", "open(\"w"):
            self.assertNotIn(forbidden, source)


if __name__ == "__main__":
    unittest.main()
