# BilliardGL Next Evolution Stages Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete the five-stage BilliardGL architecture evolution so `GameState`, physics/rules, renderer, screenshot verification, and asset loading own their intended responsibilities.

**Architecture:** Preserve CMake, C++11, GLUT/freeglut, fixed-pipeline OpenGL, SDL audio, and existing assets. Move ownership in small reversible stages: first make `GameState` authoritative, then route runtime gameplay through tested modules, then move rendering/resource responsibilities, then expand screenshot verification, and finally upgrade image loading.

**Tech Stack:** C++11, CMake, OpenGL/GLU, GLEW, GLUT/freeglut, SDL2/SDL2_mixer, existing `vec`, `ObjLoader`, `particle`, `platform_audio`, `resource_path`, `screenshot`, and `/tmp` visual artifacts.

---

## File Structure

Create or modify these files across the five stages:

- Modify: `src/Billiards/game_state.h`
- Modify: `src/Billiards/game_state.cpp`
- Modify: `src/Billiards/physics.h`
- Modify: `src/Billiards/physics.cpp`
- Modify: `src/Billiards/rules.h`
- Modify: `src/Billiards/rules.cpp`
- Modify: `src/Billiards/input.h`
- Modify: `src/Billiards/input.cpp`
- Modify: `src/Billiards/renderer.h`
- Modify: `src/Billiards/renderer.cpp`
- Modify: `src/Billiards/hud.h`
- Modify: `src/Billiards/hud.cpp`
- Modify: `src/Billiards/assets.h`
- Modify: `src/Billiards/assets.cpp`
- Modify: `src/Billiards/screenshot.h`
- Modify: `src/Billiards/screenshot.cpp`
- Modify: `src/Billiards/screenshot_gl.cpp`
- Modify: `src/Billiards/billiards.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/physics_tests.cpp`
- Modify: `tests/rules_tests.cpp`
- Modify: `tests/screenshot_tests.cpp`
- Create: `tests/game_state_tests.cpp`
- Create: `tests/assets_tests.cpp`
- Create: `src/Billiards/render_resources.h`
- Create: `src/Billiards/render_resources.cpp`
- Create: `src/Billiards/image_loader.h`
- Create: `src/Billiards/image_loader.cpp`
- Create: `src/Billiards/dependencies/include/stb_image.h`

Ownership targets:

- `billiards.cpp`: process arguments, initialize state/resources, register GLUT callbacks, dispatch callbacks.
- `game_state`: authoritative runtime model and conversion helpers required during migration.
- `physics`: movement, friction, collisions, pockets, and physical event production without OpenGL or audio dependencies.
- `rules`: player state, ball assignment, shot completion, foul/game-over state transitions without rendering dependencies.
- `input`: keyboard/mouse/trackpad state mutations on `GameState`.
- `renderer` and `render_resources`: fixed-pipeline 3D rendering and render-only resources.
- `hud`: 2D status, persistent Help hint, and Help overlay.
- `screenshot`: PPM writing and deterministic screenshot verification helpers.
- `assets` and `image_loader`: path resolution, image decoding, and clear resource failures.

## Stage 0: Prepare an Isolated Worktree

**Files:**
- No source files.

- [ ] **Step 1: Confirm master is clean and includes the plan commit**

Run:

```bash
git status --short --branch
git log --oneline -3
```

Expected:

```text
## master...origin/master [ahead 2]
1e14079 Document next BilliardGL evolution stages
<new plan commit> Add next evolution stages implementation plan
```

If `master` is only ahead by one commit because this plan has not been committed yet, commit this plan first before creating the worktree.

- [ ] **Step 2: Create the implementation worktree**

Run:

```bash
mkdir -p /Users/bytedance/.config/superpowers/worktrees/BilliardGL
git worktree add -b codex/next-evolution-stages /Users/bytedance/.config/superpowers/worktrees/BilliardGL/next-evolution-stages master
```

Expected:

```text
Preparing worktree (new branch 'codex/next-evolution-stages')
HEAD is now at <commit> <latest commit message>
```

- [ ] **Step 3: Configure and baseline-test the worktree**

Run:

```bash
cd /Users/bytedance/.config/superpowers/worktrees/BilliardGL/next-evolution-stages
cmake -S . -B build
cmake --build build
./build/BilliardsPhysicsTests
./build/BilliardsRulesTests
./build/BilliardsScreenshotTests
```

Expected:

```text
[100%] Built target Billiards
```

Each test executable should exit with status `0` and print no failure message.

## Task 1: Make `GameState` the Authoritative Runtime State

