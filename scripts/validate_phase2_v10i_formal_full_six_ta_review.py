#!/usr/bin/env python3
"""Validate the Phase2 full-six independent-TA template or completed review.

Template mode is strictly all-NOT_RUN/all-PENDING with no machine or capture
hashes. Completed mode is accepted only after a bound 6/6 machine-pass receipt
and six byte-valid 1920x1080 PPMs exist. This validator is CPU/read-only.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts.generate_phase2_v10i_formal_full_six_ta_review_template import (  # noqa: E402
    CANONICAL_AXES,
    DEFAULT_OUTPUT,
    REENTRY_GATE_PATH,
    REENTRY_GATE_SHA256,
    SCENES,
    build_template,
)


DEFAULT_REVIEW = DEFAULT_OUTPUT
SCENE_IDS = [scene[0] for scene in SCENES]
SCENE_CONTRACT = {
    scene_id: {
        "expected_semantic_camera": {
            "state": state, "preset": preset, "transitioning": False},
        "interest_region": {"name": region, "rect_normalized": rect},
    }
    for scene_id, state, preset, region, rect in SCENES
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(value, dict), f"{path}: object required")
    return value


def bound_file(relative: Any, digest: Any, label: str) -> Path:
    require(isinstance(relative, str) and relative and
            isinstance(digest, str) and len(digest) == 64,
            label + ": path/SHA required")
    path = Path(relative)
    path = path if path.is_absolute() else ROOT / path
    try:
        path = path.resolve(strict=True)
    except OSError as error:
        raise RuntimeError(label + ": bound file missing") from error
    require(path.is_file() and sha256(path) == digest,
            label + ": byte binding mismatch")
    return path


def read_ppm(path: Path) -> tuple[int, int]:
    with path.open("rb") as image:
        require(image.readline().strip() == b"P6", f"{path}: P6 required")
        dimensions = image.readline()
        while dimensions.startswith(b"#"):
            dimensions = image.readline()
        width, height = map(int, dimensions.split())
        require(image.readline().strip() == b"255", f"{path}: RGB8 required")
        pixels = image.read()
    require(len(pixels) == width * height * 3, f"{path}: pixel length")
    return width, height


def validate_gate_binding(review: dict[str, Any]) -> None:
    binding = review.get("frozen_reentry_gate_binding", {})
    require(binding == {"path": REENTRY_GATE_PATH,
                        "sha256": REENTRY_GATE_SHA256},
            "reentry gate binding drift")
    gate_path = (ROOT / REENTRY_GATE_PATH).resolve(strict=True)
    require(gate_path.is_file() and sha256(gate_path) == REENTRY_GATE_SHA256,
            "reentry gate bytes drift")
    gate = load_json(gate_path)
    require(gate.get("schema") ==
            "phase2_v10i_formal_full_six_reentry_acceptance_design_gate_v1"
            and gate.get("evidence_boundary", {}).get("gpu_executed") is False,
            "reentry gate identity/boundary")


def validate_common_structure(review: dict[str, Any]) -> None:
    require(review.get("schema") ==
            "phase2_v10i_formal_full_six_independent_ta_review_v1"
            and review.get("template_version") == 1,
            "review schema/version")
    scope = review.get("review_scope", {})
    require(scope == {
        "kind": "formal_full_six_independent_ta",
        "scene_ids_in_order": SCENE_IDS,
        "scene_count": 6,
        "canonical_axes_in_order": CANONICAL_AXES,
        "axis_count_per_scene": 7,
        "total_axis_count": 42,
        "original_resolution_required": [1920, 1080],
    }, "review scope/42-axis contract")
    require(review.get("allowed_statuses") == {
        "template_case": ["NOT_RUN"],
        "template_axis": ["PENDING"],
        "completed_case": ["PASS", "FAIL"],
        "completed_axis": ["PASS", "FAIL"],
    }, "allowed status vocabulary")
    preconditions = review.get("completion_preconditions", [])
    require(len(preconditions) == 5
            and any("6/6 machine PASS" in item for item in preconditions)
            and any("1920x1080" in item for item in preconditions)
            and any("Partial or mixed" in item for item in preconditions),
            "completion preconditions")
    cases = review.get("cases", [])
    require(isinstance(cases, list) and len(cases) == 6
            and [case.get("scene_id") for case in cases] == SCENE_IDS,
            "six case order")
    for ordinal, case in enumerate(cases, start=1):
        scene_id = case["scene_id"]
        require(case.get("ordinal") == ordinal
                and case.get("expected_semantic_camera") ==
                SCENE_CONTRACT[scene_id]["expected_semantic_camera"]
                and case.get("interest_region") ==
                SCENE_CONTRACT[scene_id]["interest_region"],
                f"{scene_id}: scene contract")
        findings = case.get("axis_findings", {})
        require(list(findings) == CANONICAL_AXES,
                f"{scene_id}: canonical axis order")


def validate_template_mode(review: dict[str, Any]) -> dict[str, Any]:
    require(review == build_template(),
            "NOT_RUN template must be byte-semantically pristine")
    return {
        "status": "PASS_FULL_SIX_TA_TEMPLATE_NOT_RUN",
        "mode": "TEMPLATE",
        "scene_count": 6,
        "axis_count": 42,
        "cases_not_run": 6,
        "axes_pending": 42,
        "machine_receipt_bound": False,
        "ppm_bound_count": 0,
        "human_ta_review_performed": False,
        "full_six_ta_pass": False,
    }


def validate_machine_and_images(review: dict[str, Any]) -> dict[str, Any]:
    binding = review.get("future_machine_pass_binding", {})
    require(binding.get("status") == "BOUND_FULL_SIX_MACHINE_PASS"
            and binding.get("required_schema") ==
                "phase2_v10i_formal_full_six_machine_pass_receipt_v1"
            and binding.get("required_status") == "FULL_SIX_MACHINE_PASS"
            and binding.get("required_scene_count") == 6
            and binding.get("required_machine_pass_scene_count") == 6
            and binding.get("required_failed_machine_check_count") == 0,
            "completed review machine binding contract")
    receipt_path = bound_file(binding.get("receipt_path"),
                              binding.get("receipt_sha256"),
                              "full-six machine receipt")
    receipt = load_json(receipt_path)
    require(receipt.get("schema") == binding["required_schema"]
            and receipt.get("status") == binding["required_status"]
            and receipt.get("reentry_gate_sha256") == REENTRY_GATE_SHA256
            and receipt.get("counts") == {
                "planned_scenes": 6,
                "actual_gpu_captures": 6,
                "machine_passed_scenes": 6,
                "failed_machine_checks": 0,
            }, "full-six machine receipt identity/counts")
    machine_scenes = receipt.get("scenes_in_order", [])
    require([scene.get("scene_id") for scene in machine_scenes] == SCENE_IDS,
            "machine receipt six-scene order")
    by_id = {scene["scene_id"]: scene for scene in machine_scenes}
    cases = {case["scene_id"]: case for case in review["cases"]}
    for scene_id in SCENE_IDS:
        machine = by_id[scene_id]
        require(machine.get("machine_result") == "PASS"
                and machine.get("semantic_camera") ==
                SCENE_CONTRACT[scene_id]["expected_semantic_camera"]
                and machine.get("interest_region") ==
                SCENE_CONTRACT[scene_id]["interest_region"],
                f"{scene_id}: machine semantic/ROI pass")
        path = bound_file(machine.get("ppm_path"), machine.get("ppm_sha256"),
                          scene_id + " machine PPM")
        width, height = read_ppm(path)
        require([width, height] == [1920, 1080]
                and machine.get("dimensions") == [1920, 1080],
                f"{scene_id}: machine PPM dimensions")
        evidence = cases[scene_id].get("future_ppm_evidence", {})
        require(evidence == {
            "path": machine.get("ppm_path"),
            "sha256": machine.get("ppm_sha256"),
            "dimensions": [1920, 1080],
        }, f"{scene_id}: review PPM must equal machine receipt")
    return receipt


def validate_completed_mode(review: dict[str, Any]) -> dict[str, Any]:
    receipt = validate_machine_and_images(review)
    overall = review.get("overall_result")
    require(overall in ("PASS", "FAIL") and review.get("status") ==
            ("FULL_SIX_TA_REVIEW_PASS" if overall == "PASS"
             else "FULL_SIX_TA_REVIEW_FAIL"),
            "completed review status/result")
    case_pass = case_fail = axis_pass = axis_fail = 0
    failed_cases: list[str] = []
    blockers: list[str] = []
    for case in review["cases"]:
        scene_id = case["scene_id"]
        result = case.get("manual_case_result")
        require(result in ("PASS", "FAIL"),
                f"{scene_id}: completed case PASS/FAIL only")
        failed_axes: list[str] = []
        for axis, finding in case["axis_findings"].items():
            status = finding.get("status")
            require(status in ("PASS", "FAIL"),
                    f"{scene_id}/{axis}: completed axis PASS/FAIL only")
            require(isinstance(finding.get("observation"), str)
                    and len(finding["observation"].strip()) >= 20,
                    f"{scene_id}/{axis}: completed observation required")
            if status == "PASS":
                axis_pass += 1
            else:
                axis_fail += 1
                failed_axes.append(axis)
        require(case.get("blocking_axes") == failed_axes
                and (result == "FAIL") == bool(failed_axes),
                f"{scene_id}: exact blocker/case consistency")
        if result == "PASS":
            case_pass += 1
        else:
            case_fail += 1
            failed_cases.append(scene_id)
            blockers.extend(f"{scene_id}:{axis}" for axis in failed_axes)
    require((overall == "PASS") == (case_fail == 0 and axis_fail == 0),
            "completed overall/case/axis consistency")
    require(review.get("review_counts") == {
        "cases_total": 6,
        "cases_not_run": 0,
        "cases_pass": case_pass,
        "cases_fail": case_fail,
        "axes_total": 42,
        "axes_pending": 0,
        "axes_pass": axis_pass,
        "axes_fail": axis_fail,
    } and review.get("failed_case_ids") == failed_cases
       and review.get("blocking_axes") == blockers,
       "completed review counts/failure inventory")
    boundary = review.get("evidence_boundary", {})
    require(boundary == {
        "template_only": False,
        "machine_receipt_bound": True,
        "capture_ppm_bound_count": 6,
        "human_ta_review_performed": True,
        "full_six_ta_pass": overall == "PASS",
        "full_six_formal_pass": overall == "PASS",
        "authorization_prepared_by_template_work": False,
        "authorization_issued_by_template_work": False,
        "gpu_executed_by_template_work": False,
        "production_modified": False,
        "m2_promotion": False,
        "golden_promotion": False,
        "product_review_performed": False,
        "product_pass": False,
        "beta_pass": False,
    }, "completed review evidence boundary")
    return {
        "status": "PASS_COMPLETED_FULL_SIX_TA_REVIEW_STRUCTURE",
        "mode": "COMPLETED",
        "overall_result": overall,
        "scene_count": 6,
        "axis_count": 42,
        "machine_receipt_sha256":
            review["future_machine_pass_binding"]["receipt_sha256"],
        "machine_scene_count": receipt["counts"]["machine_passed_scenes"],
        "ppm_bound_count": 6,
        "full_six_ta_pass": overall == "PASS",
        "m2_promotion": False,
        "golden_promotion": False,
        "product_pass": False,
    }


def validate(path: Path = DEFAULT_REVIEW) -> dict[str, Any]:
    path = path.resolve(strict=True)
    review = load_json(path)
    validate_gate_binding(review)
    validate_common_structure(review)
    if review.get("status") == (
            "TEMPLATE_NOT_RUN_WAITING_FOR_FULL_SIX_MACHINE_PASS"):
        result = validate_template_mode(review)
    else:
        result = validate_completed_mode(review)
    result["review_sha256"] = sha256(path)
    result["reentry_gate_sha256"] = REENTRY_GATE_SHA256
    result["m2_promotion"] = False
    result["golden_promotion"] = False
    result["product_pass"] = False
    result["gpu_executed"] = False
    result["production_modified"] = False
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--review", type=Path, default=DEFAULT_REVIEW)
    arguments = parser.parse_args()
    try:
        print(json.dumps(validate(arguments.review), indent=2, sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
