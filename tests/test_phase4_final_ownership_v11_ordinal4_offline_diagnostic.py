#!/usr/bin/env python3
"""Fail-closed mutation tests for frozen Phase4 v11 ordinal-4 diagnosis."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
VALIDATOR = ROOT / "scripts/validate_phase4_final_ownership_v11_ordinal4_offline_diagnostic.py"
DIAGNOSTIC = ROOT / "scripts/diagnose_phase4_final_ownership_v11_ordinal4_offline.py"
RECEIPT = ROOT / "docs/art/phase4-final-ownership-v11-ordinal4-offline-diagnostic-v1/failure-evidence-receipt-v1.json"
GATE = ROOT / "docs/art/phase4-final-ownership-v11-ordinal4-offline-diagnostic-v1/minimal-raster-support-connectivity-correction-design-gate-v1.json"


class Phase4V11Ordinal4OfflineDiagnosticTests(unittest.TestCase):
    def run_validator(self, receipt: Path = RECEIPT,
                      gate: Path = GATE) -> subprocess.CompletedProcess:
        return subprocess.run([
            "python3", str(VALIDATOR), "--receipt", str(receipt),
            "--gate", str(gate)], cwd=ROOT, text=True,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)

    def mutate(self, receipt_mutation=None, gate_mutation=None,
               expected_error: str = "") -> None:
        with tempfile.TemporaryDirectory(prefix="phase4-v11-ordinal4-") as temp:
            root = Path(temp)
            receipt = copy.deepcopy(json.loads(RECEIPT.read_text()))
            gate = copy.deepcopy(json.loads(GATE.read_text()))
            if receipt_mutation:
                receipt_mutation(receipt)
            if gate_mutation:
                gate_mutation(gate)
            receipt_path = root / "receipt.json"
            gate_path = root / "gate.json"
            receipt_path.write_text(json.dumps(receipt, indent=2, sort_keys=True))
            gate_path.write_text(json.dumps(gate, indent=2, sort_keys=True))
            result = self.run_validator(receipt_path, gate_path)
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertIn(expected_error, result.stdout)

    def test_canonical_passes(self) -> None:
        result = self.run_validator()
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn('"status": "PASS"', result.stdout)

    def test_analyzer_recomputes_exact_counterfactual(self) -> None:
        result = subprocess.run(["python3", str(DIAGNOSTIC)], cwd=ROOT,
            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            check=False)
        self.assertEqual(result.returncode, 0, result.stdout)
        value = json.loads(result.stdout)
        self.assertEqual(value["ordinal4_counterfactual"][
            "aggregate_source_visible_hidden"], [81, 78, 3])
        self.assertEqual(value["ordinal4_counterfactual"][
            "actual_source_visible_hidden"], [12, 11, 1])

    def test_receipt_cannot_authorize_retry(self) -> None:
        self.mutate(lambda value: value.update({
            "capture_retry_authorized": True}), expected_error=
            "receipt_status_or_authorization_invalid")

    def test_receipt_cannot_claim_gpu(self) -> None:
        self.mutate(lambda value: value.update({
            "gpu_cocoa_opengl_invoked": True}), expected_error=
            "receipt_status_or_authorization_invalid")

    def test_frozen_binding_mutation_fails(self) -> None:
        self.mutate(lambda value: value["frozen_evidence"].update({
            "telemetry_sha256": "0" * 64}), expected_error=
            "receipt_frozen_bindings_invalid")

    def test_root_cause_cannot_be_relabelled_visual(self) -> None:
        self.mutate(lambda value: value["root_cause"].update({
            "primary": "VISUAL_COLOR_STYLE"}), expected_error=
            "root_cause_isolation_invalid")

    def test_counterfactual_cannot_claim_machine_pass(self) -> None:
        self.mutate(lambda value: value["frozen_counterfactual"].update({
            "status": "MACHINE_PASS", "fresh_capture_still_required": False}),
            expected_error="receipt_counterfactual_invalid")

    def test_counterfactual_counts_are_pinned(self) -> None:
        self.mutate(lambda value: value["frozen_counterfactual"].update({
            "actual_source_visible_hidden": [12, 12, 0]}), expected_error=
            "receipt_counterfactual_invalid")

    def test_gate_cannot_lower_area_threshold(self) -> None:
        self.mutate(gate_mutation=lambda value: value[
            "required_algorithm"].update({
                "minimum_resolved_pixel_equivalent_area_remains": 1.0}),
            expected_error="minimal_design_gate_invalid")

    def test_gate_cannot_remove_diagonal_raster_semantics(self) -> None:
        self.mutate(gate_mutation=lambda value: value[
            "required_algorithm"].update({
                "raster_continuity_uses_deterministic_eight_neighbor_physical_pixel_adjacency":
                    False}), expected_error="minimal_design_gate_invalid")

    def test_gate_cannot_authorize_capture(self) -> None:
        self.mutate(gate_mutation=lambda value: value.update({
            "gpu_cocoa_opengl_authorized": True,
            "capture_retry_authorized": True}), expected_error=
            "minimal_design_gate_invalid")

    def test_gate_cannot_drop_forbidden_change_inventory(self) -> None:
        self.mutate(gate_mutation=lambda value: value[
            "forbidden_changes"].pop(), expected_error=
            "minimal_design_gate_invalid")

    def test_gate_cannot_promote_frozen_counterfactual(self) -> None:
        self.mutate(gate_mutation=lambda value: value[
            "frozen_counterfactual_acceptance"].update({
                "cannot_satisfy_machine_pass": False}), expected_error=
            "minimal_design_gate_invalid")


if __name__ == "__main__":
    unittest.main()
