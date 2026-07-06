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
    const float startX = state.camera.angleX;
    const float startY = state.camera.angleY;

    billiardgl::beginTrackpadOrbit(state, 10, 10);
    billiardgl::handleMouseMove(state, 20, 20);

    assert(state.input.trackpadOrbit);
    assert(!state.input.waitingForHit);
    assert(!state.input.hitRequested);
    assert(closeEnough(state.camera.angleX, startX + 0.1f));
    assert(closeEnough(state.camera.angleY, startY + 0.1f));

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

void testAimToggleSwitchesModes()
{
    billiardgl::GameState state;
    assert(state.aim.mode == billiardgl::AimMode::Observe);
    assert(!state.players.aimingAtCueBall);

    billiardgl::handleAimToggleKey(state);
    assert(state.aim.mode == billiardgl::AimMode::Aim);
    assert(state.players.aimingAtCueBall);

    billiardgl::handleAimToggleKey(state);
    assert(state.aim.mode == billiardgl::AimMode::Observe);
    assert(!state.players.aimingAtCueBall);
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

}  // namespace

int main()
{
    testHelpKeyTogglesAndClearsShotState();
    testSpecialKeysOrbitCamera();
    testRightDragOrbitsCamera();
    testTrackpadOrbitUsesLeftDragWithoutChargingShot();
    testLeftMouseChargesAndReleaseRequestsHit();
    testHelpBlocksMouseShotInput();
    testShotPowerChargesThroughInputState();
    testAimToggleSwitchesModes();
    testCameraOrbitDoesNotChangeAimInObserveMode();
    testAimModePointerMovementChangesAimNotCamera();
    return 0;
}
