import argparse
import hashlib
import json
import math
import re
from pathlib import Path


CANDIDATE_ID = "phase3_integrated_v5"
CROSS = "cross_2016_newtons_cradle"
HAN = "han_2005"
PACKAGES = {CROSS, HAN}
PASSING_RESULTS = {"PASSED", "PASSED_OR_ACCOUNTED"}
VALID_RESULTS = PASSING_RESULTS | {"FAILED"}
HEX64 = re.compile(r"[0-9a-f]{64}")


def _canonical(document):
    return json.dumps(
        document, ensure_ascii=False, indent=2, sort_keys=True,
        allow_nan=False) + "\n"


def _sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def _load(path, label):
    try:
        document = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ValueError(f"{label} is absent or unreadable") from error
    if not isinstance(document, dict):
        raise ValueError(f"{label} is invalid")
    return document


def _tree_digest(directory):
    directory = Path(directory)
    entries = []
    for path in sorted(item for item in directory.rglob("*") if item.is_file()):
        entries.append(
            f"{path.relative_to(directory).as_posix()}\0{_sha256(path)}\n")
    return hashlib.sha256("".join(entries).encode("utf-8")).hexdigest()


def _artifact(root, relative):
    path = root / relative
    if not path.is_file():
        raise ValueError(f"engineering evidence is absent: {relative}")
    return {"path": relative, "sha256": _sha256(path)}


def validate_transaction_order(ledger):
    if not isinstance(ledger, dict) or \
            set(ledger) != {"attempts", "records", "schema_version"} or \
            ledger.get("schema_version") != 1 or \
            not isinstance(ledger.get("attempts"), list) or \
            not isinstance(ledger.get("records"), list) or \
            any(not isinstance(item, dict) for item in
                ledger.get("attempts", []) + ledger.get("records", [])):
        raise ValueError("confirmation ledger is invalid")
    attempts = ledger["attempts"]
    records = ledger["records"]
    attempt_ids = [item.get("attempt_id") for item in attempts]
    record_ids = [item.get("attempt_id") for item in records]
    if len(attempt_ids) != len(set(attempt_ids)) or \
            len(record_ids) != len(set(record_ids)) or \
            set(attempt_ids) != set(record_ids):
        raise ValueError("confirmation attempt identities are not distinct and final")
    if any(item.get("dataset_id") not in PACKAGES
           for item in attempts + records):
        raise ValueError("confirmation ledger contains an unexpected package")
    by_package = {}
    for package in PACKAGES:
        package_records = [item for item in records
                           if item.get("dataset_id") == package]
        if len(package_records) > 1:
            raise ValueError(f"{package} confirmation was attempted more than once")
        if package_records:
            record = package_records[0]
            matches = [item for item in attempts
                       if item.get("attempt_id") == record.get("attempt_id")]
            if len(matches) != 1 or matches[0].get("state") != "STARTED" or \
                    record.get("result") not in VALID_RESULTS:
                raise ValueError(f"{package} transaction state is invalid")
            by_package[package] = (matches[0], record)
    if CROSS not in by_package:
        raise ValueError("Cross confirmation is absent")
    cross_passed = by_package[CROSS][1]["result"] in PASSING_RESULTS
    if cross_passed and HAN not in by_package:
        raise ValueError("Han confirmation is absent")
    if not cross_passed and HAN in by_package:
        raise ValueError("Han must be absent after failed Cross confirmation")
    return by_package


