# BilliardGL Aim Camera Decoupling Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Decouple camera observation from cue-ball shot direction by introducing explicit aim state, `Tab` aim mode, and aim-driven cue rendering and shot velocity.

**Architecture:** `GameState` owns `AimState`. A new small `shot` module owns pure aim-to-direction and aim-to-velocity helpers so runtime code, rendering, and tests use the same convention. Input mutates either camera or aim depending on mode; rendering and shot execution read aim and do not derive shot intent from camera eye/target.

**Tech Stack:** C++11, GLUT/freeglut keyboard and mouse callbacks, fixed-pipeline OpenGL rendering, CMake/CTest, existing assert-based unit tests.

---

## File Structure

- Modify `src/Billiards/game_state.h`: add `AimMode`, `AimState`, and `GameState::aim`.
- Create `src/Billiards/shot.h`: declare pure aim direction and velocity helpers.
- Create `src/Billiards/shot.cpp`: implement aim helpers.
- Modify `src/Billiards/input.h`: declare `handleAimToggleKey`.
- Modify `src/Billiards/input.cpp`: route pointer movement to aim yaw when aim mode is active; keep camera movement in observe mode.
- Modify `src/Billiards/billiards.cpp`: wire `Tab`, use aim-derived velocity on shot release, and auto-return to observe mode after a shot.
- Modify `src/Billiards/renderer.cpp`: draw cue line and cue model from `GameState::aim`, not camera eye/target.
- Modify `src/Billiards/hud.cpp`: show compact observe/aim mode hint and update help overlay.
- Modify `README.md`: document `Tab` aim mode.
- Modify `CMakeLists.txt`: add `shot.cpp` to core sources and add `BilliardsShotTests`.
- Create `tests/shot_tests.cpp`: cover aim direction and shot velocity helpers.
- Modify `tests/input_tests.cpp`: cover aim toggle and aim-mode pointer behavior.
- Modify `tests/architecture_tests.cpp`: guard against reintroducing camera-derived cue direction in `renderer.cpp` and shot direction in `billiards.cpp`.

## Task 1: Add Aim State And Shot Helpers

**Files:**
- Modify: `src/Billiards/game_state.h`
- Create: `src/Billiards/shot.h`
- Create: `src/Billiards/shot.cpp`
- Modify: `CMakeLists.txt`
- Create: `tests/shot_tests.cpp`

- [ ] **Step 1: Write the failing shot helper tests**

Create `tests/shot_tests.cpp`:

```cpp
#include "game_state.h"
#include "shot.h"

#include <cassert>
#include <cmath>
#include <cstdlib>

namespace {

bool closeEnough(float a, float b)
{
    return std::fabs(a - b) < 0.0001f;
}

void testDefaultAimState()
{
    billiardgl::GameState state;
    assert(state.aim.mode == billiardgl::AimMode::Observe);
    assert(closeEnough(state.aim.yaw, -billiardgl::kPi / 2.0f));
    assert(closeEnough(state.aim.sensitivity, 0.01f));
}

void testAimDirectionIsHorizontalAndNormalized()
{
    const billiardgl::Point3 forward = billiardgl::aimDirectionOnTable(-billiardgl::kPi / 2.0f);
    assert(closeEnough(forward.x, 0.0f));
    assert(closeEnough(forward.y, 0.0f));
    assert(closeEnough(forward.z, -1.0f));

    const billiardgl::Point3 right = billiardgl::aimDirectionOnTable(0.0f);
    assert(closeEnough(right.x, 1.0f));
    assert(closeEnough(right.y, 0.0f));
    assert(closeEnough(right.z, 0.0f));
}

void testShotVelocityUsesAimYawAndPower()
{
    const billiardgl::Point3 velocity = billiardgl::shotVelocityFromAim(0.0f, 42.0f);
    assert(closeEnough(velocity.x, 42.0f));
    assert(closeEnough(velocity.y, 0.0f));
    assert(closeEnough(velocity.z, 0.0f));
}

}  // namespace

int main()
{
    testDefaultAimState();
    testAimDirectionIsHorizontalAndNormalized();
    testShotVelocityUsesAimYawAndPower();
    return EXIT_SUCCESS;
}
```

- [ ] **Step 2: Add the test target to CMake**

Modify `CMakeLists.txt`:

```cmake
add_executable(BilliardsShotTests
    tests/shot_tests.cpp
    ${BILLIARDGL_CORE_SOURCES}
)

target_include_directories(BilliardsShotTests PRIVATE
    src/Billiards
)
```