**Files:**
- Modify: `src/Billiards/game_state.h`
- Modify: `src/Billiards/game_state.cpp`
- Modify: `src/Billiards/billiards.cpp`
- Create: `tests/game_state_tests.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add state fields needed by current runtime**

Update `src/Billiards/game_state.h` so runtime globals can move into the model. Add these fields without removing existing fields:

```cpp
struct CameraState {
    float target[3] = {0.0f, kTableHeight + kBallRadius, -kTableInLength / 4.0f};
    float eye[3] = {0.0f, 200.0f, -kTableInLength / 4.0f};
    float zoom = 120.0f;
    float angleX = -kPi / 2.0f;
    float angleY = kPi / 3.0f;
    float previousAngleX = 0.0f;
    float previousAngleY = 0.0f;
    float previousTargetX = 0.0f;
    float previousTargetY = 0.0f;
};

struct PlayerState {
    int assignedBallType[2] = {-1, -1};
    bool firstPocketedObjectBall = true;
    int currentPlayer = 0;
    int nextPlayer = 0;
    bool illegalShot = false;
    bool updatedAfterShot = false;
    bool shotTaken = false;
    bool aimingAtCueBall = true;
};

struct RuntimeConfig {
    bool windowedMode = true;
    int width = 1024;
    int height = 768;
    std::string screenshotPath;
};
```

Add `#include <string>` at the top of `game_state.h`.

- [ ] **Step 2: Add legacy ball adapter helpers**

In `src/Billiards/game_state.h`, add a small adapter type matching the fields `billiards.cpp` still renders:

```cpp
struct LegacyBallAdapter {
    Point3 position;
    Point3 velocity;
    Point3 rotationAxis;
    float speed = 0.0f;
    float rotationAngle = 0.0f;
    bool pocketed = false;
    unsigned int texture = 0;
};

void copyBallStateToLegacy(const GameState& state, std::array<LegacyBallAdapter, kBallCount>& legacyBalls);
void copyLegacyTexturesToState(const std::array<LegacyBallAdapter, kBallCount>& legacyBalls, GameState& state);
```

In `src/Billiards/game_state.cpp`, implement the helpers:

```cpp
void copyBallStateToLegacy(const GameState& state, std::array<LegacyBallAdapter, kBallCount>& legacyBalls)
{
    for (int i = 0; i < kBallCount; ++i) {
        legacyBalls[i].position = state.balls[i].position;
        legacyBalls[i].velocity = state.balls[i].velocity;
        legacyBalls[i].rotationAxis = state.balls[i].rotationAxis;
        legacyBalls[i].speed = state.balls[i].speed;
        legacyBalls[i].rotationAngle = state.balls[i].rotationAngle;
        legacyBalls[i].pocketed = state.balls[i].pocketed;
        legacyBalls[i].texture = state.balls[i].texture;
    }
}

void copyLegacyTexturesToState(const std::array<LegacyBallAdapter, kBallCount>& legacyBalls, GameState& state)
{
    for (int i = 0; i < kBallCount; ++i) {
        state.balls[i].texture = legacyBalls[i].texture;
    }
}
```

- [ ] **Step 3: Write a failing `GameState` adapter test**

Create `tests/game_state_tests.cpp`:

```cpp
#include "game_state.h"

#include <array>
#include <cstdlib>
#include <iostream>

namespace {

int fail(const char* message)
{
    std::cerr << message << std::endl;
    return EXIT_FAILURE;
}

}  // namespace

int main()
{
    billiardgl::GameState state;
    billiardgl::initializeBalls(state);
    state.balls[0].position = billiardgl::Point3{1.0f, 2.0f, 3.0f};
    state.balls[0].velocity = billiardgl::Point3{4.0f, 0.0f, 5.0f};
    state.balls[0].rotationAxis = billiardgl::Point3{0.0f, 1.0f, 0.0f};
    state.balls[0].speed = 6.0f;
    state.balls[0].rotationAngle = 7.0f;
    state.balls[0].pocketed = true;
    state.balls[0].texture = 42;

    std::array<billiardgl::LegacyBallAdapter, billiardgl::kBallCount> legacyBalls;
    billiardgl::copyBallStateToLegacy(state, legacyBalls);

    if (legacyBalls[0].position.x != 1.0f || legacyBalls[0].position.y != 2.0f || legacyBalls[0].position.z != 3.0f) {
        return fail("legacy adapter should copy position from GameState");
    }
    if (legacyBalls[0].velocity.x != 4.0f || legacyBalls[0].velocity.z != 5.0f) {
        return fail("legacy adapter should copy velocity from GameState");
    }
    if (!legacyBalls[0].pocketed || legacyBalls[0].texture != 42) {
        return fail("legacy adapter should copy pocket and texture fields");
    }

    legacyBalls[1].texture = 99;
    billiardgl::copyLegacyTexturesToState(legacyBalls, state);
    if (state.balls[1].texture != 99) {
        return fail("texture copy should preserve render-loaded texture IDs in GameState");
    }

    return EXIT_SUCCESS;
}
```

- [ ] **Step 4: Add the test target**

Modify `CMakeLists.txt`:

