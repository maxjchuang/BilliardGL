#!/usr/bin/env python3
"""CPU/static validation for approved Phase4 v11 topology-only correction."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "docs/art/phase4-final-ownership-v11-ordinal4-offline-diagnostic-v1/failure-evidence-receipt-v1.json"
GATE = ROOT / "docs/art/phase4-final-ownership-v11-ordinal4-offline-diagnostic-v1/minimal-raster-support-connectivity-correction-design-gate-v1.json"
ANALYZER = ROOT / "scripts/diagnose_phase4_final_ownership_v11_ordinal4_offline.py"
SUPERSEDED_DIAGNOSTIC_VALIDATOR = ROOT / \
    "scripts/validate_phase4_final_ownership_v11_ordinal4_offline_diagnostic.py"
EVIDENCE_SHA256 = "07f251d466b0dcad550b4951e8242403bcaaaeb97dd800cbde99cdf23c2f3f85"
GATE_SHA256 = "59bbabf437e9195392b2a41f4e48eea0513afb222706115a07e9c0965cbad059"
ANALYZER_SHA256 = "d406151148e7120363699e4f50760e0341bd7d69038d657c0d7b3fead8610ce6"

SOURCE_SHA256 = {
    "src/Billiards/training/final_semantic_ownership.cpp":
        "c4c91683b2de8bfdd6a4d256c6cd105023a4a9bd4113467a1bba012d3fe9aeba",
    "src/Billiards/training/final_semantic_ownership.h":
        "3f21ce72ee1946182d431588464831d364003e91fbf02e3ce2b7852ca4266057",
    "src/Billiards/training/trajectory_compound_visibility.cpp":
        "4b3defeed60bf931b8c7f19e8a0da73c82ab983830af23792a8ef1e6a29b2921",
    "src/Billiards/training/trajectory_compound_visibility.h":
        "4babc8a79e9b69978153dda1230de6855cfaa2f58b3a38d7e3c6ae90330e5890",
    "src/Billiards/training/phase4_final_ownership_instrumentation.cpp":
        "378d5373cb215634bea4a6a62e6c2c72d85b8a2cfd04c1055fae2896c4d0898b",
    "src/Billiards/training/phase4_final_ownership_instrumentation.h":
        "d75d5bebf95eea48ae6bf97b94f97976101b1fbac8a96d24fd9abf1d11beac61",
    "tests/phase4_final_semantic_ownership_tests.cpp":
        "491c49da1966a9f17a745ed8d23fab6e16874a2a3f4867eba4da1608034a258d",
    "tests/phase4_final_ownership_telemetry_tests.cpp":
        "1ab48db563547f6d44819bc1b8e9330b5762ca55505bbf1dfae4e90b1fdad903",
}
IMMUTABLE_VISUAL_SHA256 = {
    "src/Billiards/render/modern/modern_renderer.cpp":
        "b00cc6b370122275a42f106967f935dbb34bd5d3f81f2a750ca9ba6e503af191",
    "src/Billiards/training/trajectory_overlay_renderer.cpp":
        "09852cd4ad0a9c8aebeeac5b026d84f2d3c24624deddf7aab46330cfb1afd09e",
    "src/Billiards/training/trajectory_overlay_renderer.h":
        "19e8c1b5cae26384c55b8678289d5cf2c55bb1901425a6294211de6a57010ab6",
    "scripts/generate_phase4_overlay_v4_gpu_runner.py":
        "bd084842a10065e6f20b5826c8d38407230be2e82729fe29769dd13b4004c5a1",
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def frozen_summary() -> dict:
    specification = importlib.util.spec_from_file_location(
        "phase4_v11_frozen", ANALYZER)
    if specification is None or specification.loader is None:
        raise RuntimeError("frozen_analyzer_unloadable")
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    return module.summarize()


def superseded_diagnostic_errors() -> list[str]:
    specification = importlib.util.spec_from_file_location(
        "phase4_v11_superseded_diagnostic", SUPERSEDED_DIAGNOSTIC_VALIDATOR)
    if specification is None or specification.loader is None:
        raise RuntimeError("superseded_diagnostic_validator_unloadable")
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    return module.validate(module.DEFAULT_RECEIPT, module.DEFAULT_GATE)


def validate(source_root: Path) -> list[str]:
    errors: list[str] = []
    for path, expected, label in (
            (EVIDENCE, EVIDENCE_SHA256, "evidence"),
            (GATE, GATE_SHA256, "gate"),
            (ANALYZER, ANALYZER_SHA256, "analyzer")):
        if not path.is_file() or sha(path) != expected:
            errors.append(f"approved_{label}_binding_drift")
    for relative, expected in {**SOURCE_SHA256,
                               **IMMUTABLE_VISUAL_SHA256}.items():
        path = source_root / relative
        if not path.is_file():
            errors.append(f"source_missing:{relative}")
        elif sha(path) != expected:
            errors.append(f"source_drift:{relative}")
    if errors:
        return errors
    resolver = (source_root /
        "src/Billiards/training/final_semantic_ownership.cpp").read_text()
    header = (source_root /
        "src/Billiards/training/final_semantic_ownership.h").read_text()
    telemetry = (source_root /
        "src/Billiards/training/phase4_final_ownership_instrumentation.cpp").read_text()
    telemetry_header = (source_root /
        "src/Billiards/training/phase4_final_ownership_instrumentation.h").read_text()
    visibility = (source_root /
        "src/Billiards/training/trajectory_compound_visibility.cpp").read_text()
    visibility_header = (source_root /
        "src/Billiards/training/trajectory_compound_visibility.h").read_text()
    fixtures = (source_root /
        "tests/phase4_final_semantic_ownership_tests.cpp").read_text()

    required_tokens = {
        "pre_arbitration_support": "preArbitrationSupportPixels",
        "deterministic_eight_neighbor": "largestEightConnectedComponent",
        "same_track_filter": "sameTrack(probe.source.track, owner.track)",
        "allowed_component_filter": "allowedComponent(probe.source.track,",
        "normal_bound": "std::fabs(normal) > normalBound(probe.source)",
        "tangent_bound": "std::fabs(tangent) > tangentBound(probe.source)",
        "exclusive_area": "telemetry.resolvedPixelEquivalentArea <",
        "topology": "telemetry.preArbitrationLargestEightConnectedPixelCount <",
        "exclusive_reason": '"insufficient_exclusive_owned_area"',
        "continuity_reason": '"insufficient_raster_continuity"',
    }
    for label, token in required_tokens.items():
        if token not in resolver:
            errors.append(f"algorithm_contract_missing:{label}")
    area_position = resolver.find("telemetry.resolvedPixelEquivalentArea <")
    topology_position = resolver.find(
        "telemetry.preArbitrationLargestEightConnectedPixelCount <")
    contrast_position = resolver.find("telemetry.edgeContrastRatio <")
    support_position = resolver.find("preArbitrationSupportPixels")
    arbitration_position = resolver.find(
        "std::vector<std::vector<SampleClaim>> won")
    if not (0 <= support_position < arbitration_position < area_position <
            topology_position < contrast_position):
        errors.append("algorithm_stage_order_invalid")
    if "double minimumResolvedPixelEquivalentArea = 1.5;" not in header or \
            "std::size_t minimumRasterConnectedPhysicalPixels = 2;" not in header or \
            "double minimumVisibleCoverage = 0.90;" not in header or \
            "double minimumEdgeContrastRatio = 4.0;" not in header:
        errors.append("immutable_threshold_contract_changed")
    for token in ("InsufficientExclusiveOwnedArea",
                  "InsufficientRasterContinuity"):
        if token not in visibility_header:
            errors.append(f"reason_enum_missing:{token}")
    for token in ('return "insufficient_exclusive_owned_area";',
                  'return "insufficient_raster_continuity";'):
        if token not in visibility:
            errors.append(f"reason_name_missing:{token}")
    for token in ("exclusiveOwnedPixelCount",
                  "exclusiveLargestFourConnectedPixelCount",
                  "preArbitrationSupportPixelCount",
                  "preArbitrationLargestEightConnectedPixelCount"):
        if token not in telemetry_header:
            errors.append(f"topology_telemetry_type_missing:{token}")
    for token in ('value["exclusive_owned_pixel_count"]',
                  'value["exclusive_largest_four_connected_pixel_count"]',
                  'value["pre_arbitration_support_pixel_count"]',
                  'value["pre_arbitration_largest_eight_connected_pixel_count"]'):
        if token not in telemetry:
            errors.append(f"topology_telemetry_serialization_missing:{token}")
    fixture_ids = [
        "TOPO_P1_DIAGONAL_TWO_PIXEL_SUPPORT",
        "TOPO_P2_ORTHOGONAL_TWO_PIXEL_SUPPORT",
        "TOPO_N1_ISOLATED_SCATTER_REJECTED",
        "TOPO_N2_EXCLUSIVE_AREA_STILL_REQUIRED",
        "TOPO_N3_INELIGIBLE_SUPPORT_EXCLUDED",
        "TOPO_N4_ITERATION_ORDER_INVARIANT",
        "TOPO_N5_EXCLUSIVE_AREA_UNCHANGED",
        "TOPO_N6_REASON_SPLIT",
    ]
    if any(identifier not in fixtures for identifier in fixture_ids) or \
            "passedFixtures == 34" not in fixtures:
        errors.append("required_cpu_fixture_inventory_missing")
    try:
        summary = frozen_summary()
        counterfactual = summary["ordinal4_counterfactual"]
        if counterfactual["aggregate_source_visible_hidden"] != [81, 78, 3] or \
                counterfactual["predicted_source_visible_hidden"] != [69, 67, 2] or \
                counterfactual["actual_source_visible_hidden"] != [12, 11, 1] or \
                counterfactual["analytic_non_final_not_a_machine_pass"] is not True or \
                summary["capture_retry_authorized"] is not False:
            errors.append("frozen_v11_analytic_replay_invalid")
    except Exception as error:
        errors.append(f"frozen_v11_analytic_replay_failed:{error}")
    try:
        expected_drift = [
            "production_or_runner_source_drift:src/Billiards/training/final_semantic_ownership.cpp",
            "production_or_runner_source_drift:src/Billiards/training/final_semantic_ownership.h",
        ]
        if superseded_diagnostic_errors() != expected_drift:
            errors.append("superseded_v11_diagnostic_lineage_invalid")
    except Exception as error:
        errors.append(f"superseded_v11_diagnostic_lineage_failed:{error}")
    return errors


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, default=ROOT)
    args = parser.parse_args()
    errors = validate(args.source_root.resolve())
    print(json.dumps({
        "schema": "billiardgl.phase4.v11_raster_support_connectivity_implementation.v1",
        "status": "PASS" if not errors else "FAIL",
        "gpu_cocoa_opengl_invoked": False,
        "capture_or_readiness_authorized": False,
        "cpu_fixture_count": 34,
        "frozen_counterfactual_is_machine_pass": False,
        "errors": errors,
    }, indent=2, sort_keys=True))
    if errors:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
