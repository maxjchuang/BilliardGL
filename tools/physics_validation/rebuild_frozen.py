import argparse
import hashlib
import json
import shutil
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path


def _sha256_bytes(value):
    return hashlib.sha256(value).hexdigest()


def _run(command, cwd):
    return subprocess.run(
        [str(value) for value in command], cwd=cwd, check=True,
        capture_output=True,
    )


@dataclass(frozen=True)
class FrozenBuild:
    checkout: Path
    build_dir: Path
    executable: Path
    full_game_runner: Path


def frozen_build_paths(freeze):
    recipe = freeze.get("build_recipe")
    if not isinstance(recipe, dict) or recipe.get("schema_version") != 1:
        raise ValueError("freeze is missing a supported build recipe")
    if recipe.get("temporary_root") != "system":
        raise ValueError("unsupported frozen-build temporary root")
    leaf = recipe.get("worktree_leaf")
    relative_values = (
        recipe.get("build_directory"),
        recipe.get("executable_relative_path"),
        recipe.get("full_game_runner_relative_path"),
    )
    if not isinstance(leaf, str) or not leaf or Path(leaf).name != leaf:
        raise ValueError("unsafe frozen-build worktree leaf")
    if any(not isinstance(value, str) or not value or
           Path(value).is_absolute() or ".." in Path(value).parts
           for value in relative_values):
        raise ValueError("unsafe frozen-build relative path")
    checkout = Path(tempfile.gettempdir()) / leaf
    return FrozenBuild(
        checkout=checkout,
        build_dir=checkout / recipe["build_directory"],
        executable=checkout / recipe["executable_relative_path"],
        full_game_runner=(
            checkout / recipe["full_game_runner_relative_path"]),
    )


def remove_frozen_build(repository_root, freeze):
    repository_root = Path(repository_root).resolve()
    paths = frozen_build_paths(freeze)
    if paths.checkout.exists():
        _run(("git", "worktree", "remove", "--force", paths.checkout),
             repository_root)
    if paths.checkout.exists():
        shutil.rmtree(paths.checkout)
    _run(("git", "worktree", "prune"), repository_root)


def rebuild_frozen(repository_root, freeze_path, jobs=2):
    repository_root = Path(repository_root).resolve()
    freeze = json.loads(Path(freeze_path).read_text(encoding="utf-8"))
    paths = frozen_build_paths(freeze)
    if paths.checkout.exists():
        raise ValueError(
            f"stable frozen-build path already exists: {paths.checkout}")
    recipe = freeze["build_recipe"]
    added = False
    try:
        _run(("git", "worktree", "add", "--detach", paths.checkout,
              freeze["source_revision"]), repository_root)
        added = True
        _run((
            "cmake", "-G", recipe["cmake_generator"], "-S", paths.checkout,
            "-B", paths.build_dir,
            f"-DCMAKE_BUILD_TYPE={recipe['configuration']}",
        ), repository_root)
        _run((
            "cmake", "--build", paths.build_dir, "--config",
            recipe["configuration"], "--target", "Billiards",
            "BilliardsFullGameStress", "-j", str(jobs),
        ), repository_root)
        executable = paths.executable.read_bytes()
        if _sha256_bytes(executable) != freeze["executable_sha256"]:
            raise ValueError("rebuilt executable does not match frozen hash")
        profile = _run(
            (paths.executable, "--print-physics-profile"), paths.checkout).stdout
        if _sha256_bytes(profile) != freeze["canonical_profile_sha256"]:
            raise ValueError("rebuilt canonical profile does not match frozen hash")
        return paths
    except Exception:
        if added:
            remove_frozen_build(repository_root, freeze)
        raise


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Rebuild and verify one frozen Phase 3 candidate")
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--freeze", required=True, type=Path)
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--print-build-dir", action="store_true")
    parser.add_argument("--remove", action="store_true")
    arguments = parser.parse_args(argv)
    freeze = json.loads(arguments.freeze.read_text(encoding="utf-8"))
    if arguments.remove:
        remove_frozen_build(arguments.root, freeze)
        return 0
    build = rebuild_frozen(arguments.root, arguments.freeze, arguments.jobs)
    if arguments.print_build_dir:
        print(build.build_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
