#!/usr/bin/env python3
"""Artifact test for the Phase 4 v4 ordinal-1 pure-offline diagnosis."""

from __future__ import annotations

import hashlib
import json
import pathlib
import subprocess


ROOT = pathlib.Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "docs/art/phase4-overlay-v4-ordinal1-offline-diagnostic-v1"


def sha(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(name: str):
    return json.loads((EVIDENCE / name).read_text())


def require(value: bool, message: str) -> None:
    if not value:
        raise AssertionError(message)


def main() -> int:
    manifest = load("diagnostic-evidence-manifest-v1.json")
    require(manifest["actual_gpu_capture"] is False, "gpu claim")
    require(manifest["targeted_or_formal_runner_invoked"] is False,
            "runner invocation claim")
    for artifact in manifest["artifacts"]:
        path = EVIDENCE / artifact["path"]
        require(path.exists(), f"missing artifact:{path}")
        require(sha(path) == artifact["sha256"], f"artifact drift:{path}")
        require(path.stat().st_size == artifact["size_bytes"],
                f"artifact size drift:{path}")
    for tooling in manifest["tooling"]:
        path = ROOT / tooling["path"]
        require(sha(path) == tooling["sha256"], f"tooling drift:{path}")

    bindings = manifest["input_bindings"]
    for role in ("diagnostic_gate", "failure_receipt", "original_ppm",
                 "ready_source_bindings"):
        require(sha(ROOT / bindings[role]["path"]) == bindings[role]["sha256"],
                f"input binding drift:{role}")

    package = load("d1-frozen-cpu-package-probe-dump-v1.json")
    probes = package["source_probes"]
    require(package["source_probe_count"] == len(probes) == 95,
            "D1 denominator")
    ids = [probe["source_probe_id"] for probe in probes]
    require(len(set(ids)) == 95, "D1 unique IDs")
    require(hashlib.sha256("".join(value + "\n" for value in ids).encode()).hexdigest()
            == package["ordered_source_probe_digest"], "D1 ordered digest")
    triangles = package["semantic_triangles"]
    require(package["semantic_triangle_count"] == len(triangles) == 832,
            "D1 triangles")
    require([row["production_draw_ordinal"] for row in triangles] ==
            list(range(832)), "D1 production order")
    require(package["actual_gpu_capture"] is False and
            package["cocoa_or_opengl_used"] is False, "D1 CPU-only")
    require(package["frozen_topology_object_graph_verification"]
            ["topology_source_count"] > 40, "D1 frozen object graph")

    matrix = load("d2-frozen-ppm-probe-classification-matrix-v1.json")
    require(matrix["source_probe_count"] == matrix["row_count"] ==
            len(matrix["rows"]) == 95, "D2 rows")
    require(matrix["pass_claimed"] is False and
            matrix["final_semantic_ownership_proved"] is False,
            "D2 claim boundary")
    require(all(row["ppm_neighborhood_pixel_count"] ==
                len(row["ppm_neighborhood"]) for row in matrix["rows"]),
            "D2 neighborhoods")
    require(sum(row["bounded_neighborhood_intersects_inspector"]
                for row in matrix["rows"]) == 0, "D2 panel intersection")

    counterfactual = load("d3-exact-role-vs-compound-counterfactual-v1.json")
    exact = counterfactual["offline_exact_role_analytic_counts"]
    compound = counterfactual["offline_compound_analytic_counts"]
    require(exact["source"] == compound["source"] == 95, "D3 denominator")
    require(exact["visible"] + exact["hidden"] == 95 and
            compound["visible"] + compound["hidden"] == 95, "D3 reconciliation")
    require(counterfactual["invariants"]["threshold_changed"] is False and
            counterfactual["invariants"]["probe_changed"] is False and
            counterfactual["invariants"]["route_or_offset_changed"] is False,
            "D3 invariant change")
    require(counterfactual["compound_double_assignment_count_after_arbitration"] == 0,
            "D3 double assignment")
    require(counterfactual["pass_claimed"] is False, "D3 pass claim")

    serialization = load("d4-failure-serialization-capability-audit-v1.json")
    require(all(row["lost_on_ordinal1_failure"]
                for row in serialization["missing_field_accounting"]),
            "D4 missing-field accounting")
    require(serialization["runner_modified"] is False, "D4 runner mutation")

    conformance = load("d5-gate-to-implementation-conformance-matrix-v1.json")
    classifications = {row["classification"] for row in conformance["requirements"]}
    require(classifications == {"IMPLEMENTED", "APPROXIMATED", "MISSING"},
            "D5 classifications")
    require(conformance["implementation_proposal_admitted"] is False,
            "D5 implementation approval")

    report = load("root-cause-diagnostic-report-v1.json")
    require(report["status"] ==
            "DIAGNOSTIC_MISSING_FINAL_OWNERSHIP_TELEMETRY_FAIL_CLOSED",
            "root fail-closed status")
    require(report["root_cause_isolated"] is False and
            report["all_49_frozen_hidden_probes_reconciled_with_final_ownership_proof"]
            is False, "root isolation claim")
    require({row["id"] for row in report["falsifiable_hypotheses"]} ==
            {f"H{index}_{suffix}" for index, suffix in (
                (1, "PANEL_OCCLUSION"), (2, "SCENE_GEOMETRY_OCCLUSION"),
                (3, "TRUE_CROSS_TRACK_CORE_COVERING"),
                (4, "CROSS_COMPONENT_COMPOUND_ASSIGNMENT_GAP"),
                (5, "DETACHED_ATTACHMENT_OR_FINAL_COLOR_CORRESPONDENCE_GAP"),
                (6, "EXCLUSIVE_ASSIGNMENT_COLLISION_OR_BAD_DISTANCE_FRAME"))},
            "six hypotheses")
    require(not any(report["claims"].values()), "forbidden evidence claim")
    require(report["minimal_future_no_gpu_instrumentation_gate_proposal_not_implemented"]
            ["status"] == "NEW_GATE_REQUIRED_IMPLEMENTATION_NOT_AUTHORIZED_HERE",
            "instrumentation proposal boundary")

    binary = ROOT / manifest["diagnostic_binary"]["path"]
    require(sha(binary) == manifest["diagnostic_binary"]["sha256"],
            "binary drift")
    linked = subprocess.run(["otool", "-L", str(binary)], check=True,
                            capture_output=True, text=True).stdout
    symbols = subprocess.run(["nm", "-u", str(binary)], check=True,
                             capture_output=True, text=True).stdout
    require("Cocoa" not in linked and "OpenGL" not in linked and
            not any(token in symbols for token in
                    ("NSApplication", "CGL", "_glCreate", "_glDraw", "_glReadPixels")),
            "binary not CPU-only")

    print("PHASE4_V4_ORDINAL1_OFFLINE_DIAGNOSTIC_ARTIFACT_TEST_PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