def _validate_checkpoint(root, candidate):
    freeze_path = candidate / "freeze.json"
    readiness_path = candidate / "confirmation_readiness.json"
    inventory_path = root / "physics_models/promotion/phase3_candidates_v5.json"
    full_game = candidate / "full_game"
    freeze = _load(freeze_path, "candidate freeze")
    readiness = _load(readiness_path, "confirmation readiness")
    inventory = _load(inventory_path, "candidate inventory")
    if freeze.get("candidate_id") != CANDIDATE_ID or \
            inventory.get("candidate_id") != CANDIDATE_ID or \
            HEX64.fullmatch(str(freeze.get("executable_sha256", ""))) is None:
        raise ValueError("candidate identity is invalid")
    builds = freeze.get("clean_build_sha256", [])
    profiles = freeze.get("clean_profile_sha256", [])
    if len(builds) != 2 or len(set(builds)) != 1 or \
            builds[0] != freeze["executable_sha256"] or \
            len(profiles) != 2 or len(set(profiles)) != 1 or \
            profiles[0] != freeze.get("canonical_profile_sha256"):
        raise ValueError("candidate clean-build evidence is invalid")
    matrix_path = full_game / "matrix_summary.json"
    matrix = _load(matrix_path, "full-game summary")
    cases = matrix.get("cases", [])
    if matrix.get("passed") is not True or len(cases) != 12 or not all(
            isinstance(case, dict) and case.get("passed") is True
            for case in cases):
        raise ValueError("full-game evidence is invalid")
    package_state = readiness.get("confirmation_packages")
    if readiness.get("candidate_id") != CANDIDATE_ID or \
            readiness.get("status") != "READY" or \
            readiness.get("failures") != [] or \
            readiness.get("freeze_sha256") != _sha256(freeze_path) or \
            readiness.get("inventory_sha256") != _sha256(inventory_path) or \
            readiness.get("full_game_tree_sha256") != _tree_digest(full_game) or \
            not isinstance(package_state, dict) or \
            set(package_state) != PACKAGES or not all(
                item.get("attempt") == "UNOPENED" and item.get("ready") is True
                for item in package_state.values()):
        raise ValueError("confirmation readiness is invalid")
    return freeze_path, freeze, readiness_path, inventory_path, matrix_path


def _validate_receipt(candidate, freeze_path, freeze, package, attempt, receipt):
    output = candidate / "confirmation" / package
    receipt_path = output / "validation_receipt.json"
    committed = _load(receipt_path, f"{package} confirmation receipt")
    if committed != receipt:
        raise ValueError(f"{package} ledger and receipt differ")
    attempt_id = receipt.get("attempt_id")
    common = {
        "attempt_id": attempt_id,
        "candidate_id": CANDIDATE_ID,
        "dataset_id": package,
        "freeze_sha256": _sha256(freeze_path),
        "package_key": package,
        "partition": "CONFIRMATION",
    }
    if HEX64.fullmatch(str(attempt_id)) is None or \
            any(receipt.get(key) != value for key, value in common.items()) or \
            any(attempt.get(key) != value for key, value in common.items()):
        raise ValueError(f"{package} transaction binding is invalid")
    files = receipt.get("files")
    if not isinstance(files, dict) or not files:
        raise ValueError(f"{package} receipt has no governed outputs")
    for relative, digest in files.items():
        relative_path = Path(relative)
        path = output / relative_path
        if not isinstance(relative, str) or relative_path.is_absolute() or \
                ".." in relative_path.parts or \
                HEX64.fullmatch(str(digest)) is None or not path.is_file() or \
                _sha256(path) != digest:
            raise ValueError(f"{package} receipt output hash mismatch: {relative}")
    provenance = [output / relative for relative in files
                  if Path(relative).parts[:1] == ("provenance",)]
    if not provenance:
        raise ValueError(f"{package} has no executable provenance")
    for path in provenance:
        document = _load(path, f"{package} executable provenance")
        expected = {
            "candidate_id": CANDIDATE_ID,
            "dataset_id": package,
            "executable_sha256": freeze["executable_sha256"],
            "freeze_sha256": _sha256(freeze_path),
            "source_revision": freeze["source_revision"],
        }
        if any(document.get(key) != value for key, value in expected.items()):
            raise ValueError(f"{package} executable provenance is invalid")
    return {
        "attempt_id": attempt_id,
        "receipt": receipt,
        "receipt_sha256": _sha256(receipt_path),
        "tree_sha256": _tree_digest(output),
    }


