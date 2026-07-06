# BilliardGL Architecture Evolution Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restructure BilliardGL around `GameState`, testable physics/rules modules, isolated input/HUD/renderer/assets boundaries, while preserving current gameplay and macOS behavior.

**Architecture:** Keep CMake, GLUT, fixed-pipeline OpenGL, current resources, and current gameplay behavior. Introduce plain C++ modules around the existing code, moving logic out of `billiards.cpp` in stages so the app remains buildable and playable after each task.

**Tech Stack:** C++11, CMake, OpenGL, GLEW, GLUT/freeglut, SDL2/SDL2_mixer, existing `vec`, `ObjLoader`, `particle`, and platform wrappers.

---

## File Structure

Create or modify these files:

- Create: `src/Billiards/game_state.h`
- Create: `src/Billiards/game_state.cpp`
- Create: `src/Billiards/physics.h`
- Create: `src/Billiards/physics.cpp`
- Create: `src/Billiards/rules.h`
- Create: `src/Billiards/rules.cpp`
- Create: `src/Billiards/input.h`
- Create: `src/Billiards/input.cpp`
- Create: `src/Billiards/hud.h`
- Create: `src/Billiards/hud.cpp`
- Create: `src/Billiards/renderer.h`
- Create: `src/Billiards/renderer.cpp`
- Create: `src/Billiards/assets.h`
- Create: `src/Billiards/assets.cpp`
- Create: `tests/physics_tests.cpp`
- Create: `tests/rules_tests.cpp`
- Modify: `src/Billiards/billiards.cpp`
- Modify: `CMakeLists.txt`

Responsibilities:

- `billiards.cpp`: process startup, GLUT setup, callback registration, and dispatch only.
- `game_state`: shared state and constants; no OpenGL includes.
- `physics`: movement, friction, collisions, pockets; no OpenGL includes.
- `rules`: current player, turn completion, score/game-over decisions; no OpenGL includes.
- `input`: keyboard, mouse, trackpad-style state changes; no rendering.
- `hud`: 2D text and Help overlay drawing.
- `renderer`: 3D scene drawing and OpenGL state management.
- `assets`: resource path and loading entry points.

## Task 1: Add Test Target and `GameState`

**Files:**
- Create: `src/Billiards/game_state.h`
- Create: `src/Billiards/game_state.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create `game_state.h`**

Create `src/Billiards/game_state.h`:

```cpp
#pragma once

#include <array>

namespace billiardgl {

constexpr int kBallCount = 16;
constexpr float kWindowWidth = 1024.0f;
constexpr float kWindowHeight = 768.0f;
constexpr float kTableInWidth = 124.5f;
constexpr float kTableInLength = 252.0f;
constexpr float kTableHeight = 87.0f;
constexpr float kPocketRadius = 8.5f;
constexpr float kBallRadius = 5.715f;
constexpr float kPi = 3.1415926f;
constexpr float kDefaultTimeStep = 0.1f;
constexpr float kDefaultFrictionAcceleration = -4.0f;

struct Point3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct BallState {
    Point3 position;
    Point3 velocity;
    Point3 rotationAxis;
    float speed = 0.0f;
    float rotationAngle = 0.0f;
    bool pocketed = false;
    unsigned int texture = 0;
};

struct CameraState {
    float target[3] = {0.0f, kTableHeight + kBallRadius, -kTableInLength / 4.0f};
    float eye[3] = {0.0f, 200.0f, -kTableInLength / 4.0f};
    float zoom = 120.0f;
    float angleX = -kPi / 2.0f;
    float angleY = kPi / 3.0f;
};

struct PlayerState {
    int assignedBallType[2] = {-1, -1};
    bool firstPocketedObjectBall = true;
    int currentPlayer = 0;
    int nextPlayer = 0;
    bool illegalShot = false;
    bool updatedAfterShot = false;
    bool shotTaken = false;
};

struct InputState {
    int mouseX = 0;
    int mouseY = 0;
    bool leftMouseDown = false;
    bool rightMouseDown = false;
    bool trackpadOrbit = false;
    bool waitingForHit = false;
    bool hitRequested = false;
    float shotPower = 0.0f;
};

struct HudState {
    bool showHelp = false;
};

struct RuntimeConfig {
    bool windowedMode = true;
    int width = 1024;
    int height = 768;
};

struct GameState {
    std::array<BallState, kBallCount> balls;
    CameraState camera;
    PlayerState players;
    InputState input;
    HudState hud;
    RuntimeConfig config;
    int pocketedBallCount = 0;
    bool ballsMoving = false;
    bool transitionPerspective = false;
    bool perspectiveRecorded = false;
    bool gameOver = false;
};

void initializeBalls(GameState& state);
void updateCameraFromCueBall(GameState& state);

}  // namespace billiardgl
```

- [ ] **Step 2: Create `game_state.cpp`**

Create `src/Billiards/game_state.cpp`:

```cpp
#include "game_state.h"

#include <cmath>