Add `BilliardsShotTests` to the `foreach(test_target ...)` list and register it:

```cmake
add_test(NAME BilliardsShotTests COMMAND BilliardsShotTests)
```

- [ ] **Step 3: Run the new test and verify it fails**

Run:

```bash
cmake -S . -B /tmp/billiardgl-aim-plan-build
cmake --build /tmp/billiardgl-aim-plan-build --target BilliardsShotTests
```

Expected: build fails because `AimMode`, `GameState::aim`, `shot.h`, `aimDirectionOnTable`, or `shotVelocityFromAim` does not exist yet.

- [ ] **Step 4: Add aim state**

In `src/Billiards/game_state.h`, add after `struct CameraState`:

```cpp
enum class AimMode {
    Observe,
    Aim
};

struct AimState {
    AimMode mode = AimMode::Observe;
    float yaw = -kPi / 2.0f;
    float sensitivity = 0.01f;
};
```

Add to `struct GameState`:

```cpp
AimState aim;
```

- [ ] **Step 5: Add shot helper declarations**

Create `src/Billiards/shot.h`:

```cpp
#pragma once

#include "game_state.h"

namespace billiardgl {

Point3 aimDirectionOnTable(float yaw);
Point3 shotVelocityFromAim(float yaw, float power);

}  // namespace billiardgl
```

- [ ] **Step 6: Add shot helper implementation**

Create `src/Billiards/shot.cpp`:

```cpp
#include "shot.h"

#include <cmath>

namespace billiardgl {

Point3 aimDirectionOnTable(float yaw)
{
    return Point3{std::cos(yaw), 0.0f, std::sin(yaw)};
}

Point3 shotVelocityFromAim(float yaw, float power)
{
    const Point3 direction = aimDirectionOnTable(yaw);
    return Point3{direction.x * power, 0.0f, direction.z * power};
}

}  // namespace billiardgl
```

Add `src/Billiards/shot.cpp` to `BILLIARDGL_CORE_SOURCES` in `CMakeLists.txt`.

- [ ] **Step 7: Run the shot tests**

Run:

```bash
cmake --build /tmp/billiardgl-aim-plan-build --target BilliardsShotTests
/tmp/billiardgl-aim-plan-build/BilliardsShotTests
```

Expected: command exits with status `0`.

- [ ] **Step 8: Commit**

```bash
git add CMakeLists.txt src/Billiards/game_state.h src/Billiards/shot.h src/Billiards/shot.cpp tests/shot_tests.cpp
git commit -m "Add explicit aim state and shot helpers"
```

## Task 2: Route Input Through Observe And Aim Modes

**Files:**
- Modify: `src/Billiards/input.h`
- Modify: `src/Billiards/input.cpp`
- Modify: `tests/input_tests.cpp`

- [ ] **Step 1: Write failing input tests**

In `tests/input_tests.cpp`, add these tests before `main()`:

```cpp
void testAimToggleSwitchesModes()
{
    billiardgl::GameState state;
    assert(state.aim.mode == billiardgl::AimMode::Observe);

    billiardgl::handleAimToggleKey(state);
    assert(state.aim.mode == billiardgl::AimMode::Aim);

    billiardgl::handleAimToggleKey(state);
    assert(state.aim.mode == billiardgl::AimMode::Observe);
}

void testCameraOrbitDoesNotChangeAimInObserveMode()
{
    billiardgl::GameState state;
    const float startAim = state.aim.yaw;
    const float startCameraX = state.camera.angleX;

    billiardgl::handleMouseButton(state, billiardgl::MouseButton::Right, billiardgl::ButtonState::Down, 100, 100);
    billiardgl::handleMouseMove(state, 120, 100);

    assert(closeEnough(state.aim.yaw, startAim));
    assert(closeEnough(state.camera.angleX, startCameraX + 0.2f));
}

void testAimModePointerMovementChangesAimNotCamera()
{
    billiardgl::GameState state;
    billiardgl::handleAimToggleKey(state);
    const float startAim = state.aim.yaw;
    const float startCameraX = state.camera.angleX;
    const float startCameraY = state.camera.angleY;

    billiardgl::handleMouseButton(state, billiardgl::MouseButton::Right, billiardgl::ButtonState::Down, 100, 100);
    billiardgl::handleMouseMove(state, 125, 140);

    assert(closeEnough(state.aim.yaw, startAim + 0.25f));
    assert(closeEnough(state.camera.angleX, startCameraX));
    assert(closeEnough(state.camera.angleY, startCameraY));
}
```

