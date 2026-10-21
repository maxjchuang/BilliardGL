#!/usr/bin/env python3
"""CPU-only validator for the v10i formal six-scene readiness-v2 package."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_READY_ROOT = ROOT / "docs/art/phase2-v10i-formal-six-scene-ready-v2"
BLOCKED_SHA256 = "7cdfc1975b42c3e17a17d4f4aa940d08888faeb5b94e6e8702302c4b01632dcf"
SCENE_IDS = (
    "break", "low_aim", "ball_closeup", "pocket_closeup",
    "tactical_overhead", "post_shot_observe",
)
GAP_IDS = (
    "CPP_READY_ASSET_ATTESTATION", "CPP_LOAD_ERROR_CONTRACT",
    "PYTHON_EXPLICIT_CHILD_ENV", "SELF_CONTAINED_FORMAL_ASSET_PACKAGE",
    "FORMAL_PERFORMANCE_COLLECTION", "FORMAL_MANIFEST_CONTRACT",
)
FALSE_FIELDS = (
    "actual_gpu_used", "gpu_capture_authorized", "capture_output_created",
    "ta_review_performed", "product_review_performed",
    "formal_six_scene_evidence", "formal_pass", "golden_promotion",
    "m2_promotion",
)


class ValidationError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValidationError(message)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(value, dict), f"{path}: JSON object required")
    return value


def validate_binding(value: Any, label: str) -> Path:
    require(isinstance(value, dict), label + ": binding object required")
    path_text = value.get("canonical_path")
    digest = value.get("sha256")
    require(isinstance(path_text, str) and Path(path_text).is_absolute(),
            label + ": canonical absolute path required")
    path = Path(path_text)
    require(path.is_file(), label + ": bound file missing")
    require(path.resolve(strict=True) == path, label + ": path is not canonical")
    require(isinstance(digest, str) and len(digest) == 64 and
            digest == sha256(path), label + ": SHA mismatch")
    return path


def validate_false_flags(value: dict[str, Any], label: str) -> None:
    for field in FALSE_FIELDS:
        require(value.get(field) is False, f"{label}: {field} must remain false")


def validate(ready_root: Path = DEFAULT_READY_ROOT) -> dict[str, Any]:
    ready_root = ready_root.resolve(strict=True)
    paths = {
        "gate": ready_root / "implementation-gate-v2.json",
        "plan": ready_root / "six-scene-plan-not-run-v2.json",
        "machine": ready_root / "machine-evidence-template-not-run-v2.json",
        "authorization": ready_root / "one-shot-authorization-template-not-issued-v2.json",
        "readiness": ready_root / "readiness-manifest-v2.json",
        "receipt": ready_root / "readiness-receipt-v2.json",
    }
    require(all(path.is_file() for path in paths.values()),
            "readiness-v2 artifact set is incomplete")
    values = {name: load(path) for name, path in paths.items()}
    readiness = values["readiness"]
    require(readiness.get("schema") ==
            "phase2_v10i_formal_six_scene_readiness_manifest_v2",
            "readiness schema mismatch")
    require(readiness.get("status") == "READY_FOR_ONE_SHOT_AUTHORIZATION_NO_GPU" and
            readiness.get("implementation_ready") is True,
            "readiness status mismatch")
    require(readiness.get("blocked_manifest_sha256") == BLOCKED_SHA256,
            "blocked manifest lineage SHA mismatch")
    blocked = validate_binding(readiness.get("blocked_v1_lineage"), "blocked lineage")
    require(sha256(blocked) == BLOCKED_SHA256, "blocked manifest bytes differ")
    require(tuple(readiness.get("closed_gap_ids", ())) == GAP_IDS,
            "closed gap ids differ")
    require(tuple(readiness.get("approved_scenes", ())) == SCENE_IDS,
            "approved scene order differs")
    validate_false_flags(readiness, "readiness")

    for key, binding_value in readiness.get("source_bindings", {}).items():
        validate_binding(binding_value, "source binding " + key)
    require(readiness.get("source_bindings"), "source bindings are empty")
    validate_binding(readiness.get("fresh_release_executable"), "Release executable")
    generated = readiness.get("generated_bindings")
    require(isinstance(generated, dict) and set(generated) == {
        "implementation_gate", "six_scene_plan", "machine_template",
        "authorization_template"}, "generated binding ids differ")
    expected_generated = {
        "implementation_gate": paths["gate"], "six_scene_plan": paths["plan"],
        "machine_template": paths["machine"],
        "authorization_template": paths["authorization"],
    }
    for key, expected in expected_generated.items():
        require(validate_binding(generated[key], "generated " + key) == expected,
                "generated path mismatch: " + key)

    package_binding = readiness.get("self_contained_asset_package")
    package_manifest_path = validate_binding(package_binding, "asset package")
    package = load(package_manifest_path)
    require(package.get("schema") ==
            "phase2_v10i_self_contained_formal_asset_package_v2" and
            package.get("self_contained") is True and
            package.get("immutable") is True and
            package.get("symlinks_allowed") is False,
            "asset package contract mismatch")
    files = package.get("files")
    require(isinstance(files, list) and len(files) == 24,
            "asset package must bind exactly 24 source files")
    require(package.get("required_asset_count") == 6 and
            package.get("runtime_dependency_count") == 18,
            "asset package counts differ")
    package_root = package_manifest_path.parent.resolve(strict=True)
    require(package_binding.get("asset_root") == str(package_root),
            "asset package root mismatch")
    for item in files:
        require(isinstance(item, dict) and isinstance(item.get("filename"), str),
                "asset package file record invalid")
        path = package_root / item["filename"]
        require(path.is_file() and not path.is_symlink() and
                path.resolve(strict=True).parent == package_root,
                "asset package file escapes root or is missing")
        digest = sha256(path)
        require(item.get("packaged_sha256") == digest and
                item.get("source_sha256") == digest,
                "asset package file SHA mismatch: " + item["filename"])

    gate = values["gate"]
    require(gate.get("status") == "PASS_IMPLEMENTATION_READY_NO_GPU",
            "implementation gate status mismatch")
    validate_false_flags(gate, "implementation gate")
    gaps = gate.get("gaps")
    require(isinstance(gaps, list) and tuple(item.get("id") for item in gaps) == GAP_IDS and
            all(item.get("status") == "CLOSED_BY_SOURCE_AND_CPU_TEST" for item in gaps),
            "implementation gap closure differs")
    for item in gaps:
        for index, evidence in enumerate(item.get("evidence", [])):
            validate_binding(evidence, f"gap {item['id']} evidence {index}")

    plan = values["plan"]
    require(plan.get("status") == "NOT_RUN_READY_FOR_ONE_SHOT_AUTHORIZATION",
            "six-scene plan status mismatch")
    validate_false_flags(plan, "six-scene plan")
    rows = plan.get("rows")
    require(isinstance(rows, list) and
            tuple(row.get("scene_id") for row in rows) == SCENE_IDS and
            all(row.get("status") == "NOT_RUN" and
                row.get("process_started") is False and
                row.get("screenshot_created") is False for row in rows),
            "six-scene rows must remain exact NOT_RUN")
    contract = plan.get("capture_contract", {})
    require(contract.get("warmup_frames") == 120 and
            contract.get("sample_frames") == 300 and
            contract.get("measurement_method") == "gpu_timer_query_plus_cpu_frame",
            "capture performance contract mismatch")

    machine = values["machine"]
    require(machine.get("status") == "NOT_RUN_READY", "machine template status")
    validate_false_flags(machine, "machine template")
    require(tuple(item.get("scene_id") for item in machine.get("scenes", ())) == SCENE_IDS and
            all(item.get("status") == "NOT_RUN" for item in machine["scenes"]),
            "machine rows must remain exact NOT_RUN")

    authorization = values["authorization"]
    require(authorization.get("status") == "NOT_ISSUED" and
            authorization.get("required_readiness_manifest_sha256") ==
                "POPULATE_FROM_FINAL_READINESS_MANIFEST" and
            authorization.get("gpu_capture_authorized") is False and
            authorization.get("formal_six_scene_evidence_authorized") is False,
            "authorization template must remain non-executable")

    safety = readiness.get("offline_safety", {})
    require(all(safety.get(field) is False for field in (
        "can_start_candidate_executable", "can_create_cocoa_context",
        "can_call_opengl", "can_create_screenshot", "imports_subprocess")),
        "offline safety contract differs")
    receipt = values["receipt"]
    validate_false_flags(receipt, "receipt")
    require(validate_binding(receipt.get("readiness_manifest"), "receipt readiness") ==
            paths["readiness"], "receipt readiness path mismatch")
    require(validate_binding(receipt.get("fresh_release_executable"),
                             "receipt executable") ==
            Path(readiness["fresh_release_executable"]["canonical_path"]),
            "receipt executable differs")
    return {
        "status": "PASS_CPU_ONLY_READINESS_V2",
        "readiness_manifest_sha256": sha256(paths["readiness"]),
        "release_executable_sha256": readiness["fresh_release_executable"]["sha256"],
        "asset_package_manifest_sha256": sha256(package_manifest_path),
        "closed_gap_count": len(GAP_IDS),
        "not_run_scene_count": len(SCENE_IDS),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ready-root", type=Path, default=DEFAULT_READY_ROOT)
    arguments = parser.parse_args()
    try:
        print(json.dumps(validate(arguments.ready_root), indent=2, sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, ValidationError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
