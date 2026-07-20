import argparse
import csv
import hashlib
import json
import subprocess
from pathlib import Path

from .confirmation_transaction import (
    confirmation_declaration,
    validate_confirmation_access_from_freeze,
)


_CANDIDATE_CONTRACTS = {
    "phase3_integrated_v2": {
        "profile": "physics_models/profiles/chinese_pool_full_game_v2.json",
        "packages": {"derby_fuller_1999", "sudo_2002"},
    },
    "phase3_integrated_v3": {
        "profile": "physics_models/profiles/chinese_pool_full_game_v3.json",
        "packages": {"derby_fuller_1999", "han_2005"},
    },
    "phase3_integrated_v4": {
        "profile": "physics_models/profiles/chinese_pool_full_game_v4.json",
        "packages": {"alciatore_2005_tp_a15", "han_2005"},
    },
    "phase3_integrated_v5": {
        "profile": "physics_models/profiles/chinese_pool_full_game_v5.json",
        "packages": {"cross_2016_newtons_cradle", "han_2005"},
    },
}


def _sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def _canonical(document):
    return (json.dumps(document, ensure_ascii=False, indent=2, sort_keys=True,
                       allow_nan=False) + "\n")


def _record(checks, failures, name, condition, failure):
    checks[name] = bool(condition)
    if not condition:
        failures.append(failure)


def _tree_digest(directory):
    directory = Path(directory)
    entries = []
    for path in sorted(item for item in directory.rglob("*") if item.is_file()):
        entries.append(
            f"{path.relative_to(directory).as_posix()}\0{_sha256(path)}\n")
    return hashlib.sha256("".join(entries).encode("utf-8")).hexdigest()


def validate_contract_proof(proof, executable_sha256):
    try:
        if isinstance(proof, dict):
            document = proof
        elif isinstance(proof, bytes):
            document = json.loads(proof)
        else:
            document = json.loads(Path(proof).read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError, TypeError) as error:
        return [f"confirmation contract proof is unreadable: {error}"]
    failures = []
    if document.get("schema_version") != 1 or \
            document.get("dataset_id") != "fixture_confirmation" or \
            document.get("result") != "PASSED" or \
            document.get("parse_succeeded") is not True:
        failures.append("confirmation contract proof result is invalid")
    if not isinstance(document.get("frames"), int) or \
            document.get("frames", 0) <= 0:
        failures.append("confirmation contract proof has no frames")
    if document.get("executable_sha256") != executable_sha256:
        failures.append(
            "confirmation contract proof executable does not match freeze")
    for field in (
            "fixture_manifest_sha256", "scenario_sha256",
            "first_trace_sha256", "second_trace_sha256",
            "protocol_transcript_sha256"):
        value = document.get(field)
        if not isinstance(value, str) or len(value) != 64 or any(
                character not in "0123456789abcdef" for character in value):
            failures.append(
                f"confirmation contract proof {field} is invalid")
    if document.get("first_trace_sha256") != \
            document.get("second_trace_sha256"):
        failures.append("confirmation contract proof traces differ")
    if document.get("return_code") != 0:
        failures.append("confirmation contract proof process did not exit cleanly")
    if document.get("stderr") != "":
        failures.append("confirmation contract proof stderr is not empty")
    return failures


def _spent_fit_inputs(root):
    result = []
    for relative in (
            "physics_models/calibration/ball_collision_fit_v3_inputs.csv",
            "physics_models/calibration/cushion_fit_v3_inputs.csv"):
        with (root / relative).open(encoding="utf-8", newline="") as stream:
            result.extend(dict(row) for row in csv.DictReader(stream))
    return result


def _inventory_artifacts(inventory):
    artifacts = [
        inventory.get("profile"), inventory.get("full_game_matrix"),
        inventory.get("performance_budget"),
        *inventory.get("calibration_reports", []),
        *inventory.get("confirmation_packages", []),
        *inventory.get("metric_contracts", []),
    ]
    return [item for item in artifacts if isinstance(item, dict)]


