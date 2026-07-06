# BilliardGL Authoritative State and Glue Main Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Finish the PR #6 architecture cleanup by making `GameState` the only authoritative runtime state source and reducing `src/Billiards/billiards.cpp` to GLUT/process glue.

**Architecture:** Move input, camera, shot, HUD, config, player, and ball state reads/writes behind `GameState`. Move texture upload, OBJ ownership, VBO creation, camera setup, lighting setup, and HUD drawing out of `billiards.cpp` into focused modules that already exist in the codebase.

**Tech Stack:** C++11, CMake, CTest, GLUT, GLEW, legacy OpenGL fixed pipeline, SDL2 audio, existing `GameState`, `input`, `renderer`, `render_resources`, `hud`, `assets`, and `image_loader` modules.

---

## File Responsibility Map

- `src/Billiards/game_state.h`: authoritative data model for balls, camera, players, input, HUD, config, and events.
- `src/Billiards/input.h` and `src/Billiards/input.cpp`: pure state transitions for keyboard, mouse, trackpad orbit, shot charge, and help-mode blocking.
- `src/Billiards/renderer.h` and `src/Billiards/renderer.cpp`: render orchestration, camera projection/view setup, lighting setup, and scene drawing.
- `src/Billiards/render_resources.h` and `src/Billiards/render_resources.cpp`: OpenGL resource initialization, texture upload, OBJ ownership, VBO creation, and resource teardown.
- `src/Billiards/hud.h` and `src/Billiards/hud.cpp`: all 2D HUD and Help drawing.
- `src/Billiards/billiards.cpp`: process startup, command-line parsing, GLUT window creation, audio startup, callback registration, and thin callback wrappers.
- `tests/input_tests.cpp`: pure input and camera transition tests.
- `tests/render_resources_tests.cpp`: lightweight resource-state tests that do not require an OpenGL context.
- `CMakeLists.txt`: test target wiring and CTest registration.

## Acceptance Checks

Run these after every task that touches code:

```bash
cmake -S . -B /tmp/billiardgl-authoritative-state-build
cmake --build /tmp/billiardgl-authoritative-state-build
ctest --test-dir /tmp/billiardgl-authoritative-state-build --output-on-failure
```

Run these before every commit that claims an architecture boundary is complete:

```bash
rg -n "at\\[|leftm|rightm|TrackpadOrbit|\\bspeed\\b|CurrPlayer|NextPlayer|IsIllegal|ShowHelp|WindowedMode" src/Billiards/billiards.cpp
rg -n "initTable|initCue|initDecoration|initLight|loadTexture|ObjLoader|glGenBuffers|glTex|glLight|drawHelp|drawString" src/Billiards/billiards.cpp
wc -l src/Billiards/billiards.cpp
```

Expected final result:

- The first `rg` command returns no lines from `src/Billiards/billiards.cpp`.
- The second `rg` command returns no lines from `src/Billiards/billiards.cpp`.
- `billiards.cpp` is primarily lifecycle glue. A line count below 500 is preferred; ownership is the hard requirement.
- CTest includes and passes the three screenshot entries: `BilliardsScreenshotDefault`, `BilliardsScreenshotHelp`, and `BilliardsScreenshotAfterShot`.

---

### Task 1: Lock Down Input and Camera State with Tests

**Files:**
- Create: `tests/input_tests.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/Billiards/input.h`
- Modify: `src/Billiards/input.cpp`
- Modify: `src/Billiards/game_state.h`

- [ ] **Step 1: Add input tests before refactoring callbacks**

Create `tests/input_tests.cpp` with these tests:

```cpp
#include "game_state.h"
#include "input.h"

#include <cassert>
#include <cmath>

namespace {

bool closeEnough(float a, float b)
{
    return std::fabs(a - b) < 0.0001f;
}

void testHelpKeyTogglesAndClearsShotState()
{
    billiardgl::GameState state;
    state.input.waitingForHit = true;
    state.input.hitRequested = true;

    billiardgl::handleHelpKey(state);

    assert(state.hud.showHelp);
    assert(!state.input.waitingForHit);
    assert(!state.input.hitRequested);
}

void testSpecialKeysOrbitCamera()
{
    billiardgl::GameState state;
    const float startX = state.camera.angleX;
    const float startY = state.camera.angleY;

    billiardgl::handleSpecialKey(state, 1, 2, 3, 4, 2);
    billiardgl::handleSpecialKey(state, 1, 2, 3, 4, 4);

    assert(state.camera.angleX > startX);
    assert(state.camera.angleY > startY);
}

void testRightDragOrbitsCamera()
{
    billiardgl::GameState state;
    const float startX = state.camera.angleX;
    const float startY = state.camera.angleY;

    billiardgl::handleMouseButton(state, billiardgl::MouseButton::Right, billiardgl::ButtonState::Down, 100, 100);
    billiardgl::handleMouseMove(state, 120, 130);

    assert(closeEnough(state.camera.angleX, startX + 0.2f));
    assert(closeEnough(state.camera.angleY, startY + 0.3f));
    assert(state.input.rightMouseDown);
}

void testTrackpadOrbitUsesLeftDragWithoutChargingShot()
{
    billiardgl::GameState state;

    billiardgl::beginTrackpadOrbit(state, 10, 10);
    billiardgl::handleMouseMove(state, 20, 20);

    assert(state.input.trackpadOrbit);
    assert(!state.input.waitingForHit);
    assert(!state.input.hitRequested);
    assert(state.camera.angleX > 0.0f);
    assert(state.camera.angleY > 0.0f);

    billiardgl::endTrackpadOrbit(state);

    assert(!state.input.trackpadOrbit);
    assert(!state.input.leftMouseDown);
}

void testLeftMouseChargesAndReleaseRequestsHit()
{
    billiardgl::GameState state;

    billiardgl::handleMouseButton(state, billiardgl::MouseButton::Left, billiardgl::ButtonState::Down, 50, 50);
    assert(state.input.leftMouseDown);
    assert(state.input.waitingForHit);
    assert(!state.input.hitRequested);

    billiardgl::handleMouseButton(state, billiardgl::MouseButton::Left, billiardgl::ButtonState::Up, 50, 50);
    assert(!state.input.leftMouseDown);
    assert(!state.input.waitingForHit);
    assert(state.input.hitRequested);
}

void testHelpBlocksMouseShotInput()
{
    billiardgl::GameState state;
    state.hud.showHelp = true;

    billiardgl::handleMouseButton(state, billiardgl::MouseButton::Left, billiardgl::ButtonState::Down, 50, 50);
    billiardgl::handleMouseButton(state, billiardgl::MouseButton::Left, billiardgl::ButtonState::Up, 50, 50);

    assert(!state.input.leftMouseDown);
    assert(!state.input.waitingForHit);
    assert(!state.input.hitRequested);
}

}

int main()
{
    testHelpKeyTogglesAndClearsShotState();
    testSpecialKeysOrbitCamera();
    testRightDragOrbitsCamera();
    testTrackpadOrbitUsesLeftDragWithoutChargingShot();
    testLeftMouseChargesAndReleaseRequestsHit();
    testHelpBlocksMouseShotInput();
    return 0;
}
```

- [ ] **Step 2: Register the input test target**

Modify `CMakeLists.txt`:

```cmake
add_executable(BilliardsInputTests
    tests/input_tests.cpp
    ${BILLIARDGL_CORE_SOURCES}
)

target_include_directories(BilliardsInputTests PRIVATE
    src/Billiards
)
```

Replace the current test-target loop header with this complete list so `BilliardsInputTests` receives `BILLIARDGL_ASSET_ROOT`:

```cmake
foreach(test_target
    BilliardsPhysicsTests
    BilliardsRulesTests
    BilliardsScreenshotTests
    BilliardsGameStateTests
    BilliardsAssetsTests
    BilliardsInputTests
)
```

Then register it:

```cmake
add_test(NAME BilliardsInputTests COMMAND BilliardsInputTests)
```

- [ ] **Step 3: Run the new test and confirm the missing API failures**

Run:

```bash
cmake -S . -B /tmp/billiardgl-authoritative-state-build
cmake --build /tmp/billiardgl-authoritative-state-build
```

Expected: the build fails because `beginTrackpadOrbit` and `endTrackpadOrbit` are not declared.

- [ ] **Step 4: Add explicit trackpad orbit helpers**

