import argparse
import hashlib
import json
import shutil
import subprocess
import tempfile
from pathlib import Path

from .model_candidate import load_candidate_freeze, write_candidate_freeze


def _sha256_bytes(value):
    return hashlib.sha256(value).hexdigest()


def _sha256_file(path):
    return _sha256_bytes(Path(path).read_bytes())


def _run(command, cwd):
    return subprocess.run(
        [str(value) for value in command], cwd=cwd, check=True,
        capture_output=True, text=False,
    )


def _clean_build_evidence(repository_root, source_revision, jobs=2):
    """Build twice from detached checkouts at one stable, freshly cleared path."""
    repository_root = Path(repository_root).resolve()
    checkout = Path(tempfile.gettempdir()) / "billiardgl-phase3-freeze-worktree"
    if checkout.exists():
        raise ValueError(f"stable clean-build path already exists: {checkout}")
    executable_digests = []
    profile_digests = []
    try:
        for _ in range(2):
            added = False
            try:
                _run(("git", "worktree", "add", "--detach", checkout,
                      source_revision), repository_root)
                added = True
                build = checkout / "build"
                _run(("cmake", "-S", checkout, "-B", build,
                      "-DCMAKE_BUILD_TYPE=Release"), repository_root)
                _run(("cmake", "--build", build, "--target", "Billiards",
                      "-j", str(jobs)), repository_root)
                executable = build / "Billiards"
                executable_digests.append(_sha256_file(executable))
                profile = _run(
                    (executable, "--print-physics-profile"), checkout).stdout
                parsed = json.loads(profile)
                if parsed.get("id") != "chinese_pool_full_game_v2":
                    raise ValueError("clean build selected an unexpected profile")
                profile_digests.append(_sha256_bytes(profile))
            finally:
                if added:
                    _run(("git", "worktree", "remove", "--force", checkout),
                         repository_root)
                if checkout.exists():
                    shutil.rmtree(checkout)
    finally:
        _run(("git", "worktree", "prune"), repository_root)
    return executable_digests, profile_digests


def _inventory_artifacts(inventory):
    values = [inventory["profile"], inventory["full_game_matrix"],
              inventory["performance_budget"]]
    values.extend(inventory["calibration_reports"])
    values.extend(inventory["confirmation_packages"])
    values.extend(inventory["metric_contracts"])
    artifacts = []
    for value in values:
        artifacts.append({
            "path": value["path"],
            "role": value["role"],
            "sha256": value["sha256"],
        })
    return sorted(artifacts, key=lambda item: (item["role"], item["path"]))


def phase3_freeze_document(source_revision, build_digests, profile_digests,
                           inventory):
    if len(build_digests) != 2 or len(set(build_digests)) != 1:
        raise ValueError("two clean build executable digests differ")
    if len(profile_digests) != 2 or len(set(profile_digests)) != 1:
        raise ValueError("two clean build profile outputs differ")
    return {
        "schema_version": 2,
        "candidate_id": "phase3_integrated_v2",
        "source_revision": source_revision,
        "executable_sha256": build_digests[0],
        "clean_build_sha256": list(build_digests),
        "canonical_profile_sha256": profile_digests[0],
        "clean_profile_sha256": list(profile_digests),
        "profile": {
            "path": inventory["profile"]["path"],
            "role": "profile",
            "sha256": inventory["profile"]["sha256"],
        },
        "artifacts": _inventory_artifacts(inventory),
    }


