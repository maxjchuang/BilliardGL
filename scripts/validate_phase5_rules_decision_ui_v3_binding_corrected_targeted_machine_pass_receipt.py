#!/usr/bin/env python3
"""Read-only validator for the frozen Phase5 v3 corrected targeted28 pass."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import stat
import struct
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_RECEIPT = (
    ROOT / "docs/evidence/phase5-rules-decision-ui-v3-binding-corrected-"
    "targeted-machine-pass-receipt-v1.json")
EXPECTED_RECEIPT_SHA256 = "c7be73e9759dca152da94108efd1a87893242d8d4c72b6362c66e002723e1c4c"
EXPECTED_READY_SHA256 = (
    "19c99b37d60247022cc7ce142ecefdc9c412aab836a0db9049bde32f16b07e12")
EXPECTED_AUTH_SHA256 = (
    "71ccea571e0c02536f88bdf6c547f9748229348ea54fc9fc503aeb1b981c2d42")
EXPECTED_MARKER_SHA256 = (
    "8b7fb32ae2ebfe301de9c7c71b73da04ca6261faa24531694aa4d0d3edbc7186")
EXPECTED_RESULTS_SHA256 = (
    "0368c8086f68a1f744cd47173d79dbfef8b29ba488c151132c216ea1fa8f49ad")
EXPECTED_EVIDENCE_SHA256 = (
    "b7dc8fcc3ee266b8e8b30f818a8df98aade195efeb08774a217f0db1b833d612")
EXPECTED_CASE_AGGREGATE = (
    "ac9288a18896384b0f2a40ed8f000e3b7b3830e5ccd391d21b1c3c982700f7e4")
EXPECTED_OUTPUT_AGGREGATE = (
    "741ac3a09b678ffc7b2f355e11248172195ee58752d5561418bda209c3a89284")


class MachinePassReceiptError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise MachinePassReceiptError(message)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(value, dict), str(path) + ": JSON object required")
    return value


def bound_file(relative: Any, digest: Any, label: str) -> Path:
    require(isinstance(relative, str), label + ": path missing")
    require(isinstance(digest, str) and len(digest) == 64,
            label + ": SHA missing")
    path = (ROOT / relative).resolve(strict=True)
    require(path.is_file() and sha256(path) == digest,
            label + ": missing or SHA mismatch")
    return path


def mode(path: Path) -> str:
    return f"{stat.S_IMODE(path.stat().st_mode):04o}"


def read_ppm(path: Path) -> tuple[int, int]:
    with path.open("rb") as source:
        require(source.readline().strip() == b"P6", str(path) + ": P6 required")
        dimensions = source.readline().split()
        require(len(dimensions) == 2, str(path) + ": dimensions")
        width, height = map(int, dimensions)
        require(source.readline().strip() == b"255",
                str(path) + ": RGB8 required")
        pixels = source.read()
    require(len(pixels) == width * height * 3,
            str(path) + ": pixel byte length")
    return width, height


def read_png_dimensions(path: Path) -> tuple[int, int]:
    with path.open("rb") as source:
        require(source.read(8) == b"\x89PNG\r\n\x1a\n",
                str(path) + ": PNG signature")
        length = struct.unpack(">I", source.read(4))[0]
        chunk_type = source.read(4)
        data = source.read(length)
    require(chunk_type == b"IHDR" and length == 13,
            str(path) + ": first IHDR required")
    width, height = struct.unpack(">II", data[:8])
    require(width > 0 and height > 0, str(path) + ": dimensions")
    return width, height


def output_aggregate(root: Path, files: list[Path]) -> str:
    digest = hashlib.sha256()
    for path in sorted(files):
        relative = path.relative_to(root).as_posix()
        digest.update(relative.encode("utf-8"))
        digest.update(b"\0")
        digest.update(sha256(path).encode("ascii"))
        digest.update(b"\n")
    return digest.hexdigest()


def validate_receipt(path: Path = DEFAULT_RECEIPT,
                     enforce_frozen_sha: bool = True) -> dict[str, Any]:
    path = path.resolve(strict=True)
    if enforce_frozen_sha:
        require(sha256(path) == EXPECTED_RECEIPT_SHA256,
                "frozen machine pass receipt SHA mismatch")
    receipt = load(path)
    require(
        receipt.get("schema") ==
            "phase5_rules_decision_ui_v3_binding_corrected_targeted_machine_pass_receipt_v1" and
        receipt.get("status") ==
            "TARGETED_GPU_MACHINE_28_PASS_TA_REVIEW_PENDING",
        "machine pass receipt identity")

    lineage = receipt.get("lineage", {})
    ready_path = bound_file(
        lineage.get("readiness_manifest_path"),
        lineage.get("readiness_manifest_sha256"), "readiness")
    require(lineage.get("readiness_manifest_sha256") == EXPECTED_READY_SHA256,
            "readiness frozen SHA")
    ready = load(ready_path)
    expected_ids = ready.get("case_ids_in_order")
    require(isinstance(expected_ids, list) and len(expected_ids) == 28 and
            len(set(expected_ids)) == 28,
            "readiness exact 28 IDs")

    auth_path = bound_file(
        lineage.get("authorization_path"),
        lineage.get("authorization_sha256"), "authorization")
    require(lineage.get("authorization_sha256") == EXPECTED_AUTH_SHA256 and
            mode(auth_path) == "0444",
            "authorization frozen SHA/permission")
    auth = load(auth_path)
    require(
        auth.get("authorization_id") == lineage.get("authorization_id") and
        auth.get("authorized") is True and auth.get("consumed") is False and
        auth.get("case_count") == 28 and
        auth.get("scene_ids_in_order") == expected_ids and
        auth.get("ready_manifest_sha256") == EXPECTED_READY_SHA256 and
        auth.get("authorized_output_canonical_path") ==
            str((ROOT / lineage.get("output_root", "")).resolve()) and
        auth.get("targeted_visual_repair_gpu_authorized") is True and
        auth.get("product_exposed") is False and
        auth.get("golden_promotion") is False and
        auth.get("m2_promotion") is False,
        "authorization scope/boundary")

    marker_path = bound_file(
        lineage.get("consumed_marker_path"),
        lineage.get("consumed_marker_sha256"), "consumed marker")
    require(
        lineage.get("consumed_marker_sha256") == EXPECTED_MARKER_SHA256 and
        mode(marker_path) == "0444" and
        marker_path.name.endswith("-" + EXPECTED_AUTH_SHA256) and
        marker_path.read_text(encoding="utf-8") ==
            auth["authorization_id"] + "\n",
        "consumed marker identity/permission/content")

    output_root = (ROOT / lineage.get("output_root", "")).resolve(strict=True)
    require(output_root.is_dir() and mode(output_root) == "0555",
            "output root frozen permission")
    ppm_root, png_root = output_root / "ppm", output_root / "png"
    require(ppm_root.is_dir() and png_root.is_dir() and
            mode(ppm_root) == mode(png_root) == "0555",
            "image directories frozen permission")
    results_path = output_root / "rules-decision-ui-v3-targeted-results.jsonl"
    evidence_path = output_root / "rules-decision-ui-v3-targeted-evidence.json"
    require(
        sha256(results_path) ==
            lineage.get("results_jsonl_sha256") == EXPECTED_RESULTS_SHA256 and
        sha256(evidence_path) ==
            lineage.get("evidence_manifest_sha256") == EXPECTED_EVIDENCE_SHA256,
        "results/evidence frozen SHA")
    results = [
        json.loads(line) for line in
        results_path.read_text(encoding="utf-8").splitlines() if line
    ]
    evidence = load(evidence_path)
    evidence_rows = evidence.get("cases")
    require(
        len(results) == 28 and isinstance(evidence_rows, list) and
        len(evidence_rows) == 28 and
        [row.get("capture_id") for row in results] == expected_ids and
        [row.get("capture_id") for row in evidence_rows] == expected_ids,
        "exact 28 result/evidence rows and order")
    require(
        evidence.get("schema") ==
            "phase5_rules_decision_ui_v3_targeted_actual_gpu_evidence_v1" and
        evidence.get("status") ==
            "TARGETED_GPU_MACHINE_EVIDENCE_TA_REVIEW_PENDING" and
        evidence.get("authorization_id") == auth.get("authorization_id") and
        evidence.get("ready_manifest_sha256") == EXPECTED_READY_SHA256 and
        evidence.get("case_count") == 28 and
        evidence.get("claims") == {
            "actual_gpu_completed": 28,
            "golden_promotion": False,
            "m2_promotion": False,
            "product_exposed": False,
            "technical_art_review_completed": 0,
        }, "evidence manifest boundary")

    plan_dimensions: dict[str, tuple[int, int]] = {}
    plan_path = Path(ready["paths"]["plan"])
    for line in plan_path.read_text(encoding="utf-8").splitlines():
        fields = line.split("\t")
        require(len(fields) == 12, "plan field count")
        plan_dimensions[fields[0]] = (int(fields[7]), int(fields[8]))
    require(list(plan_dimensions) == expected_ids, "plan exact ID order")

    case_digest = hashlib.sha256()
    expected_inventory = {results_path, evidence_path}
    dimension_counts: dict[str, int] = {}
    for capture_id, row, evidence_row in zip(
            expected_ids, results, evidence_rows):
        for key, value in row.items():
            require(evidence_row.get(key) == value,
                    capture_id + ": evidence/result row drift: " + key)
        require(
            row.get("actual_gpu_capture") is True and
            row.get("rules_ui_after_tone_map") is True and
            isinstance(row.get("gpu_draw_calls"), int) and
            row["gpu_draw_calls"] >= 1 and
            row.get("canonical_sha256") ==
                row.get("gpu_canonical_sha256"),
            capture_id + ": GPU/draw/canonical contract")
        passes = row.get("render_passes")
        require(
            isinstance(passes, list) and
            passes.index("aces_tone_map") < passes.index("product_ui") <
                passes.index("product_present"),
            capture_id + ": after-tonemap pass order")

        expected_dimensions = plan_dimensions[capture_id]
        ppm_path = ppm_root / (capture_id + ".ppm")
        png_path = png_root / (capture_id + ".png")
        require(Path(row.get("ppm_path", "")).resolve() == ppm_path.resolve() and
                Path(evidence_row.get("png_path", "")).resolve() ==
                    png_path.resolve(),
                capture_id + ": exact image paths")
        require(ppm_path.is_file() and png_path.is_file() and
                mode(ppm_path) == mode(png_path) == "0444",
                capture_id + ": image files/permissions")
        ppm_sha, png_sha = sha256(ppm_path), sha256(png_path)
        require(
            ppm_sha == row.get("ppm_sha256") ==
                evidence_row.get("ppm_sha256") and
            png_sha == evidence_row.get("png_sha256"),
            capture_id + ": image SHA")
        require(
            read_ppm(ppm_path) == expected_dimensions and
            read_png_dimensions(png_path) == expected_dimensions and
            (evidence_row.get("png_physical_width"),
             evidence_row.get("png_physical_height")) == expected_dimensions,
            capture_id + ": decoded physical dimensions")
        label = f"{expected_dimensions[0]}x{expected_dimensions[1]}"
        dimension_counts[label] = dimension_counts.get(label, 0) + 1
        case_digest.update(capture_id.encode("utf-8"))
        case_digest.update(b"\0")
        case_digest.update(ppm_sha.encode("ascii"))
        case_digest.update(b"\0")
        case_digest.update(png_sha.encode("ascii"))
        case_digest.update(b"\0")
        case_digest.update((label + "\n").encode("ascii"))
        expected_inventory.update((ppm_path, png_path))

    all_files = [item for item in output_root.rglob("*") if item.is_file()]
    require(set(all_files) == expected_inventory and len(all_files) == 58,
            "exact output file inventory")
    require(all(mode(item) == "0444" for item in all_files),
            "all output files frozen read-only")
    counts = receipt.get("verified_counts")
    require(counts == {
        "planned_cases": 28, "result_rows": 28, "evidence_cases": 28,
        "ppm_files": 28, "png_files": 28, "output_files_total": 58,
    }, "verified counts")
    require(
        dimension_counts ==
            receipt.get("decoded_physical_dimension_counts") == {
                "1280x720": 14, "1920x1080": 7, "2560x1440": 7,
            }, "decoded dimension counts")

    aggregates = receipt.get("stable_aggregates", {})
    require(
        case_digest.hexdigest() ==
            aggregates.get("case_evidence_aggregate_sha256") ==
            EXPECTED_CASE_AGGREGATE and
        output_aggregate(output_root, all_files) ==
            aggregates.get("output_aggregate_sha256") ==
            EXPECTED_OUTPUT_AGGREGATE,
        "stable evidence aggregates")
    runtime = receipt.get("frozen_runtime_bindings", {})
    for field, digest in ready.get("frozen_bindings", {}).items():
        if field in runtime:
            require(auth.get(field) == digest == runtime.get(field),
                    "runtime binding: " + field)
    live_paths = {
        "source_binding_manifest_sha256":
            Path(ready["paths"]["source_binding_manifest"]),
        "runner_binary_sha256": Path(ready["paths"]["runner_binary"]),
        "billiards_binary_sha256": Path(ready["paths"]["billiards_binary"]),
        "font_sha256": Path(ready["paths"]["font"]),
        "hud_token_sha256": Path(ready["paths"]["hud_token"]),
        "shell_token_sha256": Path(ready["paths"]["shell_token"]),
        "runner_source_sha256":
            ROOT / "spikes/render_core_profile/"
            "phase5_rules_decision_ui_gpu_capture_runner.mm",
    }
    for field, live_path in live_paths.items():
        require(live_path.is_file() and sha256(live_path) == runtime.get(field),
                "live runtime binding: " + field)

    stdout = receipt.get("runner_stdout_observation", {})
    require(
        stdout.get("text") == "EVIDENCE_CAPTURED 60/60" and
        stdout.get("classification") ==
            "LEGACY_MESSAGE_DRIFT_NON_AUTHORITATIVE" and
        stdout.get("used_as_case_count") is False and
        len(stdout.get("authoritative_count_sources", [])) == 4,
        "legacy stdout must be non-authoritative")
    require(receipt.get("claims") == {
        "targeted_machine_28_pass": True,
        "actual_gpu_completed": 28,
        "technical_art_review_completed": 0,
        "product_exposed": False,
        "golden_promotion": False,
        "m2_promotion": False,
    }, "machine-only claims boundary")
    return {
        "status":
            "PASS_FROZEN_PHASE5_V3_BINDING_CORRECTED_TARGETED_MACHINE_28",
        "machine_pass_receipt_sha256": sha256(path),
        "case_count": 28,
        "ppm_count": 28,
        "png_count": 28,
        "case_evidence_aggregate_sha256": case_digest.hexdigest(),
        "output_aggregate_sha256": output_aggregate(output_root, all_files),
        "legacy_stdout_used_as_count": False,
        "technical_art_review_completed": 0,
        "product_exposed": False,
        "golden_promotion": False,
        "m2_promotion": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--receipt", type=Path, default=DEFAULT_RECEIPT)
    args = parser.parse_args()
    try:
        print(json.dumps(validate_receipt(args.receipt), indent=2,
                         sort_keys=True))
    except (OSError, ValueError, IndexError, KeyError,
            json.JSONDecodeError, MachinePassReceiptError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
