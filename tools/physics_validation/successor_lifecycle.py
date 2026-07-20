import hashlib
import json
from pathlib import Path


REJECTING_SOURCE_REVISION = "948c93678d4c459a841131b2d6a29acf35a902ec"


def _sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def _v2_paths(root):
    root = Path(root).resolve()
    candidate = root / "physics_models/candidates/phase3_integrated_v2"
    return {
        "freeze": candidate / "freeze.json",
        "receipt": candidate / "confirmation/sudo_2002/validation_receipt.json",
        "ledger": candidate / "confirmation_consumption.json",
        "manifest": root / "tests/physics_validation/reference_data/sudo_2002/manifest.json",
    }


def _failed_receipt(paths):
    receipt = json.loads(paths["receipt"].read_text(encoding="utf-8"))
    if receipt.get("candidate_id") != "phase3_integrated_v2" or \
            receipt.get("dataset_id") != "sudo_2002" or \
            receipt.get("result") != "FAILED":
        raise ValueError("v2 Sudo rejection receipt is not the immutable failure")
    return receipt


def build_rejection(root):
    paths = _v2_paths(root)
    _failed_receipt(paths)
    return {
        "schema_version": 1,
        "candidate_id": "phase3_integrated_v2",
        "status": "REJECTED",
        "freeze_sha256": _sha256(paths["freeze"]),
        "receipt_sha256": _sha256(paths["receipt"]),
        "ledger_sha256": _sha256(paths["ledger"]),
        "rejecting_source_revision": REJECTING_SOURCE_REVISION,
        "reason": "sole Sudo confirmation transaction failed closed",
    }


def build_spent_transition(root, dataset_id, dataset_version,
                           rejected_candidate):
    if (dataset_id, dataset_version, rejected_candidate) != (
            "sudo_2002", "1.0.0", "phase3_integrated_v2"):
        raise ValueError("only the rejected v2 Sudo partition can transition")
    paths = _v2_paths(root)
    rejection = build_rejection(root)
    return {
        "schema_version": 1,
        "dataset_id": dataset_id,
        "dataset_version": dataset_version,
        "from": "confirmation",
        "to": "spent",
        "rejected_candidate": rejected_candidate,
        "receipt_sha256": rejection["receipt_sha256"],
        "ledger_sha256": rejection["ledger_sha256"],
        "package_manifest_sha256": _sha256(paths["manifest"]),
        "reason": "the sole confirmation attempt is immutable and may inform successors",
    }