```cmake
add_executable(BilliardsGameStateTests
    tests/game_state_tests.cpp
    ${BILLIARDGL_CORE_SOURCES}
)

target_include_directories(BilliardsGameStateTests PRIVATE
    src/Billiards
)
```

- [ ] **Step 5: Run the new test and confirm it fails before implementation**

Run:

```bash
cmake --build build --target BilliardsGameStateTests
./build/BilliardsGameStateTests
```

Expected before implementing the helpers:

```text
error: no member named 'LegacyBallAdapter'
```

or another compile failure proving the test reaches the missing API.

- [ ] **Step 6: Convert `billiards.cpp` to write runtime state through `GameState`**

Replace legacy authoritative globals with accessors or references to `Game`. The target pattern is:

```cpp
static billiardgl::GameState Game;
static std::array<billiardgl::LegacyBallAdapter, billiardgl::kBallCount> RenderBalls;

static bool& showHelp()
{
    return Game.hud.showHelp;
}

static int& currentPlayer()
{
    return Game.players.currentPlayer;
}

static int& nextPlayer()
{
    return Game.players.nextPlayer;
}

static bool& illegalShot()
{
    return Game.players.illegalShot;
}
```

Then replace runtime uses:

```text
ShowHelp -> showHelp()
CurrPlayer -> currentPlayer()
NextPlayer -> nextPlayer()
IsIllegal -> illegalShot()
WindowedMode -> Game.config.windowedMode
ScreenshotPath -> Game.config.screenshotPath
Ball[i].p -> Game.balls[i].position
Ball[i].v -> Game.balls[i].velocity
Ball[i].a -> Game.balls[i].rotationAxis
Ball[i].mv -> Game.balls[i].speed
Ball[i].ma -> Game.balls[i].rotationAngle
Ball[i].isIn -> Game.balls[i].pocketed
Ball[i].texture -> Game.balls[i].texture
```

For rendering-only code that cannot be moved yet, call:

```cpp
billiardgl::copyBallStateToLegacy(Game, RenderBalls);
```

immediately before rendering functions that still expect the legacy shape.

- [ ] **Step 7: Remove duplicate initialization**

Replace manual ball rack initialization in `billiards.cpp` with:

```cpp
billiardgl::initializeBalls(Game);
```

After textures are loaded, preserve texture IDs in `Game.balls[i].texture`. Do not keep a second ball-position initialization path.

- [ ] **Step 8: Run state and regression tests**

Run:

```bash
cmake --build build
./build/BilliardsGameStateTests
./build/BilliardsPhysicsTests
./build/BilliardsRulesTests
./build/BilliardsScreenshotTests
mkdir -p /tmp/billiardgl-next-evolution
./build/Billiards --screenshot /tmp/billiardgl-next-evolution/stage1-default.ppm
```

Expected:

```text
P6
pixel_bytes= 2359296
nonzero_bytes= <positive number>
```

- [ ] **Step 9: Commit Stage 1**

Run:

```bash
git add CMakeLists.txt src/Billiards/game_state.h src/Billiards/game_state.cpp src/Billiards/billiards.cpp tests/game_state_tests.cpp
git commit -m "Use GameState as runtime state source"
```

## Task 2: Route Runtime Gameplay Through `physics` and `rules`

**Files:**
- Modify: `src/Billiards/game_state.h`
- Modify: `src/Billiards/physics.h`
- Modify: `src/Billiards/physics.cpp`
- Modify: `src/Billiards/rules.h`
- Modify: `src/Billiards/rules.cpp`
- Modify: `src/Billiards/billiards.cpp`
- Modify: `tests/physics_tests.cpp`
- Modify: `tests/rules_tests.cpp`

- [ ] **Step 1: Add testable gameplay events**

In `game_state.h`, add event fields that isolate physics from audio/rendering:

```cpp
struct GameplayEvents {
    bool ballCollision = false;
    bool railCollision = false;
    bool ballPocketed = false;
    bool cueBallPocketed = false;
    bool eightBallPocketed = false;
    bool shotEnded = false;
};

struct GameState {
    std::array<BallState, kBallCount> balls;
    CameraState camera;
    PlayerState players;
    InputState input;
    HudState hud;
    RuntimeConfig config;
    GameplayEvents events;
    int pocketedBallCount = 0;
    bool ballsMoving = false;
    bool transitionPerspective = false;
    bool perspectiveRecorded = false;
    bool gameOver = false;
};

void clearGameplayEvents(GameState& state);
```

Implement in `game_state.cpp`:

```cpp
void clearGameplayEvents(GameState& state)
{
    state.events = GameplayEvents{};
}
```

- [ ] **Step 2: Extend physics tests for pocket event behavior**

Add to `tests/physics_tests.cpp`:

```cpp
    billiardgl::GameState pocketState;
    billiardgl::initializeBalls(pocketState);
    pocketState.balls[0].position.x = -billiardgl::kTableInWidth / 2.0f + billiardgl::kPocketRadius;
    pocketState.balls[0].position.z = -billiardgl::kTableInLength / 2.0f + billiardgl::kPocketRadius;
    if (!billiardgl::updatePocketedBall(pocketState, 0)) {
        return fail("cue ball at pocket center should be updated as pocketed");
    }
    if (!pocketState.players.illegalShot || !pocketState.events.cueBallPocketed) {
        return fail("cue ball pocket should mark illegal shot and cue pocket event");
    }

    billiardgl::GameState eightState;
    billiardgl::initializeBalls(eightState);
    eightState.balls[8].position.x = -billiardgl::kTableInWidth / 2.0f + billiardgl::kPocketRadius;
    eightState.balls[8].position.z = -billiardgl::kTableInLength / 2.0f + billiardgl::kPocketRadius;
    if (!billiardgl::updatePocketedBall(eightState, 8)) {
        return fail("eight ball at pocket center should be updated as pocketed");
    }
    if (!eightState.gameOver || !eightState.events.eightBallPocketed) {
        return fail("eight ball pocket should mark game over and eight ball event");
    }
```

- [ ] **Step 3: Extend rules tests for object assignment and player continuation**

Add to `tests/rules_tests.cpp`:

```cpp
    billiardgl::GameState assignmentState;
    assignmentState.players.currentPlayer = 0;
    assignmentState.players.nextPlayer = 1;
    billiardgl::assignPlayerBallTypeForPocketedObjectBall(assignmentState, 9);
    if (assignmentState.players.assignedBallType[0] != 1 || assignmentState.players.assignedBallType[1] != 0) {
        return fail("first pocketed stripe should assign stripes to current player");
    }
    if (assignmentState.players.nextPlayer != 0) {
        return fail("first valid pocket should let current player continue");
    }

    billiardgl::GameState wrongTypeState;
    wrongTypeState.players.currentPlayer = 0;
    wrongTypeState.players.assignedBallType[0] = 0;
    wrongTypeState.players.assignedBallType[1] = 1;
    wrongTypeState.players.firstPocketedObjectBall = false;
    billiardgl::assignPlayerBallTypeForPocketedObjectBall(wrongTypeState, 9);
    if (!wrongTypeState.players.illegalShot) {
        return fail("pocketing opponent type should be illegal");
    }
```

- [ ] **Step 4: Run tests and confirm they fail before implementation**

Run:

```bash
cmake --build build --target BilliardsPhysicsTests BilliardsRulesTests
./build/BilliardsPhysicsTests
./build/BilliardsRulesTests
```

Expected before event implementation:

```text
error: no member named 'events'
```

or a failing assertion for event behavior.

- [ ] **Step 5: Emit events from physics without audio dependencies**

Update `physics.cpp` by keeping the current `collideBalls` calculation and changing `updatePhysics` to this shape:

```cpp
void updatePhysics(GameState& state, float timeStep)
{
    clearGameplayEvents(state);
    const bool wasMoving = state.ballsMoving;
    bool anyMoving = false;

    for (int i = 0; i < kBallCount; ++i) {
        BallState& ball = state.balls[i];
        if (ball.pocketed) {
            continue;
        }
        for (int j = i + 1; j < kBallCount; ++j) {
            if (!state.balls[j].pocketed && collideBalls(ball, state.balls[j])) {
                state.events.ballCollision = true;
            }
        }
        const float previousX = ball.velocity.x;
        const float previousZ = ball.velocity.z;
        collideWithTableEdge(ball);
        if (previousX != ball.velocity.x || previousZ != ball.velocity.z) {
            state.events.railCollision = true;
        }
        updatePocketedBall(state, i);
        applyFrictionAndMove(ball, timeStep, kDefaultFrictionAcceleration);
        if (ball.speed > 0.0f) {
            anyMoving = true;
        }
    }

    state.ballsMoving = anyMoving;
    state.events.shotEnded = wasMoving && !anyMoving && state.players.shotTaken;
}
```

Update `updatePocketedBall` to set:

```cpp
state.events.ballPocketed = true;
state.events.cueBallPocketed = true;
state.events.eightBallPocketed = true;
```

only for the matching pocket cases.

- [ ] **Step 6: Route pocketed object balls into rules**

In `physics.cpp`, after non-cue, non-eight object balls are pocketed, call:

```cpp
assignPlayerBallTypeForPocketedObjectBall(state, ballIndex);
```

Add `#include "rules.h"` to `physics.cpp`. Keep `rules.cpp` independent from OpenGL.

- [ ] **Step 7: Replace old `myIdle` physics path**

In `billiards.cpp`, replace the manual movement/collision/pocket block in `myIdle` with:

