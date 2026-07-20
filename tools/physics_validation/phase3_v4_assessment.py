import argparse
import hashlib
import json
import re
from pathlib import Path


CANDIDATE_ID = "phase3_integrated_v4"
ALCIATORE = "alciatore_2005_tp_a15"
HAN = "han_2005"
PACKAGES = (ALCIATORE, HAN)
PASSING_RESULTS = {"PASSED", "PASSED_OR_ACCOUNTED"}
VALID_RESULTS = PASSING_RESULTS | {"FAILED"}
HEX64 = re.compile(r"[0-9a-f]{64}")


def _canonical(document):
    return (json.dumps(document, ensure_ascii=False, indent=2, sort_keys=True,
                       allow_nan=False) + "\n")


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


def _receipt_result(receipt):
    return receipt.get("result") in PASSING_RESULTS


def _validate_checkpoint(candidate):
    freeze_path = candidate / "freeze.json"
    readiness_path = candidate / "confirmation_readiness.json"
    full_game = candidate / "full_game"
    freeze = _load(freeze_path, "candidate freeze")
    readiness = _load(readiness_path, "confirmation readiness")
    if freeze.get("candidate_id") != CANDIDATE_ID or \
            freeze.get("schema_version") not in {2, 3} or \
            HEX64.fullmatch(str(freeze.get("executable_sha256", ""))) is None:
        raise ValueError("candidate freeze is invalid")
    matrix = _load(full_game / "matrix_summary.json", "full-game summary")
    cases = matrix.get("cases", [])
    if matrix.get("passed") is not True or len(cases) != 12 or not all(
            isinstance(case, dict) and case.get("passed") is True
            for case in cases):
        raise ValueError("full-game evidence is invalid")
    if readiness.get("candidate_id") != CANDIDATE_ID or \
            readiness.get("status") != "READY" or \
            readiness.get("failures") != [] or \
            readiness.get("freeze_sha256") != _sha256(freeze_path) or \
            readiness.get("full_game_tree_sha256") != _tree_digest(full_game):
        raise ValueError("confirmation readiness is invalid")
    package_state = readiness.get("confirmation_packages")
    if not isinstance(package_state, dict) or set(package_state) != set(PACKAGES) \
            or not all(isinstance(value, dict) and
                       value.get("attempt") == "UNOPENED" and
                       value.get("ready") is True
                       for value in package_state.values()):
        raise ValueError("confirmation readiness packages are invalid")
    return freeze_path, freeze, readiness_path


def _validate_receipt(candidate, freeze_path, freeze, package, receipt, attempt):
    output = candidate / "confirmation" / package
    receipt_path = output / "validation_receipt.json"
    committed = _load(receipt_path, f"{package} confirmation receipt")
    if committed != receipt:
        raise ValueError(f"{package} ledger and receipt differ")
    attempt_id = receipt.get("attempt_id")
    if not isinstance(attempt_id, str) or HEX64.fullmatch(attempt_id) is None:
        raise ValueError(f"{package} attempt identity is invalid")
    common = {
        "attempt_id": attempt_id,
        "candidate_id": CANDIDATE_ID,
        "dataset_id": package,
        "freeze_sha256": _sha256(freeze_path),
        "package_key": package,
        "partition": "CONFIRMATION",
    }
    if any(receipt.get(key) != value for key, value in common.items()) or \
            any(attempt.get(key) != value for key, value in common.items()):
        raise ValueError(f"{package} transaction binding is invalid")
    if attempt.get("state") != "STARTED" or \
            receipt.get("result") not in VALID_RESULTS:
        raise ValueError(f"{package} transaction state is invalid")
    files = receipt.get("files")
    if not isinstance(files, dict) or not files:
        raise ValueError(f"{package} receipt has no governed outputs")
    for relative, expected in files.items():
        relative_path = Path(relative)
        if not isinstance(relative, str) or relative_path.is_absolute() or \
                ".." in relative_path.parts or \
                HEX64.fullmatch(str(expected)) is None:
            raise ValueError(f"{package} receipt output declaration is invalid")
        path = output / relative_path
        if not path.is_file() or _sha256(path) != expected:
            raise ValueError(f"{package} receipt output hash mismatch: {relative}")
    provenance_paths = [
        output / relative for relative in files
        if Path(relative).parts[:1] == ("provenance",) and
        Path(relative).suffix == ".json"
    ]
    if not provenance_paths:
        raise ValueError(f"{package} has no executable provenance")
    for path in provenance_paths:
        provenance = _load(path, f"{package} executable provenance")
        expected_provenance = {
            "candidate_id": CANDIDATE_ID,
            "dataset_id": package,
            "executable_sha256": freeze["executable_sha256"],
            "freeze_sha256": _sha256(freeze_path),
            "source_revision": freeze["source_revision"],
        }
        if any(provenance.get(key) != value
               for key, value in expected_provenance.items()):
            raise ValueError(f"{package} executable provenance is invalid")
    return {
        "attempt_id": attempt_id,
        "receipt": receipt,
        "receipt_sha256": _sha256(receipt_path),
        "tree_sha256": _tree_digest(output),
    }


