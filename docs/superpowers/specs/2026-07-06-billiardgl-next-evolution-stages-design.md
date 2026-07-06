# BilliardGL Next Evolution Stages Design

## Context

BilliardGL has completed the first architecture pass. The project now has `GameState`, `physics`, `rules`, `input`, `renderer`, `hud`, `assets`, and screenshot support modules. Unit tests cover selected physics, rules, and screenshot helpers. The game runs on macOS in windowed mode by default, supports fullscreen by option, supports trackpad camera control, has a Help overlay, and renders readable billiard balls.

The current architecture is an intermediate state. New modules exist, but `src/Billiards/billiards.cpp` still owns a large amount of runtime logic and rendering code. The next phase should finish the migration in controlled steps instead of doing another broad refactor.

## Goal

Move the game from "modules exist" to "modules own their responsibilities" through five ordered stages:

1. Unify the runtime state source.
2. Run gameplay through the extracted physics and rules modules.
3. Continue shrinking `billiards.cpp` into platform and callback glue.
4. Expand automated visual verification.
5. Upgrade resource loading while preserving existing assets.

Each stage should be independently buildable, testable, and reviewable.

## Non-Goals

- Do not migrate from GLUT to GLFW or SDL windowing in these stages.
- Do not migrate to modern OpenGL or shader-based rendering yet.
- Do not rewrite the full pool rule set unless required to preserve current behavior.
- Do not redesign the art direction or replace all assets.
- Do not commit screenshots, build outputs, generated logs, or temporary verification files.

## Stage 1: Unify Runtime State

### Problem

The repository now has `billiardgl::GameState`, but `billiards.cpp` still has legacy globals such as `Ball[16]`, player variables, camera variables, HUD variables, and window/runtime flags. This creates two possible sources of truth. Any future physics, rendering, or input change can accidentally update one state model but render or test another.

### Design

Make `GameState` the authoritative runtime model.

State ownership should move as follows:

| Current responsibility | Target owner |
| --- | --- |
| Ball position, velocity, speed, pocketed state | `GameState::balls` |
| Current player, next player, foul and shot state | `GameState::players` |
| Camera target, eye, angles, zoom | `GameState::camera` |
| Mouse and trackpad state | `GameState::input` |
| Help overlay state | `GameState::hud` |
| Windowed/fullscreen and dimensions | `GameState::config` |

If rendering still needs the old `Ball` structure during migration, keep it as a short-lived compatibility adapter rather than an independent state model. The adapter should be populated from `GameState`, not the other way around.

### Acceptance Criteria

- `GameState` is the only authoritative source for ball positions and velocities during gameplay.
- Player, camera, HUD, and runtime configuration mutations go through `GameState`.
- Legacy globals are deleted or clearly marked as render-only compatibility data.
- Existing physics, rules, screenshot tests pass.
- Manual launch still starts in windowed mode by default.

## Stage 2: Use Extracted Physics and Rules at Runtime

### Problem

`physics.cpp` and `rules.cpp` are testable, but the active `myIdle` path still contains legacy movement, collision, pocket, and turn-update logic. This means tests validate extracted logic that is not yet the actual game loop.

### Design

Make the runtime loop call the extracted modules:

```text
myIdle
  -> update input-derived shot intent
  -> updatePhysics(Game, dt)
  -> updatePlayerAfterShot(Game)
  -> update camera and render scheduling
```

The migration should preserve current gameplay behavior first, then improve rules in later work. Where legacy code plays sound during physical events, avoid moving audio directly into `physics`. Prefer a simple event output model, such as collision, pocket, and shot-ended events, so physics remains testable and platform-independent.

### Test Coverage

Add or extend tests for:

- Cue ball pocket handling.
- Eight ball pocket and game-over behavior.
- First object ball assignment.
- Player switching after an empty shot.
- Player continuing after pocketing their own ball.
- Illegal shot state transitions that already exist in current gameplay.

### Acceptance Criteria

- Active gameplay uses `updatePhysics` and `updatePlayerAfterShot`.
- Duplicated physics/rules logic is removed from `billiards.cpp`.
- Audio behavior remains functional through game events or a similarly isolated mechanism.
- Unit tests cover the rule transitions above.
- Manual shot testing still moves balls, pockets balls, updates players, and renders correctly.

## Stage 3: Continue Shrinking `billiards.cpp`

### Problem

`renderer.cpp` currently wraps rendering entry points, but many OpenGL drawing functions and resource setup details still live in `billiards.cpp`. This keeps the main file hard to reason about and makes rendering changes risky.

### Design

Move rendering ownership into dedicated modules while keeping the fixed-pipeline OpenGL implementation.

Expected target shape:

```text
billiards.cpp
  process args
  initialize GameState
  initialize platform/window/audio
  register GLUT callbacks
  dispatch input, idle, display

renderer.cpp
  render room
  render table
  render balls
  render cue
  render particles
  own 3D OpenGL state setup

hud.cpp
  render player/status HUD
  render help prompt and help overlay

assets.cpp or renderer resources
  resolve and load texture/model/audio resources
```

