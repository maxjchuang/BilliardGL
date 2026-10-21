#!/usr/bin/env python3
"""Execute corrected Phase5 Rules UI v3 exact-28 GPU set when authorized."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts.run_phase5_rules_decision_ui_gpu_capture import convert_png


READY_STATUS = (
    "V3_TARGETED_28_BINDING_CORRECTED_READY_EXPLICIT_AUTHORIZATION_"
    "REQUIRED_NOT_RUN")
AUTH_SCHEMA = "phase5_rules_decision_ui_v3_targeted_gpu_authorization_v1"
RUNNER_REQUIRED_AUTH_FIELDS = frozenset({
    "schema", "authorized", "consumed", "case_count",
    "authorization_id", "approved_by", "approved_at",
    "expires_unix_seconds", "visual_repair_design_gate_sha256",
    "hud_token_sha256", "shell_token_sha256", "runner_source_sha256",
    "plan_sha256", "billiards_binary_sha256", "font_sha256",
    "source_binding_manifest_sha256", "runner_binary_sha256",
    "source_aggregate_sha256",
})
TOKEN_ROWS = {
    "assets/modern/hud/focused_practice_hud_shipping_tokens_v1.json":
        "hud_token_sha256",
    "assets/modern/hud/focused_practice_shell_shipping_tokens_v1.json":
        "shell_token_sha256",
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(value: bool, message: str) -> None:
    if not value:
        raise RuntimeError(message)


def bound_path(value: str) -> Path:
    path = Path(value)
    return path if path.is_absolute() else ROOT / path


def load(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(value, dict), str(path) + ": object required")
    return value


def validate_pre_process(
        ready_path: Path, auth_path: Path,
        output: Path) -> tuple[dict[str, Any], dict[str, Any], dict[str, Path]]:
    ready = load(ready_path)
    auth = load(auth_path)
    claims = {
        "actual_gpu_authorized": False,
        "actual_gpu_completed": 0,
        "golden_promotion": False,
        "m2_promotion": False,
        "product_exposed": False,
        "technical_art_review_completed": 0,
    }
    require(
        ready.get("status") == READY_STATUS and
        ready.get("case_count") == 28 and
        ready.get("repair_case_count") == 20 and
        ready.get("regression_control_count") == 8 and
        ready.get("claims") == claims,
        "corrected v3 readiness identity/boundary")
    missing = sorted(RUNNER_REQUIRED_AUTH_FIELDS - set(auth))
    require(not missing, "corrected v3 authorization runner fields missing: " +
            ",".join(missing))
    require(
        auth.get("schema") == AUTH_SCHEMA and
        auth.get("authorized") is True and
        auth.get("consumed") is False and
        auth.get("case_count") == 28 and
        auth.get("scene_ids_in_order") == ready.get("case_ids_in_order") and
        auth.get("ready_manifest_sha256") == sha(ready_path) and
        auth.get("authorized_output_canonical_path") == str(output.resolve()) and
        auth.get("targeted_visual_repair_gpu_authorized") is True and
        auth.get("product_exposed") is False and
        auth.get("golden_promotion") is False and
        auth.get("m2_promotion") is False,
        "corrected v3 authorization exact scope/boundary")
    for field in ("authorization_id", "approved_by", "approved_at"):
        require(isinstance(auth.get(field), str) and bool(auth[field]),
                "corrected v3 authorization identity missing: " + field)
    require(auth.get("expires_unix_seconds", 0) > time.time(),
            "corrected v3 authorization expired")
    frozen = ready.get("frozen_bindings", {})
    for field, expected in frozen.items():
        require(auth.get(field) == expected,
                "corrected v3 authorization frozen binding mismatch: " + field)

    marker = Path(str(auth_path) + ".consumed-" + sha(auth_path))
    require(not marker.exists(), "corrected v3 authorization already consumed")
    require(not output.exists(), "corrected v3 output root must be fresh")
    paths = {key: bound_path(value)
             for key, value in ready.get("paths", {}).items()}
    checks = {
        "plan_sha256": "plan",
        "source_binding_manifest_sha256": "source_binding_manifest",
        "runner_binary_sha256": "runner_binary",
        "billiards_binary_sha256": "billiards_binary",
        "cpu_preflight_binary_sha256": "cpu_preflight_binary",
        "cpu_preflight_report_sha256": "cpu_preflight_report",
        "machine_report_sha256": "machine_report",
        "font_sha256": "font",
        "hud_token_sha256": "hud_token",
        "shell_token_sha256": "shell_token",
    }
    require(set(checks.values()).issubset(paths),
            "corrected v3 readiness runtime paths missing")
    for field, path_key in checks.items():
        path = paths[path_key]
        require(path.is_file() and sha(path) == frozen.get(field),
                "corrected v3 runtime binding mismatch: " + field)

    bindings = load(paths["source_binding_manifest"])
    require(bindings.get("aggregate_sha256") ==
            frozen.get("source_aggregate_sha256"),
            "corrected v3 source aggregate mismatch")
    rows = {row.get("path"): row for row in bindings.get("files", [])}
    for relative, field in TOKEN_ROWS.items():
        row = rows.get(relative, {})
        require(
            row.get("scope") == "read_only_runtime_asset" and
            row.get("sha256") == frozen.get(field) and
            sha(ROOT / relative) == frozen.get(field),
            "corrected v3 token binding mismatch: " + relative)
    return ready, auth, paths


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ready", type=Path, required=True)
    parser.add_argument("--authorization", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--execute-authorized-capture", action="store_true")
    args = parser.parse_args()
    require(args.execute_authorized_capture,
            "refusing GPU work without --execute-authorized-capture")
    ready_path = args.ready.resolve(strict=True)
    auth_path = args.authorization.resolve(strict=True)
    output = args.output_dir.resolve()
    ready, auth, paths = validate_pre_process(ready_path, auth_path, output)
    ppm_root = output / "ppm"
    results_path = output / "rules-decision-ui-v3-targeted-results.jsonl"
    subprocess.run([
        str(paths["runner_binary"]), str(auth_path), str(paths["plan"]),
        str(ppm_root), str(results_path), str(paths["billiards_binary"]),
        str(paths["font"]), str(paths["source_binding_manifest"]),
    ], cwd=ROOT, check=True)
    rows = [json.loads(line) for line in
            results_path.read_text(encoding="utf-8").splitlines() if line]
    require(
        len(rows) == 28 and
        [row.get("capture_id") for row in rows] ==
            ready["case_ids_in_order"] and
        all(row.get("actual_gpu_capture") is True for row in rows),
        "corrected v3 runner did not produce exact 28 rows")
    png_root = output / "png"
    png_root.mkdir(exist_ok=False)
    for row in rows:
        destination = png_root / (row["capture_id"] + ".png")
        width, height = convert_png(Path(row["ppm_path"]), destination)
        row.update({
            "png_path": str(destination),
            "png_sha256": sha(destination),
            "png_physical_width": width,
            "png_physical_height": height,
        })
    evidence = {
        "schema":
            "phase5_rules_decision_ui_v3_targeted_actual_gpu_evidence_v1",
        "status": "TARGETED_GPU_MACHINE_EVIDENCE_TA_REVIEW_PENDING",
        "authorization_id": auth["authorization_id"],
        "ready_manifest_sha256": sha(ready_path),
        "case_count": 28,
        "cases": rows,
        "claims": {
            "actual_gpu_completed": 28,
            "golden_promotion": False,
            "m2_promotion": False,
            "product_exposed": False,
            "technical_art_review_completed": 0,
        },
    }
    evidence_path = output / "rules-decision-ui-v3-targeted-evidence.json"
    evidence_path.write_text(
        json.dumps(evidence, ensure_ascii=False, indent=2, sort_keys=True) +
        "\n", encoding="utf-8")
    print(str(evidence_path))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