```cpp
if (Game.input.hitRequested) {
    const float dx = Game.camera.target[0] - Game.camera.eye[0];
    const float dz = Game.camera.target[2] - Game.camera.eye[2];
    const float length = std::sqrt(dx * dx + dz * dz);
    if (length > 0.0f) {
        billiardgl::setBallVelocity(Game.balls[0], speed * dx / length, 0.0f, speed * dz / length);
        Game.players.shotTaken = true;
        Game.players.updatedAfterShot = false;
        Game.ballsMoving = true;
    }
    speed = 0.0f;
    Game.input.hitRequested = false;
}

billiardgl::updatePhysics(Game, billiardgl::kDefaultTimeStep);

if (Game.events.shotEnded) {
    billiardgl::updatePlayerAfterShot(Game);
}

billiardgl::updateCameraFromCueBall(Game);
glutPostRedisplay();
```

Keep any camera-transition behavior that still exists, but make it read and write `Game.camera` and `Game.transitionPerspective`.

- [ ] **Step 8: Map gameplay events to audio**

In `billiards.cpp`, after `billiardgl::updatePhysics(Game, billiardgl::kDefaultTimeStep)`, play existing sounds based on events:

```cpp
if (Game.events.ballCollision || Game.events.railCollision) {
    billiardgl::playHit();
}
if (Game.events.ballPocketed || Game.events.cueBallPocketed) {
    billiardgl::playBallIn();
}
if (Game.events.eightBallPocketed) {
    billiardgl::playGameOver();
}
```

Keep these calls near the GLUT/runtime layer; do not add audio dependencies to `physics.cpp`.

- [ ] **Step 9: Run regression tests and a deterministic screenshot**

Run:

```bash
cmake --build build
./build/BilliardsGameStateTests
./build/BilliardsPhysicsTests
./build/BilliardsRulesTests
./build/BilliardsScreenshotTests
mkdir -p /tmp/billiardgl-next-evolution
./build/Billiards --screenshot /tmp/billiardgl-next-evolution/stage2-default.ppm
```

Expected: all tests exit `0`; screenshot command prints a positive `nonzero_bytes` value.

- [ ] **Step 10: Commit Stage 2**

Run:

```bash
git add src/Billiards/game_state.h src/Billiards/game_state.cpp src/Billiards/physics.h src/Billiards/physics.cpp src/Billiards/rules.h src/Billiards/rules.cpp src/Billiards/billiards.cpp tests/physics_tests.cpp tests/rules_tests.cpp
git commit -m "Run gameplay through physics and rules modules"
```

## Task 3: Move Scene Rendering and Render Resources Out of `billiards.cpp`

**Files:**
- Modify: `src/Billiards/renderer.h`
- Modify: `src/Billiards/renderer.cpp`
- Create: `src/Billiards/render_resources.h`
- Create: `src/Billiards/render_resources.cpp`
- Modify: `src/Billiards/assets.h`
- Modify: `src/Billiards/assets.cpp`
- Modify: `src/Billiards/billiards.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Introduce render resource ownership**

Create `src/Billiards/render_resources.h`:

```cpp
#pragma once

#include "game_state.h"

#include <array>

class ObjModel;
class emitter;

namespace billiardgl {

struct RenderResources {
    std::array<unsigned int, kBallCount> ballTextures;
    unsigned int wallTexture = 0;
    unsigned int floorTexture = 0;
    unsigned int tableTexture = 0;
    unsigned int cueTexture = 0;
    ObjModel* cueModel = nullptr;
    std::array<emitter*, kBallCount> emitters;
};

void applyBallTexturesToState(const RenderResources& resources, GameState& state);

}  // namespace billiardgl
```

Create `src/Billiards/render_resources.cpp`:

```cpp
#include "render_resources.h"

