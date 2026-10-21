#!/usr/bin/env python3
"""Read-only validator for dispatcher-aware Phase5 v4 exact16 readiness v2."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from scripts import prepare_phase5_rules_decision_ui_v4_exact16_ready as v1
from scripts import prepare_phase5_rules_decision_ui_v4_exact16_ready_v2 as prep


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def validate(root: Path = prep.DEFAULT_OUTPUT) -> dict:
    root = root.resolve(strict=True)
    names = {
        "ready": "rules-decision-ui-v4-exact16-ready.json",
        "auth": "rules-decision-ui-v4-authorization-template-not-issued.json",
        "receipt": "offline-readiness-receipt.json",
        "sources": "rules-decision-ui-v4-source-bindings.json",
        "plan": "rules-decision-ui-v4-exact16-plan.tsv",
        "machine": "rules-decision-ui-v4-machine.json",
        "cpu": "rules-decision-ui-v4-exact16-cpu-preflight.json",
    }
    paths = {key: root / name for key, name in names.items()}
    require(all(path.is_file() for path in paths.values()), "v2 artifact set incomplete")
    require(not any(root.rglob("*.ppm")) and not any(root.rglob("*.png")) and
            not any(root.rglob("*.consumed*")), "v2 contains GPU/consumption evidence")
    ready = json.loads(paths["ready"].read_text())
    auth = json.loads(paths["auth"].read_text())
    receipt = json.loads(paths["receipt"].read_text())
    sources = json.loads(paths["sources"].read_text())
    ids = v1.case_ids()
    claims = {"gpu_authorized": False, "gpu_executed": False,
              "technical_art_review": False, "product_exposed": False,
              "golden_promotion": False, "m2_promotion": False}
    require(ready.get("schema") == "phase5_rules_decision_ui_v4_exact16_readiness_v2" and
            ready.get("status") == "V4_EXACT16_DISPATCH_READY_NOT_RUN_NO_AUTH" and
            ready.get("case_count") == 16 and ready.get("repair_case_count") == 4 and
            ready.get("regression_control_count") == 12 and
            ready.get("case_ids_in_order") == ids and ready.get("claims") == claims,
            "v2 readiness identity mismatch")
    require(ready.get("case_results") == [
        {"ordinal": i + 1, "capture_id": item, "status": "NOT_RUN"}
        for i, item in enumerate(ids)], "v2 NOT_RUN rows mismatch")
    gate = ready.get("dispatch_gate", {})
    require(gate.get("single_use_dispatcher_implemented") is True and
            gate.get("preprocess_o_excl_flush_fsync_consumption") is True and
            gate.get("failure_permanently_consumed_and_bytes_preserved") is True and
            gate.get("exact16_result_telemetry_summary_validation") is True and
            gate.get("compatible_capture_producer_bound") is False and
            gate.get("authorization_may_not_be_issued") is True,
            "v2 producer fail-closed gate mismatch")
    frozen = ready["frozen_bindings"]
    mapping = {"plan_sha256": "plan", "machine_report_sha256": "machine_report",
               "cpu_preflight_report_sha256": "cpu_preflight_report",
               "source_binding_manifest_sha256": "source_binding_manifest",
               "billiards_binary_sha256": "billiards_binary", "font_sha256": "font",
               "hud_token_sha256": "hud_token", "shell_token_sha256": "shell_token",
               "dispatcher_sha256": "dispatcher",
               "dispatch_validator_sha256": "dispatch_validator",
               "runner_source_sha256": "runner_source_read_only"}
    for field, key in mapping.items():
        live = Path(ready["paths"][key])
        require(live.is_file() and v1.sha(live) == frozen[field],
                "v2 live binding drift: " + field)
    require(paths["plan"].read_text().splitlines() == v1.plan_rows(),
            "v2 exact16 plan drift")
    require(sources.get("schema") ==
            "phase5_rules_decision_ui_v4_exact16_source_bindings_v2" and
            sources.get("file_count") == len(sources.get("files", [])) and
            sources.get("aggregate_sha256") == frozen["source_aggregate_sha256"] and
            v1.aggregate(sources["files"]) == sources["aggregate_sha256"],
            "v2 source manifest invalid")
    for row in sources["files"]:
        path = ROOT / row["path"]
        require(path.is_file() and v1.sha(path) == row["sha256"],
                "v2 live source drift: " + row["path"])
    require(auth.get("schema") ==
            "phase5_rules_decision_ui_v4_exact16_gpu_authorization_v2" and
            auth.get("status") == "NOT_ISSUED_INCOMPATIBLE_CAPTURE_PRODUCER_NOT_BOUND" and
            auth.get("authorized") is False and auth.get("consumed") is False and
            auth.get("case_count") == 16 and auth.get("scene_ids_in_order") == ids and
            auth.get("runner_binary_path") is None and
            auth.get("runner_binary_sha256") is None and
            auth.get("exact16_capture_producer_contract") is False and
            all(auth.get(field) is False for field in claims),
            "v2 auth template must remain non-executable")
    require(auth.get("ready_manifest_sha256") == v1.sha(paths["ready"]),
            "v2 auth readiness binding mismatch")
    require(receipt.get("status") ==
            "DISPATCHER_PASS_EXACT16_NOT_RUN_PRODUCER_NOT_BOUND_NO_AUTH" and
            receipt.get("readiness_sha256") == v1.sha(paths["ready"]) and
            receipt.get("authorization_template_sha256") == v1.sha(paths["auth"]) and
            receipt.get("claims") == claims, "v2 receipt mismatch")
    return {"status": "PASS_PHASE5_V4_EXACT16_DISPATCH_READY_PRODUCER_BLOCKED_NO_GPU_NO_AUTH",
            "case_count": 16, "readiness_sha256": v1.sha(paths["ready"]),
            "authorization_may_not_be_issued": True,
            "compatible_capture_producer_bound": False}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path, nargs="?", default=prep.DEFAULT_OUTPUT)
    try:
        print(json.dumps(validate(parser.parse_args().root), sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