Introduce a `RenderResources` or equivalent structure if it removes global texture/model state from `billiards.cpp`. This structure should own texture IDs, OBJ model data, and other render-only resources.

### Acceptance Criteria

- `billiards.cpp` is primarily startup and callback glue.
- Scene drawing functions live in `renderer.cpp` or renderer-owned files.
- HUD drawing stays in `hud.cpp`.
- Render resources are grouped rather than scattered across unrelated globals.
- Visual output matches the current game closely enough for screenshot checks and manual play.

## Stage 4: Expand Automated Visual Verification

### Problem

The project can write framebuffer screenshots in-process, and screenshot helper tests exist. However, visual coverage is still too shallow to catch regressions like broken ball rendering, HUD overlap, missing Help overlay, or a failed post-shot scene.

### Design

Extend screenshot mode into deterministic visual scenarios:

```text
./build/Billiards --screenshot /tmp/billiardgl/default.ppm
./build/Billiards --screenshot /tmp/billiardgl/help.ppm --screenshot-scene help
./build/Billiards --screenshot /tmp/billiardgl/after-shot.ppm --screenshot-scene after-shot
```

The exact option names can change during implementation, but the behavior should cover:

- Default table and rack.
- Help overlay visible.
- A deterministic shot after physics has advanced.

Verification should include lightweight pixel/layout checks:

- Output file exists and has a valid PPM header.
- Image has non-zero visible pixels.
- The expected scene differs from an all-background frame.
- HUD/help regions have visible foreground pixels when enabled.
- Ball regions contain enough continuous colored pixels to catch the previous speckled-ball regression.

Optional developer convenience:

- Add a script or CMake helper to convert PPM to PNG when ImageMagick, `sips`, or another local converter is available.
- Keep converted screenshots under `/tmp` unless the user explicitly asks to save them.

### Acceptance Criteria

- Automated screenshot tests cover default, Help, and after-shot scenes.
- Visual verification works without relying on macOS window capture APIs.
- Temporary screenshots are written outside the repository by default.
- Failures produce actionable messages.

## Stage 5: Upgrade Resource Loading

### Problem

The project still relies on a narrow hand-written BMP path for images. This limits future visual work and makes resource failures harder to diagnose.

### Design

Introduce a small, explicit image-loading layer. `stb_image` is a good candidate because it is simple, portable, and supports PNG, JPG, and BMP. Existing BMP assets should remain valid.

Target responsibilities:

- Load texture images through one entry point.
- Support BMP, PNG, and JPG where practical.
- Preserve existing texture coordinate and OpenGL upload behavior.
- Report missing or invalid resources with clear paths and reasons.
- Keep resource path resolution centralized in `assets`/`resource_path`.

### Acceptance Criteria

- Existing BMP assets still load.
- PNG and JPG textures can be loaded by the same texture path.
- Missing files produce clear errors.
- The main game renders the same existing assets after migration.
- Tests or a small verification path cover at least one successful image load and one missing-resource case where feasible.

## Dependency Order

The stages should be executed in order.

Stage 1 must come first because every later stage relies on a single source of truth.

Stage 2 should follow immediately because physics and rules tests only become fully meaningful when the runtime uses the tested modules.

Stage 3 is safer after state and gameplay ownership are settled; renderer extraction is mostly structural but touches a lot of OpenGL code.

Stage 4 should happen before resource expansion so visual checks can catch regressions introduced by broader asset work.

Stage 5 should come last because it changes IO and rendering resources, and it benefits from the screenshot coverage created in Stage 4.

## Working Rules

- Use a dedicated branch or worktree for implementation work, not direct development on `master`.
- Commit each stage separately when it reaches its acceptance criteria.
- Run build and relevant tests after each stage.
- Use `gh` for GitHub pull request operations.
- Keep screenshots and temporary visual artifacts in `/tmp`.
- Preserve current macOS local run behavior unless a stage explicitly changes it.
- Prefer small compatibility adapters over broad rewrites when that keeps the game playable between stages.

## Open Questions

- Should physics emit explicit event objects for audio and HUD updates, or should `GameState` keep transient event flags?
- What minimum screenshot checks are strict enough to catch visual regressions without becoming brittle?
- Should `stb_image` be vendored into the repository or managed through a package/dependency mechanism?
- What maximum size should `billiards.cpp` target after Stage 3: under 800 lines, under 500 lines, or simply "startup glue only"?

## Final Definition of Done

The five-stage evolution is complete when:

- `GameState` owns active gameplay state.
- Runtime gameplay uses the extracted `physics` and `rules` modules.
- `billiards.cpp` no longer contains major game logic or scene rendering logic.
- Automated visual verification covers default, Help, and after-shot scenes.
- Resource loading supports current BMP assets plus common future image formats.
- The project still builds, tests, launches locally on macOS, and remains playable.