Modify `src/Billiards/input.h` with these declarations:

```cpp
void beginTrackpadOrbit(GameState& state, int x, int y);
void endTrackpadOrbit(GameState& state);
```

Modify `src/Billiards/input.cpp` so `handleMouseButton` ignores shot input while Help is open and so trackpad orbit has explicit start/end functions:

```cpp
void beginTrackpadOrbit(GameState& state, int x, int y)
{
    state.input.mouseX = x;
    state.input.mouseY = y;
    state.input.leftMouseDown = false;
    state.input.rightMouseDown = false;
    state.input.trackpadOrbit = true;
    state.input.waitingForHit = false;
    state.input.hitRequested = false;
}

void endTrackpadOrbit(GameState& state)
{
    state.input.leftMouseDown = false;
    state.input.rightMouseDown = false;
    state.input.trackpadOrbit = false;
    state.input.waitingForHit = false;
}
```

Replace the beginning of `handleMouseButton` with this guard and state update:

```cpp
state.input.mouseX = x;
state.input.mouseY = y;

if (state.hud.showHelp) {
    state.input.leftMouseDown = false;
    state.input.rightMouseDown = false;
    state.input.trackpadOrbit = false;
    state.input.waitingForHit = false;
    state.input.hitRequested = false;
    return;
}

const bool isDown = buttonState == ButtonState::Down;
```

- [ ] **Step 5: Run input tests**

Run:

```bash
cmake --build /tmp/billiardgl-authoritative-state-build
/tmp/billiardgl-authoritative-state-build/BilliardsInputTests
```

Expected: `BilliardsInputTests` exits with status `0`.

- [ ] **Step 6: Commit input test harness**

Run:

```bash
git add CMakeLists.txt tests/input_tests.cpp src/Billiards/input.h src/Billiards/input.cpp
git commit -m "Add authoritative input state tests"
```

---

### Task 2: Move Camera, Shot, and Player Runtime State into `GameState`

**Files:**
- Modify: `src/Billiards/game_state.h`
- Modify: `src/Billiards/input.h`
- Modify: `src/Billiards/input.cpp`
- Modify: `src/Billiards/billiards.cpp`
- Test: `tests/input_tests.cpp`

- [ ] **Step 1: Add test coverage for shot power state**

Append this test to `tests/input_tests.cpp` and call it from `main()`:

```cpp
void testShotPowerChargesThroughInputState()
{
    billiardgl::GameState state;
    state.input.waitingForHit = true;
    state.input.shotPower = 198.0f;

    billiardgl::chargeShotPower(state, 200.0f, 2.0f);
    assert(closeEnough(state.input.shotPower, 200.0f));

    billiardgl::chargeShotPower(state, 200.0f, 2.0f);
    assert(closeEnough(state.input.shotPower, 0.0f));

    state.input.waitingForHit = false;
    billiardgl::chargeShotPower(state, 200.0f, 2.0f);
    assert(closeEnough(state.input.shotPower, 0.0f));
}
```

- [ ] **Step 2: Add shot power helper declaration**

Modify `src/Billiards/input.h`:

```cpp
void chargeShotPower(GameState& state, float maxPower, float increment);
```

- [ ] **Step 3: Implement shot power helper**

Modify `src/Billiards/input.cpp`:

```cpp
void chargeShotPower(GameState& state, float maxPower, float increment)
{
    if (!state.input.waitingForHit) {
        return;
    }

    if (state.input.shotPower >= maxPower) {
        state.input.shotPower = 0.0f;
    } else {
        state.input.shotPower += increment;
        if (state.input.shotPower > maxPower) {
            state.input.shotPower = maxPower;
        }
    }
}
```

- [ ] **Step 4: Replace callback aliases and legacy state in `billiards.cpp`**

Because `src/Billiards/billiards.cpp` contains non-UTF-8 legacy comments, byte-safe editing may be required for this file. Preserve behavior while removing these declarations:

