import importlib.util
from pathlib import Path
import unittest

SPEC = importlib.util.spec_from_file_location("timing_summary",
    Path(__file__).resolve().parents[1] / "scripts/summarize_presented_frame_timing.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def frame(number, duration=16):
    details = dict(frame_id=str(number),
        timing_scope="event_loop_through_swap_completion", frame_timing_valid=True,
        includes_simulation=True, includes_display_swap=True, measures_scanout=False,
        loop_through_swap_ms=duration - 2, swap_ms=3)
    if number > 1:
        details["swap_to_swap_ms"] = duration
    return dict(kind="swap_submitted", details=details)


class TimingSummaryTest(unittest.TestCase):
    def test_percentiles_and_warmup(self):
        records = [frame(i, 100 if i < 3 else i + 10) for i in range(1, 103)]
        report = MODULE.summarize(list(reversed(records)), 2)
        self.assertEqual(report["sample_count"], 100)
        self.assertEqual(report["series"]["swap_to_swap"],
                         dict(p50_ms=62, p95_ms=107, p99_ms=111))
        self.assertFalse(report["qualifies_as_product_frame_budget"])

    def test_rotated_duplicate_and_empty_rejected(self):
        for records in ([], [frame(2)], [frame(1), frame(3)], [frame(1), frame(1)]):
            with self.assertRaises(ValueError):
                MODULE.summarize(records)

    def test_invalid_contract_and_times_rejected(self):
        for key, value in (("includes_display_swap", False), ("frame_timing_valid", False),
                           ("swap_to_swap_ms", float("nan")), ("swap_to_swap_ms", 1),
                           ("swap_ms", -1), ("swap_ms", True),
                           ("loop_through_swap_ms", float("inf"))):
            records = [frame(1), frame(2)]
            records[1]["details"][key] = value
            with self.assertRaises(ValueError):
                MODULE.summarize(records)

    def test_no_invented_first_cadence_or_samples(self):
        record = frame(1)
        record["details"]["swap_to_swap_ms"] = 0
        with self.assertRaises(ValueError):
            MODULE.summarize([record, frame(2)])
        for warmup in (2, -1):
            with self.assertRaises(ValueError):
                MODULE.summarize([frame(1), frame(2)], warmup)

    def test_frame_identity_cannot_be_coerced(self):
        for identity in (1.5, True, 1, "01", "+1", " 1", "1.0", "١", "0"):
            records = [frame(1), frame(2)]
            records[0]["details"]["frame_id"] = identity
            with self.assertRaises(ValueError):
                MODULE.summarize(records)
        with self.assertRaises(ValueError):
            MODULE.summarize([frame(1), frame(2)], True)

    def test_mixed_drawable_sizes_are_disclosed(self):
        records = [frame(1), frame(2), frame(3), frame(4)]
        for record, width in zip(records[:3], (1920, 1920, 1280)):
            record["details"].update(drawable_width_px=width, drawable_height_px=1080,
                                     render_quality_enum=1, requested_msaa_samples=4)
        report = MODULE.summarize(records)
        configs = report["render_configurations"]
        self.assertEqual([c["drawable_width_px"] for c in configs], [1920, 1280, None])
        self.assertEqual([c["sample_count"] for c in configs], [1, 1, 1])
        self.assertFalse(report["qualifies_as_product_frame_budget"])


if __name__ == "__main__":
    unittest.main()
