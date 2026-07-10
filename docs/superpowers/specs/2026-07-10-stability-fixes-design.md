# BilliardGL Stability Fixes Design

Date: 2026-07-10

## Goal

Fix the highest-risk stability issues found on latest `master` without changing gameplay, controls, camera behavior, visual output, asset layout, or the public build flow.

The first phase is intentionally narrow: remove undefined behavior, make asset loading fail predictably, and complete render resource cleanup. Broader architecture and repository hygiene work is deferred to later phases.

## Current Evidence

- `master` is at `4e6859c416d3fcca29c2414d7df95ca77dc14fb2`.
- Clean build succeeds but reports `src/Billiards/vec.cpp:69:9: warning: reference to stack memory associated with local variable 'ans' returned [-Wreturn-stack-address]`.
- Full CTest suite passes: 14 tests, 0 failures.
- Resource loading assumes valid OBJ/MTL shape and at least two table/cue materials.
- `destroyRenderResources` deletes VBOs but does not delete OpenGL textures.

## Scope

In scope:

- Fix `vec` lifetime and boundary behavior.
- Harden OBJ/MTL parsing against missing files, malformed records, missing material declarations, and out-of-range indices.
- Make render resource initialization fail cleanly when required object data or materials are unavailable.
- Release all OpenGL textures created by render resource initialization.
- Add focused tests for the new failure paths and vector edge cases.

Out of scope:

- Physics or gameplay tuning.
- Main loop timing changes.
- CMake target restructuring.
- CI, sanitizer, or warning policy changes.
- `Release` asset cleanup, Git LFS, license work, or dependency modernization.
- OpenGL fixed-function or GLU migration.

## Design

### Vector Math

`vec::CrossProduct` will return `vec` by value instead of `const vec&`. This matches the existing arithmetic operators and removes the stack-reference bug.

`vec::Normalize` will become a no-op for zero-length vectors. Non-zero vectors keep the same normalization behavior.

`vec::toFloat` will be corrected to return initialized component data or removed if no call sites exist. If retained, ownership must be explicit enough that callers can safely release the returned memory. The preferred implementation is to avoid heap allocation in new code.

### OBJ and MTL Loading

`ObjLoader` will track whether loading succeeded. Constructor failures will leave the object in a valid but unusable state instead of partially initialized data that later crashes.

Parsing will validate token counts before indexing. Face parsing will validate vertex, texture, and normal indices before using them. Unsupported or malformed records will fail the loader for required assets rather than throwing uncaught exceptions or reading out of bounds.

MTL parsing will require `newmtl` before material properties are assigned. Missing MTL files or material names will mark the loader invalid.

### Render Resource Initialization

`initializeRenderResources` will validate each `ObjLoader` before reading materials. It will check the required material counts before indexing table and cue materials.

If any required resource fails after partial creation, initialization will clean up what has already been created before returning `false`. This keeps repeated initialization attempts from leaking GPU handles.

`destroyRenderResources` will delete all owned textures and VBOs and reset stored handles to zero. It should tolerate being called on a default, partially initialized, or already-destroyed `RenderResources`.

## Error Handling

Asset loading failures should return `false` from `initializeRenderResources` and print a concise diagnostic with the failing path or reason. They should not call `exit`, throw uncaught exceptions, or dereference empty vectors.

The command-line screenshot path should keep its current behavior: the app exits non-zero if rendering resources cannot initialize or the framebuffer cannot be saved.

## Testing

Add or update tests to cover:

- `CrossProduct` returns a stable value.
- Normalizing a zero vector does not produce NaN or infinity.
- `ObjLoader` reports failure for a missing OBJ file.
- `ObjLoader` reports failure for malformed face or material records.
- `initializeRenderResources` returns `false` instead of crashing when object/material data is invalid. This can use temporary fixture files or a small test-specific asset root if needed.
- `destroyRenderResources` is safe on default and partially initialized resource structs.

Existing tests must continue to pass:

```sh
cmake --build /tmp/billiardgl-audit-build --clean-first --parallel 4
ctest --test-dir /tmp/billiardgl-audit-build --output-on-failure
```

The clean build should no longer report the `vec.cpp:69` stack-reference warning after implementation.

## Acceptance Criteria

- No gameplay, input, camera, screenshot scene, or visual asset behavior is intentionally changed.
- Clean build succeeds.
- Full CTest suite passes.
- The `vec.cpp:69` `-Wreturn-stack-address` warning is gone.
- Missing or malformed OBJ/MTL files fail gracefully instead of crashing.
- Render texture and VBO handles owned by `RenderResources` are deleted and reset during cleanup.