namespace billiardgl {

void applyBallTexturesToState(const RenderResources& resources, GameState& state)
{
    for (int i = 0; i < kBallCount; ++i) {
        state.balls[i].texture = resources.ballTextures[i];
    }
}

}  // namespace billiardgl
```

- [ ] **Step 2: Add `render_resources.cpp` to CMake**

Modify the `Billiards` executable sources:

```cmake
add_executable(Billiards
    src/Billiards/billiards.cpp
    src/Billiards/hud.cpp
    src/Billiards/renderer.cpp
    src/Billiards/render_resources.cpp
    src/Billiards/screenshot.cpp
    src/Billiards/screenshot_gl.cpp
    src/Billiards/ObjLoader.cpp
    src/Billiards/particle.cpp
    src/Billiards/platform_audio.cpp
    ${BILLIARDGL_CORE_SOURCES}
)
```

- [ ] **Step 3: Move one render function at a time**

Move functions from `billiards.cpp` to `renderer.cpp` in this order:

```text
renderRoom
renderTable
renderDecoration
renderBall
renderCue
renderRect if it is still 3D-scene related
```

For each moved function:

1. Copy the function body into `renderer.cpp`.
2. Replace direct global reads with parameters from `const GameState& state` and `const RenderResources& resources`.
3. Build.
4. Delete the old function body from `billiards.cpp`.

The target renderer API should become:

```cpp
void renderScene(const GameState& state, const RenderResources& resources);
```

instead of `RenderHooks`.

- [ ] **Step 4: Keep HUD rendering separate**

Keep these calls in `hud.cpp`:

```cpp
billiardgl::drawHud(Game);
billiardgl::drawHelpPrompt(Game);
billiardgl::drawHelpOverlay(Game);
```

Do not move HUD text drawing into `renderer.cpp`.

- [ ] **Step 5: Update `billiards.cpp` display glue**

The final `myDisplay` shape should be:

```cpp
void myDisplay(void)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    setupCameraFromGameState(Game);
    billiardgl::renderScene(Game, RenderResources);
    billiardgl::drawHud(Game);
    billiardgl::drawHelpPrompt(Game);
    billiardgl::drawHelpOverlay(Game);
    glutSwapBuffers();
}
```

Use the actual local camera setup helper name. If no helper exists yet, create a `static void setupCameraFromGameState(const billiardgl::GameState& state)` inside `billiards.cpp`.

- [ ] **Step 6: Verify `billiards.cpp` is materially smaller**

Run:

```bash
wc -l src/Billiards/billiards.cpp src/Billiards/renderer.cpp src/Billiards/render_resources.cpp
rg -n "void render(Room|Table|Decoration|Ball|Cue|Rect)" src/Billiards/billiards.cpp
```

Expected:

```text
rg should return no scene render function definitions in billiards.cpp
```

- [ ] **Step 7: Run build, tests, and screenshot**

Run:

```bash
cmake --build build
./build/BilliardsGameStateTests
./build/BilliardsPhysicsTests
./build/BilliardsRulesTests
./build/BilliardsScreenshotTests
mkdir -p /tmp/billiardgl-next-evolution
./build/Billiards --screenshot /tmp/billiardgl-next-evolution/stage3-default.ppm
```

Expected: all tests pass; screenshot has positive `nonzero_bytes`.

- [ ] **Step 8: Commit Stage 3**

Run:

```bash
git add CMakeLists.txt src/Billiards/renderer.h src/Billiards/renderer.cpp src/Billiards/render_resources.h src/Billiards/render_resources.cpp src/Billiards/assets.h src/Billiards/assets.cpp src/Billiards/billiards.cpp
git commit -m "Move scene rendering out of main file"
```

## Task 4: Add Deterministic Screenshot Scenes and Visual Checks

**Files:**
- Modify: `src/Billiards/game_state.h`
- Modify: `src/Billiards/screenshot.h`
- Modify: `src/Billiards/screenshot.cpp`
- Modify: `src/Billiards/screenshot_gl.cpp`
- Modify: `src/Billiards/billiards.cpp`
- Modify: `tests/screenshot_tests.cpp`

- [ ] **Step 1: Add screenshot scene configuration**

In `game_state.h`, extend `RuntimeConfig`:

```cpp
enum class ScreenshotScene {
    Default,
    Help,
    AfterShot
};

struct RuntimeConfig {
    bool windowedMode = true;
    int width = 1024;
    int height = 768;
    std::string screenshotPath;
    ScreenshotScene screenshotScene = ScreenshotScene::Default;
};
```

- [ ] **Step 2: Parse `--screenshot-scene`**

In `billiards.cpp`, add argument handling:

```cpp
else if (strcmp(argv[i], "--screenshot-scene") == 0 && i + 1 < argc) {
    const char* scene = argv[++i];
    if (strcmp(scene, "default") == 0) {
        Game.config.screenshotScene = billiardgl::ScreenshotScene::Default;
    } else if (strcmp(scene, "help") == 0) {
        Game.config.screenshotScene = billiardgl::ScreenshotScene::Help;
    } else if (strcmp(scene, "after-shot") == 0) {
        Game.config.screenshotScene = billiardgl::ScreenshotScene::AfterShot;
    } else {
        std::cerr << "Unknown screenshot scene: " << scene << std::endl;
        return EXIT_FAILURE;
    }
}
```

- [ ] **Step 3: Prepare deterministic scenes before first screenshot render**

Add a helper in `billiards.cpp`:

```cpp
static void prepareScreenshotScene()
{
    if (Game.config.screenshotScene == billiardgl::ScreenshotScene::Help) {
        Game.hud.showHelp = true;
        return;
    }

    if (Game.config.screenshotScene == billiardgl::ScreenshotScene::AfterShot) {
        billiardgl::setBallVelocity(Game.balls[0], 30.0f, 0.0f, 0.0f);
        Game.players.shotTaken = true;
        for (int i = 0; i < 30; ++i) {
            billiardgl::updatePhysics(Game, billiardgl::kDefaultTimeStep);
        }
        billiardgl::updateCameraFromCueBall(Game);
    }
}
```

Call it after ball/resource initialization and before entering screenshot capture mode.

- [ ] **Step 4: Add reusable image-region checks**

In `screenshot.h`, add:

```cpp
int countVisiblePixelsInRegion(const std::vector<unsigned char>& rgb, int width, int height, int x, int y, int regionWidth, int regionHeight);
```

In `screenshot.cpp`, implement:

```cpp
int countVisiblePixelsInRegion(const std::vector<unsigned char>& rgb, int width, int height, int x, int y, int regionWidth, int regionHeight)
{
    int count = 0;
    const int xEnd = x + regionWidth;
    const int yEnd = y + regionHeight;
    for (int row = y; row < yEnd && row < height; ++row) {
        for (int col = x; col < xEnd && col < width; ++col) {
            const int index = (row * width + col) * 3;
            if (index + 2 < static_cast<int>(rgb.size()) &&
                (rgb[index] != 0 || rgb[index + 1] != 0 || rgb[index + 2] != 0)) {
                ++count;
            }
        }
    }
    return count;
}
```

- [ ] **Step 5: Extend screenshot tests**

Add to `tests/screenshot_tests.cpp`:

```cpp
    const std::vector<unsigned char> regionImage = {
        0, 0, 0,   0, 0, 0,   0, 0, 0,
        0, 0, 0,   5, 6, 7,   0, 0, 0,
        0, 0, 0,   0, 0, 0,   8, 9, 10,
    };
    if (billiardgl::countVisiblePixelsInRegion(regionImage, 3, 3, 1, 1, 2, 2) != 2) {
        return fail("region visible pixel count should detect non-black pixels in bounds");
    }