def _cross_root_cause(candidate):
    output = candidate / "confirmation" / CROSS
    report = _load(output / "reference_report.json", "Cross report")
    trace_path = output / "traces/cross_cue_frozen_pair.json"
    trace = json.loads(trace_path.read_text(encoding="utf-8"))
    if report.get("result") != "FAILED" or len(trace) < 2:
        raise ValueError("Cross failure evidence is invalid")
    metrics = report.get("summary_metrics", {})
    integrity = (
        "complete_frames_passed", "contact_passed",
        "deterministic_repeated_execution_passed",
        "finite_microtrace_passed", "finite_state_passed",
        "microtrace_complete_passed", "no_recontact_passed",
        "nonincreasing_total_energy_passed", "passive_microtrace_passed",
        "release_passed",
    )
    if not all(metrics.get(name) is True for name in integrity) or \
            metrics.get("stable_release_passed") is not False or \
            metrics.get("uncertainty_aware_equal_speed_passed") is not False:
        raise ValueError("Cross failure classification is not the frozen result")

    def ball_speed(frame, index):
        ball = next(item for item in frame["balls"] if item["index"] == index)
        velocity = ball["velocity_cm_s"]
        return math.sqrt(sum(velocity[axis] ** 2 for axis in "xyz"))

    previous = [ball_speed(trace[-2], index) for index in (0, 1)]
    final = [ball_speed(trace[-1], index) for index in (0, 1)]
    relative_change = max(abs(after - before) / before
                          for before, after in zip(previous, final))
    ratio = final[0] / final[1]
    limit = metrics["equal_speed_limit"]
    if relative_change <= 0.001 or abs(ratio - 1.0) > limit:
        raise ValueError("Cross derived failure mechanism is inconsistent")
    return {
        "category": "CONFIRMATION_EVALUATION_CONTRACT_INTEGRATION_FAILURE",
        "condition": (
            "two consecutive absolute speeds must change by no more than 0.001"),
        "derived_final_back_to_front_speed_ratio": ratio,
        "derived_final_frame_relative_speed_change": relative_change,
        "equal_speed_limit": limit,
        "evaluator": (
            "tools/physics_validation/cross_2016_confirmation.py::"
            "_stable_release_speeds"),
        "explanation": (
            "Production rolling resistance changes both absolute speeds "
            "between 0.1 second frames even though their ratio satisfies the "
            "Cross equal-speed limit."),
        "failed_gates": [
            "stable_release_passed",
            "uncertainty_aware_equal_speed_passed",
        ],
        "physical_integrity_gates": {name: True for name in integrity},
        "trace_sha256": _sha256(trace_path),
    }


def _engineering_evidence(root, candidate, freeze_path, freeze,
                          readiness_path, inventory_path, matrix_path):
    full_game = candidate / "full_game"
    ordinary = _load(
        root / "physics_models/promotion/phase3_v5_ordinary_equivalence.json",
        "ordinary equivalence")
    if ordinary.get("ordinary_physics_identical") is not True:
        raise ValueError("ordinary-shot equivalence is invalid")
    return {
        "alciatore_regression": {
            name: _artifact(root, path) for name, path in {
                "inputs": "physics_models/calibration/alciatore_frozen_contact_v5_inputs.csv",
                "report": "physics_models/calibration/alciatore_frozen_contact_v5_report.json",
                "residuals": "physics_models/calibration/alciatore_frozen_contact_v5_residuals.csv",
                "sensitivity": "physics_models/calibration/alciatore_frozen_contact_v5_sensitivity.csv",
            }.items()
        },
        "clean_build": {
            "canonical_profile_sha256": list(freeze["clean_profile_sha256"]),
            "executable_sha256": list(freeze["clean_build_sha256"]),
            "source_revision": freeze["source_revision"],
        },
        "convergence_contract": _artifact(
            root, "tests/coupled_cue_contact_tests.cpp"),
        "freeze": {"path": str(freeze_path.relative_to(root)),
                   "sha256": _sha256(freeze_path)},
        "full_game": {
            "case_count": 12,
            "matrix_summary_sha256": _sha256(matrix_path),
            "passed": True,
            "tree_sha256": _tree_digest(full_game),
        },
        "inventory": {"path": str(inventory_path.relative_to(root)),
                      "sha256": _sha256(inventory_path)},
        "ordinary_equivalence": {
            "baseline": ordinary["baseline"],
            "evidence": _artifact(
                root,
                "physics_models/promotion/phase3_v5_ordinary_equivalence.json"),
            "physics_identical": True,
        },
        "performance_budget": _artifact(
            root,
            "physics_models/promotion/full_game_performance_budget_v5.json"),
        "profile": _artifact(
            root, "physics_models/profiles/chinese_pool_full_game_v5.json"),
        "readiness": {"path": str(readiness_path.relative_to(root)),
                      "sha256": _sha256(readiness_path)},
    }