def build_readiness(root, freeze, inventory, full_game, contract_proof=None):
    root = Path(root).resolve()
    freeze_path = Path(freeze).resolve()
    inventory_path = Path(inventory).resolve()
    full_game = Path(full_game).resolve()
    freeze_document = json.loads(freeze_path.read_text(encoding="utf-8"))
    inventory_document = json.loads(
        inventory_path.read_text(encoding="utf-8"))
    matrix_summary_path = full_game / "matrix_summary.json"
    matrix_summary = json.loads(
        matrix_summary_path.read_text(encoding="utf-8"))
    checks = {}
    failures = []
    candidate_id = freeze_document.get("candidate_id")
    candidate_contract = _CANDIDATE_CONTRACTS.get(candidate_id)

    _record(checks, failures, "single_candidate_identity",
            isinstance(candidate_id, str) and bool(candidate_id) and
            inventory_document.get("candidate_id") == candidate_id and
            freeze_path.parent.name == candidate_id,
            "freeze, inventory, and candidate directory identities differ")
    _record(checks, failures, "candidate_contract_admitted",
            candidate_contract is not None and
            inventory_document.get("profile", {}).get("path") ==
            candidate_contract.get("profile"),
            "candidate or profile is not admitted by the exact readiness contract")
    inventory_artifacts = _inventory_artifacts(inventory_document)
    frozen_artifacts = freeze_document.get("artifacts", [])
    inventory_bindings = {
        (item.get("path"), item.get("role"), item.get("sha256"))
        for item in inventory_artifacts}
    frozen_bindings = {
        (item.get("path"), item.get("role"), item.get("sha256"))
        for item in frozen_artifacts if isinstance(item, dict)}
    _record(checks, failures, "complete_inventory_bound",
            len(inventory_bindings) == len(inventory_artifacts) and
            inventory_bindings == frozen_bindings,
            "freeze does not bind the exact pre-freeze inventory")
    build_hashes = freeze_document.get("clean_build_sha256", [])
    profile_hashes = freeze_document.get("clean_profile_sha256", [])
    _record(checks, failures, "reproducible_clean_build",
            len(build_hashes) == 2 and len(set(build_hashes)) == 1 and
            build_hashes[0] == freeze_document.get("executable_sha256"),
            "freeze clean-build evidence is not reproducible")
    _record(checks, failures, "reproducible_runtime_profile",
            len(profile_hashes) == 2 and len(set(profile_hashes)) == 1 and
            profile_hashes[0] == freeze_document.get(
                "canonical_profile_sha256"),
            "freeze profile evidence is not reproducible")
    ancestry = subprocess.run(
        ["git", "merge-base", "--is-ancestor",
         freeze_document.get("source_revision", ""), "HEAD"],
        cwd=root, capture_output=True).returncode == 0
    _record(checks, failures, "frozen_source_is_ancestor", ancestry,
            "frozen source is not an ancestor of the checkpoint")

    cases = matrix_summary.get("cases", [])
    _record(checks, failures, "full_game_matrix_passed",
            matrix_summary.get("passed") is True and len(cases) == 12 and
            all(case.get("passed") is True for case in cases),
            "the complete 12-case frozen full-game matrix did not pass")

    inventory_package_rows = inventory_document.get(
        "confirmation_packages", [])
    inventory_packages = {
        item.get("package_id"): item
        for item in inventory_package_rows if isinstance(item, dict)}
    package_keys = set(inventory_packages)
    expected_packages = (
        candidate_contract.get("packages", set())
        if candidate_contract is not None else set())
    fit_rows = _spent_fit_inputs(root)
    _record(checks, failures, "fit_inputs_are_spent_only",
            bool(fit_rows) and
            {row.get("lifecycle") for row in fit_rows} == {"spent"} and
            not package_keys.intersection(
                row.get("dataset_id") for row in fit_rows),
            "v3 fit inputs are not isolated to spent evidence")

    _record(checks, failures, "confirmation_packages_preregistered",
            len(inventory_package_rows) == 2 and len(package_keys) == 2 and
            package_keys == expected_packages and
            None not in package_keys and all(
                isinstance(item, dict) and
                item.get("partition") == "confirmation" and
                item.get("role") == "confirmation_package_manifest"
                for item in inventory_package_rows),
            "inventory must preregister exactly two confirmation packages")
    ledger = freeze_path.parent / "confirmation_consumption.json"
    output_root = freeze_path.parent / "confirmation"
    _record(checks, failures, "confirmation_state_absent",
            not ledger.exists() and not output_root.exists(),
            "a confirmation attempt or output already exists")

    package_state = {}
    for package_key in sorted(package_keys):
        try:
            declaration = confirmation_declaration(
                root, freeze_path, package_key)
            access_failures = validate_confirmation_access_from_freeze(
                root, freeze_path, package_key, ledger)
            inventory_entry = inventory_packages.get(package_key, {})
            declared = (
                inventory_entry.get("path") ==
                declaration.manifest_relative_path and
                inventory_entry.get("sha256") == declaration.manifest_sha256)
            if not declared:
                failures.append(
                    f"{package_key} inventory and freeze declarations differ")
            failures.extend(
                f"{package_key}: {failure}" for failure in access_failures)
            package_state[package_key] = {
                "attempt": "UNOPENED",
                "manifest_path": declaration.manifest_relative_path,
                "manifest_sha256": declaration.manifest_sha256,
                "ready": declared and not access_failures,
            }
        except Exception as error:
            failures.append(f"{package_key}: {error}")
            package_state[package_key] = {
                "attempt": "UNOPENED", "ready": False}
    checks["confirmation_access_ready"] = (
        len(package_state) == 2 and
        all(item.get("ready") for item in package_state.values()))

    proof_sha256 = None
    if contract_proof is not None:
        proof_failures = validate_contract_proof(
            contract_proof, freeze_document.get("executable_sha256"))
        checks["confirmation_contract_real_path"] = not proof_failures
        failures.extend(proof_failures)
        if not isinstance(contract_proof, (dict, bytes)):
            proof_sha256 = _sha256(contract_proof)

    failures = sorted(set(failures))
    document = {
        "candidate_id": candidate_id,
        "checks": checks,
        "confirmation_packages": package_state,
        "failures": failures,
        "freeze_sha256": _sha256(freeze_path),
        "full_game_case_count": len(cases),
        "full_game_matrix_summary_sha256": _sha256(matrix_summary_path),
        "full_game_tree_sha256": _tree_digest(full_game),
        "inventory_sha256": _sha256(inventory_path),
        "schema_version": 1,
        "source_revision": freeze_document.get("source_revision"),
        "status": "READY" if not failures else "BLOCKED",
    }
    if proof_sha256 is not None:
        document["confirmation_contract_proof_sha256"] = proof_sha256
    return document


