import hashlib
import json
from pathlib import Path


FAILURE_MESSAGE = (
    "invalid_scenario: expectations must be a nonempty array")


def _sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def build_derby_spent_transition(root):
    root = Path(root).resolve()
    candidate = root / "physics_models/candidates/phase3_integrated_v3"
    paths = {
        "freeze": candidate / "freeze.json",
        "failure": candidate / "confirmation/derby_fuller_1999/failure.json",
        "receipt": candidate / (
            "confirmation/derby_fuller_1999/validation_receipt.json"),
        "ledger": candidate / "confirmation_consumption.json",
        "rejection": root / (
            "physics_models/promotion/phase3_integrated_v3_rejection.json"),
        "package_manifest": root / (
            "tests/physics_validation/reference_data/"
            "derby_fuller_1999/manifest.json"),
    }
    receipt = json.loads(paths["receipt"].read_text(encoding="utf-8"))
    failure = json.loads(paths["failure"].read_text(encoding="utf-8"))
    if (receipt.get("candidate_id"), receipt.get("dataset_id"),
            receipt.get("result")) != (
                "phase3_integrated_v3", "derby_fuller_1999", "FAILED"):
        raise ValueError("v3 Derby receipt is not the immutable failure")
    if failure.get("message") != FAILURE_MESSAGE:
        raise ValueError(
            "v3 Derby failure does not match the diagnosed root cause")
    return {
        "schema_version": 1,
        "dataset_id": "derby_fuller_1999",
        "dataset_version": "1.0.0",
        "from": "confirmation",
        "to": "spent",
        "rejected_candidate": "phase3_integrated_v3",
        "failure_category": "SCENARIO_CONTRACT_INTEGRATION_FAILURE",
        "failure_message": FAILURE_MESSAGE,
        "evidence_sha256": {
            key: _sha256(path) for key, path in sorted(paths.items())
        },
        "reason": (
            "the sole Derby transaction is immutable and cannot be reused"),
    }