```cpp
int leftm = 0, rightm = 0;
bool TrackpadOrbit = false;
bool& ShowHelp = Game.hud.showHelp;
GLfloat rx, ry, rz, speed = 0;
static GLfloat kx = 0, ky = 0, kz = 0;
static GLfloat at[6] = { 0, 200, -TABLE_IN_LENGTH / 4, 0, TABLE_HEIGHT + Radius, -TABLE_IN_LENGTH / 4 };
int& CurrPlayer = Game.players.currentPlayer;
int& NextPlayer = Game.players.nextPlayer;
bool& IsIllegal = Game.players.illegalShot;
bool& WindowedMode = Game.config.windowedMode;
```

Use `Game.camera`, `Game.input`, `Game.players`, `Game.hud`, and `Game.config` directly at every call site.

- [ ] **Step 5: Replace camera update helper with a `GameState` mutator**

Create or update a helper in `billiards.cpp` with this shape:

```cpp
void updateCameraEyeFromState()
{
    Game.camera.eye[0] = Game.camera.zoom * std::cos(Game.camera.angleX) * std::sin(Game.camera.angleY) + Game.camera.target[0];
    Game.camera.eye[1] = Game.camera.zoom * std::cos(Game.camera.angleY) + Game.camera.target[1];
    Game.camera.eye[2] = Game.camera.zoom * std::sin(Game.camera.angleX) * std::sin(Game.camera.angleY) + Game.camera.target[2];
}

void followCueBall()
{
    Game.camera.target[0] = Game.balls[0].position.x;
    Game.camera.target[1] = Game.balls[0].position.y;
    Game.camera.target[2] = Game.balls[0].position.z;
    updateCameraEyeFromState();
}
```

Then replace `at[0..5]` reads and writes with `Game.camera.eye` and `Game.camera.target`.

- [ ] **Step 6: Replace shot application with input state**

In `myIdle`, replace legacy `speed`, `WaitHit`, and `Hit` behavior with this logic:

```cpp
if (Game.input.waitingForHit) {
    billiardgl::chargeShotPower(Game, 200.0f, 2.0f);
}

if (Game.input.hitRequested) {
    const float dx = Game.camera.target[0] - Game.camera.eye[0];
    const float dz = Game.camera.target[2] - Game.camera.eye[2];
    const float length = std::sqrt(dx * dx + dz * dz);
    if (length > 0.0001f) {
        billiardgl::setBallVelocity(Game.balls[0], Game.input.shotPower * dx / length, 0.0f, Game.input.shotPower * dz / length);
        Game.players.shotTaken = true;
    }
    Game.input.shotPower = 0.0f;
    Game.input.hitRequested = false;
}
```

- [ ] **Step 7: Replace player aliases with direct state writes**

Replace every `CurrPlayer`, `NextPlayer`, `IsIllegal`, `updated`, and `Hitted` read/write in `billiards.cpp` with:

```cpp
Game.players.currentPlayer
Game.players.nextPlayer
Game.players.illegalShot
Game.players.updatedAfterShot
Game.players.shotTaken
```

Keep the existing rule behavior unchanged.

- [ ] **Step 8: Replace Help, window mode, and mouse globals**

Replace every `ShowHelp`, `WindowedMode`, `leftm`, `rightm`, and `TrackpadOrbit` read/write in `billiards.cpp` with:

```cpp
Game.hud.showHelp
Game.config.windowedMode
Game.input.leftMouseDown
Game.input.rightMouseDown
Game.input.trackpadOrbit
```

In `myMouse`, use `beginTrackpadOrbit(Game, x, y)` when GLUT reports Shift plus left-button down for trackpad orbit. Use `endTrackpadOrbit(Game)` on left-button up while `Game.input.trackpadOrbit` is true.

- [ ] **Step 9: Run state ownership scan**

Run:

```bash
rg -n "at\\[|leftm|rightm|TrackpadOrbit|\\bspeed\\b|CurrPlayer|NextPlayer|IsIllegal|ShowHelp|WindowedMode" src/Billiards/billiards.cpp
```

Expected: no output.

- [ ] **Step 10: Run full verification**

Run:

