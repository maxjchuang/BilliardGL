#!/usr/bin/env python3
"""Static, no-GPU conformance gate for the scoped Phase5 v5 repair."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts import run_phase5_rules_decision_ui_v3_visual_repair_machine as v3

GATE = ROOT / (
    "docs/art/phase5-rules-decision-ui-v5-pixel-visible-shape-"
    "repair-design-gate-v1.json")
GATE_SHA = "657f3ab6f4ccac4db23fb0ab72889bc49d091f27fd5f63bd5610b736b1aa2a10"
DRAW_LIST = ROOT / "src/Billiards/render/ui/rules_decision_ui_draw_list.cpp"
PRODUCT_DRAW = ROOT / "src/Billiards/render/ui/rules_decision_product_draw.cpp"
CPU_RASTER = ROOT / "tests/phase5_rules_decision_ui_v5_cpu_raster_tests.cpp"
CMAKE = ROOT / "CMakeLists.txt"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def validate_sources(draw_list: str, product_draw: str,
                     cpu_raster: str, cmake: str) -> dict[str, Any]:
    require("confirm_copy_inset" in draw_list and
            "confirmLeadingInset" in draw_list and
            "confirmTrailingInset" in draw_list and
            '{"confirm", RulesDecisionDrawKind::Confirm' in draw_list and
            '"", fixture.armedActionId, actionFont' in draw_list and
            "intersects(confirmCopyItem->bounds, focus->bounds)" in draw_list and
            "intersects(confirmCopyItem->bounds, confirmShape->bounds)" in draw_list,
            "dedicated leading focus slot/copy inset contract")
    require(product_draw.count(
            "const UiColor roleForeground = cvd ? kPanelCvd : kPanelStandard;") == 3,
            "three role families require independent dark foreground")
    for marker in ("case RulesDecisionDrawKind::FocusChevron:",
                   "case RulesDecisionDrawKind::ArmedDiamond:",
                   "case RulesDecisionDrawKind::ConfirmDoubleFrameEnterArrow:"):
        require(marker in product_draw, "missing role emission: " + marker)
    require(all(token in product_draw for token in (
        "kRulesUiFocusChevronV4Token",
        "kRulesUiSelectedCheckDiamondV4Token",
        "kRulesUiConfirmDoubleFrameEnterArrowV4Token")),
        "three immutable token identities")
    for required in (
        "Surface raster(const ProductUiFrame& frame",
        "componentCount(const Mask& mask)",
        "enclosedRegionCount(const Mask& mask)",
        "intersectionCount(surface.focus, surface.text) == 0",
        "minimumContrast + 0.001f >= 4.0f",
        "p1", "p2", "p3", "p4",
        "color_vision_safe", "transform(const Pixel& pixel",
        "verifyNegativeFixtures", "rejected == 12",
        "snapshot_case_count", "snapshot_aggregate_sha256",
        "actual_gpu_invoked", "authorization_issued",
    ):
        require(required in cpu_raster,
                "CPU final-raster implementation missing: " + required)
    require("BilliardsPhase5RulesDecisionUiV5CpuRasterTests" in cmake and
            "cpu-raster;pixel-topology;phase5" in cmake,
            "v5 CPU raster CMake target")
    return {
        "status": "PASS_PHASE5_V5_PIXEL_VISIBLE_IMPLEMENTATION_STATIC",
        "production_file_count": 2,
        "cpu_raster_profiles": 4,
        "palette_count": 2,
        "simulations_per_palette": 4,
        "negative_fixture_count": 12,
        "gpu_authorized": False,
        "gpu_executed": False,
        "product_exposed": False,
    }


def validate() -> dict[str, Any]:
    require(sha(GATE) == GATE_SHA, "v5 design gate SHA drift")
    gate = json.loads(GATE.read_text(encoding="utf-8"))
    for binding in gate["read_only_bindings"]:
        require(sha(ROOT / binding["path"]) == binding["sha256"],
                "v5 immutable evidence drift: " + binding["path"])
    for relative, digest in v3.FORBIDDEN_BINDINGS.items():
        require(sha(ROOT / relative) == digest,
                "forbidden authority/rules drift: " + relative)
    result = validate_sources(
        DRAW_LIST.read_text(encoding="utf-8"),
        PRODUCT_DRAW.read_text(encoding="utf-8"),
        CPU_RASTER.read_text(encoding="utf-8"),
        CMAKE.read_text(encoding="utf-8"))
    result["gate_sha256"] = GATE_SHA
    result["source_sha256"] = {
        str(path.relative_to(ROOT)): sha(path)
        for path in (DRAW_LIST, PRODUCT_DRAW, CPU_RASTER, CMAKE)
    }
    return result


if __name__ == "__main__":
    try:
        print(json.dumps(validate(), sort_keys=True))
    except (OSError, ValueError, KeyError, json.JSONDecodeError,
            RuntimeError) as error:
        print("FAIL: " + str(error))
        raise SystemExit(2)
