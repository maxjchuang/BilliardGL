#!/usr/bin/env python3
"""CPU-only validator for v10i formal six-scene readiness v3."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
from typing import Any

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPOSITORY_ROOT))

from scripts import prepare_phase2_v10i_formal_six_scene_ready_v3 as v3
from scripts import validate_phase2_v10i_formal_six_scene_ready_v2 as base


DEFAULT_READY_ROOT = v3.V3_ROOT


def load(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    base.require(isinstance(value, dict), f"{path}: object required")
    return value


def validate(ready_root: Path = DEFAULT_READY_ROOT) -> dict[str, Any]:
    ready_root = ready_root.resolve(strict=True)
    paths = {
        "gate": ready_root / "implementation-gate-v3.json",
        "plan": ready_root / "six-scene-plan-not-run-v3.json",
        "machine": ready_root / "machine-evidence-template-not-run-v3.json",
        "authorization":
            ready_root / "one-shot-authorization-template-not-issued-v3.json",
        "readiness": ready_root / "readiness-manifest-v3.json",
        "receipt": ready_root / "readiness-receipt-v3.json",
    }
    base.require(all(path.is_file() for path in paths.values()),
                 "readiness-v3 artifact set incomplete")
    values = {key: load(path) for key, path in paths.items()}
    readiness = values["readiness"]
    base.require(readiness.get("schema") ==
                 "phase2_v10i_formal_six_scene_readiness_manifest_v3",
                 "readiness-v3 schema")
    base.require(readiness.get("status") ==
                 "READY_FOR_ONE_SHOT_AUTHORIZATION_NO_GPU" and
                 readiness.get("implementation_ready") is True,
                 "readiness-v3 status")
    base.validate_false_flags(readiness, "readiness-v3")
    base.require(readiness.get("blocked_manifest_sha256") ==
                 v3.v2.BLOCKED_MANIFEST_SHA256,
                 "blocked-v1 SHA lineage")
    base.require(readiness.get("v2_readiness_manifest_sha256") ==
                 v3.V2_READINESS_SHA256,
                 "v2 readiness SHA lineage")
    base.require(base.validate_binding(readiness.get("v2_lineage"),
                                       "v2 lineage") == v3.V2_READINESS,
                 "v2 lineage path")
    base.require(tuple(readiness.get("closed_gap_ids", ())) == v3.GAP_IDS,
                 "closed gap ids")
    base.require(readiness.get("closed_v2_drift_ids") ==
                 ["FORMAL_RUNNER_EXPLICIT_MODERN_RENDERER"],
                 "v2 drift closure id")
    base.require(tuple(readiness.get("approved_scenes", ())) == v3.SCENE_IDS,
                 "approved scene order")

    source_bindings = readiness.get("source_bindings")
    base.require(isinstance(source_bindings, dict) and
                 set(source_bindings) == set(v3.SOURCE_BINDINGS),
                 "source binding ids")
    for name, binding in source_bindings.items():
        base.validate_binding(binding, "source " + name)
    executable = base.validate_binding(
        readiness.get("fresh_release_executable"), "Release executable")

    generated = readiness.get("generated_bindings")
    expected_generated = {
        "implementation_gate": paths["gate"],
        "six_scene_plan": paths["plan"],
        "machine_template": paths["machine"],
        "authorization_template": paths["authorization"],
    }
    base.require(isinstance(generated, dict) and
                 set(generated) == set(expected_generated),
                 "generated binding ids")
    for key, expected in expected_generated.items():
        base.require(base.validate_binding(generated[key], "generated " + key)
                     == expected, "generated path: " + key)

    package_binding = readiness.get("self_contained_asset_package")
    package_manifest_path = base.validate_binding(
        package_binding, "asset package")
    package = load(package_manifest_path)
    package_root = package_manifest_path.parent.resolve(strict=True)
    base.require(package.get("schema") ==
                 "phase2_v10i_self_contained_formal_asset_package_v2" and
                 package.get("self_contained") is True and
                 package.get("immutable") is True and
                 package.get("symlinks_allowed") is False,
                 "asset package contract")
    files = package.get("files")
    base.require(isinstance(files, list) and len(files) == 24 and
                 package.get("required_asset_count") == 6 and
                 package.get("runtime_dependency_count") == 18,
                 "asset package counts")
    base.require(package_binding.get("asset_root") == str(package_root),
                 "asset package root")
    for item in files:
        path = package_root / item["filename"]
        base.require(path.is_file() and not path.is_symlink() and
                     path.resolve(strict=True).parent == package_root,
                     "asset path/root: " + item["filename"])
        digest = base.sha256(path).removeprefix("sha256:")
        base.require(item.get("source_sha256") == digest and
                     item.get("packaged_sha256") == digest,
                     "asset SHA: " + item["filename"])

    gate = values["gate"]
    base.require(gate.get("status") == "PASS_IMPLEMENTATION_READY_NO_GPU",
                 "implementation gate status")
    base.validate_false_flags(gate, "implementation gate v3")
    gaps = gate.get("gaps")
    base.require(isinstance(gaps, list) and
                 tuple(item.get("id") for item in gaps) == v3.GAP_IDS and
                 all(item.get("status") == "CLOSED_BY_SOURCE_AND_CPU_TEST"
                     for item in gaps), "six gap closure")
    for item in gaps:
        for index, evidence in enumerate(item.get("evidence", [])):
            base.validate_binding(evidence,
                                  f"gap {item['id']} evidence {index}")
    drift = gate.get("v2_drift_finding", {})
    base.require(drift.get("id") == "FORMAL_RUNNER_EXPLICIT_MODERN_RENDERER" and
                 drift.get("status") == "CLOSED_BY_SOURCE_AND_CPU_TEST",
                 "explicit modern renderer closure")
    base.validate_binding(drift.get("evidence"), "modern renderer evidence")

    plan = values["plan"]
    base.require(plan.get("status") ==
                 "NOT_RUN_READY_FOR_ONE_SHOT_AUTHORIZATION",
                 "plan status")
    base.validate_false_flags(plan, "plan v3")
    contract = plan.get("capture_contract", {})
    base.require(contract.get("renderer") == "modern" and
                 contract.get("automation_extra_args") ==
                     ["--renderer", "modern"] and
                 contract.get("resolution") == {"width": 1920, "height": 1080} and
                 contract.get("quality") == "medium" and
                 contract.get("msaa_samples") == 4 and
                 contract.get("warmup_frames") == 120 and
                 contract.get("sample_frames") == 300 and
                 contract.get("measurement_method") ==
                     "gpu_timer_query_plus_cpu_frame",
                 "capture contract")
    rows = plan.get("rows")
    base.require(isinstance(rows, list) and
                 tuple(row.get("scene_id") for row in rows) == v3.SCENE_IDS and
                 all(row.get("status") == "NOT_RUN" and
                     row.get("process_started") is False and
                     row.get("screenshot_created") is False
                     for row in rows), "six rows NOT_RUN")

    machine = values["machine"]
    base.require(machine.get("status") == "NOT_RUN_READY",
                 "machine status")
    base.validate_false_flags(machine, "machine v3")
    base.require(tuple(item.get("scene_id")
                       for item in machine.get("scenes", ())) == v3.SCENE_IDS and
                 all(item.get("status") == "NOT_RUN"
                     for item in machine["scenes"]), "machine scenes NOT_RUN")

    authorization = values["authorization"]
    base.require(authorization.get("schema") ==
                 "phase2_v10i_formal_six_scene_one_shot_authorization_v3" and
                 authorization.get("status") == "NOT_ISSUED" and
                 authorization.get("authorized_output_canonical_path") ==
                     "POPULATE_WITH_ABSOLUTE_ABSENT_CAPTURE_OUTPUT" and
                 authorization.get("required_readiness_manifest_sha256") ==
                     "POPULATE_FROM_FINAL_READINESS_MANIFEST" and
                 authorization.get("gpu_capture_authorized") is False and
                 authorization.get("formal_six_scene_evidence_authorized") is False and
                 authorization.get("authorized_capture_count") == 0 and
                 authorization.get("consumed_capture_count") == 0,
                 "authorization template must be non-executable")

    safety = readiness.get("offline_safety", {})
    base.require(all(safety.get(field) is False for field in (
        "can_start_candidate_executable", "can_create_cocoa_context",
        "can_call_opengl", "can_create_screenshot", "imports_subprocess")),
        "offline safety contract")
    receipt = values["receipt"]
    base.validate_false_flags(receipt, "receipt v3")
    base.require(base.validate_binding(receipt.get("readiness_manifest"),
                                       "receipt readiness") == paths["readiness"],
                 "receipt readiness path")
    base.require(base.validate_binding(receipt.get("fresh_release_executable"),
                                       "receipt executable") == executable,
                 "receipt executable path")
    return {
        "status": "PASS_CPU_ONLY_READINESS_V3",
        "readiness_manifest_sha256":
            base.sha256(paths["readiness"]).removeprefix("sha256:"),
        "release_executable_sha256": base.sha256(executable).removeprefix("sha256:"),
        "asset_package_manifest_sha256":
            base.sha256(package_manifest_path).removeprefix("sha256:"),
        "closed_gap_count": len(v3.GAP_IDS),
        "closed_v2_drift_count": 1,
        "not_run_scene_count": len(v3.SCENE_IDS),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ready-root", type=Path, default=DEFAULT_READY_ROOT)
    arguments = parser.parse_args()
    try:
        print(json.dumps(validate(arguments.ready_root), indent=2,
                         sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError,
            base.ValidationError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
