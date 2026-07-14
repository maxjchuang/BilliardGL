import hashlib
import json
from pathlib import Path


def _sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def build_validation_artifact_manifest(root, inventory_path):
    root = Path(root).resolve()
    inventory = json.loads(Path(inventory_path).read_text(encoding="utf-8"))
    files = []
    for candidate in inventory["candidates"]:
        for receipt in candidate["receipts"]:
            validation = (root / receipt["path"]).parent
            report = json.loads((validation / "reference_report.json").read_text(
                encoding="utf-8"))
            dataset_id = report["metadata"]["dataset_id"]
            for kind in ("provenance", "traces"):
                for path in sorted((validation / kind).glob("*.json")):
                    files.append({
                        "candidate_id": candidate["id"],
                        "dataset_id": dataset_id,
                        "kind": kind,
                        "path": path.relative_to(root).as_posix(),
                        "sha256": _sha256(path),
                    })
    return {"schema_version": 1, "files": files}


def validate_validation_artifact_manifest(path, root, inventory_path):
    try:
        committed = json.loads(Path(path).read_text(encoding="utf-8"))
        expected = build_validation_artifact_manifest(root, inventory_path)
    except (OSError, KeyError, TypeError, ValueError, json.JSONDecodeError) as error:
        return [f"validation artifact manifest is unreadable: {error}"]
    if committed != expected:
        return ["validation artifact manifest is stale or incomplete"]
    return []
