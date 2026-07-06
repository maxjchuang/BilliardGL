# BilliardGL Architecture Evolution Design

## Context

BilliardGL is currently a small OpenGL/GLUT billiards game that has already been updated for macOS local development, default windowed launch, trackpad camera control, readable ball rendering, HUD overlap fixes, and an in-game help overlay.

The next goal is broader than a narrow bug fix. The project should evolve toward a maintainable, better-playing, more product-like game. Teaching value is useful, but it is not the primary driver. The architecture should make future work safer: adding rules, improving visuals, changing input, and preparing a release should not require editing one large file full of unrelated responsibilities.

## Goals

- Split the current monolithic game code into clear modules.
- Introduce a central `GameState` model for world, player, input, camera, HUD, and configuration state.
- Extract physics and rules into OpenGL-independent code that can be unit tested.
- Keep existing gameplay and rendering behavior stable during the refactor.
- Preserve current macOS behavior: windowed default, optional fullscreen, trackpad camera control, Help overlay, and readable HUD.
- Build a foundation for future productization without migrating rendering APIs in this phase.

## Non-Goals

- Do not migrate to modern OpenGL in this phase.
- Do not replace GLUT with GLFW or SDL in this phase.
- Do not rewrite the full billiards rule set.
- Do not introduce an ECS architecture.
- Do not redesign all visual assets.
- Do not commit screenshots, temporary logs, or build artifacts.

## Recommended Direction

Use a `GameState`-centered modular architecture, but execute it in a staged way that first protects physics and rules.

This combines the long-term benefit of a coherent state model with the short-term safety of testable logic extraction. A simple file split would reduce file size but leave the real coupling in place. A full rendering rewrite would create too much risk before the game logic has test coverage.

## Target Modules

### `game_state`

Owns the data model for the current game.

Expected responsibilities:

- Ball positions, velocities, pocketed state, and identities.
- Player state, current player, score, and turn metadata.
- Camera position, orientation, zoom, and input-derived camera motion.
- Shot state such as cue angle, power, and whether balls are currently moving.
- HUD and Help overlay state.
- Runtime configuration such as windowed/fullscreen mode.

This module should not call OpenGL. It should expose plain data and small helper functions where useful.

### `physics`

Updates the physical world.

Expected responsibilities:

- Move balls according to velocity and elapsed time.
- Apply friction and stop very slow balls.
- Resolve ball-to-ball collisions.
- Resolve ball-to-table-boundary collisions.
- Detect pocket entry and remove pocketed balls from active collision.

This module should depend on `game_state` and math helpers such as `vec`, but not on OpenGL or GLUT.

### `rules`

Updates gameplay rules based on state changes.

Expected responsibilities:

- Decide when a shot has ended.
- Switch current player when required.
- Update scores.
- Preserve existing game-over behavior.
- Provide extension points for future foul or full rule handling.

This module should remain independent from rendering.

### `input`

Translates platform callbacks into game intent.

Expected responsibilities:

- Handle keyboard, mouse, and trackpad-style camera input.
- Toggle Help.
- Adjust shot power and camera state.
- Trigger shot intent.

GLUT callback functions may remain in `billiards.cpp`, but they should delegate behavior to `input` instead of directly mutating scattered global variables.

### `renderer`

Draws the 3D scene from `GameState`.

Expected responsibilities:

- Render the table, balls, cue, room, particles, and lighting.
- Own and isolate OpenGL state changes as much as practical.
- Delegate 2D overlays to `hud`.
- Read game state but avoid changing physics or rules state.

This phase should move existing fixed-pipeline OpenGL code without modernizing it. Modern OpenGL can be considered after the module boundaries are stable.

### `hud`

Draws 2D overlays.

Expected responsibilities:

- Current player and player labels.
- Score/status text.
- Persistent "Press H for help" hint.
- Help overlay content.

HUD work should be isolated so future UI fixes do not require touching physics or 3D rendering code.

### `assets`

Centralizes resource loading entry points.

Expected responsibilities:

- Resolve resource paths.
- Load or expose textures, OBJ models, and audio paths.
- Provide clearer missing-resource errors.

The existing `ObjLoader`, `resource_path`, and audio wrappers can stay in place initially. A later task can migrate image loading to `stb_image` or another library.

### Existing Support Modules

Keep these modules initially:

- `ObjLoader.h/.cpp`
- `particle.h/.cpp`
- `vec.h/.cpp`
- `platform_audio.h/.cpp`
- `platform_time.h`
- `resource_path.h/.cpp`

They can be adopted by the new modules without being rewritten immediately.

## Target File Layout

```text
src/Billiards/
  billiards.cpp

  game_state.h/.cpp
  physics.h/.cpp
  rules.h/.cpp
  input.h/.cpp
  renderer.h/.cpp
  hud.h/.cpp
  assets.h/.cpp

  ObjLoader.h/.cpp
  particle.h/.cpp
  vec.h/.cpp
  platform_audio.h/.cpp
  platform_time.h
  resource_path.h/.cpp
```

