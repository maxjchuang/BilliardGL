#!/usr/bin/env python3
"""NO-GPU static validator for the exact16 capture-only producer implementation."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GATE = ROOT / "docs/art/phase5-rules-decision-ui-v4-exact16-capture-producer-contract-correction-gate-v1.json"
PLAN = ROOT / "docs/evidence/phase5/rules-decision-ui-v4-exact16-ready-v2/rules-decision-ui-v4-exact16-plan.tsv"
RUNNER = ROOT / "spikes/render_core_profile/phase5_rules_decision_ui_gpu_capture_runner.mm"
CMAKE = ROOT / "CMakeLists.txt"
DISPATCHER = ROOT / "scripts/run_phase5_rules_decision_ui_v4_exact16_capture_dispatch.py"
EXACT16 = json.loads(GATE.read_text())["exact16_case_ids_in_order"]


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(value: bool, message: str) -> None:
    if not value:
        raise RuntimeError(message)


def validate_plan_lines(lines: list[str]) -> None:
    fields = [line.split("\t") for line in lines if line]
    require(len(fields) == 16, "exact16 plan count")
    require(all(len(row) == 12 for row in fields), "exact16 plan field count")
    ids = [row[0] for row in fields]
    require(ids == EXACT16 and len(set(ids)) == 16, "exact16 plan ID/order/duplicate")


def validate() -> dict:
    require(sha(GATE) ==
            "dbab785638be91550e25380141a62819c28ea72da9082eb23c02c5e4869866ad",
            "producer gate drift")
    validate_plan_lines(PLAN.read_text().splitlines())
    runner = RUNNER.read_text()
    cmake = CMAKE.read_text()
    dispatcher = DISPATCHER.read_text()
    require("#if defined(BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16)" in runner and
            '#error "Phase5 Rules UI capture runner modes are mutually exclusive"' in runner,
            "compile mode exclusivity missing")
    for token in (
            "phase5_rules_ui_v4_plan_requires_exactly_16_cases",
            "phase5_rules_ui_v4_plan_id_or_order_mismatch",
            "phase5_rules_decision_ui_v4_exact16_gpu_authorization_v2",
            "AUTHORIZED_FOR_EXACTLY_16_SINGLE_USE_CAPTURES",
            "exact16_capture_producer_contract", "ready_manifest_sha256",
            "dispatcher_sha256", "runner_source_sha256", "runner_binary_sha256",
            "plan_sha256", "source_binding_manifest_sha256",
            "source_aggregate_sha256", "billiards_binary_sha256", "font_sha256",
            "hud_token_sha256", "shell_token_sha256",
            "authorized_output_canonical_path", "scene_ids_in_order",
            "phase5_rules_ui_v4_dispatch_consumption_marker_mismatch",
            "phase5_rules_ui_v4_runner_invocation_already_claimed",
            "phase5_rules_ui_v4_runner_claim_durability_failed",
            "rules-decision-ui-v4-exact16-telemetry.jsonl",
            "EVIDENCE_CAPTURED 16/16"):
        require(token in runner, "runner exact16 contract missing: " + token)
    require("phase5_rules_ui_v3_plan_requires_exactly_28_cases" in runner and
            "phase5_rules_ui_plan_requires_60_cases" in runner and
            "EVIDENCE_CAPTURED 60/60" in runner,
            "v3/default regression contract drift")
    target = "BilliardsPhase5RulesDecisionUiV4Exact16GpuCaptureRunner"
    require(cmake.count("add_executable(" + target) == 1 and
            cmake.count("BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16=1") >= 2 and
            "BILLIARDGL_PHASE5_RULES_UI_V3_TARGETED=1" in cmake,
            "distinct exact16 CMake target/definition missing")
    require("str(ready_path), str(Path(__file__).resolve())" in dispatcher and
            dispatcher.index("validate_auth(") < dispatcher.index("consume(") <
            dispatcher.index("subprocess.run(command"),
            "dispatcher producer binding/consume/process order drift")
    forbidden = ("src/Billiards/render/ui/rules_decision_product_draw.cpp",
                 "src/Billiards/render/ui/rules_decision_ui_draw_list.cpp",
                 "src/Billiards/render/modern/modern_renderer.cpp")
    gate = json.loads(GATE.read_text())
    require(all(path not in [item.get("allowed_path") for item in
                             gate["allowed_modification_surface"]]
                for path in forbidden), "forbidden product/raster surface expanded")
    return {"status": "PASS_CAPTURE_PRODUCER_CONTRACT_IMPLEMENTATION_ONLY",
            "runner_source_sha256": sha(RUNNER), "cmake_sha256": sha(CMAKE),
            "dispatcher_sha256": sha(DISPATCHER), "case_count": 16,
            "gpu_executed": False, "authorization_issued": False}


if __name__ == "__main__":
    try:
        print(json.dumps(validate(), sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print("FAIL: " + str(error))
        raise SystemExit(2)
