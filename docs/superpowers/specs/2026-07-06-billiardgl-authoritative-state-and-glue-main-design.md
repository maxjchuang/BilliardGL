# BilliardGL Authoritative State and Glue Main Design

## Context

PR #6 completed the first pass of the five-stage evolution, but code review identified two remaining architecture gaps:

1. `GameState` is not yet the complete authoritative runtime state source.
2. `src/Billiards/billiards.cpp` is still more than GLUT glue because it owns resource loading, OpenGL setup, camera glue, and some legacy UI/input state.

This spec defines the stricter target needed before the five stages can be considered fully complete.

## Goals

- Make `GameState` the single runtime source for camera, input, shot, HUD, config, player, and ball state.
- Remove legacy runtime globals from `billiards.cpp`.
- Move render resource initialization and OpenGL scene setup out of `billiards.cpp`.
- Keep gameplay, visuals, windowed default behavior, Help, trackpad orbit, and screenshot mode stable.
- Keep all verification available through CTest and deterministic screenshot scenes.

## Non-Goals

- Do not migrate from GLUT to another windowing library.
- Do not migrate to modern OpenGL or shaders.
- Do not redesign visual assets.
- Do not expand the full pool rule set beyond preserving existing behavior.
- Do not merge PR #6 until this cleanup is either completed or explicitly accepted as future work.

## Design Part 1: Authoritative `GameState`

`billiards.cpp` currently still uses legacy globals such as `at[6]`, `speed`, `leftm`, `rightm`, `TrackpadOrbit`, and reference aliases like `CurrPlayer`, `ShowHelp`, and `WindowedMode`. Some of those values are mirrored into `GameState`, but the actual runtime logic still mutates the legacy names.

The target is direct ownership:

| Current runtime state | Target owner |
| --- | --- |
| `at[0..2]` camera eye | `Game.camera.eye` |
| `at[3..5]` camera target | `Game.camera.target` |
| `zoom`, `anglex`, `angley` | `Game.camera.zoom`, `angleX`, `angleY` |
| `speed` shot power | `Game.input.shotPower` |
| `WaitHit`, `Hit` | `Game.input.waitingForHit`, `hitRequested` |
| `mx`, `my`, `leftm`, `rightm`, `TrackpadOrbit` | `Game.input` |
| `CurrPlayer`, `NextPlayer`, `IsIllegal`, `updated`, `Hitted` | `Game.players` |
| `ShowHelp` | `Game.hud.showHelp` |
| `WindowedMode`, `ScreenshotPath`, `width`, `height` | `Game.config` |

The GLUT callback functions may remain in `billiards.cpp`, but they should translate platform events and delegate behavior to `input.cpp`.

Expected callback shape:

```text
myKeyboard
  -> input helpers update Game

myMouse
  -> map GLUT button/modifier to input enum
  -> handleMouseButton(Game, button, state, x, y)

mouseMove
  -> handleMouseMove(Game, x, y)

myIdle
  -> apply shot intent from Game.input
  -> updatePhysics(Game, kDefaultTimeStep)
  -> updatePlayerAfterShot(Game)
  -> updateCameraFromCueBall(Game) when needed
```

`set_camera` should become a read-only render setup helper:

```text
setupCameraFromGameState(const GameState&)
```

It should not calculate or mutate the authoritative camera state.

## Design Part 2: Glue-Only `billiards.cpp`

`billiards.cpp` should own only process and GLUT lifecycle:

- Parse command-line options.
- Create the GLUT window.
- Initialize `GameState`.
- Initialize platform audio.
- Initialize render resources through renderer/resource modules.
- Register GLUT callbacks.
- Dispatch callbacks to modules.

It should not own:

- Texture loading.
- OBJ model ownership.
- VBO creation.
- Lighting setup.
- Scene rendering functions.
- HUD text drawing helpers.
- Physics, rule, or input logic.

The target ownership is:

```text
renderer.cpp
  renderScene
  setupCameraFromGameState
  setupLights

render_resources.cpp
  initializeRenderResources
  destroyRenderResources
  uploadTexture
  create VBOs
  own ObjLoader instances

hud.cpp
  all HUD/help drawing

assets.cpp / image_loader.cpp
  path resolution and image decoding

billiards.cpp
  startup and callback wrappers only
```

`RenderResources` should stop being assembled manually in `main`. Instead:

```cpp
bool initializeRenderResources(RenderResources& resources, GameState& state);
void destroyRenderResources(RenderResources& resources);
```

The initializer should load textures, load OBJ files, create VBOs, bind ball texture IDs into `GameState`, and return `false` with clear stderr output if a required resource fails.

## Testing and Verification

Existing tests remain required:

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Additional focused tests should be added or extended:

- `BilliardsInputTests`: camera orbit, trackpad orbit, shot charge/release, Help blocking input.
- `BilliardsAssetsTests`: resource path loading from build directory and missing-image failure.
- CTest screenshot entries for default/help/after-shot remain required.

Manual or automated source scans become acceptance checks:

```bash
rg -n "at\\[|leftm|rightm|TrackpadOrbit|\\bspeed\\b|CurrPlayer|NextPlayer|IsIllegal|ShowHelp|WindowedMode" src/Billiards/billiards.cpp
rg -n "initTable|initCue|initDecoration|initLight|loadTexture|ObjLoader|glGenBuffers|glTex|glLight|drawHelp|drawString" src/Billiards/billiards.cpp
wc -l src/Billiards/billiards.cpp
```

Expected result:

- No authoritative runtime-state globals remain in `billiards.cpp`.
- No render-resource initialization functions remain in `billiards.cpp`.
- `billiards.cpp` is materially smaller and primarily callback glue. A target under 500 lines is preferred, but the harder requirement is ownership, not line count.

## Risks

- Input migration can break trackpad orbit or shot charging if state transitions are not covered by tests.
- Resource migration can silently break textures if texture IDs are not copied into `GameState`.
- Moving OpenGL setup can create initialization-order bugs if GLEW/VBO setup happens before the context exists.

The implementation should therefore migrate in two separate commits:

1. Authoritative state/input/camera cleanup.
2. Render resource and glue-main cleanup.

Each commit must pass CTest and the three screenshot CTest entries.

## Definition of Done

- `GameState` owns active camera, input, shot, HUD, config, player, and ball state.
- `billiards.cpp` contains no legacy authoritative state globals.
- `billiards.cpp` contains no texture loading, VBO setup, OBJ ownership, lighting setup, or HUD drawing helpers.
- Runtime still supports windowed default, optional fullscreen, Help, trackpad orbit, shooting, physics/rules, and screenshot scenes.
- `ctest --test-dir build --output-on-failure` passes from a clean build directory.
