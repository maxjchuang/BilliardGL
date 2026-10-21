from __future__ import annotations

from pathlib import Path
import unittest

from scripts import (
    validate_phase5_rules_decision_ui_v5_pixel_visible_implementation
    as validator,
)


class Phase5V5PixelVisibleImplementationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.draw = validator.DRAW_LIST.read_text()
        cls.product = validator.PRODUCT_DRAW.read_text()
        cls.cpu = validator.CPU_RASTER.read_text()
        cls.cmake = validator.CMAKE.read_text()

    def reject(self, draw=None, product=None, cpu=None, cmake=None) -> None:
        with self.assertRaises(RuntimeError):
            validator.validate_sources(
                self.draw if draw is None else draw,
                self.product if product is None else product,
                self.cpu if cpu is None else cpu,
                self.cmake if cmake is None else cmake)

    def test_live_scoped_implementation_passes(self) -> None:
        result = validator.validate()
        self.assertEqual(
            result["status"],
            "PASS_PHASE5_V5_PIXEL_VISIBLE_IMPLEMENTATION_STATIC")
        self.assertEqual(result["production_file_count"], 2)
        self.assertFalse(result["gpu_authorized"])
        self.assertFalse(result["gpu_executed"])
        self.assertFalse(result["product_exposed"])

    def test_missing_focus_inset_rejected(self) -> None:
        self.reject(draw=self.draw.replace("confirm_copy_inset", "missing"))

    def test_text_role_overlap_guard_rejected(self) -> None:
        self.reject(draw=self.draw.replace(
            "intersects(confirmCopyItem->bounds, focus->bounds)",
            "false"))

    def test_accent_on_accent_rejected(self) -> None:
        self.reject(product=self.product.replace(
            "const UiColor roleForeground = cvd ? kPanelCvd : kPanelStandard;",
            "const UiColor roleForeground = cvd ? kAccentCvd : kAccent;", 1))

    def test_missing_connected_component_rejected(self) -> None:
        self.reject(cpu=self.cpu.replace(
            "componentCount(const Mask& mask)", "componentCountRemoved"))

    def test_missing_border_flood_fill_rejected(self) -> None:
        self.reject(cpu=self.cpu.replace(
            "enclosedRegionCount(const Mask& mask)",
            "enclosedRegionCountRemoved"))

    def test_contrast_relaxation_rejected(self) -> None:
        self.reject(cpu=self.cpu.replace(
            "minimumContrast + 0.001f >= 4.0f",
            "minimumContrast >= 1.0f"))

    def test_missing_text_mask_intersection_rejected(self) -> None:
        self.reject(cpu=self.cpu.replace(
            "intersectionCount(surface.focus, surface.text) == 0",
            "true"))

    def test_negative_fixture_count_relaxation_rejected(self) -> None:
        self.reject(cpu=self.cpu.replace("rejected == 12", "rejected >= 1"))

    def test_cmake_target_removal_rejected(self) -> None:
        self.reject(cmake=self.cmake.replace(
            "BilliardsPhase5RulesDecisionUiV5CpuRasterTests", "Removed"))


if __name__ == "__main__":
    unittest.main()