namespace billiardgl {

void initializeBalls(GameState& state)
{
    const Point3 start{0.0f, kTableHeight + kBallRadius, 0.0f};
    const float xDis = 2.0f * kBallRadius;
    const float yDis = 2.0f * kBallRadius * std::sin(kPi / 3.0f);

    state.balls[0].position = Point3{start.x, start.y, -start.z};
    state.balls[1].position = Point3{start.x, start.y, start.z};
    state.balls[2].position = Point3{start.x - xDis, start.y, start.z + yDis};
    state.balls[3].position = Point3{start.x + xDis, start.y, start.z + yDis};
    state.balls[4].position = Point3{start.x - 2.0f * xDis, start.y, start.z + 2.0f * yDis};
    state.balls[8].position = Point3{start.x, start.y, start.z + 2.0f * yDis};
    state.balls[6].position = Point3{start.x + 2.0f * xDis, start.y, start.z + 2.0f * yDis};
    state.balls[7].position = Point3{start.x - 3.0f * xDis, start.y, start.z + 3.0f * yDis};
    state.balls[5].position = Point3{start.x - xDis, start.y, start.z + 3.0f * yDis};
    state.balls[9].position = Point3{start.x + xDis, start.y, start.z + 3.0f * yDis};
    state.balls[10].position = Point3{start.x + 3.0f * xDis, start.y, start.z + 3.0f * yDis};
    state.balls[11].position = Point3{start.x - 4.0f * xDis, start.y, start.z + 4.0f * yDis};
    state.balls[12].position = Point3{start.x - 2.0f * xDis, start.y, start.z + 4.0f * yDis};
    state.balls[13].position = Point3{start.x, start.y, start.z + 4.0f * yDis};
    state.balls[14].position = Point3{start.x + 2.0f * xDis, start.y, start.z + 4.0f * yDis};
    state.balls[15].position = Point3{start.x + 4.0f * xDis, start.y, start.z + 4.0f * yDis};

    for (BallState& ball : state.balls) {
        ball.velocity = Point3{};
        ball.rotationAxis = Point3{};
        ball.speed = 0.0f;
        ball.rotationAngle = 0.0f;
        ball.pocketed = false;
    }
}

void updateCameraFromCueBall(GameState& state)
{
    state.camera.target[0] = state.balls[0].position.x;
    state.camera.target[1] = state.balls[0].position.y;
    state.camera.target[2] = state.balls[0].position.z;
    state.camera.eye[0] = state.camera.zoom * std::cos(state.camera.angleX) + state.camera.target[0];
    state.camera.eye[1] = state.camera.zoom * std::cos(state.camera.angleY) + state.camera.target[1];
    state.camera.eye[2] = state.camera.zoom * std::sin(state.camera.angleX) * std::sin(state.camera.angleY) + state.camera.target[2];
}

}  // namespace billiardgl
```

- [ ] **Step 3: Update CMake with shared source list and test executable**

Modify `CMakeLists.txt` so the executable includes `game_state.cpp` and there is a test target that can build without OpenGL:

```cmake
set(BILLIARDGL_CORE_SOURCES
    src/Billiards/game_state.cpp
    src/Billiards/vec.cpp
)

add_executable(Billiards
    src/Billiards/billiards.cpp
    src/Billiards/ObjLoader.cpp
    src/Billiards/particle.cpp
    src/Billiards/resource_path.cpp
    src/Billiards/platform_audio.cpp
    ${BILLIARDGL_CORE_SOURCES}
)

add_executable(BilliardsPhysicsTests
    tests/physics_tests.cpp
    ${BILLIARDGL_CORE_SOURCES}
)

add_executable(BilliardsRulesTests
    tests/rules_tests.cpp
    ${BILLIARDGL_CORE_SOURCES}
)

target_include_directories(BilliardsPhysicsTests PRIVATE
    src/Billiards
)

target_include_directories(BilliardsRulesTests PRIVATE
    src/Billiards
)
```

Keep the existing `target_include_directories(Billiards ...)`, `target_compile_definitions(Billiards ...)`, and `target_link_libraries(Billiards ...)` blocks for the game executable.

- [ ] **Step 4: Add placeholder-free test files that compile**

Create `tests/physics_tests.cpp`:

```cpp
#include "game_state.h"

#include <cstdlib>
#include <iostream>

