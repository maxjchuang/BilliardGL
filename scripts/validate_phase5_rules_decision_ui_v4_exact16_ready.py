#!/usr/bin/env python3
"""Read-only validator for Phase5 Rules UI v4 exact16 readiness."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts import prepare_phase5_rules_decision_ui_v4_exact16_ready as prep
from scripts import validate_phase5_rules_decision_ui_v4_exact_shape_machine as machine_validator


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def load(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(value, dict), str(path) + ": object required")
    return value


def validate(root: Path = prep.READY_ROOT) -> dict[str, Any]:
    root = root.resolve(strict=True)
    paths = {
        "plan": root / "rules-decision-ui-v4-exact16-plan.tsv",
        "machine": root / "rules-decision-ui-v4-machine.json",
        "cpu": root / "rules-decision-ui-v4-exact16-cpu-preflight.json",
        "sources": root / "rules-decision-ui-v4-source-bindings.json",
        "readiness": root / "rules-decision-ui-v4-exact16-ready.json",
        "authorization": root / "rules-decision-ui-v4-authorization-template-not-issued.json",
        "receipt": root / "offline-readiness-receipt.json",
    }
    require(all(path.is_file() for path in paths.values()),
            "v4 readiness artifact set incomplete")
    require(not any(root.rglob("*.ppm")) and not any(root.rglob("*.png")) and
            not any(root.rglob("*.consumed*")),
            "v4 readiness contains GPU/consumption evidence")
    ready = load(paths["readiness"])
    auth = load(paths["authorization"])
    receipt = load(paths["receipt"])
    ids = prep.case_ids()
    claims = {"gpu_authorized": False, "gpu_executed": False,
              "technical_art_review": False, "product_exposed": False,
              "golden_promotion": False, "m2_promotion": False}
    require(ready.get("schema") ==
            "phase5_rules_decision_ui_v4_exact16_readiness_v1" and
            ready.get("status") ==
            "V4_EXACT16_NOT_RUN_GPU_AUTHORIZATION_NOT_ISSUED" and
            ready.get("case_count") == 16 and
            ready.get("repair_case_count") == 4 and
            ready.get("regression_control_count") == 12 and
            ready.get("case_ids_in_order") == ids and
            ready.get("machine_matrix_passed") == 540 and
            ready.get("machine_matrix_byte_reproducible") is True and
            ready.get("cpu_preflight_passed") == 16 and
            ready.get("bad_input_passed") == 5 and
            ready.get("product_integration_passed") is True and
            ready.get("claims") == claims,
            "v4 readiness identity/count/boundary mismatch")
    require(ready.get("case_results") == [
        {"ordinal": index + 1, "capture_id": case_id, "status": "NOT_RUN"}
        for index, case_id in enumerate(ids)], "v4 exact16 NOT_RUN rows mismatch")
    dispatch = ready.get("future_dispatch_gate", {})
    require(dispatch.get("implemented_by_this_readiness") is False and
            dispatch.get("authorization_may_not_be_issued_until_exact16_single_use_dispatch_exists") is True and
            dispatch.get("output_root_must_be_fresh") is True,
            "v4 future dispatch fail-closed gate mismatch")
    live_paths = {key: Path(value) for key, value in ready.get("paths", {}).items()}
    require(all(path.is_file() for path in live_paths.values()),
            "v4 readiness live path missing")
    frozen = ready.get("frozen_bindings", {})
    mapping = {
        "plan_sha256": "plan", "machine_report_sha256": "machine_report",
        "source_binding_manifest_sha256": "source_binding_manifest",
        "billiards_binary_sha256": "billiards_binary",
        "cpu_preflight_binary_sha256": "cpu_preflight_binary",
        "cpu_preflight_report_sha256": "cpu_preflight_report",
        "product_integration_test_binary_sha256": "product_integration_test_binary",
        "font_sha256": "font", "hud_token_sha256": "hud_token",
        "shell_token_sha256": "shell_token",
    }
    for field, key in mapping.items():
        require(frozen.get(field) == prep.sha(live_paths[key]),
                "v4 frozen binding mismatch: " + field)
    require(frozen.get("gate_sha256") == prep.machine.GATE_SHA and
            prep.sha(prep.machine.GATE) == prep.machine.GATE_SHA,
            "v4 gate binding mismatch")
    require(paths["plan"].read_text(encoding="utf-8").splitlines() ==
            prep.plan_rows(), "v4 exact16 plan mismatch")
    machine_validator.validate(load(paths["machine"]))
    cpu = load(paths["cpu"])
    require(cpu.get("schema") ==
            "phase5_rules_decision_ui_v4_exact16_cpu_frame_preflight_v1" and
            cpu.get("status") == "CPU_FRAME_PREFLIGHT_PASS_16_OF_16_NO_GPU" and
            cpu.get("case_count") == cpu.get("passed_count") == 16 and
            cpu.get("bad_input_count") == cpu.get("bad_input_passed_count") == 5 and
            all(cpu.get(field) is False for field in (
                "actual_gpu_invoked", "ns_application_invoked",
                "opengl_invoked", "gpu_output_created")),
            "v4 CPU preflight mismatch")
    sources = load(paths["sources"])
    require(sources.get("schema") ==
            "phase5_rules_decision_ui_v4_exact16_source_bindings_v1" and
            sources.get("gate_sha256") == prep.machine.GATE_SHA and
            sources.get("file_count") == len(sources.get("files", [])) and
            sources.get("aggregate_sha256") == frozen.get("source_aggregate_sha256"),
            "v4 source binding manifest mismatch")
    require(prep.aggregate(sources["files"]) == sources["aggregate_sha256"],
            "v4 source aggregate mismatch")
    for row in sources["files"]:
        path = prep.ROOT / row["path"]
        require(path.is_file() and prep.sha(path) == row["sha256"],
                "v4 live source drift: " + row["path"])
    require(auth.get("schema") ==
            "phase5_rules_decision_ui_v4_exact16_gpu_authorization_v1" and
            auth.get("status") == "NOT_ISSUED" and
            auth.get("authorized") is False and auth.get("consumed") is False and
            auth.get("case_count") == 16 and
            auth.get("scene_ids_in_order") == ids and
            auth.get("exact_shape_gpu_authorized") is False and
            auth.get("single_use_dispatch_implemented") is False and
            auth.get("ready_manifest_sha256") == prep.sha(paths["readiness"]) and
            all(auth.get(field) is False for field in claims),
            "v4 authorization template must remain non-executable")
    for field, digest in frozen.items():
        require(auth.get(field) == digest,
                "v4 auth frozen binding mismatch: " + field)
    require(receipt == {
        "schema": "phase5_rules_decision_ui_v4_exact16_offline_readiness_v1",
        "status": "CPU_16_MACHINE_540_PRODUCT_INTEGRATION_PASS_GPU_NOT_AUTHORIZED_NOT_RUN",
        "ready_manifest_sha256": prep.sha(paths["readiness"]),
        "authorization_template_sha256": prep.sha(paths["authorization"]),
        "source_binding_manifest_sha256": prep.sha(paths["sources"]),
        "claims": claims,
    }, "v4 readiness receipt mismatch")
    return {"status": "PASS_PHASE5_RULES_UI_V4_EXACT16_READY_NO_GPU_NO_AUTH",
            "readiness_sha256": prep.sha(paths["readiness"]),
            "case_count": 16, "repair_case_count": 4,
            "control_case_count": 12, "gpu_authorized": False,
            "gpu_executed": False}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path, nargs="?", default=prep.READY_ROOT)
    arguments = parser.parse_args()
    try:
        print(json.dumps(validate(arguments.root), sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print("FAIL: " + str(error))
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