def build_rejection(root, freeze, readiness):
    root = Path(root).resolve()
    freeze_path = Path(freeze).resolve()
    readiness_path = Path(readiness).resolve()
    candidate = freeze_path.parent
    ledger_path = candidate / "confirmation_consumption.json"
    receipt_path = candidate / (
        "confirmation/derby_fuller_1999/validation_receipt.json")
    failure_path = candidate / "confirmation/derby_fuller_1999/failure.json"
    ledger = json.loads(ledger_path.read_text(encoding="utf-8"))
    receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    failure = json.loads(failure_path.read_text(encoding="utf-8"))
    if receipt.get("result") != "FAILED" or \
            receipt.get("failure_code") != "FAILED_EVALUATOR_EXCEPTION":
        raise ValueError("Derby receipt is not the expected immutable failure")
    if not any(record.get("attempt_id") == receipt.get("attempt_id")
               for record in ledger.get("records", [])):
        raise ValueError("Derby failure is not finalized in the ledger")
    if failure.get("message") != \
            "invalid_scenario: expectations must be a nonempty array":
        raise ValueError("Derby failure does not match the diagnosed contract bug")
    if any(record.get("dataset_id") == "han_2005" for record in
           ledger.get("attempts", []) + ledger.get("records", [])):
        raise ValueError("Han must remain unopened after Derby failure")
    return {
        "candidate_id": receipt["candidate_id"],
        "derby_attempt_id": receipt["attempt_id"],
        "derby_failure_code": receipt["failure_code"],
        "derby_failure_sha256": _sha256(failure_path),
        "derby_receipt_sha256": _sha256(receipt_path),
        "disposition": "REJECTED",
        "freeze_sha256": _sha256(freeze_path),
        "han_2005": "NOT_EXECUTED",
        "ledger_sha256": _sha256(ledger_path),
        "policy": "failed_confirmation_is_consumed_and_never_replayed",
        "readiness_sha256": _sha256(readiness_path),
        "root_cause": {
            "category": "SCENARIO_CONTRACT_INTEGRATION_FAILURE",
            "generator": "tools/physics_validation/confirmation_run.py::_scenario",
            "invalid_value": "expectations=[]",
            "parser_contract": "src/Billiards/physics_scenario.cpp",
            "message": failure["message"],
        },
        "schema_version": 1,
    }


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Build a non-consuming Phase 3 confirmation checkpoint")
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--freeze", required=True, type=Path)
    parser.add_argument("--inventory", required=True, type=Path)
    parser.add_argument("--full-game", required=True, type=Path)
    parser.add_argument("--contract-proof", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    arguments = parser.parse_args(argv)
    document = build_readiness(
        arguments.root, arguments.freeze, arguments.inventory,
        arguments.full_game, arguments.contract_proof)
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_text(_canonical(document), encoding="utf-8")
    return 0 if document["status"] == "READY" else 1


if __name__ == "__main__":
    raise SystemExit(main())