int main()
{
    billiardgl::GameState state;
    billiardgl::initializeBalls(state);
    if (state.balls[0].position.y != billiardgl::kTableHeight + billiardgl::kBallRadius) {
        std::cerr << "cue ball should start on the table" << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
```

Create `tests/rules_tests.cpp`:

```cpp
int main()
{
    return 0;
}
```

- [ ] **Step 5: Run build**

Run:

```bash
cmake --build build
```

Expected: `Billiards`, `BilliardsPhysicsTests`, and `BilliardsRulesTests` compile successfully.

- [ ] **Step 6: Run initial tests**

Run:

```bash
./build/BilliardsPhysicsTests && ./build/BilliardsRulesTests
```

Expected: process exits with code `0`.

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt src/Billiards/game_state.h src/Billiards/game_state.cpp tests/physics_tests.cpp tests/rules_tests.cpp
git commit -m "add game state model and test target"
```

## Task 2: Extract Physics

**Files:**
- Create: `src/Billiards/physics.h`
- Create: `src/Billiards/physics.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/physics_tests.cpp`
- Modify: `src/Billiards/billiards.cpp`

- [ ] **Step 1: Write physics tests**

Replace `tests/physics_tests.cpp` with:

```cpp
#include "game_state.h"
#include "physics.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

bool nearlyEqual(float a, float b, float epsilon = 0.001f)
{
    return std::fabs(a - b) < epsilon;
}

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

    state.balls[0].velocity.x = 20.0f;
    billiardgl::applyFrictionAndMove(state.balls[0], 0.1f, -4.0f);
    if (!(state.balls[0].speed < 20.0f)) {
        return fail("friction should reduce speed");
    }
    if (!(state.balls[0].position.x > 0.0f)) {
        return fail("ball should move after physics update");
    }

    state.balls[0].velocity.x = 0.1f;
    state.balls[0].velocity.z = 0.0f;
    billiardgl::applyFrictionAndMove(state.balls[0], 0.1f, -4.0f);
    if (!nearlyEqual(state.balls[0].speed, 0.0f)) {
        return fail("very slow ball should stop");
    }

    state.balls[0].position.x = billiardgl::kTableInWidth / 2.0f;
    state.balls[0].velocity.x = 5.0f;
    billiardgl::collideWithTableEdge(state.balls[0]);
    if (!(state.balls[0].velocity.x < 0.0f)) {
        return fail("wall collision should reverse x velocity");
    }

    state.balls[1].position = billiardgl::Point3{0.0f, billiardgl::kTableHeight + billiardgl::kBallRadius, 0.0f};
    state.balls[2].position = billiardgl::Point3{2.0f * billiardgl::kBallRadius - 0.6f, billiardgl::kTableHeight + billiardgl::kBallRadius, 0.0f};
    state.balls[1].velocity.x = 10.0f;
    state.balls[2].velocity.x = 0.0f;
    billiardgl::collideBalls(state.balls[1], state.balls[2]);
    if (!(state.balls[2].velocity.x > 0.0f)) {
        return fail("ball collision should transfer velocity");
    }

    state.balls[3].position.x = -billiardgl::kTableInWidth / 2.0f + billiardgl::kPocketRadius;
    state.balls[3].position.z = -billiardgl::kTableInLength / 2.0f + billiardgl::kPocketRadius;
    if (!billiardgl::isInPocket(state.balls[3])) {
        return fail("ball at pocket center should be detected as pocketed");
    }

    return EXIT_SUCCESS;
}
```

- [ ] **Step 2: Run tests to verify failure**

Run:

```bash
cmake --build build && ./build/BilliardsPhysicsTests && ./build/BilliardsRulesTests
```

Expected: build fails because `physics.h` does not exist.

- [ ] **Step 3: Create `physics.h`**

Create `src/Billiards/physics.h`:

```cpp
#pragma once

#include "game_state.h"

namespace billiardgl {

void applyFrictionAndMove(BallState& ball, float timeStep, float frictionAcceleration);
bool collideBalls(BallState& first, BallState& second);
void collideWithTableEdge(BallState& ball);
bool isInPocket(const BallState& ball);
bool updatePocketedBall(GameState& state, int ballIndex);
void updatePhysics(GameState& state, float timeStep);

}  // namespace billiardgl
```

- [ ] **Step 4: Create `physics.cpp`**

Create `src/Billiards/physics.cpp`:

```cpp
#include "physics.h"

#include <cmath>

namespace billiardgl {

void applyFrictionAndMove(BallState& ball, float timeStep, float frictionAcceleration)
{
    const float speedSquared = ball.velocity.x * ball.velocity.x + ball.velocity.z * ball.velocity.z;
    if (speedSquared <= 0.1f) {
        ball.speed = 0.0f;
        ball.velocity.x = 0.0f;
        ball.velocity.z = 0.0f;
        return;
    }

    ball.speed = std::sqrt(speedSquared);
    const float vx = ball.velocity.x / ball.speed;
    const float vz = ball.velocity.z / ball.speed;
    ball.speed += frictionAcceleration * timeStep;

    if (ball.speed <= 0.0f) {
        ball.speed = 0.0f;
        ball.velocity.x = 0.0f;
        ball.velocity.z = 0.0f;
        return;
    }

    ball.velocity.x = ball.speed * vx;
    ball.velocity.z = ball.speed * vz;
    ball.position.x += ball.velocity.x * timeStep;
    ball.position.z += ball.velocity.z * timeStep;
    ball.rotationAxis.x = -vz;
    ball.rotationAxis.z = vx;
    ball.rotationAngle += -180.0f * ball.speed * timeStep / (kBallRadius * kPi);
}

bool collideBalls(BallState& first, BallState& second)
{
    const float dx = second.position.x - first.position.x;
    const float dz = second.position.z - first.position.z;
    const float distance = std::sqrt(dx * dx + dz * dz);
    if (distance <= 0.0f || distance >= 2.0f * kBallRadius - 0.5f) {
        return false;
    }

    const float cosValue = dx / distance;
    const float sinValue = dz / distance;
    const float cCos = -sinValue;
    const float cSin = cosValue;
    const float v1c = first.velocity.x * cosValue + first.velocity.z * sinValue;
    const float v1cc = first.velocity.x * cCos + first.velocity.z * cSin;
    const float v2c = second.velocity.x * cosValue + second.velocity.z * sinValue;
    const float v2cc = second.velocity.x * cCos + second.velocity.z * cSin;

    first.velocity.x = v1cc * cCos + v2c * cosValue;
    first.velocity.z = v1cc * cSin + v2c * sinValue;
    second.velocity.x = v1c * cosValue + v2cc * cCos;
    second.velocity.z = v1c * sinValue + v2cc * cSin;
    second.position.x = first.position.x + 2.0f * kBallRadius * cosValue;
    second.position.z = first.position.z + 2.0f * kBallRadius * sinValue;
    return true;
}

void collideWithTableEdge(BallState& ball)
{
    if (std::fabs(ball.position.x) > kTableInWidth / 2.0f - kBallRadius) {
        ball.position.x = ball.position.x > 0.0f
            ? kTableInWidth / 2.0f - kBallRadius
            : -kTableInWidth / 2.0f + kBallRadius;
        ball.velocity.x *= -1.0f;
    }

    if (std::fabs(ball.position.z) > kTableInLength / 2.0f - kBallRadius) {
        ball.position.z = ball.position.z > 0.0f
            ? kTableInLength / 2.0f - kBallRadius
            : -kTableInLength / 2.0f + kBallRadius;
        ball.velocity.z *= -1.0f;
    }
}

bool isInPocket(const BallState& ball)
{
    const float pocketX[6] = {
        -kTableInWidth / 2.0f + kPocketRadius,
        kTableInWidth / 2.0f - kPocketRadius,
        -kTableInWidth / 2.0f + kPocketRadius,
        kTableInWidth / 2.0f - kPocketRadius,
        -kTableInWidth / 2.0f + kPocketRadius,
        kTableInWidth / 2.0f - kPocketRadius,
    };
    const float pocketZ[6] = {
        -kTableInLength / 2.0f + kPocketRadius,
        -kTableInLength / 2.0f + kPocketRadius,
        kTableInLength / 2.0f - kPocketRadius,
        kTableInLength / 2.0f - kPocketRadius,
        0.0f,
        0.0f,
    };

    for (int i = 0; i < 6; ++i) {
        const float dx = ball.position.x - pocketX[i];
        const float dz = ball.position.z - pocketZ[i];
        if (std::sqrt(dx * dx + dz * dz) < kBallRadius / 4.0f) {
            return true;
        }
    }
    return false;
}

bool updatePocketedBall(GameState& state, int ballIndex)
{
    BallState& ball = state.balls[ballIndex];
    if (!isInPocket(ball)) {
        return false;
    }

    if (ballIndex == 0) {
        ball.position = Point3{0.0f, kTableHeight + kBallRadius, -kTableInLength / 4.0f};
        state.players.illegalShot = true;
    } else {
        ball.pocketed = true;
        state.pocketedBallCount += 1;
        ball.position.z = -100.0f + static_cast<float>(state.pocketedBallCount) * 20.0f;
        ball.position.y = kTableHeight - kBallRadius;
        if (ballIndex != 8) {
            ball.position.y = -100.0f;
        }
        if (ballIndex == 8) {
            state.gameOver = true;
        }
    }

    ball.velocity = Point3{};
    return true;
}

void updatePhysics(GameState& state, float timeStep)
{
    bool anyMoving = false;
    for (int i = 0; i < kBallCount; ++i) {
        BallState& ball = state.balls[i];
        if (ball.pocketed) {
            continue;
        }
        for (int j = i + 1; j < kBallCount; ++j) {
            if (!state.balls[j].pocketed) {
                collideBalls(ball, state.balls[j]);
            }
        }
        collideWithTableEdge(ball);
        updatePocketedBall(state, i);
        applyFrictionAndMove(ball, timeStep, kDefaultFrictionAcceleration);
        if (ball.speed > 0.0f) {
            anyMoving = true;
        }
    }
    state.ballsMoving = anyMoving;
}

}  // namespace billiardgl
```

- [ ] **Step 5: Update CMake**

Add `src/Billiards/physics.cpp` to `BILLIARDGL_CORE_SOURCES`:

```cmake
set(BILLIARDGL_CORE_SOURCES
    src/Billiards/game_state.cpp
    src/Billiards/physics.cpp
    src/Billiards/vec.cpp
)
```

- [ ] **Step 6: Run tests**

Run:

```bash
cmake --build build && ./build/BilliardsPhysicsTests && ./build/BilliardsRulesTests
```

Expected: tests pass.

- [ ] **Step 7: Integrate `updatePhysics` into `billiards.cpp`**

Add includes:

```cpp
#include "game_state.h"
#include "physics.h"
```

Add a temporary global state near the existing globals:

```cpp
static billiardgl::GameState Game;
```

At the start of `initBall()`, add:

```cpp
billiardgl::initializeBalls(Game);
```

In `myIdle()`, keep the existing implementation until the renderer is migrated, but add a comment marking the intended handoff:

```cpp
// Physics has a GameState-backed implementation in physics.cpp.
// The old globals remain active until rendering is moved to GameState.
```

This step intentionally does not remove old physics yet because rendering still reads the old `Ball` array.

- [ ] **Step 8: Build and run tests**

Run:

```bash
cmake --build build && ./build/BilliardsPhysicsTests && ./build/BilliardsRulesTests
```

Expected: build and tests pass.

- [ ] **Step 9: Commit**

```bash
git add CMakeLists.txt src/Billiards/game_state.h src/Billiards/game_state.cpp src/Billiards/physics.h src/Billiards/physics.cpp tests/physics_tests.cpp src/Billiards/billiards.cpp
git commit -m "extract testable physics helpers"
```

## Task 3: Extract Rules

**Files:**
- Create: `src/Billiards/rules.h`
- Create: `src/Billiards/rules.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/rules_tests.cpp`

- [ ] **Step 1: Write rules tests**

Replace `tests/rules_tests.cpp` with:

```cpp
#include "game_state.h"
#include "rules.h"

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
    state.players.currentPlayer = 0;
    state.players.nextPlayer = 0;
    state.players.illegalShot = false;
    state.players.shotTaken = true;
    state.transitionPerspective = false;

    billiardgl::updatePlayerAfterShot(state);
    if (state.players.currentPlayer != 0) {
        return fail("legal shot with same next player should keep current player");
    }
    if (!state.players.updatedAfterShot) {
        return fail("player update should be marked complete");
    }

    state.players.currentPlayer = 0;
    state.players.nextPlayer = 1;
    state.players.illegalShot = false;
    state.players.updatedAfterShot = false;
    state.players.shotTaken = true;

    billiardgl::updatePlayerAfterShot(state);
    if (state.players.currentPlayer != 1) {
        return fail("next player mismatch should switch player");
    }

    state.players.currentPlayer = 0;
    state.players.nextPlayer = 0;
    state.players.illegalShot = true;
    state.players.updatedAfterShot = false;
    state.players.shotTaken = true;

    billiardgl::updatePlayerAfterShot(state);
    if (state.players.currentPlayer != 1) {
        return fail("illegal shot should switch player");
    }

    return EXIT_SUCCESS;
}
```

- [ ] **Step 2: Run tests to verify failure**

Run:

```bash
cmake --build build && ./build/BilliardsPhysicsTests && ./build/BilliardsRulesTests
```

Expected: build fails because `rules.h` does not exist.

- [ ] **Step 3: Create `rules.h`**

Create `src/Billiards/rules.h`:

```cpp
#pragma once

#include "game_state.h"

namespace billiardgl {

void assignPlayerBallTypeForPocketedObjectBall(GameState& state, int ballIndex);
void updatePlayerAfterShot(GameState& state);

}  // namespace billiardgl
```

- [ ] **Step 4: Create `rules.cpp`**

Create `src/Billiards/rules.cpp`:

```cpp
#include "rules.h"

namespace billiardgl {

void assignPlayerBallTypeForPocketedObjectBall(GameState& state, int ballIndex)
{
    if (ballIndex <= 0 || ballIndex == 8) {
        return;
    }

    const int current = state.players.currentPlayer;
    const int other = 1 - current;
    const int pocketedType = ballIndex > 8 ? 1 : 0;

    if (state.players.firstPocketedObjectBall) {
        state.players.assignedBallType[current] = pocketedType;
        state.players.assignedBallType[other] = 1 - pocketedType;
        state.players.firstPocketedObjectBall = false;
        state.players.nextPlayer = current;
        return;
    }

    if (state.players.assignedBallType[current] == pocketedType) {
        state.players.nextPlayer = current;
    } else {
        state.players.illegalShot = true;
    }
}

void updatePlayerAfterShot(GameState& state)
{
    if (state.players.updatedAfterShot) {
        return;
    }
    if (state.transitionPerspective || !state.players.shotTaken) {
        return;
    }
    if (state.players.illegalShot || state.players.nextPlayer != state.players.currentPlayer) {
        state.players.currentPlayer = 1 - state.players.currentPlayer;
    }
    state.players.updatedAfterShot = true;
}

}  // namespace billiardgl
```

- [ ] **Step 5: Update CMake**

Add `src/Billiards/rules.cpp` to `BILLIARDGL_CORE_SOURCES`:

```cmake
set(BILLIARDGL_CORE_SOURCES
    src/Billiards/game_state.cpp
    src/Billiards/physics.cpp
    src/Billiards/rules.cpp
    src/Billiards/vec.cpp
)
```

- [ ] **Step 6: Run tests**

Run:

```bash
cmake --build build && ./build/BilliardsPhysicsTests && ./build/BilliardsRulesTests
```

Expected: tests pass.

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt src/Billiards/rules.h src/Billiards/rules.cpp tests/rules_tests.cpp
git commit -m "extract testable rule helpers"
```

## Task 4: Extract Input State Handling

**Files:**
- Create: `src/Billiards/input.h`
- Create: `src/Billiards/input.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/Billiards/billiards.cpp`

- [ ] **Step 1: Create `input.h`**

Create `src/Billiards/input.h`:

```cpp
#pragma once

#include "game_state.h"

namespace billiardgl {

enum class MouseButton {
    Left,
    Right,
    Other
};

enum class ButtonState {
    Down,
    Up
};

void handleHelpKey(GameState& state);
void handleSpecialKey(GameState& state, int keyLeft, int keyRight, int keyUp, int keyDown, int key);
void handleMouseButton(GameState& state, MouseButton button, ButtonState buttonState, int x, int y);
void handleMouseMove(GameState& state, int x, int y);
void clampCameraAngles(GameState& state);

}  // namespace billiardgl
```

- [ ] **Step 2: Create `input.cpp`**

Create `src/Billiards/input.cpp`:

```cpp
#include "input.h"

namespace billiardgl {

void clampCameraAngles(GameState& state)
{
    if (state.camera.angleY <= 0.0f) {
        state.camera.angleY = 0.1f;
    }
    if (state.camera.angleY > kPi / 2.0f) {
        state.camera.angleY = kPi / 2.0f;
    }
}

void handleHelpKey(GameState& state)
{
    state.hud.showHelp = !state.hud.showHelp;
    state.input.waitingForHit = false;
    state.input.hitRequested = false;
}

void handleSpecialKey(GameState& state, int keyLeft, int keyRight, int keyUp, int keyDown, int key)
{
    const float orbitStep = 0.08f;
    if (key == keyLeft) {
        state.camera.angleX -= orbitStep;
    } else if (key == keyRight) {
        state.camera.angleX += orbitStep;
    } else if (key == keyUp) {
        state.camera.angleY -= orbitStep;
    } else if (key == keyDown) {
        state.camera.angleY += orbitStep;
    }
    clampCameraAngles(state);
}

void handleMouseButton(GameState& state, MouseButton button, ButtonState buttonState, int x, int y)
{
    state.input.mouseX = x;
    state.input.mouseY = y;
    const bool isDown = buttonState == ButtonState::Down;
    if (button == MouseButton::Left) {
        state.input.leftMouseDown = isDown;
        state.input.waitingForHit = isDown;
        if (!isDown) {
            state.input.hitRequested = true;
        }
    } else if (button == MouseButton::Right) {
        state.input.rightMouseDown = isDown;
    }
}

void handleMouseMove(GameState& state, int x, int y)
{
    const int dx = x - state.input.mouseX;
    const int dy = y - state.input.mouseY;
    state.input.mouseX = x;
    state.input.mouseY = y;

    if (state.input.rightMouseDown || state.input.trackpadOrbit) {
        state.camera.angleX += static_cast<float>(dx) * 0.01f;
        state.camera.angleY += static_cast<float>(dy) * 0.01f;
        clampCameraAngles(state);
    }
}

}  // namespace billiardgl
```

- [ ] **Step 3: Update CMake**

Add `src/Billiards/input.cpp` to `BILLIARDGL_CORE_SOURCES`:

```cmake
set(BILLIARDGL_CORE_SOURCES
    src/Billiards/game_state.cpp
    src/Billiards/physics.cpp
    src/Billiards/rules.cpp
    src/Billiards/input.cpp
    src/Billiards/vec.cpp
)
```

- [ ] **Step 4: Wire keyboard special keys**

In `src/Billiards/billiards.cpp`, include:

```cpp
#include "input.h"
```

In `mySpecialKeyboard`, call the helper before syncing old globals:

```cpp
billiardgl::handleSpecialKey(Game, GLUT_KEY_LEFT, GLUT_KEY_RIGHT, GLUT_KEY_UP, GLUT_KEY_DOWN, key);
anglex = Game.camera.angleX;
angley = Game.camera.angleY;
```

Keep the existing switch during this task only if needed to preserve behavior. If both paths are present, remove the duplicate angle updates before committing.

- [ ] **Step 5: Wire Help key**

In `myKeyboard`, replace the Help key branch with:

```cpp
if (key == 'h' || key == 'H')
{
    billiardgl::handleHelpKey(Game);
    ShowHelp = Game.hud.showHelp;
    WaitHit = Game.input.waitingForHit ? 1 : 0;
    Hit = Game.input.hitRequested ? 1 : 0;
    return;
}
```

- [ ] **Step 6: Build**

Run:

```bash
cmake --build build && ./build/BilliardsPhysicsTests && ./build/BilliardsRulesTests
```

Expected: build and tests pass.

- [ ] **Step 7: Manual E2E**

Run:

```bash
./build/Billiards
```

Expected:

- Windowed launch opens.
- Arrow keys rotate camera.
- `H` toggles Help.
- Existing mouse behavior remains usable.

- [ ] **Step 8: Commit**

```bash
git add CMakeLists.txt src/Billiards/input.h src/Billiards/input.cpp src/Billiards/billiards.cpp
git commit -m "extract input state helpers"
```

## Task 5: Extract HUD Rendering

**Files:**
- Create: `src/Billiards/hud.h`
- Create: `src/Billiards/hud.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/Billiards/billiards.cpp`

- [ ] **Step 1: Create `hud.h`**

Create `src/Billiards/hud.h`:

```cpp
#pragma once

#include "game_state.h"

namespace billiardgl {

void drawHud(const GameState& state);
void drawHelpPrompt(const GameState& state);
void drawHelpOverlay(const GameState& state);

}  // namespace billiardgl
```

- [ ] **Step 2: Create `hud.cpp`**

Create `src/Billiards/hud.cpp` by moving the existing 2D helpers from `billiards.cpp`:

```cpp
#define GLEW_STATIC
#include <GL/glew.h>
#include <GL/freeglut.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include "hud.h"

#include <string>

namespace billiardgl {

namespace {

void drawStringAt(float x, float y, void* font, const char* text)
{
    glRasterPos2f(x, y);
    for (const char* c = text; *c != '\0'; ++c) {
        glutBitmapCharacter(font, *c);
    }
}

void drawScreenRect(float left, float bottom, float right, float top, float r, float g, float b, float a)
{
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(left, bottom);
    glVertex2f(right, bottom);
    glVertex2f(right, top);
    glVertex2f(left, top);
    glEnd();
}

}  // namespace

void drawHelpPrompt(const GameState& state)
{
    glColor3f(1.0f, 1.0f, 1.0f);
    const char* prompt = state.hud.showHelp ? "Press H to close help" : "Press H for help";
    drawStringAt(16.0f, 24.0f, GLUT_BITMAP_HELVETICA_18, prompt);
}

void drawHelpOverlay(const GameState& state)
{
    if (!state.hud.showHelp) {
        return;
    }
    drawScreenRect(120.0f, 100.0f, 904.0f, 668.0f, 0.0f, 0.0f, 0.0f, 0.75f);
    glColor3f(1.0f, 1.0f, 1.0f);
    drawStringAt(160.0f, 620.0f, GLUT_BITMAP_HELVETICA_18, "BilliardGL Help");
    drawStringAt(160.0f, 580.0f, GLUT_BITMAP_HELVETICA_18, "Right mouse drag      Orbit view");
    drawStringAt(160.0f, 550.0f, GLUT_BITMAP_HELVETICA_18, "Arrow keys            Orbit view");
    drawStringAt(160.0f, 520.0f, GLUT_BITMAP_HELVETICA_18, "Left mouse hold       Charge shot");
    drawStringAt(160.0f, 490.0f, GLUT_BITMAP_HELVETICA_18, "Left mouse release    Hit cue ball");
    drawStringAt(160.0f, 460.0f, GLUT_BITMAP_HELVETICA_18, "H                     Toggle help");
    drawStringAt(160.0f, 430.0f, GLUT_BITMAP_HELVETICA_18, "Esc                   Quit");
}

void drawHud(const GameState& state)
{
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0.0, state.config.width, 0.0, state.config.height);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor3f(1.0f, 1.0f, 1.0f);
    const std::string player = "Current Player: Player " + std::to_string(state.players.currentPlayer + 1);
    drawStringAt(16.0f, static_cast<float>(state.config.height) - 32.0f, GLUT_BITMAP_HELVETICA_18, player.c_str());
    drawHelpPrompt(state);
    drawHelpOverlay(state);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

}  // namespace billiardgl
```

- [ ] **Step 3: Update CMake**

Add `src/Billiards/hud.cpp` to the `Billiards` executable source list, not to `BILLIARDGL_CORE_SOURCES`, because it depends on OpenGL:

```cmake
add_executable(Billiards
    src/Billiards/billiards.cpp
    src/Billiards/hud.cpp
    src/Billiards/ObjLoader.cpp
    src/Billiards/particle.cpp
    src/Billiards/resource_path.cpp
    src/Billiards/platform_audio.cpp
    ${BILLIARDGL_CORE_SOURCES}
)
```

- [ ] **Step 4: Replace HUD call in `billiards.cpp`**

Include:

```cpp
#include "hud.h"
```

In the display path, replace calls to old HUD functions with:

```cpp
Game.config.width = width;
Game.config.height = height;
Game.players.currentPlayer = CurrPlayer;
Game.hud.showHelp = ShowHelp;
billiardgl::drawHud(Game);
```

Remove old HUD helper function declarations and definitions only after the new HUD output matches the existing screen.

- [ ] **Step 5: Build**

Run:

```bash
cmake --build build && ./build/BilliardsPhysicsTests && ./build/BilliardsRulesTests
```

Expected: build and tests pass.

- [ ] **Step 6: Visual E2E**

Run:

```bash
mkdir -p /tmp/billiardgl-shots
./build/Billiards
```

Expected:

- HUD current player text is not overlapped.
- Persistent Help prompt is visible.
- `H` opens overlay.
- Screenshot can be saved manually to `/tmp/billiardgl-shots`.

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt src/Billiards/hud.h src/Billiards/hud.cpp src/Billiards/billiards.cpp
git commit -m "extract hud and help overlay"
```

## Task 6: Extract Assets Entry Points

**Files:**
- Create: `src/Billiards/assets.h`
- Create: `src/Billiards/assets.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/Billiards/billiards.cpp`

- [ ] **Step 1: Create `assets.h`**

Create `src/Billiards/assets.h`:

```cpp
#pragma once

#include <string>

namespace billiardgl {

struct AssetPaths {
    std::string tableObj;
    std::string cueObj;
    std::string benchObj;
    std::string wardrobeObj;
};

AssetPaths getDefaultAssetPaths();
std::string getTexturePath(const char* name);
std::string getObjectPath(const char* name);
std::string getAudioPath(const char* name);

}  // namespace billiardgl
```

- [ ] **Step 2: Create `assets.cpp`**

Create `src/Billiards/assets.cpp`:

```cpp
#include "assets.h"

#include "resource_path.h"

namespace billiardgl {

AssetPaths getDefaultAssetPaths()
{
    return AssetPaths{
        objectPath("table.obj"),
        objectPath("cue.obj"),
        objectPath("bench.obj"),
        objectPath("wardrobe.obj")
    };
}

std::string getTexturePath(const char* name)
{
    return texturePath(name);
}

std::string getObjectPath(const char* name)
{
    return objectPath(name);
}

std::string getAudioPath(const char* name)
{
    return audioPath(name);
}

}  // namespace billiardgl
```

- [ ] **Step 3: Update CMake**

Add `src/Billiards/assets.cpp` to `BILLIARDGL_CORE_SOURCES`:

```cmake
set(BILLIARDGL_CORE_SOURCES
    src/Billiards/game_state.cpp
    src/Billiards/physics.cpp
    src/Billiards/rules.cpp
    src/Billiards/input.cpp
    src/Billiards/assets.cpp
    src/Billiards/vec.cpp
)
```

- [ ] **Step 4: Replace direct resource path calls**

In `billiards.cpp`, include:

```cpp
#include "assets.h"
```

Replace direct texture path calls like:

```cpp
loadTexture(billiardgl::texturePath("B1.bmp").c_str())
```

with:

```cpp
loadTexture(billiardgl::getTexturePath("B1.bmp").c_str())
```

Replace object path initialization like:

```cpp
ObjLoader tableObj(billiardgl::objectPath("table.obj"));
```

with:

```cpp
ObjLoader tableObj(billiardgl::getObjectPath("table.obj"));
```

- [ ] **Step 5: Build and smoke test**

Run:

```bash
cmake --build build && ./build/BilliardsPhysicsTests && ./build/BilliardsRulesTests
```

Expected: build and tests pass.

Run:

```bash
./build/Billiards
```

Expected: all textures and OBJ models still load.

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt src/Billiards/assets.h src/Billiards/assets.cpp src/Billiards/billiards.cpp
git commit -m "centralize asset path entry points"
```

## Task 7: Extract Renderer Shell

**Files:**
- Create: `src/Billiards/renderer.h`
- Create: `src/Billiards/renderer.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/Billiards/billiards.cpp`

- [ ] **Step 1: Create `renderer.h`**

Create `src/Billiards/renderer.h`:

```cpp
#pragma once

#include "game_state.h"

namespace billiardgl {

struct RenderHooks {
    void (*renderRoom)();
    void (*renderTable)();
    void (*renderBall)();
    void (*renderCue)();
    void (*renderDecoration)();
};

void renderScene(const GameState& state, const RenderHooks& hooks);

}  // namespace billiardgl
```

- [ ] **Step 2: Create `renderer.cpp`**

Create `src/Billiards/renderer.cpp`:

```cpp
#define GLEW_STATIC
#include <GL/glew.h>
#include <GL/freeglut.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include "renderer.h"

namespace billiardgl {

void renderScene(const GameState& state, const RenderHooks& hooks)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(
        state.camera.eye[0], state.camera.eye[1], state.camera.eye[2],
        state.camera.target[0], state.camera.target[1], state.camera.target[2],
        0.0, 1.0, 0.0);

    if (hooks.renderRoom) {
        hooks.renderRoom();
    }
    if (hooks.renderTable) {
        hooks.renderTable();
    }
    if (hooks.renderBall) {
        hooks.renderBall();
    }
    if (hooks.renderCue) {
        hooks.renderCue();
    }
    if (hooks.renderDecoration) {
        hooks.renderDecoration();
    }
}

}  // namespace billiardgl
```

- [ ] **Step 3: Update CMake**

Add `src/Billiards/renderer.cpp` to the `Billiards` executable source list, not to `BILLIARDGL_CORE_SOURCES`:

```cmake
add_executable(Billiards
    src/Billiards/billiards.cpp
    src/Billiards/hud.cpp
    src/Billiards/renderer.cpp
    src/Billiards/ObjLoader.cpp
    src/Billiards/particle.cpp
    src/Billiards/resource_path.cpp
    src/Billiards/platform_audio.cpp
    ${BILLIARDGL_CORE_SOURCES}
)
```

- [ ] **Step 4: Route display through renderer shell**

In `billiards.cpp`, include:

```cpp
#include "renderer.h"
```

In `myDisplay`, prepare state before rendering:

```cpp
Game.camera.eye[0] = at[0];
Game.camera.eye[1] = at[1];
Game.camera.eye[2] = at[2];
Game.camera.target[0] = at[3];
Game.camera.target[1] = at[4];
Game.camera.target[2] = at[5];
```

Replace direct scene body calls with:

```cpp
const billiardgl::RenderHooks hooks{
    renderRoom,
    renderTable,
    renderBall,
    renderCue,
    renderDecoration
};
billiardgl::renderScene(Game, hooks);
```

Keep post-scene HUD and buffer swap in `myDisplay` during this task.

- [ ] **Step 5: Build and visual test**

Run:

```bash
cmake --build build && ./build/BilliardsPhysicsTests && ./build/BilliardsRulesTests
```

Expected: build and tests pass.

Run:

```bash
./build/Billiards
```

Expected:

- Table, room, balls, cue, and decoration still render.
- Ball readability does not regress.
- HUD remains visible.

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt src/Billiards/renderer.h src/Billiards/renderer.cpp src/Billiards/billiards.cpp
git commit -m "route scene rendering through renderer shell"
```

## Task 8: Thin `billiards.cpp` by Moving Ball State Synchronization

**Files:**
- Modify: `src/Billiards/game_state.h`
- Modify: `src/Billiards/game_state.cpp`
- Modify: `src/Billiards/billiards.cpp`
- Modify: `tests/physics_tests.cpp`

- [ ] **Step 1: Add conversion helpers to `game_state.h`**

Add:

```cpp
void setBallVelocity(BallState& ball, float x, float y, float z);
bool anyBallMoving(const GameState& state);
```

- [ ] **Step 2: Implement helpers**

Add to `game_state.cpp`:

```cpp
void setBallVelocity(BallState& ball, float x, float y, float z)
{
    ball.velocity.x = x;
    ball.velocity.y = y;
    ball.velocity.z = z;
}

bool anyBallMoving(const GameState& state)
{
    for (const BallState& ball : state.balls) {
        if (!ball.pocketed && ball.speed > 0.0f) {
            return true;
        }
    }
    return false;
}
```

- [ ] **Step 3: Add test coverage**

Append to `tests/physics_tests.cpp` before `return EXIT_SUCCESS;`:

```cpp
state.balls[4].speed = 1.0f;
if (!billiardgl::anyBallMoving(state)) {
    return fail("anyBallMoving should detect active ball speed");
}
state.balls[4].speed = 0.0f;
if (billiardgl::anyBallMoving(state)) {
    return fail("anyBallMoving should be false when all speeds are zero");
}
```

- [ ] **Step 4: Start replacing direct global state writes**

In `billiards.cpp`, when cue ball velocity is assigned for a hit, mirror the write into `Game`:

```cpp
Game.balls[0].velocity.x = Ball[0].v.x;
Game.balls[0].velocity.y = Ball[0].v.y;
Game.balls[0].velocity.z = Ball[0].v.z;
Game.players.shotTaken = Hitted;
```

When `CurrPlayer` changes, mirror it:

```cpp
Game.players.currentPlayer = CurrPlayer;
```

This task intentionally mirrors state rather than deleting all old globals. The renderer still uses the old `Ball` array.

- [ ] **Step 5: Build and run**

Run:

```bash
cmake --build build && ./build/BilliardsPhysicsTests && ./build/BilliardsRulesTests
```

Expected: build and tests pass.

Run:

```bash
./build/Billiards
```

Expected: gameplay behavior is unchanged.

- [ ] **Step 6: Commit**

```bash
git add src/Billiards/game_state.h src/Billiards/game_state.cpp src/Billiards/billiards.cpp tests/physics_tests.cpp
git commit -m "mirror runtime state into game state"
```

## Task 9: Final Verification and Cleanup

**Files:**
- Modify only files already touched if verification reveals compile issues.

- [ ] **Step 1: Check for accidental artifacts**

Run:

```bash
git status --short
```

Expected: no screenshots, build outputs, or temporary files staged.

- [ ] **Step 2: Full build**

Run:

```bash
cmake --build build
```

Expected: build succeeds.

- [ ] **Step 3: Unit tests**

Run:

```bash
./build/BilliardsPhysicsTests && ./build/BilliardsRulesTests
```

Expected: exits with code `0`.

- [ ] **Step 4: Windowed launch verification**

Run:

```bash
./build/Billiards
```

Expected:

- Launches in a window.
- Help hint is visible.
- `H` toggles overlay.
- A ball can be hit.
- Trackpad or mouse camera orbit still works.

- [ ] **Step 5: Fullscreen launch verification**

Run:

```bash
./build/Billiards --fullscreen
```

Expected: launches fullscreen.

- [ ] **Step 6: Screenshot review**

Save screenshots manually or through the available UI tooling into:

```bash
/tmp/billiardgl-architecture-evolution/
```

Expected scenes:

- default launch view
- after striking one ball
- Help overlay open
- HUD current player area

Do not add these screenshots to git.

- [ ] **Step 7: Inspect main file size and responsibilities**

Run:

```bash
wc -l src/Billiards/billiards.cpp
```

Expected: line count is lower than before the refactor, or the remaining content is clearly marked as rendering migration residue handled by the renderer shell.

- [ ] **Step 8: Final status**

Run:

```bash
git status --short --branch
```

Expected: branch contains only intentional source, CMake, test, and documentation changes.

- [ ] **Step 9: Commit cleanup if needed**

If verification fixes were made:

```bash
git add CMakeLists.txt src/Billiards tests
git commit -m "verify architecture evolution"
```

If no fixes were made, do not create an empty commit.

## Self-Review

Spec coverage:

- `GameState` is covered by Task 1 and Task 8.
- `Physics` and `Rules` extraction are covered by Task 2 and Task 3.
- `Input`, `HUD`, `Renderer`, and `Assets` boundaries are covered by Tasks 4 through 7.
- macOS windowed/fullscreen, Help, HUD, camera, ball hit, tests, and screenshots are covered by Task 9.
- Non-goals are respected: no modern OpenGL migration, no GLUT replacement, no ECS, no gameplay rewrite, no asset redesign.

Placeholder scan:

- The plan contains no unresolved marker words or placeholder sections.
- Each code creation step includes concrete code.
- Each verification step includes exact commands and expected outcomes.

Type consistency:

- `GameState`, `BallState`, `Point3`, and helper function names are consistent across tasks.
- `physics`, `rules`, and `input` depend only on `game_state` and standard C++.
- OpenGL-dependent modules are excluded from `BILLIARDGL_CORE_SOURCES` and kept in the `Billiards` executable only.