```

- [ ] **Step 6: Run all screenshot scenes**

Run:

```bash
cmake --build build
./build/BilliardsScreenshotTests
mkdir -p /tmp/billiardgl-next-evolution
./build/Billiards --screenshot /tmp/billiardgl-next-evolution/default.ppm --screenshot-scene default
./build/Billiards --screenshot /tmp/billiardgl-next-evolution/help.ppm --screenshot-scene help
./build/Billiards --screenshot /tmp/billiardgl-next-evolution/after-shot.ppm --screenshot-scene after-shot
ls -lh /tmp/billiardgl-next-evolution/default.ppm /tmp/billiardgl-next-evolution/help.ppm /tmp/billiardgl-next-evolution/after-shot.ppm
```

Expected: three non-empty PPM files under `/tmp/billiardgl-next-evolution`.

- [ ] **Step 7: Commit Stage 4**

Run:

```bash
git add src/Billiards/game_state.h src/Billiards/screenshot.h src/Billiards/screenshot.cpp src/Billiards/screenshot_gl.cpp src/Billiards/billiards.cpp tests/screenshot_tests.cpp
git commit -m "Add deterministic screenshot scenes"
```

## Task 5: Upgrade Image Loading Through an Asset Layer

**Files:**
- Modify: `src/Billiards/assets.h`
- Modify: `src/Billiards/assets.cpp`
- Create: `src/Billiards/image_loader.h`
- Create: `src/Billiards/image_loader.cpp`
- Create: `src/Billiards/dependencies/include/stb_image.h`
- Create: `tests/assets_tests.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/Billiards/billiards.cpp`

- [ ] **Step 1: Add an image loader interface**

Create `src/Billiards/image_loader.h`:

```cpp
#pragma once

#include <string>
#include <vector>

namespace billiardgl {

struct ImageData {
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<unsigned char> pixels;
    std::string error;
};

ImageData loadImageFile(const std::string& path);

}  // namespace billiardgl
```

- [ ] **Step 2: Vendor `stb_image.h`**

Add `src/Billiards/dependencies/include/stb_image.h` from the official single-header `stb_image` distribution. Keep the original license header intact.

- [ ] **Step 3: Implement the loader**

Create `src/Billiards/image_loader.cpp`:

```cpp
#include "image_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace billiardgl {

ImageData loadImageFile(const std::string& path)
{
    ImageData image;
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (pixels == nullptr) {
        image.error = stbi_failure_reason() ? stbi_failure_reason() : "unknown image loading error";
        return image;
    }

    image.width = width;
    image.height = height;
    image.channels = 4;
    image.pixels.assign(pixels, pixels + width * height * 4);
    stbi_image_free(pixels);
    return image;
}

}  // namespace billiardgl
```

- [ ] **Step 4: Add asset tests**

Create `tests/assets_tests.cpp`:

```cpp
#include "image_loader.h"

#include <cstdlib>
#include <iostream>

namespace {

int fail(const char* message)
{
    std::cerr << message << std::endl;
    return EXIT_FAILURE;
}

}  // namespace

int main()
{
    const billiardgl::ImageData missing = billiardgl::loadImageFile("/tmp/billiardgl-file-that-does-not-exist.png");
    if (missing.error.empty()) {
        return fail("missing image should return a clear error");
    }

    return EXIT_SUCCESS;
}
```

- [ ] **Step 5: Wire the new test and source into CMake**

Modify `CMakeLists.txt`:

```cmake
set(BILLIARDGL_CORE_SOURCES
    src/Billiards/assets.cpp
    src/Billiards/game_state.cpp
    src/Billiards/image_loader.cpp
    src/Billiards/physics.cpp
    src/Billiards/resource_path.cpp
    src/Billiards/rules.cpp
    src/Billiards/input.cpp
    src/Billiards/vec.cpp
)