```bash
cmake --build /tmp/billiardgl-authoritative-state-build
ctest --test-dir /tmp/billiardgl-authoritative-state-build --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 11: Commit authoritative runtime state cleanup**

Run:

```bash
git add src/Billiards/game_state.h src/Billiards/input.h src/Billiards/input.cpp src/Billiards/billiards.cpp tests/input_tests.cpp
git commit -m "Make GameState authoritative for runtime input"
```

---

### Task 3: Move Resource Initialization into `render_resources`

**Files:**
- Modify: `src/Billiards/render_resources.h`
- Modify: `src/Billiards/render_resources.cpp`
- Modify: `src/Billiards/renderer.cpp`
- Modify: `src/Billiards/billiards.cpp`
- Modify: `CMakeLists.txt`
- Create: `tests/render_resources_tests.cpp`

- [ ] **Step 1: Add a context-free render resource test**

Create `tests/render_resources_tests.cpp`:

```cpp
#include "game_state.h"
#include "render_resources.h"

#include <cassert>

int main()
{
    billiardgl::GameState state;
    billiardgl::RenderResources resources;

    for (int i = 0; i < billiardgl::kBallCount; ++i) {
        resources.ballTextures[i] = static_cast<unsigned int>(100 + i);
    }

    billiardgl::applyBallTexturesToState(resources, state);

    for (int i = 0; i < billiardgl::kBallCount; ++i) {
        assert(state.balls[i].texture == static_cast<unsigned int>(100 + i));
    }

    return 0;
}
```

- [ ] **Step 2: Register render resource tests**

Modify `CMakeLists.txt`:

```cmake
add_executable(BilliardsRenderResourcesTests
    tests/render_resources_tests.cpp
    src/Billiards/render_resources.cpp
    ${BILLIARDGL_CORE_SOURCES}
)

target_include_directories(BilliardsRenderResourcesTests PRIVATE
    src/Billiards
    src/Billiards/dependencies/include
    ${OPENGL_INCLUDE_DIR}
    ${GLEW_INCLUDE_DIRS}
)
```

Replace the test-target loop header with this complete list so `BilliardsRenderResourcesTests` receives `BILLIARDGL_ASSET_ROOT`:

```cmake
foreach(test_target
    BilliardsPhysicsTests
    BilliardsRulesTests
    BilliardsScreenshotTests
    BilliardsGameStateTests
    BilliardsAssetsTests
    BilliardsInputTests
    BilliardsRenderResourcesTests
)
```

Then register it:

```cmake
add_test(NAME BilliardsRenderResourcesTests COMMAND BilliardsRenderResourcesTests)
```

Link this target with OpenGL and GLEW because `render_resources.cpp` will include OpenGL upload code:

```cmake
target_link_libraries(BilliardsRenderResourcesTests PRIVATE
    OpenGL::GL
    GLEW::GLEW
)
```

- [ ] **Step 3: Change `RenderResources` to own loaded OBJ models**

Modify `src/Billiards/render_resources.h`:

```cpp
#include <memory>

class ObjLoader;

namespace billiardgl {

struct RenderResources {
    std::unique_ptr<ObjLoader> tableObj;
    std::unique_ptr<ObjLoader> cueObj;
    std::unique_ptr<ObjLoader> benchObj;
    std::unique_ptr<ObjLoader> wardObj;

    unsigned int tableVertexVBO = 0;
    unsigned int cueVertexVBO = 0;
    unsigned int benchVertexVBO = 0;
    unsigned int wardVertexVBO = 0;
    unsigned int paint1VertexVBO = 0;

    unsigned int tableTextures[2] = {};
    unsigned int cueTextures[2] = {};
    unsigned int ballTextures[kBallCount] = {};
    unsigned int groundTexture = 0;
    unsigned int wallTexture = 0;
    unsigned int wall1Texture = 0;
    unsigned int wall2Texture = 0;
    unsigned int ceilingTexture = 0;
    unsigned int blackTexture = 0;
    unsigned int tableClothTexture = 0;
    unsigned int tableTexture = 0;
    unsigned int cueTexture = 0;
    unsigned int greenTexture = 0;
    unsigned int pocketTexture = 0;
    unsigned int wardrobeTexture = 0;
    unsigned int paint1Texture = 0;
    unsigned int paint2Texture = 0;
    unsigned int flameTexture = 0;

    bool tableFire = false;
    bool benchFire = false;
    bool wardFire = false;
    bool paint1Fire = false;