def validated_confirmation_evidence(root):
    root = Path(root).resolve()
    candidate = root / "physics_models/candidates" / CANDIDATE_ID
    freeze_path, freeze, _ = _validate_checkpoint(candidate)
    ledger_path = candidate / "confirmation_consumption.json"
    if not ledger_path.is_file():
        raise ValueError("Alciatore confirmation is absent")
    ledger = _load(ledger_path, "confirmation ledger")
    if set(ledger) != {"attempts", "records", "schema_version"} or \
            ledger.get("schema_version") != 1 or \
            not isinstance(ledger.get("attempts"), list) or \
            not isinstance(ledger.get("records"), list):
        raise ValueError("confirmation ledger is invalid")
    attempts = ledger["attempts"]
    records = ledger["records"]
    if any(not isinstance(item, dict) for item in attempts + records):
        raise ValueError("confirmation ledger entries are invalid")
    attempt_ids = [item.get("attempt_id") for item in attempts]
    record_ids = [item.get("attempt_id") for item in records]
    if len(attempt_ids) != len(set(attempt_ids)) or \
            len(record_ids) != len(set(record_ids)) or \
            set(attempt_ids) != set(record_ids):
        raise ValueError("confirmation attempt identities are not distinct and final")
    if any(item.get("dataset_id") not in PACKAGES
           for item in attempts + records):
        raise ValueError("confirmation ledger contains an unexpected package")

    evidence = {}
    for package in PACKAGES:
        package_records = [item for item in records
                           if item.get("dataset_id") == package]
        if len(package_records) > 1:
            raise ValueError(f"{package} confirmation was attempted more than once")
        if not package_records:
            continue
        receipt = package_records[0]
        matching_attempts = [item for item in attempts
                             if item.get("attempt_id") ==
                             receipt.get("attempt_id")]
        if len(matching_attempts) != 1:
            raise ValueError(f"{package} confirmation attempt is missing")
        evidence[package] = _validate_receipt(
            candidate, freeze_path, freeze, package, receipt,
            matching_attempts[0])
    if ALCIATORE not in evidence:
        raise ValueError("Alciatore confirmation is absent")
    if _receipt_result(evidence[ALCIATORE]["receipt"]):
        if HAN not in evidence:
            raise ValueError("Han confirmation is absent")
    elif HAN in evidence:
        raise ValueError("Han must be absent after failed Alciatore confirmation")
    return evidence


def build_final_assessment(root):
    evidence = validated_confirmation_evidence(Path(root).resolve())
    alciatore = evidence[ALCIATORE]
    han = evidence.get(HAN)
    if han is None and _receipt_result(alciatore["receipt"]):
        raise ValueError("Han confirmation is absent")
    accepted = _receipt_result(alciatore["receipt"]) and \
        han is not None and _receipt_result(han["receipt"])
    return {
        "schema_version": 1,
        "candidate_id": CANDIDATE_ID,
        "disposition": "ACCEPTED" if accepted else "REJECTED",
        "confirmations": evidence,
        "policy": "two_independent_one_time_confirmations_required",
    }


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Build the immutable Phase 3 v4 final assessment")
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--rejection-output", type=Path)
    arguments = parser.parse_args(argv)
    assessment = build_final_assessment(arguments.root)
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_text(_canonical(assessment), encoding="utf-8")
    if assessment["disposition"] == "REJECTED" and \
            arguments.rejection_output is not None:
        rejection = {
            **assessment,
            "assessment_sha256": _sha256(arguments.output),
            "han_2005": "NOT_EXECUTED",
        }
        arguments.rejection_output.parent.mkdir(parents=True, exist_ok=True)
        arguments.rejection_output.write_text(
            _canonical(rejection), encoding="utf-8")
    return 0 if assessment["disposition"] == "ACCEPTED" else 1


if __name__ == "__main__":
    raise SystemExit(main())
