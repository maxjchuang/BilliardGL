from __future__ import annotations

import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts.validate_phase4_v11_ordinal4_independent_ta_addendum import (  # noqa: E402
    AXES,
    AXIS_RESULTS,
    BINDINGS,
    DEFAULT_ADDENDUM,
    FALSE_BOUNDARY_FIELDS,
    load_json,
    ppm_dimensions,
    validate,
    validate_bindings,
    validate_boundary,
    validate_broadcast_failure,
    validate_dispatch,
    validate_telemetry,
    validate_visual_decisions,
)


class Phase4V11Ordinal4IndependentTaAddendumTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.addendum = load_json(DEFAULT_ADDENDUM)

    def test_committed_addendum_passes_as_failure_receipt(self) -> None:
        result = validate(DEFAULT_ADDENDUM)
        self.assertEqual(result["status"],
                         "PASS_PHASE4_V11_ORDINAL4_TA_FAILURE_ADDENDUM")
        self.assertEqual(result["reviewed_ppm_count"], 4)
        self.assertEqual(result["control_machine_pass_count"], 3)
        self.assertEqual(result["broadcast_machine_status"], "FAILED")
        self.assertEqual(result["broadcast_actual_visible_over_source"], [6, 12])
        self.assertEqual(result["visual_axes_pass"], 2)
        self.assertEqual(result["visual_axes_fail"], 5)
        self.assertFalse(result["targeted11_machine_pass"])
        self.assertFalse(result["technical_art_acceptance"])
        self.assertFalse(result["formal68_acceptance"])

    def test_json_round_trip_preserves_addendum(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "addendum.json"
            path.write_text(json.dumps(self.addendum, indent=2) + "\n")
            self.assertEqual(json.loads(path.read_text()), self.addendum)

    def test_any_bound_sha_drift_fails_closed(self) -> None:
        for index in range(len(BINDINGS)):
            addendum = copy.deepcopy(self.addendum)
            addendum["read_only_bindings"][index]["sha256"] = "0" * 64
            with self.assertRaises(RuntimeError):
                validate_bindings(addendum)

    def test_all_four_ppms_decode_at_original_dimensions(self) -> None:
        for path, _ in BINDINGS[6:10]:
            self.assertEqual(ppm_dimensions(ROOT / path), (1920, 1080))

    def test_dispatch_cannot_be_relabelled_complete(self) -> None:
        addendum = copy.deepcopy(self.addendum)
        addendum["authoritative_dispatch_result"]["completed_count"] = 11
        with self.assertRaises(RuntimeError):
            validate_dispatch(addendum)

    def test_broadcast_machine_failure_cannot_be_relabelled_pass(self) -> None:
        addendum = copy.deepcopy(self.addendum)
        addendum["ordinal_comparison"][3]["machine_status"] = "PASS"
        with self.assertRaises(RuntimeError):
            validate_telemetry(addendum)
        addendum = copy.deepcopy(self.addendum)
        addendum["ordinal_comparison"][3]["actual_source_visible_hidden_coverage"] = [12, 12, 0, 1.0]
        with self.assertRaises(RuntimeError):
            validate_telemetry(addendum)

    def test_all_actual_unique_area_failures_are_required(self) -> None:
        addendum = copy.deepcopy(self.addendum)
        addendum["broadcast_failure_evidence"]["actual_unique_area_failures"] = \
            addendum["broadcast_failure_evidence"]["actual_unique_area_failures"][:-1]
        with self.assertRaises(RuntimeError):
            validate_broadcast_failure(addendum)

    def test_below_four_contrast_failures_cannot_be_hidden(self) -> None:
        addendum = copy.deepcopy(self.addendum)
        addendum["broadcast_failure_evidence"]["predicted_local_contrast_failures"][0]["contrast_ratio"] = 4.0
        with self.assertRaises(RuntimeError):
            validate_broadcast_failure(addendum)

    def test_exact_seven_axes_keep_five_failures(self) -> None:
        axes = self.addendum["canonical_axis_decisions"]
        self.assertEqual([item["axis_id"] for item in axes], AXES)
        self.assertEqual([item["result"] for item in axes], AXIS_RESULTS)
        for index, result in enumerate(AXIS_RESULTS):
            if result == "FAIL":
                addendum = copy.deepcopy(self.addendum)
                addendum["canonical_axis_decisions"][index]["result"] = "PASS"
                with self.assertRaises(RuntimeError):
                    validate_visual_decisions(addendum)

    def test_endpoint_ticks_do_not_upgrade_actual_track(self) -> None:
        self.assertEqual(
            self.addendum["focused_visual_decisions"][
                "broadcast_endpoint_and_tick_readability"], "PASS")
        self.assertEqual(
            self.addendum["focused_visual_decisions"][
                "broadcast_actual_solid_core_unique_area"], "FAIL")
        addendum = copy.deepcopy(self.addendum)
        addendum["focused_visual_decisions"][
            "ordinal4_independent_visual_result"] = "PASS"
        with self.assertRaises(RuntimeError):
            validate_visual_decisions(addendum)

    def test_cross_track_integrity_does_not_upgrade_same_axis_failure(self) -> None:
        focused = self.addendum["focused_visual_decisions"]
        self.assertEqual(focused["broadcast_cross_track_ownership_integrity"],
                         "PASS_CONTROL_ONLY")
        self.assertEqual(focused["broadcast_same_axis_independent_traceability"],
                         "FAIL")
        addendum = copy.deepcopy(self.addendum)
        addendum["broadcast_failure_evidence"]["ownership_integrity_controls"][
            "foreign_track_evidence_count"] = 1
        with self.assertRaises(RuntimeError):
            validate_broadcast_failure(addendum)

    def test_machine_ta_formal_and_mutation_boundaries_remain_false(self) -> None:
        for field in FALSE_BOUNDARY_FIELDS:
            addendum = copy.deepcopy(self.addendum)
            addendum["proof_boundary"][field] = True
            with self.assertRaises(RuntimeError):
                validate_boundary(addendum)


if __name__ == "__main__":
    unittest.main()
