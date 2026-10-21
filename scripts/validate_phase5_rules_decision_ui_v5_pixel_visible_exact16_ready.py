#!/usr/bin/env python3
"""Read-only validator for Phase5 v5 pixel-visible exact16 readiness."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from scripts import prepare_phase5_rules_decision_ui_v4_exact16_ready as v1
from scripts import prepare_phase5_rules_decision_ui_v5_pixel_visible_exact16_ready as prep
from scripts import run_phase5_rules_decision_ui_v4_exact16_capture_dispatch as dispatch


def require(value: bool, message: str) -> None:
    if not value:
        raise RuntimeError(message)


def dimensions(path: Path) -> tuple[int, int]:
    with path.open("rb") as stream:
        magic = stream.readline().strip()
        size = stream.readline().strip().split()
        maximum = stream.readline().strip()
    require(magic in {b"P5", b"P6"} and len(size) == 2 and maximum == b"255",
            "CPU raster image header invalid: " + str(path))
    return int(size[0]), int(size[1])


def validate(root: Path = prep.OUTPUT) -> dict:
    root = root.resolve(strict=True)
    paths = {name: root / filename for name, filename in {
        "ready": "rules-decision-ui-v5-exact16-ready.json",
        "auth": "rules-decision-ui-v5-authorization-template-not-issued.json",
        "sources": "rules-decision-ui-v5-source-bindings.json",
        "receipt": "offline-readiness-receipt.json",
        "plan": "rules-decision-ui-v5-exact16-plan.tsv",
        "machine": "rules-decision-ui-v5-machine-540.json",
        "cpu": "rules-decision-ui-v5-exact16-cpu-preflight.json",
        "raster": "rules-decision-ui-v5-final-cpu-raster-manifest.json",
    }.items()}
    require(all(path.is_file() for path in paths.values()),
            "v5 readiness artifact set incomplete")
    require(not any(root.rglob("*.ppm")) and not any(root.rglob("*.png")) and
            not any(root.rglob("*.consumed*")),
            "v5 readiness contains GPU/capture/consumption evidence")
    ready = json.loads(paths["ready"].read_text(encoding="utf-8"))
    auth = json.loads(paths["auth"].read_text(encoding="utf-8"))
    sources = json.loads(paths["sources"].read_text(encoding="utf-8"))
    receipt = json.loads(paths["receipt"].read_text(encoding="utf-8"))
    raster = json.loads(paths["raster"].read_text(encoding="utf-8"))
    ids = v1.case_ids()
    claims = {"gpu_authorized": False, "gpu_executed": False,
              "technical_art_review": False, "product_exposed": False,
              "golden_promotion": False, "m2_promotion": False}
    require(ready.get("schema") ==
            "phase5_rules_decision_ui_v5_exact16_readiness_v1" and
            ready.get("status") ==
            "V5_PIXEL_VISIBLE_EXACT16_PRODUCER_BOUND_NOT_RUN_NO_AUTH" and
            ready.get("case_count") == 16 and
            ready.get("repair_case_count") == 4 and
            ready.get("regression_control_count") == 12 and
            ready.get("case_ids_in_order") == ids and
            ready.get("case_results") == [
                {"ordinal": index + 1, "capture_id": item, "status": "NOT_RUN"}
                for index, item in enumerate(ids)] and
            ready.get("claims") == claims,
            "v5 identity/scope/claims mismatch")
    require(ready.get("capture_producer") == {
        "target": "BilliardsPhase5RulesDecisionUiV4Exact16GpuCaptureRunner",
        "compile_definition": "BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16=1",
        "mode_exclusive": True}, "v5 producer identity mismatch")
    gate = ready.get("dispatch_gate", {})
    require(gate.get("compatible_capture_producer_bound") is True and
            gate.get("authorization_may_not_be_issued") is False and
            gate.get("authorization_requires_separate_root_review") is True and
            gate.get("preprocess_o_excl_flush_fsync_consumption") is True and
            gate.get("failure_permanently_consumed_and_bytes_preserved") is True,
            "v5 dispatch boundary mismatch")

    lineage = ready.get("successor_lineage", {})
    approved = lineage.get("approved_production_drift_only")
    require(lineage.get("supersedes_pre_implementation_live_binding_check") is True and
            lineage.get("old_gate_validator_expected_to_fail_after_authorized_implementation") is True and
            lineage.get("old_gate_and_failure_receipt_bytes_unchanged") is True and
            lineage.get("unapproved_drift_count") == 0 and
            isinstance(approved, list) and len(approved) == 2 and
            {row.get("path") for row in approved} == set(prep.ALLOWLIST),
            "v5 successor lineage/allowlist mismatch")
    for row in approved:
        require((row.get("pre_implementation_sha256"),
                 row.get("post_implementation_sha256")) ==
                prep.ALLOWLIST[row["path"]] and
                v1.sha(ROOT / row["path"]) == row["post_implementation_sha256"],
                "v5 approved lineage SHA mismatch: " + row["path"])
    for relative, old_sha in prep.gate_validator.SOURCE_BINDINGS.items():
        if relative not in prep.ALLOWLIST:
            require(v1.sha(ROOT / relative) == old_sha,
                    "v5 non-allowlisted source drift: " + relative)

    frozen = ready.get("frozen_bindings", {})
    mapping = {
        "gate_sha256": "gate", "plan_sha256": "plan",
        "machine_report_sha256": "machine_report",
        "cpu_preflight_report_sha256": "cpu_preflight_report",
        "cpu_raster_report_sha256": "cpu_raster_report",
        "cpu_raster_manifest_sha256": "cpu_raster_manifest",
        "source_binding_manifest_sha256": "source_binding_manifest",
        "billiards_binary_sha256": "billiards_binary",
        "font_sha256": "font", "hud_token_sha256": "hud_token",
        "shell_token_sha256": "shell_token", "dispatcher_sha256": "dispatcher",
        "dispatch_validator_sha256": "dispatch_validator",
        "runner_source_sha256": "runner_source",
        "runner_binary_sha256": "runner_binary", "cmake_sha256": "cmake",
    }
    for field, key in mapping.items():
        live = Path(ready["paths"][key])
        require(live.is_file() and v1.sha(live) == frozen.get(field),
                "v5 live binding drift: " + field)
    require(paths["plan"].read_text(encoding="utf-8").splitlines() ==
            v1.plan_rows(), "v5 exact plan drift")
    machine = json.loads(paths["machine"].read_text(encoding="utf-8"))
    require(machine.get("v4_totals", {}).get("machine_passed") == 540 and
            len(machine.get("cases", [])) == 540 and
            all(case.get("passed") is True for case in machine["cases"]),
            "v5 inherited 540 machine report mismatch")
    cpu = json.loads(paths["cpu"].read_text(encoding="utf-8"))
    require(cpu.get("case_count") == cpu.get("passed_count") == 16 and
            cpu.get("bad_input_count") == cpu.get("bad_input_passed_count") == 5 and
            cpu.get("actual_gpu_invoked") is False,
            "v5 CPU preflight mismatch")
    report = json.loads(Path(ready["paths"]["cpu_raster_report"]).read_text())
    require(report.get("snapshot_case_count") == 16 and
            report.get("bad_input_passed_count") == 12 and
            report.get("actual_gpu_invoked") is False and
            report.get("snapshot_aggregate_sha256") ==
            frozen.get("cpu_raster_snapshot_aggregate_sha256"),
            "v5 final CPU raster report mismatch")
    require(raster.get("file_count") == 32 and
            raster.get("report_sha256") == frozen["cpu_raster_report_sha256"] and
            raster.get("snapshot_aggregate_sha256") ==
            frozen["cpu_raster_snapshot_aggregate_sha256"] and
            raster.get("actual_gpu_invoked") is False and
            raster.get("authorization_issued") is False and
            v1.aggregate(raster["files"]) ==
            raster.get("file_manifest_aggregate_sha256"),
            "v5 CPU raster manifest mismatch")
    for row in raster["files"]:
        image = ROOT / row["path"]
        require(image.is_file() and v1.sha(image) == row["sha256"] and
                image.stat().st_size == row["bytes"],
                "v5 CPU raster file drift: " + row["path"])
        width, height = dimensions(image)
        require(width > 0 and height > 0, "v5 CPU raster dimensions invalid")

    require(sources.get("schema") ==
            "phase5_rules_decision_ui_v5_exact16_source_bindings_v1" and
            sources.get("capture_target") == ready["capture_producer"]["target"] and
            sources.get("aggregate_sha256") == frozen["source_aggregate_sha256"] and
            v1.aggregate(sources["files"]) == sources["aggregate_sha256"],
            "v5 source manifest contract mismatch")
    for row in sources["files"]:
        source = ROOT / row["path"]
        require(source.is_file() and v1.sha(source) == row["sha256"],
                "v5 source drift: " + row["path"])
    require(auth.get("schema") == dispatch.AUTH_SCHEMA and
            auth.get("status") == "NOT_ISSUED" and
            auth.get("authorized") is False and auth.get("consumed") is False and
            auth.get("case_count") == 16 and
            auth.get("scene_ids_in_order") == ids and
            auth.get("ready_manifest_sha256") == v1.sha(paths["ready"]) and
            auth.get("runner_binary_sha256") == frozen["runner_binary_sha256"] and
            auth.get("exact16_capture_producer_contract") is True and
            all(auth.get(field) is False for field in claims),
            "v5 auth template must remain NOT_ISSUED")
    require(receipt.get("status") ==
            "PIXEL_VISIBLE_PRODUCER_BOUND_EXACT16_NOT_RUN_NO_AUTH" and
            receipt.get("readiness_sha256") == v1.sha(paths["ready"]) and
            receipt.get("authorization_template_sha256") == v1.sha(paths["auth"]) and
            receipt.get("cpu_raster_manifest_sha256") == v1.sha(paths["raster"]) and
            receipt.get("claims") == claims,
            "v5 offline receipt mismatch")
    dispatch.validate_ready(paths["ready"])
    return {"status": "PASS_PHASE5_V5_PIXEL_VISIBLE_EXACT16_READY_NO_GPU_NO_AUTH",
            "readiness_sha256": v1.sha(paths["ready"]),
            "runner_binary_sha256": frozen["runner_binary_sha256"],
            "case_count": 16, "cpu_raster_file_count": 32,
            "authorization_issued": False, "gpu_executed": False}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path, nargs="?", default=prep.OUTPUT)
    try:
        print(json.dumps(validate(parser.parse_args().root), sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print("FAIL: " + str(error))
        raise SystemExit(2)
