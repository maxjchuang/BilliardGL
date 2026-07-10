# Engineering Infrastructure Design

## Goal

Establish a repeatable engineering workflow for BilliardGL so local development and CI use the same configure, build, and test path.

This is the first batch of phase 2, “engineering.” It follows the completed phase 1 stability work and deliberately avoids phase 3 repository cleanup.

## Scope

In scope:

- Add a single local verification entry point.
- Reduce duplicated CMake test-target configuration.
- Add GitHub Actions CI that runs the same verification entry point.
- Document dependency, build, test, and screenshot verification commands in the README.

Out of scope:

- Removing `Release/`.
- Removing Visual Studio project files.
- Removing or migrating vendored dependencies.
- Reorganizing asset directories.
- Changing gameplay, rendering behavior, physics behavior, or resource formats.
- Treating existing deprecation warnings as failures.

## Current Context

The project already has:

- A root `CMakeLists.txt`.
- CTest-enabled unit and screenshot tests.
- A README with gameplay and screenshot verification notes.
- A macOS-compatible build path using OpenGL, GLEW, GLUT, SDL2, and SDL2_mixer.

The current engineering gaps are:

- No single command documents and executes the expected validation workflow.
- No CI workflow exists.
- Test target declarations in `CMakeLists.txt` repeat include directories, compile definitions, and registration patterns.
- README does not clearly present prerequisites, configure/build/test commands, or CI-equivalent validation.

## Design

### Verification Script

Create `scripts/check.sh` as the canonical verification command for local development and CI.

Behavior:

- Run from any working directory.
- Resolve the repository root from the script location.
- Configure CMake into a build directory.
- Build the project.
- Run CTest with `--output-on-failure`.
- Accept an optional build directory through `BILLIARDGL_BUILD_DIR`.
- Default to `build/check`.

The script should use conservative shell behavior:

- `set -euo pipefail`
- quoted paths
- no destructive cleanup

Default command:

```bash
./scripts/check.sh
```

CI command:

```bash
BILLIARDGL_BUILD_DIR="$RUNNER_TEMP/billiardgl-build" ./scripts/check.sh
```

### CMake Maintainability

Refactor the root `CMakeLists.txt` without changing build outputs.

Target improvements:

- Keep existing executable and test target names stable.
- Add helper functions for repeated test setup.
- Centralize common include directories.
- Centralize test compile definitions:
  - `BILLIARDGL_ASSET_ROOT`
  - `BILLIARDGL_SOURCE_ROOT`
- Keep macOS-specific `GL_SILENCE_DEPRECATION` behavior unchanged.
- Keep screenshot CTest entries unchanged.

This refactor should make future tests require only:

- test target name
- test source file
- source list required by that test
- optional libraries

### GitHub Actions CI

Add `.github/workflows/ci.yml`.

Initial CI target:

- `macos-latest`
- checkout repository
- install runtime/build dependencies with Homebrew:
  - `cmake`
  - `pkg-config`
  - `glew`
  - `sdl2`
  - `sdl2_mixer`
- run `scripts/check.sh`

CI should not introduce warning-as-error yet. Existing macOS SDK and OpenGL deprecation warnings are known and will be addressed in a later engineering slice.

### Documentation

Update README with an engineering-focused section that covers:

- macOS prerequisites.
- configure/build/test commands.
- the preferred single-command verification path.
- screenshot verification command.
- CI behavior.

The README should distinguish:

- canonical local verification: `./scripts/check.sh`
- manual CMake workflow for debugging:

```bash
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Testing Strategy

Verification must prove that the engineering changes preserve current behavior.

Required local checks:

```bash
./scripts/check.sh
```

Expected result:

- CMake configure succeeds.
- Build succeeds.
- CTest reports all tests passing.

Additional checks:

- Run the script from a directory outside the repository root to confirm root resolution works.
- Run with `BILLIARDGL_BUILD_DIR` set to a temporary directory to confirm CI usage works.
- Inspect GitHub Actions YAML for valid syntax and command consistency.

## Risks and Mitigations

Risk: CI dependency installation differs from the local machine.

Mitigation: Use Homebrew package names already implied by the current successful macOS build.

Risk: CMake refactor accidentally changes target compile definitions or include paths.

Mitigation: Keep target names and source lists stable, then verify with the full CTest suite.

Risk: Screenshot tests may be environment-sensitive on CI.

Mitigation: Keep screenshot tests enabled initially because the project already supports framebuffer screenshot mode. If CI later proves the runner cannot support the OpenGL context, adjust CI in a follow-up engineering slice based on evidence.

Risk: Existing warnings obscure new warnings.

Mitigation: Do not enable warning-as-error in this slice. Warning cleanup is a separate engineering task.

## Acceptance Criteria

- `scripts/check.sh` exists, is executable, and succeeds locally.
- `CMakeLists.txt` has less duplicated test target setup while preserving target names.
- `.github/workflows/ci.yml` runs the canonical verification script on macOS.
- README documents prerequisites and the canonical verification command.
- No gameplay, rendering, physics, or asset behavior changes are introduced.
- Full local verification passes after implementation.
