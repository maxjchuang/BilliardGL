#!/usr/bin/env python3
"""Fail-closed validator for the combined 27-case official-rack TA review."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_REVIEW = ROOT / "docs/art/wpa-official-fixed-seed-rack-combined27-independent-ta-review-template-v1.json"
GENERATOR_PATH = ROOT / "scripts/generate_wpa_official_fixed_seed_rack_combined27_ta_review_template.py"
BATCH_ORDER = ["phase2_full_six", "phase4_targeted5", "phase5_exact16"]
REQUIRED_MACHINE_PASSES = {"phase2_full_six": 6, "phase4_targeted5": 5,
                           "phase5_exact16": 16}
HEX = set("0123456789abcdef")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def is_sha(value: Any) -> bool:
    return isinstance(value, str) and len(value) == 64 and set(value) <= HEX


def load_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(value, dict), "JSON root must be an object")
    return value


def load_generator() -> Any:
    spec = importlib.util.spec_from_file_location("rack27_generator", GENERATOR_PATH)
    require(spec is not None and spec.loader is not None, "load generator")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def resolve_bound_file(value: Any, digest: Any, label: str) -> Path:
    require(isinstance(value, str) and value and not Path(value).is_absolute(),
            label + " must be a repository-relative path")
    path = (ROOT / value).resolve()
    require(path.is_relative_to(ROOT.resolve()), label + " escapes repository")
    require(path.is_file(), label + " missing")
    require(is_sha(digest) and sha256(path) == digest, label + " SHA mismatch")
    return path


def ppm_dimensions(path: Path) -> tuple[int, int]:
    raw = path.read_bytes()
    tokens: list[bytes] = []
    index = 0
    while len(tokens) < 4:
        require(index < len(raw), "truncated PPM header")
        while index < len(raw) and raw[index:index + 1].isspace():
            index += 1
        if raw[index:index + 1] == b"#":
            while index < len(raw) and raw[index:index + 1] != b"\n":
                index += 1
            continue
        end = index
        while end < len(raw) and not raw[end:end + 1].isspace():
            end += 1
        tokens.append(raw[index:end])
        index = end
    require(tokens[0] == b"P6" and tokens[3] == b"255", "PPM must be P6 RGB8")
    width, height = int(tokens[1]), int(tokens[2])
    require(index < len(raw) and raw[index:index + 1].isspace(),
            "PPM header/data delimiter")
    index += 2 if raw[index:index + 2] == b"\r\n" else 1
    require(len(raw) - index == width * height * 3, "PPM physical byte count")
    return width, height


def png_dimensions(path: Path) -> tuple[int, int]:
    raw = path.read_bytes()
    require(len(raw) >= 24 and raw[:8] == b"\x89PNG\r\n\x1a\n"
            and raw[12:16] == b"IHDR", "PNG signature/IHDR")
    return struct.unpack(">II", raw[16:24])


def canonical_sha(rows: list[dict[str, Any]]) -> str:
    payload = json.dumps(rows, sort_keys=True, separators=(",", ":"),
                         ensure_ascii=False).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def field_any(value: dict[str, Any], names: tuple[str, ...]) -> Any:
    for name in names:
        if name in value:
            return value[name]
    summary = value.get("summary")
    if isinstance(summary, dict):
        for name in names:
            if name in summary:
                return summary[name]
    return None


def validate_machine_receipt(path: Path, case_ids: list[str], required: int) -> None:
    receipt = load_json(path)
    ids = field_any(receipt, ("case_ids_in_order", "ordered_case_ids"))
    passed = field_any(receipt, ("machine_pass_count", "passed_count", "pass_count"))
    failed = field_any(receipt, ("machine_failure_count", "failed_count", "fail_count"))
    not_run = field_any(receipt, ("machine_not_run_count", "unrun_count", "not_run_count"))
    require(ids == case_ids, "machine receipt exact case order")
    require(passed == required and failed == 0 and not_run == 0,
            "machine receipt must be all PASS with zero FAIL/NOT_RUN")


def validate_stable_manifest(path: Path, rows: list[dict[str, Any]]) -> None:
    manifest = load_json(path)
    items = manifest.get("cases")
    require(isinstance(items, list), "stable PNG manifest cases")
    expected = [{
        "case_id": row["case_id"],
        "png_path": row["future_evidence"]["png_path"],
        "png_sha256": row["future_evidence"]["png_sha256"],
        "png_physical_dimensions": row["future_evidence"]["png_physical_dimensions"],
    } for row in rows]
    normalized = [{key: item.get(key) for key in expected[0]} for item in items]
    require(normalized == expected, "stable PNG manifest exact ordered bindings")


def validate_frozen_structure(review: dict[str, Any], expected: dict[str, Any]) -> None:
    for field in ("schema", "schema_version", "frozen_bindings", "scope",
                  "aggregate_contract", "completion_preconditions",
                  "allowed_status_transitions"):
        require(review.get(field) == expected[field], "frozen structure drift: " + field)
    require(len(review.get("cases", [])) == 27, "exact 27 cases")
    mutable_case_fields = {"future_evidence", "applicable_axis_findings",
                           "manual_case_result", "blocking_axes"}
    for actual, frozen in zip(review["cases"], expected["cases"]):
        require({k: v for k, v in actual.items() if k not in mutable_case_fields} ==
                {k: v for k, v in frozen.items() if k not in mutable_case_fields},
                "frozen case structure drift: " + str(frozen["case_id"]))
        require(actual.get("not_applicable_axis_findings") ==
                frozen["not_applicable_axis_findings"],
                "Phase4 rack N/A findings are immutable")
        require(list(actual.get("applicable_axis_findings", {}).keys()) ==
                list(frozen["applicable_axis_findings"].keys()),
                "applicable axis order drift: " + str(frozen["case_id"]))


def validate_template(review: dict[str, Any], expected: dict[str, Any]) -> dict[str, Any]:
    require(review == expected, "NOT_RUN template differs from deterministic generator output")
    return {
        "status": "PASS_COMBINED27_TA_TEMPLATE_NOT_RUN",
        "case_count": 27,
        "batch_case_counts": REQUIRED_MACHINE_PASSES,
        "applicable_axes_pending": 406,
        "phase4_rack_axes_not_applicable": 35,
        "total_axis_records": 441,
        "future_evidence_bound": False,
        "review_performed": False,
        "ta_acceptance": False,
        "product_acceptance": False,
    }


def validate_completed(review: dict[str, Any], expected: dict[str, Any]) -> dict[str, Any]:
    validate_frozen_structure(review, expected)
    require(review.get("status") == "COMPLETED_INDEPENDENT_TA_REVIEW"
            and review.get("overall_result") in ("PASS", "FAIL")
            and review.get("review_performed") is True,
            "completed review status/result")

    combined = review.get("combined_future_evidence", {})
    build = resolve_bound_file(combined.get("fresh_shared_build_manifest_path"),
                               combined.get("fresh_shared_build_manifest_sha256"),
                               "fresh shared build manifest")
    load_json(build)
    require(is_sha(combined.get("fresh_shared_binary_sha256")),
            "fresh shared binary SHA")

    all_ppm_rows: list[dict[str, Any]] = []
    all_png_rows: list[dict[str, Any]] = []
    all_case_rows: list[dict[str, Any]] = []
    all_axis_statuses: list[str] = []
    all_case_results: list[str] = []
    for batch in BATCH_ORDER:
        rows = [row for row in review["cases"] if row["batch_id"] == batch]
        case_ids = [row["case_id"] for row in rows]
        future = review.get("batch_future_evidence", {}).get(batch, {})
        required = REQUIRED_MACHINE_PASSES[batch]
        resolve_bound_file(future.get("readiness_path"),
                           future.get("readiness_sha256"), batch + " readiness")
        machine = resolve_bound_file(future.get("machine_receipt_path"),
                                     future.get("machine_receipt_sha256"),
                                     batch + " machine receipt")
        require(future.get("required_machine_pass_count") == required
                and future.get("machine_pass_count") == required
                and future.get("machine_failure_count") == 0
                and future.get("machine_not_run_count") == 0,
                batch + " explicit machine all-PASS counts")
        validate_machine_receipt(machine, case_ids, required)

        ppm_rows: list[dict[str, Any]] = []
        png_rows: list[dict[str, Any]] = []
        case_rows: list[dict[str, Any]] = []
        for row in rows:
            evidence = row.get("future_evidence", {})
            ppm = resolve_bound_file(evidence.get("ppm_path"),
                                     evidence.get("ppm_sha256"),
                                     row["case_id"] + " PPM")
            png = resolve_bound_file(evidence.get("png_path"),
                                     evidence.get("png_sha256"),
                                     row["case_id"] + " PNG")
            dims = row["expected_physical_dimensions"]
            require(evidence.get("ppm_physical_dimensions") == dims
                    and evidence.get("png_physical_dimensions") == dims
                    and list(ppm_dimensions(ppm)) == dims
                    and list(png_dimensions(png)) == dims,
                    row["case_id"] + " independently decoded physical dimensions")
            ppm_row = {key: evidence[key] if key != "case_id" else row["case_id"]
                       for key in expected["aggregate_contract"]
                       ["batch_ppm_rows_in_case_order"]}
            png_row = {key: evidence[key] if key != "case_id" else row["case_id"]
                       for key in expected["aggregate_contract"]
                       ["batch_png_rows_in_case_order"]}
            case_row = {key: evidence[key] if key != "case_id" else row["case_id"]
                        for key in expected["aggregate_contract"]
                        ["batch_case_evidence_rows_in_case_order"]}
            ppm_rows.append(ppm_row)
            png_rows.append(png_row)
            case_rows.append(case_row)

            findings = row.get("applicable_axis_findings", {})
            statuses: list[str] = []
            for axis, finding in findings.items():
                status = finding.get("status")
                coords = finding.get("physical_coordinates")
                require(status in ("PASS", "FAIL")
                        and isinstance(finding.get("observation"), str)
                        and bool(finding["observation"].strip())
                        and isinstance(coords, list) and len(coords) > 0,
                        row["case_id"] + "/" + axis + " completed finding")
                require(all(isinstance(point, list) and len(point) == 2
                            and all(isinstance(value, int) for value in point)
                            and 0 <= point[0] < dims[0]
                            and 0 <= point[1] < dims[1] for point in coords),
                        row["case_id"] + "/" + axis + " physical coordinates")
                statuses.append(status)
            blocking = [axis for axis, finding in findings.items()
                        if finding["status"] == "FAIL"]
            derived = "FAIL" if blocking else "PASS"
            require(row.get("manual_case_result") == derived
                    and row.get("blocking_axes") == blocking,
                    row["case_id"] + " result/blocking axes")
            all_axis_statuses.extend(statuses)
            all_case_results.append(derived)

        stable = resolve_bound_file(future.get("stable_png_manifest_path"),
                                    future.get("stable_png_manifest_sha256"),
                                    batch + " stable PNG manifest")
        validate_stable_manifest(stable, rows)
        require(future.get("ppm_aggregate_sha256") == canonical_sha(ppm_rows)
                and future.get("png_aggregate_sha256") == canonical_sha(png_rows)
                and future.get("case_evidence_aggregate_sha256") == canonical_sha(case_rows),
                batch + " canonical aggregates")
        all_ppm_rows.extend(ppm_rows)
        all_png_rows.extend(png_rows)
        all_case_rows.extend(case_rows)

    require(combined.get("ordered_27_ppm_aggregate_sha256") == canonical_sha(all_ppm_rows)
            and combined.get("ordered_27_png_aggregate_sha256") == canonical_sha(all_png_rows)
            and combined.get("ordered_27_case_evidence_aggregate_sha256") == canonical_sha(all_case_rows),
            "combined ordered-27 canonical aggregates")
    derived_overall = "FAIL" if "FAIL" in all_case_results else "PASS"
    require(review["overall_result"] == derived_overall,
            "overall result must derive from all 27 cases")
    boundary = review.get("evidence_boundary", {})
    always_false = ("production_modified", "gpu_executed", "readiness_created",
                    "authorization_created", "m2", "m3", "golden", "beta",
                    "product_acceptance")
    require(all(boundary.get(field) is False for field in always_false),
            "forbidden evidence/product boundary changed")
    require(boundary.get("machine_acceptance") is True
            and boundary.get("review_performed") is True
            and boundary.get("ta_acceptance") is (derived_overall == "PASS"),
            "completed machine/review/TA boundary")
    return {
        "status": "PASS_COMBINED27_COMPLETED_TA_REVIEW_VALIDATION",
        "overall_result": derived_overall,
        "case_count": len(all_case_results),
        "axis_pass_count": all_axis_statuses.count("PASS"),
        "axis_fail_count": all_axis_statuses.count("FAIL"),
        "machine_batches_all_pass": True,
        "ta_acceptance": derived_overall == "PASS",
        "product_acceptance": False,
    }


def validate(path: Path | str = DEFAULT_REVIEW) -> dict[str, Any]:
    review_path = Path(path)
    if not review_path.is_absolute():
        review_path = ROOT / review_path
    review = load_json(review_path)
    expected = load_generator().build_template()
    if review.get("overall_result") == "NOT_RUN":
        return validate_template(review, expected)
    return validate_completed(review, expected)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("review", nargs="?", type=Path, default=DEFAULT_REVIEW)
    args = parser.parse_args()
    try:
        print(json.dumps(validate(args.review), ensure_ascii=False, indent=2,
                         sort_keys=True))
    except (OSError, ValueError, RuntimeError, KeyError, TypeError) as error:
        print("FAIL: " + str(error))
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
