from __future__ import annotations

import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts.validate_phase4_v10_ordinal1_independent_ta_addendum import (  # noqa: E402
    AXES,
    BINDINGS,
    DEFAULT_ADDENDUM,
    EXPECTED_ROI,
    RESULTS,
    load_json,
    roi_metrics,
    validate,
    validate_authoritative_v10,
    validate_bindings,
    validate_boundary,
    validate_comparison,
    validate_visual_decisions,
)


class Phase4V10Ordinal1IndependentTaAddendumTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.addendum = load_json(DEFAULT_ADDENDUM)

    def test_committed_addendum_passes_as_failure_receipt(self) -> None:
        result = validate(DEFAULT_ADDENDUM)
        self.assertEqual(
            result["status"],
            "PASS_PHASE4_V10_ORDINAL1_TA_FAILURE_ADDENDUM")
        self.assertEqual(result["visual_axes_pass"], 6)
        self.assertEqual(result["visual_axes_fail"], 1)
        self.assertEqual(result["machine_status"], "FAILED")
        self.assertEqual(result["hidden_outer_halo_probes"], 6)
        self.assertFalse(result["capture_acceptance"])
        self.assertFalse(result["technical_art_acceptance"])
        self.assertFalse(result["production_modified"])
        self.assertFalse(result["gpu_executed_by_addendum"])

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

    def test_original_ppm_roi_metrics_are_recomputed(self) -> None:
        indices = {8: 8, 9: 6, 10: 0}
        for version, index in indices.items():
            self.assertEqual(
                roi_metrics(ROOT / BINDINGS[index][0]), EXPECTED_ROI[version])

    def test_machine_failure_cannot_be_relabelled_pass(self) -> None:
        addendum = copy.deepcopy(self.addendum)
        addendum["authoritative_v10_result"]["machine_status"] = "PASS"
        with self.assertRaises(RuntimeError):
            validate_authoritative_v10(addendum)
        addendum = copy.deepcopy(self.addendum)
        addendum["authoritative_v10_result"]["hidden_probe_count"] = 0
        with self.assertRaises(RuntimeError):
            validate_authoritative_v10(addendum)

    def test_all_six_local_contrast_failures_are_required(self) -> None:
        addendum = copy.deepcopy(self.addendum)
        addendum["failed_local_contrast_evidence"] = (
            addendum["failed_local_contrast_evidence"][:-1])
        with self.assertRaises(RuntimeError):
            validate_authoritative_v10(addendum)
        addendum = copy.deepcopy(self.addendum)
        addendum["failed_local_contrast_evidence"][0][
            "contrast_ratio"] = 4.0
        with self.assertRaises(RuntimeError):
            validate_authoritative_v10(addendum)

    def test_v8_v9_v10_comparison_cannot_hide_remaining_three_color_rows(self) -> None:
        addendum = copy.deepcopy(self.addendum)
        addendum["v8_v9_v10_comparison"]["v10"][
            "triple_color_rows"] = 0
        with self.assertRaises(RuntimeError):
            validate_comparison(addendum)
        addendum = copy.deepcopy(self.addendum)
        addendum["v8_v9_v10_comparison"]["interpretation"] = [
            "all three-color pixels eliminated"]
        with self.assertRaises(RuntimeError):
            validate_comparison(addendum)

    def test_exact_seven_axes_keep_outer_silhouette_fail(self) -> None:
        axes = self.addendum["canonical_axis_decisions"]
        self.assertEqual([row["axis_id"] for row in axes], AXES)
        self.assertEqual([row["result"] for row in axes], RESULTS)
        addendum = copy.deepcopy(self.addendum)
        addendum["canonical_axis_decisions"][3]["result"] = "PASS"
        with self.assertRaises(RuntimeError):
            validate_visual_decisions(addendum)

    def test_style_improvement_does_not_equal_formal_acceptance(self) -> None:
        focused = self.addendum["focused_visual_decisions"]
        self.assertEqual(
            focused["adaptive_narrow_rim_removed_continuous_three_line_artifact"],
            "PASS")
        self.assertEqual(focused["primary_track_color_dominance"], "PASS")
        self.assertEqual(focused["local_contrast_and_readability"], "FAIL")
        self.assertEqual(focused["formal_ordinal1_ta_result"], "FAIL")
        addendum = copy.deepcopy(self.addendum)
        addendum["focused_visual_decisions"]["formal_ordinal1_ta_result"] = (
            "PASS")
        with self.assertRaises(RuntimeError):
            validate_visual_decisions(addendum)

    def test_capture_and_ta_acceptance_must_remain_false(self) -> None:
        for field in (
                "capture_acceptance", "technical_art_review_authorized",
                "technical_art_acceptance", "production_source_modified",
                "gpu_authorized_or_executed_by_this_addendum",
                "threshold_changed", "formal68_authorized", "product_exposed",
                "golden_promotion", "m3_promotion", "beta_pass"):
            addendum = copy.deepcopy(self.addendum)
            addendum["proof_boundary"][field] = True
            with self.assertRaises(RuntimeError):
                validate_boundary(addendum)


if __name__ == "__main__":
    unittest.main()