add_executable(BilliardsAssetsTests
    tests/assets_tests.cpp
    ${BILLIARDGL_CORE_SOURCES}
)

target_include_directories(BilliardsAssetsTests PRIVATE
    src/Billiards
    src/Billiards/dependencies/include
)
```

- [ ] **Step 6: Replace the old BMP-only texture loader**

In `billiards.cpp` or the renderer resource initialization file, replace the hand-written BMP loader with a texture upload function that uses `ImageData`:

```cpp
static GLuint loadTextureFromImage(const std::string& path)
{
    const billiardgl::ImageData image = billiardgl::loadImageFile(path);
    if (!image.error.empty()) {
        std::cerr << "Failed to load texture '" << path << "': " << image.error << std::endl;
        return 0;
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.pixels.data());
    return texture;
}
```

Keep the old BMP assets and filenames. The important change is that the loading path can now decode BMP, PNG, or JPG through the same function.

- [ ] **Step 7: Verify existing BMP assets still render**

Run:

```bash
cmake --build build
./build/BilliardsAssetsTests
./build/BilliardsGameStateTests
./build/BilliardsPhysicsTests
./build/BilliardsRulesTests
./build/BilliardsScreenshotTests
mkdir -p /tmp/billiardgl-next-evolution
./build/Billiards --screenshot /tmp/billiardgl-next-evolution/stage5-default.ppm --screenshot-scene default
```

Expected: all tests pass; screenshot has positive `nonzero_bytes`; missing image test reports no failure.

- [ ] **Step 8: Commit Stage 5**

Run:

```bash
git add CMakeLists.txt src/Billiards/assets.h src/Billiards/assets.cpp src/Billiards/image_loader.h src/Billiards/image_loader.cpp src/Billiards/dependencies/include/stb_image.h src/Billiards/billiards.cpp tests/assets_tests.cpp
git commit -m "Load textures through image asset layer"
```

## Final Verification and PR

- [ ] **Step 1: Run full local verification**

Run:

```bash
cmake --build build
./build/BilliardsGameStateTests
./build/BilliardsPhysicsTests
./build/BilliardsRulesTests
./build/BilliardsScreenshotTests
./build/BilliardsAssetsTests
mkdir -p /tmp/billiardgl-next-evolution
./build/Billiards --screenshot /tmp/billiardgl-next-evolution/final-default.ppm --screenshot-scene default
./build/Billiards --screenshot /tmp/billiardgl-next-evolution/final-help.ppm --screenshot-scene help
./build/Billiards --screenshot /tmp/billiardgl-next-evolution/final-after-shot.ppm --screenshot-scene after-shot
```

Expected: all commands exit `0`; the three screenshot files exist under `/tmp/billiardgl-next-evolution`.

- [ ] **Step 2: Inspect source ownership**

Run:

```bash
rg -n "Ball\\[|CurrPlayer|NextPlayer|IsIllegal|ShowHelp|WindowedMode|void render(Room|Table|Decoration|Ball|Cue)" src/Billiards/billiards.cpp
wc -l src/Billiards/billiards.cpp
```

Expected:

```text
No legacy authoritative state variables remain.
No scene render function definitions remain in billiards.cpp.
```

`billiards.cpp` should be materially smaller than the current 1469-line baseline.

- [ ] **Step 3: Push branch and open PR with `gh`**

Run:

```bash
git status --short --branch
git push -u origin codex/next-evolution-stages
gh pr create --base master --head codex/next-evolution-stages --title "Complete BilliardGL next evolution stages" --body "Completes the staged architecture evolution: GameState runtime ownership, physics/rules runtime path, renderer extraction, deterministic screenshot verification, and upgraded image loading."
```

Expected:

```text
https://github.com/maxjchuang/BilliardGL/pull/<number>
```

## Spec Coverage Review

- Stage 1 in the spec is covered by Task 1.
- Stage 2 in the spec is covered by Task 2.
- Stage 3 in the spec is covered by Task 3.
- Stage 4 in the spec is covered by Task 4.
- Stage 5 in the spec is covered by Task 5.
- Working rules are covered by Stage 0, per-task commits, `/tmp` screenshot paths, and final `gh pr create`.
- The open event question is resolved in Task 2 by using `GameplayEvents`.
- The screenshot brittleness question is resolved in Task 4 by using lightweight positive-pixel and scene-specific checks.
- The `stb_image` dependency question is resolved in Task 5 by vendoring the single header.
- The `billiards.cpp` size question is handled qualitatively: startup/callback glue only, plus final line-count verification.