Call them from `main()`:

```cpp
testAimToggleSwitchesModes();
testCameraOrbitDoesNotChangeAimInObserveMode();
testAimModePointerMovementChangesAimNotCamera();
```

- [ ] **Step 2: Run input tests and verify they fail**

Run:

```bash
cmake --build /tmp/billiardgl-aim-plan-build --target BilliardsInputTests
/tmp/billiardgl-aim-plan-build/BilliardsInputTests
```

Expected: build fails because `handleAimToggleKey` is not declared or implemented.

- [ ] **Step 3: Declare the aim toggle helper**

In `src/Billiards/input.h`, add:

```cpp
void handleAimToggleKey(GameState& state);
```

- [ ] **Step 4: Implement aim toggle and aim-mode pointer routing**

In `src/Billiards/input.cpp`, add after `handleHelpKey`:

```cpp
void handleAimToggleKey(GameState& state)
{
    state.aim.mode = state.aim.mode == AimMode::Observe ? AimMode::Aim : AimMode::Observe;
    state.input.rightMouseDown = false;
    state.input.trackpadOrbit = false;
}
```

Modify `handleMouseMove` so the aim-mode branch comes before camera orbit:

```cpp
void handleMouseMove(GameState& state, int x, int y)
{
    const int dx = x - state.input.mouseX;
    const int dy = y - state.input.mouseY;
    state.input.mouseX = x;
    state.input.mouseY = y;

    if (state.aim.mode == AimMode::Aim) {
        state.aim.yaw += static_cast<float>(dx) * state.aim.sensitivity;
        return;
    }

    if (state.input.rightMouseDown || state.input.trackpadOrbit) {
        state.camera.angleX += static_cast<float>(dx) * 0.01f;
        state.camera.angleY += static_cast<float>(dy) * 0.01f;
        clampCameraAngles(state);
    }
}
```

- [ ] **Step 5: Run input tests**

Run:

```bash
cmake --build /tmp/billiardgl-aim-plan-build --target BilliardsInputTests
/tmp/billiardgl-aim-plan-build/BilliardsInputTests
```

Expected: command exits with status `0`.

- [ ] **Step 6: Commit**

```bash
git add src/Billiards/input.h src/Billiards/input.cpp tests/input_tests.cpp
git commit -m "Route pointer input through aim mode"
```

## Task 3: Use Aim For Runtime Shot Velocity

**Files:**
- Modify: `src/Billiards/billiards.cpp`
- Modify: `tests/architecture_tests.cpp`

- [ ] **Step 1: Add architecture assertions against camera-derived shot velocity**

In `tests/architecture_tests.cpp`, add:

```cpp
assertNotContains(billiards, "Game.camera.target[0] - Game.camera.eye[0]");
assertNotContains(billiards, "Game.camera.target[2] - Game.camera.eye[2]");
assertNotContains(billiards, "pow(Game.camera.target[0] - Game.camera.eye[0]");
```

- [ ] **Step 2: Run architecture tests and verify they fail**

Run:

```bash
cmake --build /tmp/billiardgl-aim-plan-build --target BilliardsArchitectureTests
/tmp/billiardgl-aim-plan-build/BilliardsArchitectureTests
```

Expected: assertion fails because `billiards.cpp` still computes shot velocity from camera target and eye.

- [ ] **Step 3: Include shot helpers in runtime**

In `src/Billiards/billiards.cpp`, add:

```cpp
#include "shot.h"
```

- [ ] **Step 4: Wire `Tab` to aim-mode toggle**

In `myKeyboard`, after the `H` key block and before the help-overlay early return, add:

```cpp
if (key == '\t')
{
    billiardgl::handleAimToggleKey(Game);
    return;
}
```

Keep the existing `A / D` cases unchanged.

- [ ] **Step 5: Replace camera-derived shot velocity**

Replace the `if (Game.input.hitRequested == 1)` block in `myIdle` with:

```cpp
if (Game.input.hitRequested == 1)
{
    const billiardgl::Point3 velocity = billiardgl::shotVelocityFromAim(Game.aim.yaw, Game.input.shotPower);
    billiardgl::setBallVelocity(Game.balls[0], velocity.x, velocity.y, velocity.z);
    Game.players.shotTaken = true;
    Game.players.updatedAfterShot = false;
    Game.ballsMoving = true;
    Game.aim.mode = billiardgl::AimMode::Observe;
    Game.input.hitRequested = 0;
    Game.input.shotPower = 0;
    billiardgl::playHit();
}
```