`billiards.cpp` should become glue code: process startup, GLUT initialization, callback registration, and dispatch to the modules.

## Dependency Direction

```text
billiards.cpp
  -> game_state
  -> input
  -> physics
  -> rules
  -> renderer
  -> assets
  -> platform_audio

physics
  -> game_state
  -> vec

rules
  -> game_state

input
  -> game_state

renderer
  -> game_state
  -> assets
  -> hud
  -> ObjLoader
  -> particle

hud
  -> game_state

assets
  -> resource_path
  -> ObjLoader
```

The important boundary is that `physics`, `rules`, and `input` should not depend on OpenGL headers. They should be understandable and testable without opening a window.

## Runtime Flow

The intended runtime flow is:

```text
GLUT callback
  -> Input translates raw events into GameState changes or game actions
  -> Physics advances ball motion using elapsed time
  -> Rules updates turn, score, and game-over state
  -> Renderer draws the current GameState
  -> Audio plays events triggered by game state transitions
```

Rendering is a representation of state. It should not be the source of game state changes.

## Migration Plan

### Stage 1: Establish `GameState`

Create the initial `GameState` structures. Move the most important state concepts into them: balls, players, camera, shot state, HUD/Help state, and config. Temporary coexistence with old globals is acceptable during this stage, but new state should move into `GameState`.

Acceptance:

- Project still builds.
- Default windowed mode and fullscreen argument still work.
- Current behavior is intentionally unchanged.

### Stage 2: Extract `Physics` and `Rules`

Move ball motion, friction, collision, pocket detection, player switching, scoring, and game-over decisions into OpenGL-independent modules.

Acceptance:

- Physics and rules compile without OpenGL dependencies.
- Basic tests cover friction, low-speed stopping, wall collision, pocketed-ball state, and player switching.
- A real game run still allows hitting a ball and seeing expected motion.

### Stage 3: Extract `Input`

Keep GLUT callbacks in the entry file, but move behavioral handling into `input`.

Acceptance:

- Keyboard shot controls still work.
- Help toggling still works.
- macOS trackpad camera control still works.
- Input code modifies state through clear functions rather than scattered globals.

### Stage 4: Extract `HUD`

Move 2D text and Help overlay drawing out of the main file.

Acceptance:

- Persistent Help hint is visible without interfering with gameplay.
- `H` overlay still opens and closes.
- Current player and player labels do not overlap.

### Stage 5: Extract `Renderer`

Move 3D drawing into `renderer`, keeping the existing fixed-pipeline OpenGL approach.

Acceptance:

- Table, balls, cue, room, particles, lighting, and textures render as before.
- Ball readability does not regress.
- Screenshots from default view, post-shot view, HUD, and Help overlay look coherent.
- `billiards.cpp` is significantly thinner.

### Stage 6: Organize `Assets`

Centralize resource path and loading entry points. Keep current loaders unless replacement is clearly isolated.

Acceptance:

- Normal startup finds resources correctly.
- Missing-resource failures are easier to diagnose.
- No resource behavior is silently changed.

## Verification Strategy

Use three layers of verification.

### Unit Tests

Add a test target for logic that does not require OpenGL.

Initial tests:

- Friction reduces ball speed.
- Very slow balls stop.
- Wall collision reverses the correct velocity component.
- Pocketed balls no longer participate in active play.
- Player switches after a completed shot according to current behavior.
- Existing game-over behavior is preserved.

### Manual E2E

Run the actual game after each major stage.

Manual checks:

- macOS local build succeeds.
- Default launch is windowed.
- Fullscreen launch still works when requested.
- A ball can be struck.
- Balls move, collide, and enter pockets without obvious regression.
- Trackpad camera control works.
- HUD text does not overlap.
- Help hint and overlay work.
- Audio does not block gameplay.

### Visual Checks

Capture screenshots into a temporary directory only.

Recommended scenes:

- Default startup view.
- After a shot.
- Help overlay open.
- HUD/current-player area.

Screenshots are review aids, not repository artifacts.

## Risk Controls

- Do not combine rule rewrites with structural refactoring.
- Do not combine rendering API migration with renderer extraction.
- Do not modernize OpenGL during this architecture pass.
- Keep each stage in a separate commit where practical.
- Run at least one build verification per stage.
- Run the real app and capture screenshots for stages that affect rendering or input.
- Do not revert unrelated user changes if the worktree becomes dirty.

## Final Acceptance Criteria

- The project builds on macOS.
- The game launches in windowed mode by default.
- Fullscreen mode remains available through an explicit argument.
- Ball striking, movement, collision, pocketing, HUD, Help, audio, and trackpad camera behavior do not regress.
- `GameState`, `Physics`, `Rules`, `Input`, `Renderer`, `HUD`, and `Assets` have clear source boundaries.
- `Physics` and `Rules` have basic unit test coverage.
- `billiards.cpp` is reduced to entry, initialization, callback registration, and dispatch responsibilities.
- Temporary screenshots and generated artifacts are not committed.