def freeze_phase3_candidate(repository_root, source_revision, inventory_path,
                            output, jobs=2, require_clean=True):
    repository_root = Path(repository_root).resolve()
    if require_clean:
        status = _run(("git", "status", "--porcelain"), repository_root).stdout
        if status:
            raise ValueError("phase 3 freeze requires a clean working tree")
    inventory_path = Path(inventory_path)
    inventory = json.loads(inventory_path.read_text(encoding="utf-8"))
    if inventory.get("candidate_id") != "phase3_integrated_v2":
        raise ValueError("unexpected phase 3 inventory candidate")
    for artifact in _inventory_artifacts(inventory):
        path = repository_root / artifact["path"]
        if not path.is_file() or _sha256_file(path) != artifact["sha256"]:
            raise ValueError(f"pre-freeze artifact mismatch: {artifact['path']}")
    revision = _run(
        ("git", "rev-parse", f"{source_revision}^{{commit}}"),
        repository_root,
    ).stdout.decode("ascii").strip()
    build_digests, profile_digests = _clean_build_evidence(
        repository_root, revision, jobs=jobs)
    document = phase3_freeze_document(
        revision, build_digests, profile_digests, inventory)
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(document, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return output


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Create or verify a canonical frozen physics candidate")
    parser.add_argument("--verify", type=Path)
    parser.add_argument("--candidate-id")
    parser.add_argument("--formula-version")
    parser.add_argument("--source-revision")
    parser.add_argument("--profile", type=Path)
    parser.add_argument("--executable", type=Path)
    parser.add_argument(
        "--calibration-report", action="append", type=Path, default=[])
    parser.add_argument("--dataset-manifest", action="append", type=Path)
    parser.add_argument("--supplemental-artifact", action="append", type=Path)
    parser.add_argument("--created-at")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--phase3-inventory", type=Path)
    parser.add_argument("--two-clean-builds", action="store_true")
    parser.add_argument("--jobs", type=int, default=2)
    arguments = parser.parse_args(argv)

    if arguments.phase3_inventory is not None or arguments.two_clean_builds:
        if not arguments.phase3_inventory or not arguments.two_clean_builds \
                or not arguments.source_revision or not arguments.output:
            parser.error(
                "phase 3 freeze requires --phase3-inventory, "
                "--two-clean-builds, --source-revision, and --output")
        freeze_phase3_candidate(
            Path.cwd(), arguments.source_revision, arguments.phase3_inventory,
            arguments.output, jobs=arguments.jobs)
        return 0

    if arguments.profile is None or arguments.executable is None:
        parser.error("--profile and --executable are required")

    if arguments.verify is not None:
        creation_values = (
            arguments.candidate_id,
            arguments.formula_version,
            arguments.source_revision,
            arguments.created_at,
            arguments.output,
        )
        if any(value is not None for value in creation_values):
            parser.error("--verify is mutually exclusive with candidate creation")
        freeze = load_candidate_freeze(arguments.verify)
        if freeze.schema_version == 1:
            if len(arguments.calibration_report) != 1 \
                    or arguments.supplemental_artifact:
                parser.error(
                    "schema v1 verification requires one calibration report "
                    "and no supplemental artifacts")
            verify_arguments = {
                "calibration_report": arguments.calibration_report[0],
            }
        else:
            verify_arguments = {
                "calibration_reports": arguments.calibration_report,
                "supplemental_artifacts": arguments.supplemental_artifact or (),
                "repository_root": Path.cwd(),
            }
        freeze.verify(
            profile=arguments.profile,
            executable=arguments.executable,
            dataset_manifests=arguments.dataset_manifest,
            **verify_arguments,
        )
        return 0

    required = {
        "--candidate-id": arguments.candidate_id,
        "--formula-version": arguments.formula_version,
        "--source-revision": arguments.source_revision,
        "--dataset-manifest": arguments.dataset_manifest,
        "--calibration-report": arguments.calibration_report,
        "--created-at": arguments.created_at,
        "--output": arguments.output,
    }
    missing = [name for name, value in required.items() if not value]
    if missing:
        parser.error("candidate creation requires " + ", ".join(missing))
    use_v2 = len(arguments.calibration_report) > 1 \
        or bool(arguments.supplemental_artifact)
    write_candidate_freeze(
        candidate_id=arguments.candidate_id,
        formula_version=arguments.formula_version,
        source_revision=arguments.source_revision,
        profile=arguments.profile,
        executable=arguments.executable,
        calibration_report=(
            None if use_v2 else arguments.calibration_report[0]),
        calibration_reports=(
            arguments.calibration_report if use_v2 else None),
        dataset_manifests=arguments.dataset_manifest,
        supplemental_artifacts=arguments.supplemental_artifact or (),
        repository_root=Path.cwd(),
        created_at=arguments.created_at,
        output=arguments.output,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