- [ ] **Step 6: Run architecture and shot tests**

Run:

```bash
cmake --build /tmp/billiardgl-aim-plan-build --target BilliardsArchitectureTests BilliardsShotTests
/tmp/billiardgl-aim-plan-build/BilliardsArchitectureTests
/tmp/billiardgl-aim-plan-build/BilliardsShotTests
```

Expected: both test executables exit with status `0`.

- [ ] **Step 7: Commit**

```bash
git add src/Billiards/billiards.cpp tests/architecture_tests.cpp
git commit -m "Use aim direction for shot velocity"
```

## Task 4: Use Aim For Cue Rendering

**Files:**
- Modify: `src/Billiards/renderer.cpp`
- Modify: `tests/architecture_tests.cpp`

- [ ] **Step 1: Add architecture assertions against camera-derived cue rendering**

In `tests/architecture_tests.cpp`, read `renderer.cpp`:

```cpp
const std::string renderer = readFile(std::string(BILLIARDGL_SOURCE_ROOT) + "/src/Billiards/renderer.cpp");
assertNotContains(renderer, "resources.cameraEye[0] - resources.cameraTarget[0]");
assertNotContains(renderer, "resources.cameraEye[2] - resources.cameraTarget[2]");
assertNotContains(renderer, "state.camera.angleX * 180.0f");
```

- [ ] **Step 2: Run architecture tests and verify they fail**

Run:

```bash
cmake --build /tmp/billiardgl-aim-plan-build --target BilliardsArchitectureTests
/tmp/billiardgl-aim-plan-build/BilliardsArchitectureTests
```

Expected: assertion fails because `renderCue` still derives cue line and rotation from camera state.

- [ ] **Step 3: Include shot helpers in renderer**

In `src/Billiards/renderer.cpp`, add:

```cpp
#include "shot.h"
```

- [ ] **Step 4: Replace cue direction with aim direction**

In `renderCue`, replace the `dx/dz/lxz` camera block with:

```cpp
const Point3 aimDirection = aimDirectionOnTable(state.aim.yaw);
const float dx = aimDirection.x;
const float dz = aimDirection.z;
```

Replace the cue line endpoint:

```cpp
glVertex3f(150.0f * dx, 0.0f, 150.0f * dz);
```

Replace the cue model translation:

```cpp
glTranslatef(
    cueBall.position.x - (resources.shotPower + 6.0f) * 0.1f * dx,
    cueBall.position.y,
    cueBall.position.z - (resources.shotPower + 6.0f) * 0.1f * dz);
```

Replace cue model rotation:

```cpp
glRotatef(180.5f - 90.0f - state.aim.yaw * 180.0f / kPi, 0.0f, 1.0f, 0.0f);
```

If the cue model points opposite the line in screenshot verification, keep the line and shot velocity stable and adjust only the rotation offset by `180.0f`.

- [ ] **Step 5: Run architecture tests**

Run:

```bash
cmake --build /tmp/billiardgl-aim-plan-build --target BilliardsArchitectureTests
/tmp/billiardgl-aim-plan-build/BilliardsArchitectureTests
```

Expected: command exits with status `0`.

- [ ] **Step 6: Commit**

```bash
git add src/Billiards/renderer.cpp tests/architecture_tests.cpp
git commit -m "Render cue from aim direction"
```

## Task 5: Update HUD, Help, And README

**Files:**
- Modify: `src/Billiards/hud.cpp`
- Modify: `README.md`

- [ ] **Step 1: Add mode hint to HUD**

In `src/Billiards/hud.cpp`, add:

```cpp
const char* aimModeText(const GameState& state)
{
    return state.aim.mode == AimMode::Aim ? "Mode: Aim | Tab Observe" : "Mode: Observe | Tab Aim";
}
```

Place it in the anonymous namespace near `drawScreenRect`.

In `drawHud`, after the player text and before `drawHelpPrompt`, add:

```cpp
drawStringAt(18.0f, static_cast<float>(state.config.height) - 94.0f, GLUT_BITMAP_HELVETICA_18, aimModeText(state));
```

Then move `drawHelpPrompt` down to avoid overlap:

```cpp
drawHelpPrompt(state);
```

Modify `drawHelpPrompt` to use `height - 120.0f`:

