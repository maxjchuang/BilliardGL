# Repository Governance Design

## Goal

Reduce repository noise and tracked generated artifacts while preserving current build behavior and legacy compatibility paths.

This is the first batch of phase 3, “repository governance.” It follows the completed phase 1 stability work and the completed phase 2 engineering infrastructure work.

## Scope

In scope:

- Remove tracked `Release/` artifacts from the repository.
- Move root-level legacy presentation/report files into `docs/legacy/`.
- Strengthen `.gitignore` so build, release, temporary, and screenshot outputs do not get reintroduced.
- Update README with the repository structure and legacy artifact policy.
- Verify the repository still builds and tests through `./scripts/check.sh`.

Out of scope:

- Removing Visual Studio solution/project files.
- Removing `src/Billiards/dependencies/`.
- Migrating Windows dependency management.
- Changing source code behavior.
- Changing CMake target structure.
- Changing asset paths used by the current CMake build.
- Rewriting historical documentation.

## Current Context

The repository currently contains three major governance issues:

- `Release/` is tracked and contains a historical executable, DLLs, dependencies, duplicated assets, and copied OBJ/texture/audio files.
- Root-level `Report.pdf` and `Representation.pptx` are legacy project artifacts but are not organized under `docs/`.
- `.gitignore` currently ignores `.worktrees/`, `build/`, and `docs/superpowers/plans/`, but does not clearly document release/output/screenshot artifact rules.

The project also contains Windows/Visual Studio files and vendored dependencies:

- `src/Billiards.sln`
- `src/Billiards/Billiards.vcxproj`
- `src/Billiards/Billiards.vcxproj.filters`
- `src/Billiards/dependencies/`

These are intentionally preserved in this governance batch because deleting them would expand the scope from safe repository cleanup into platform migration.

## Design

### Remove Tracked Release Artifacts

Delete the tracked `Release/` tree from git.

Rationale:

- It duplicates runtime assets already available under `src/Billiards/`.
- It includes compiled binaries and copied dependency payloads.
- It is not used by the current canonical CMake verification path.
- Future release packages should be generated outside source control.

Implementation intent:

```bash
git rm -r Release
```

### Archive Legacy Documents

Move legacy root-level project documents under `docs/legacy/`.

Files:

- `Report.pdf` → `docs/legacy/Report.pdf`
- `Representation.pptx` → `docs/legacy/Representation.pptx`

Rationale:

- These files are historical project material, not active build inputs.
- Moving them keeps the repository root focused on active development files.
- Moving is safer than deletion because the materials remain accessible.

Implementation intent:

```bash
mkdir -p docs/legacy
git mv Report.pdf docs/legacy/Report.pdf
git mv Representation.pptx docs/legacy/Representation.pptx
```

### Preserve Legacy Windows Compatibility Inputs

Do not remove the Visual Studio solution/project files or vendored dependency tree in this batch.

Preserved paths:

- `src/Billiards.sln`
- `src/Billiards/Billiards.vcxproj`
- `src/Billiards/Billiards.vcxproj.filters`
- `src/Billiards/dependencies/`

Rationale:

- The repository still includes historical Windows project structure.
- The current task is repository governance, not Windows build migration.
- Removing these paths without replacing the Windows workflow would be a compatibility break.

### Strengthen Ignore Rules

Update `.gitignore` to make artifact boundaries explicit.

Required rules:

```gitignore
.worktrees/
build/
cmake-build-*/
out/
Release/
*.ppm
*.log
docs/superpowers/plans/
```

Notes:

- `Release/` should be ignored after removal to prevent generated release directories from reappearing.
- `*.ppm` covers screenshot outputs generated during local visual verification.
- `*.log` covers local diagnostic/build logs.
- Existing ignore rules should be preserved.

### Update README

Add a repository-structure/governance note near the engineering workflow section.

The README should state:

- Active build/test workflow uses CMake and `./scripts/check.sh`.
- Generated build outputs should not be committed.
- Release packages should be produced outside source control.
- Legacy report/presentation files live under `docs/legacy/`.
- Visual Studio files and vendored Windows dependencies are retained for legacy compatibility.

## Testing Strategy

Required verification after implementation:

```bash
./scripts/check.sh
```

Expected result:

- CMake configure succeeds.
- Build succeeds.
- CTest reports all 16 tests passing.

Additional repository checks:

```bash
git status --short
git ls-files Release
test -f docs/legacy/Report.pdf
test -f docs/legacy/Representation.pptx
```

Expected result:

- `git ls-files Release` returns no files after the removal is staged/committed.
- Legacy documents exist under `docs/legacy/`.
- `.gitignore` includes `Release/`.

## Risks and Mitigations

Risk: `Release/` contains the only copy of an asset.

Mitigation: Compare active asset directories before deletion. Current observed duplicate runtime assets live under `src/Billiards/obj`, `src/Billiards/tex`, and source-controlled audio paths; verification through `./scripts/check.sh` must pass after removal.

Risk: Users expect downloadable Windows binaries in the repository.

Mitigation: README will clarify that release packages are generated artifacts and should live outside source control.

Risk: Removing vendored dependencies would break legacy Windows builds.

Mitigation: This batch explicitly preserves `src/Billiards/dependencies/` and Visual Studio files.

Risk: Moving binary documents could break external references.

Mitigation: Move them to a stable `docs/legacy/` path and document that location in README.

## Acceptance Criteria

- `Release/` is no longer tracked.
- `Report.pdf` and `Representation.pptx` are moved to `docs/legacy/`.
- `.gitignore` prevents common generated outputs from being reintroduced.
- README documents the repository governance policy and legacy artifact location.
- Visual Studio files and `src/Billiards/dependencies/` remain tracked.
- `./scripts/check.sh` passes after the governance changes.
