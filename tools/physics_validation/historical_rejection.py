import hashlib
import json
from pathlib import Path


def _sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def _relative(path, root):
    return Path(path).resolve().relative_to(Path(root).resolve()).as_posix()


def discover_failed_v1_receipts(root):
    root = Path(root)
    receipts = set()
    candidates = root / "physics_models/candidates"
    for path in sorted(candidates.glob("*_v1/**/validation_receipt.json")):
        receipt = json.loads(path.read_text(encoding="utf-8"))
        if receipt.get("result") == "FAILED":
            receipts.add(_relative(path, root))
    return receipts


def discover_v1_accounting(root):
    root = Path(root)
    discovered = {
        "unallowlistable_integration_failures": [],
        "new_model_mismatches": [],
    }
    for receipt_path in sorted(discover_failed_v1_receipts(root)):
        report = root / Path(receipt_path).parent / "reference_report.json"
        document = json.loads(report.read_text(encoding="utf-8"))
        accounting = document.get("accounting", {})
        report_path = _relative(report, root)
        for source_key, target_key in (
            ("unallowlistable_failures", "unallowlistable_integration_failures"),
            ("new_model_mismatches", "new_model_mismatches"),
        ):
            for value in accounting.get(source_key, []):
                item = dict(value)
                item["report_path"] = report_path
                discovered[target_key].append(item)
    for values in discovered.values():
        values.sort(key=lambda item: (
            item["dataset_id"], item["case_id"], item["metric"],
            item["code"], item["report_path"],
        ))
    return discovered


def validate_historical_rejection(path, root):
    root = Path(root)
    document = json.loads(Path(path).read_text(encoding="utf-8"))
    failures = []
    release_path = document.get("rejected_release_path", "")
    release = root / release_path
    try:
        release.resolve().relative_to(root.resolve())
    except ValueError:
        failures.append("rejected release path escapes repository")
    if document.get("schema_version") != 1 or document.get("status") != "REJECTED":
        failures.append("historical rejection status or schema is invalid")
    if (not release.is_file() or
            _sha256(release) != document.get("rejected_release_sha256")):
        failures.append("rejected release hash mismatch")
    if set(document.get("failed_receipts", [])) != discover_failed_v1_receipts(root):
        failures.append("failed receipt inventory mismatch")
    discovered = discover_v1_accounting(root)
    for field in ("unallowlistable_integration_failures", "new_model_mismatches"):
        if document.get(field) != discovered[field]:
            failures.append(f"{field} inventory mismatch")
    findings = document.get("review_findings", [])
    required_findings = {
        "FAILED_RECEIPTS_PROMOTED",
        "INCOMPLETE_CONTACT_SOLVER",
        "NONTRANSACTIONAL_SOLVER_FAILURE",
        "NONEXECUTABLE_FULL_GAME_MATRIX",
        "STALE_RELEASE_SOURCE_REVISION",
    }
    if {value.get("code") for value in findings} != required_findings:
        failures.append("review finding inventory mismatch")
    revision = document.get("rejecting_source_revision", "")
    if len(revision) != 40 or any(character not in "0123456789abcdef" for character in revision):
        failures.append("rejecting source revision is not immutable")
    return failures