    float shotPower = 0.0f;
    bool showCue = true;
    bool showPowerMeter = false;
    int viewportWidth = 800;
    int viewportHeight = 600;
};

bool initializeRenderResources(RenderResources& resources, GameState& state);
void destroyRenderResources(RenderResources& resources);
unsigned int uploadTexture(const std::string& path);
void applyBallTexturesToState(const RenderResources& resources, GameState& state);

}  // namespace billiardgl
```

Preserve any fields already used by `renderer.cpp`; rename references consistently rather than dropping behavior.

- [ ] **Step 4: Move texture upload into `render_resources.cpp`**

Move the body of `loadTexture` from `billiards.cpp` into `src/Billiards/render_resources.cpp` as:

```cpp
unsigned int uploadTexture(const std::string& path)
{
    billiardgl::Image image;
    std::string error;
    if (!billiardgl::loadImage(path, image, error)) {
        std::cerr << "Failed to load texture '" << path << "': " << error << std::endl;
        return 0;
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    gluBuild2DMipmaps(GL_TEXTURE_2D, image.channels, image.width, image.height, image.format, GL_UNSIGNED_BYTE, image.pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    return texture;
}
```

Add required includes:

```cpp
#include "assets.h"
#include "image_loader.h"
#include "ObjLoader.h"

#include <GL/glew.h>
#include <GL/glu.h>

#include <iostream>
#include <string>
```

- [ ] **Step 5: Move OBJ loading and VBO creation into `initializeRenderResources`**

Move the logic currently inside `initTable`, `initCue`, and `initDecoration` from `billiards.cpp` into `render_resources.cpp`. Keep the same vertex buffer packing and texture path choices. The initializer should:

```cpp
bool initializeRenderResources(RenderResources& resources, GameState& state)
{
    resources.tableObj.reset(new ObjLoader(billiardgl::getObjectPath("table.obj")));
    resources.cueObj.reset(new ObjLoader(billiardgl::getObjectPath("cue.obj")));
    resources.benchObj.reset(new ObjLoader(billiardgl::getObjectPath("bench.obj")));
    resources.wardObj.reset(new ObjLoader(billiardgl::getObjectPath("wardrobe.obj")));

    resources.tableTextures[0] = uploadTexture(billiardgl::getTexturePath("green.bmp"));
    resources.tableTextures[1] = uploadTexture(billiardgl::getTexturePath("wood.bmp"));
    resources.cueTextures[0] = uploadTexture(billiardgl::getTexturePath("wood.bmp"));
    resources.cueTextures[1] = uploadTexture(billiardgl::getTexturePath("black.bmp"));

    const char* ballTextureNames[billiardgl::kBallCount] = {
        "B16.bmp", "B1.bmp", "B2.bmp", "B3.bmp", "B4.bmp", "B5.bmp", "B6.bmp", "B7.bmp",
        "B8.bmp", "B9.bmp", "B10.bmp", "B11.bmp", "B12.bmp", "B13.bmp", "B14.bmp", "B15.bmp"
    };
    for (int i = 0; i < billiardgl::kBallCount; ++i) {
        resources.ballTextures[i] = uploadTexture(billiardgl::getTexturePath(ballTextureNames[i]));
    }

    resources.groundTexture = uploadTexture(billiardgl::getTexturePath("ground.bmp"));
    resources.wallTexture = uploadTexture(billiardgl::getTexturePath("wall.bmp"));
    resources.wall1Texture = uploadTexture(billiardgl::getTexturePath("wall1.bmp"));
    resources.wall2Texture = uploadTexture(billiardgl::getTexturePath("wall2.bmp"));
    resources.ceilingTexture = uploadTexture(billiardgl::getTexturePath("ceiling.bmp"));
    resources.blackTexture = uploadTexture(billiardgl::getTexturePath("black.bmp"));
    resources.tableClothTexture = uploadTexture(billiardgl::getTexturePath("green.bmp"));
    resources.tableTexture = uploadTexture(billiardgl::getTexturePath("wood.bmp"));
    resources.cueTexture = uploadTexture(billiardgl::getTexturePath("wood.bmp"));
    resources.greenTexture = uploadTexture(billiardgl::getTexturePath("green.bmp"));
    resources.pocketTexture = uploadTexture(billiardgl::getTexturePath("black.bmp"));
    resources.wardrobeTexture = uploadTexture(billiardgl::getTexturePath("5.bmp"));
    resources.paint1Texture = uploadTexture(billiardgl::getTexturePath("6.bmp"));
    resources.paint2Texture = uploadTexture(billiardgl::getTexturePath("7.bmp"));
    resources.flameTexture = uploadTexture(billiardgl::getTexturePath("flame2.bmp"));

    applyBallTexturesToState(resources, state);

    return resources.tableObj && resources.cueObj && resources.benchObj && resources.wardObj;
}
```

After this scaffold is in place, copy the existing VBO upload blocks from `initTable`, `initCue`, and `initDecoration` into helper functions in `render_resources.cpp` and call them from `initializeRenderResources`.

- [ ] **Step 6: Add resource teardown**

Implement:

```cpp
void destroyRenderResources(RenderResources& resources)
{
    GLuint buffers[] = {
        resources.tableVertexVBO,
        resources.cueVertexVBO,
        resources.benchVertexVBO,
        resources.wardVertexVBO,
        resources.paint1VertexVBO
    };
    glDeleteBuffersARB(5, buffers);

    resources.tableObj.reset();
    resources.cueObj.reset();
    resources.benchObj.reset();
    resources.wardObj.reset();
}
```

Call `destroyRenderResources(Render)` from the shutdown path in `billiards.cpp` if the process exits without relying on `exit(0)`.

- [ ] **Step 7: Update renderer for `unique_ptr` fields**

Modify `src/Billiards/renderer.cpp` references from raw pointer checks to `unique_ptr` checks:

```cpp
if (resources.tableObj) {
    ObjLoader& tableObj = *resources.tableObj;
}
```

Keep the existing draw behavior and texture binding order unchanged.

- [ ] **Step 8: Replace resource setup in `billiards.cpp`**

Remove these declarations and definitions from `billiards.cpp`:

```cpp
GLuint loadTexture(const char* file_name);
void initDecoration();
void initTable();
void initCue();
void initLoadTexture();
```

Replace main startup calls:

```cpp
initCue();
initTable();
initDecoration();
initLoadTexture();
```

with:

```cpp
if (!billiardgl::initializeRenderResources(Render, Game)) {
    std::cerr << "Failed to initialize render resources" << std::endl;
    return 1;
}
```

- [ ] **Step 9: Run render resource ownership scan**

Run:

```bash
rg -n "initTable|initCue|initDecoration|loadTexture|ObjLoader|glGenBuffers|glTex" src/Billiards/billiards.cpp
```

Expected: no output.

- [ ] **Step 10: Run full verification**

Run:

```bash
cmake --build /tmp/billiardgl-authoritative-state-build
ctest --test-dir /tmp/billiardgl-authoritative-state-build --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 11: Commit render resource cleanup**

Run:

```bash
git add CMakeLists.txt src/Billiards/render_resources.h src/Billiards/render_resources.cpp src/Billiards/renderer.cpp src/Billiards/billiards.cpp tests/render_resources_tests.cpp
git commit -m "Move render resource initialization out of main"
```

---

### Task 4: Move Camera Setup, Lighting Setup, and Remaining HUD Drawing Out of `billiards.cpp`

**Files:**
- Modify: `src/Billiards/renderer.h`
- Modify: `src/Billiards/renderer.cpp`
- Modify: `src/Billiards/hud.h`
- Modify: `src/Billiards/hud.cpp`
- Modify: `src/Billiards/billiards.cpp`

- [ ] **Step 1: Add renderer setup APIs**

Modify `src/Billiards/renderer.h`:

```cpp
void setupCameraFromGameState(const GameState& state);
void setupLights();
```

- [ ] **Step 2: Move `set_camera` behavior into renderer**

Move the final OpenGL projection/view setup from `set_camera` into `renderer.cpp`:

```cpp
void setupCameraFromGameState(const GameState& state)
{
    glViewport(0, 0, state.config.width, state.config.height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, static_cast<double>(state.config.width) / static_cast<double>(state.config.height), 1.0, 2000.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(
        state.camera.eye[0], state.camera.eye[1], state.camera.eye[2],
        state.camera.target[0], state.camera.target[1], state.camera.target[2],
        0.0, 1.0, 0.0
    );
}
```

Call this function from `myDisplay` before `renderScene(Game, Render)`.

- [ ] **Step 3: Move `initLight` into renderer**

Move `initLight` from `billiards.cpp` into `renderer.cpp` and expose it as:

```cpp
void setupLights()
```

Call `billiardgl::setupLights()` in `main` after the OpenGL context exists and before entering the GLUT main loop.

- [ ] **Step 4: Remove legacy HUD helpers from `billiards.cpp`**

Remove these declarations and definitions from `billiards.cpp`:

```cpp
void drawString();
void drawHelpPrompt();
void drawHelpOverlay();
void drawScreenRect(float left, float bottom, float width, float height, const float color[4]);
```

Use the existing `billiardgl::drawHud(Game)` call from `hud.cpp` for all player text, prompt, and Help overlay drawing.

- [ ] **Step 5: Run glue-only scan**

Run:

```bash
rg -n "initLight|glLight|drawHelp|drawString|drawScreenRect|gluLookAt|gluPerspective" src/Billiards/billiards.cpp
```

Expected: no output.

- [ ] **Step 6: Run full verification**

Run:

```bash
cmake --build /tmp/billiardgl-authoritative-state-build
ctest --test-dir /tmp/billiardgl-authoritative-state-build --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 7: Commit glue-main cleanup**

Run:

```bash
git add src/Billiards/renderer.h src/Billiards/renderer.cpp src/Billiards/hud.h src/Billiards/hud.cpp src/Billiards/billiards.cpp
git commit -m "Move OpenGL setup out of main"
```

---

### Task 5: Final Verification, PR Update, and Review Gate

**Files:**
- Modify only files changed by failed verification or code review.

- [ ] **Step 1: Run a clean build and all CTest entries**

Run:

```bash
cmake -S . -B /tmp/billiardgl-authoritative-state-final-build
cmake --build /tmp/billiardgl-authoritative-state-final-build
ctest --test-dir /tmp/billiardgl-authoritative-state-final-build --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 2: Run final architecture scans**

Run:

```bash
rg -n "at\\[|leftm|rightm|TrackpadOrbit|\\bspeed\\b|CurrPlayer|NextPlayer|IsIllegal|ShowHelp|WindowedMode" src/Billiards/billiards.cpp
rg -n "initTable|initCue|initDecoration|initLight|loadTexture|ObjLoader|glGenBuffers|glTex|glLight|drawHelp|drawString" src/Billiards/billiards.cpp
wc -l src/Billiards/billiards.cpp
```

Expected: both `rg` commands return no output. Record the `wc -l` output in the PR comment.

- [ ] **Step 3: Verify screenshot files are produced by CTest scenes**

Run:

```bash
ls -lh /tmp/billiardgl-ctest-default.ppm /tmp/billiardgl-ctest-help.ppm /tmp/billiardgl-ctest-after-shot.ppm
```

Expected: all three files exist and have non-zero size.

- [ ] **Step 4: Push the branch**

Run:

```bash
git status --short
git push
```

Expected: `git status --short` is empty before push, or it only shows files intentionally fixed after failed verification and already committed before push.

- [ ] **Step 5: Update PR #6 with verification evidence using `gh`**

Run:

```bash
gh pr comment 6 --body "Architecture cleanup verification:
- Clean build: passed
- CTest: passed
- Screenshot CTest scenes: passed
- Runtime-state scan in billiards.cpp: no matches
- Render-resource scan in billiards.cpp: no matches"
```

- [ ] **Step 6: Request final code review before merge**

Use `superpowers:requesting-code-review` to dispatch a dedicated review of PR #6. The review prompt must ask the reviewer to verify:

```text
1. GameState is the authoritative runtime source for camera, input, shot, HUD, config, player, and ball state.
2. billiards.cpp no longer owns texture loading, OBJ ownership, VBO creation, lighting setup, camera setup, or HUD drawing helpers.
3. Existing gameplay behavior remains intact: windowed default, optional fullscreen, Help, trackpad orbit, shooting, screenshot scenes.
4. CTest and source-scan evidence are sufficient.
```

If the reviewer finds blocking issues, fix them in a new commit and repeat Task 5. If the reviewer finds no blocking issues, PR #6 can be merged with `gh pr merge 6 --squash` or the repository's preferred merge strategy.