```cpp
drawStringAt(18.0f, static_cast<float>(state.config.height) - 120.0f, GLUT_BITMAP_HELVETICA_18, prompt);
```

- [ ] **Step 2: Update help overlay text**

In `drawHelpOverlay`, after `Shift + trackpad drag Orbit view`, add:

```cpp
y -= 24.0f;
drawStringAt(textLeft, y, GLUT_BITMAP_HELVETICA_18, "Tab                   Toggle aim mode");
y -= 24.0f;
drawStringAt(textLeft, y, GLUT_BITMAP_HELVETICA_18, "Aim mode horizontal   Adjust shot line");
```

Increase `panelHeight` from `330.0f` to `380.0f` so the added lines fit.

- [ ] **Step 3: Update README controls**

In `README.md`, under 操作方式, add:

```markdown
  * tab: toggle aim mode
  * horizontal pointer movement in aim mode: adjust shot line
```

Keep existing `'a': camera shift left` and `'d': camera shift right` lines unchanged.

- [ ] **Step 4: Build the game**

Run:

```bash
cmake --build /tmp/billiardgl-aim-plan-build --target Billiards
```

Expected: build succeeds.

- [ ] **Step 5: Commit**

```bash
git add src/Billiards/hud.cpp README.md
git commit -m "Document aim mode in HUD and README"
```

## Task 6: Full Verification And Screenshot Evidence

**Files:**
- No source files expected unless verification finds a defect.
- Temporary screenshots under `/tmp/billiardgl-aim-camera-decoupling`.

- [ ] **Step 1: Run all automated tests**

Run:

```bash
cmake --build /tmp/billiardgl-aim-plan-build
ctest --test-dir /tmp/billiardgl-aim-plan-build --output-on-failure
```

Expected: all tests pass, including screenshot tests.

- [ ] **Step 2: Capture default screenshot**

Run:

```bash
mkdir -p /tmp/billiardgl-aim-camera-decoupling
/tmp/billiardgl-aim-plan-build/Billiards --screenshot /tmp/billiardgl-aim-camera-decoupling/default.ppm --screenshot-scene default
```

Expected: command exits with status `0`, and `/tmp/billiardgl-aim-camera-decoupling/default.ppm` exists with a valid `P6` header.

- [ ] **Step 3: Capture help screenshot**

Run:

```bash
/tmp/billiardgl-aim-plan-build/Billiards --screenshot /tmp/billiardgl-aim-camera-decoupling/help.ppm --screenshot-scene help
```

Expected: command exits with status `0`, and `/tmp/billiardgl-aim-camera-decoupling/help.ppm` exists with visible HUD/help content.

- [ ] **Step 4: Convert screenshots for manual inspection if available**

Run:

```bash
sips -s format png /tmp/billiardgl-aim-camera-decoupling/default.ppm --out /tmp/billiardgl-aim-camera-decoupling/default.png
sips -s format png /tmp/billiardgl-aim-camera-decoupling/help.ppm --out /tmp/billiardgl-aim-camera-decoupling/help.png
```

Expected on macOS: PNG files are created. If `sips` is unavailable, skip conversion and keep the PPM files.

- [ ] **Step 5: Manual play check**

Run:

```bash
/tmp/billiardgl-aim-plan-build/Billiards --windowed
```

Expected:

- `A / D` still pan the camera.
- `Tab` toggles the HUD between observe and aim mode.
- In observe mode, right mouse drag or trackpad orbit changes camera view without changing the cue line.
- In aim mode, horizontal pointer movement changes the cue line without moving the camera.
- Holding and releasing left mouse hits the cue ball along the visible cue line.
- After the shot, mode returns to observe.

- [ ] **Step 6: Final commit only if verification required fixes**

If Task 6 found and fixed a defect:

```bash
git add CMakeLists.txt README.md src/Billiards tests
git commit -m "Fix aim camera decoupling verification issues"
```

If no source files changed, do not create an empty commit.

## Self-Review

- Spec coverage: covered explicit aim state, `Tab` mode toggle, observe-mode camera preservation, aim-mode pointer yaw, shot velocity from aim, cue rendering from aim, HUD/README updates, and screenshot/manual verification.
- Deferred by design: camera soft-follow and collision prediction remain out of scope for this first pass.
- Placeholder scan: no `TBD`, `TODO`, "implement later", or unspecified test steps remain.
- Type consistency: plan uses `AimMode`, `AimState`, `GameState::aim`, `aimDirectionOnTable`, and `shotVelocityFromAim` consistently across tasks.