def build_final_assessment(root):
    root = Path(root).resolve()
    candidate = root / "physics_models/candidates" / CANDIDATE_ID
    freeze_path, freeze, readiness_path, inventory_path, matrix_path = \
        _validate_checkpoint(root, candidate)
    ledger = _load(
        candidate / "confirmation_consumption.json", "confirmation ledger")
    transactions = validate_transaction_order(ledger)
    evidence = {
        package: _validate_receipt(
            candidate, freeze_path, freeze, package, *transaction)
        for package, transaction in transactions.items()
    }
    cross_passed = evidence[CROSS]["receipt"]["result"] in PASSING_RESULTS
    han = evidence.get(HAN)
    accepted = cross_passed and han is not None and \
        han["receipt"]["result"] in PASSING_RESULTS
    assessment = {
        "candidate_id": CANDIDATE_ID,
        "confirmations": evidence,
        "disposition": "ACCEPTED" if accepted else "REJECTED",
        "engineering_evidence": _engineering_evidence(
            root, candidate, freeze_path, freeze, readiness_path,
            inventory_path, matrix_path),
        "han_2005": "EXECUTED" if han is not None else "NOT_EXECUTED",
        "policy": "two_independent_one_time_confirmations_required",
        "schema_version": 1,
    }
    if not accepted:
        assessment["root_cause"] = _cross_root_cause(candidate)
    return assessment


def build_rejection_document(root, assessment_path, assessment=None):
    root = Path(root).resolve()
    assessment_path = Path(assessment_path).resolve()
    assessment = assessment or build_final_assessment(root)
    if assessment.get("disposition") != "REJECTED":
        raise ValueError("only a rejected assessment can produce rejection evidence")
    cross = assessment["confirmations"][CROSS]
    report = cross["receipt"]["files"]["reference_report.json"]
    return {
        **assessment,
        "assessment_sha256": _sha256(assessment_path),
        "cross_2016": {
            "attempt_id": cross["attempt_id"],
            "derived_final_back_to_front_speed_ratio":
                assessment["root_cause"][
                    "derived_final_back_to_front_speed_ratio"],
            "derived_final_ratio_residual":
                assessment["root_cause"][
                    "derived_final_back_to_front_speed_ratio"] - 1.0,
            "equal_speed_limit":
                assessment["root_cause"]["equal_speed_limit"],
            "output_tree_sha256": cross["tree_sha256"],
            "receipt_sha256": cross["receipt_sha256"],
            "report_sha256": report,
            "result": cross["receipt"]["result"],
            "trace_sha256": assessment["root_cause"]["trace_sha256"],
        },
        "freeze_sha256": assessment["engineering_evidence"]["freeze"][
            "sha256"],
        "ledger_sha256": _sha256(
            root / "physics_models/candidates/phase3_integrated_v5/"
            "confirmation_consumption.json"),
        "readiness_sha256": assessment["engineering_evidence"][
            "readiness"]["sha256"],
    }


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Build the immutable Phase 3 v5 final assessment")
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--rejection-output", type=Path)
    arguments = parser.parse_args(argv)
    assessment = build_final_assessment(arguments.root)
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_text(_canonical(assessment), encoding="utf-8")
    if assessment["disposition"] == "REJECTED" and \
            arguments.rejection_output is not None:
        rejection = build_rejection_document(
            arguments.root, arguments.output, assessment)
        arguments.rejection_output.parent.mkdir(parents=True, exist_ok=True)
        arguments.rejection_output.write_text(
            _canonical(rejection), encoding="utf-8")
    return 0 if assessment["disposition"] == "ACCEPTED" else 1


if __name__ == "__main__":
    raise SystemExit(main())
