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

void testObserveModeLeftDragOrbitsCamera()
{
    billiardgl::GameState state;
    const float startX = state.camera.angleX;
    const float startY = state.camera.angleY;

    billiardgl::handleMouseButton(state, billiardgl::MouseButton::Left, billiardgl::ButtonState::Down, 100, 100);
    billiardgl::handleMouseMove(state, 120, 130);

    assert(closeEnough(state.camera.angleX, startX + 0.2f));
    assert(closeEnough(state.camera.angleY, startY + 0.3f));
    assert(!state.input.waitingForHit);
    assert(!state.input.hitRequested);
    assert(state.input.leftMouseDown);

    billiardgl::handleMouseButton(state, billiardgl::MouseButton::Left, billiardgl::ButtonState::Up, 120, 130);
    assert(!state.input.leftMouseDown);
}

void testObserveModeRightDragDoesNotOrbitCamera()
{
    billiardgl::GameState state;
    const float startX = state.camera.angleX;
    const float startY = state.camera.angleY;

    billiardgl::handleMouseButton(state, billiardgl::MouseButton::Right, billiardgl::ButtonState::Down, 100, 100);
    billiardgl::handleMouseMove(state, 120, 130);

    assert(closeEnough(state.camera.angleX, startX));
    assert(closeEnough(state.camera.angleY, startY));
    assert(!state.input.waitingForHit);
    assert(!state.input.hitRequested);
}

void testObserveModeMouseWheelZoomsCamera()
{
    billiardgl::GameState state;
    const float startZoom = state.camera.zoom;

    billiardgl::handleMouseWheel(state, 1, 10.0f, 5.0f, 200.0f);
    assert(closeEnough(state.camera.zoom, startZoom - 10.0f));

    state.camera.zoom = 12.0f;
    billiardgl::handleMouseWheel(state, 1, 10.0f, 5.0f, 200.0f);
    assert(closeEnough(state.camera.zoom, 10.0f));

    state.camera.zoom = 496.0f;
    billiardgl::handleMouseWheel(state, -1, 10.0f, 5.0f, 200.0f);
    assert(closeEnough(state.camera.zoom, 500.0f));
}

void testCameraAnchorToggleAndReturnToCueBall()
{
    billiardgl::GameState state;
    state.balls[0].position = billiardgl::Point3{12.0f, 92.715f, -34.0f};
    state.camera.target[0] = 40.0f;
    state.camera.target[1] = 120.0f;
    state.camera.target[2] = 50.0f;

    assert(state.camera.anchorMode == billiardgl::CameraAnchorMode::FollowCueBall);

    billiardgl::handleCameraAnchorToggleKey(state);
    assert(state.camera.anchorMode == billiardgl::CameraAnchorMode::FreeLook);
    assert(closeEnough(state.camera.target[0], 40.0f));
    assert(closeEnough(state.camera.target[2], 50.0f));

    billiardgl::handleCameraReturnToCueBallKey(state);
    assert(state.camera.anchorMode == billiardgl::CameraAnchorMode::FollowCueBall);
    assert(closeEnough(state.camera.target[0], 12.0f));
    assert(closeEnough(state.camera.target[1], 92.715f));
    assert(closeEnough(state.camera.target[2], -34.0f));
}

void testFreeLookPanMovesCameraTarget()
{
    billiardgl::GameState state;
    state.camera.target[0] = 10.0f;
    state.camera.target[1] = 92.715f;
    state.camera.target[2] = -20.0f;
    state.camera.angleX = 0.0f;

    billiardgl::beginCameraPan(state, 100, 100);
    billiardgl::handleMouseMove(state, 100, 130);

    assert(state.camera.anchorMode == billiardgl::CameraAnchorMode::FreeLook);
    assert(closeEnough(state.camera.target[0], 7.0f));
    assert(closeEnough(state.camera.target[2], -20.0f));
    assert(!state.input.leftMouseDown);
    assert(!state.input.waitingForHit);

    billiardgl::endCameraPan(state);
    assert(!state.input.cameraPan);

    state.camera.target[0] = 10.0f;
    state.camera.target[2] = -20.0f;
    billiardgl::beginCameraPan(state, 100, 100);
    billiardgl::handleMouseMove(state, 130, 100);

    assert(closeEnough(state.camera.target[0], 10.0f));
    assert(closeEnough(state.camera.target[2], -17.0f));

    billiardgl::endCameraPan(state);
}

void testAimModeLeftClickRequestsHitWithoutCameraOrbit()
{
    billiardgl::GameState state;
    billiardgl::handleAimToggleKey(state);
    const float startCameraX = state.camera.angleX;

    billiardgl::handleMouseButton(state, billiardgl::MouseButton::Left, billiardgl::ButtonState::Down, 50, 50);
    billiardgl::handleMouseMove(state, 70, 50);
    billiardgl::handleMouseButton(state, billiardgl::MouseButton::Left, billiardgl::ButtonState::Up, 70, 50);

    assert(!state.input.leftMouseDown);
    assert(!state.input.waitingForHit);
    assert(state.input.hitRequested);
    assert(closeEnough(state.camera.angleX, startCameraX));
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

void testAimModeMouseWheelAdjustsShotPower()
{
    billiardgl::GameState state;
    billiardgl::handleAimToggleKey(state);
    const float startPower = state.input.shotPower;

    billiardgl::handleMouseWheel(state, 1, 10.0f, 5.0f, 200.0f);
    assert(closeEnough(state.input.shotPower, startPower + 5.0f));

    state.input.shotPower = 198.0f;
    billiardgl::handleMouseWheel(state, 1, 10.0f, 5.0f, 200.0f);
    assert(closeEnough(state.input.shotPower, 200.0f));

    state.input.shotPower = 2.0f;
    billiardgl::handleMouseWheel(state, -1, 10.0f, 5.0f, 200.0f);
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

void testEnteringAimModeUsesCameraForwardDirection()
{
    billiardgl::GameState state;
    state.camera.eye[0] = 10.0f;
    state.camera.eye[1] = 200.0f;
    state.camera.eye[2] = -20.0f;
    state.camera.target[0] = -40.0f;
    state.camera.target[1] = 92.715f;
    state.camera.target[2] = -20.0f;

    billiardgl::handleAimToggleKey(state);

    assert(state.aim.mode == billiardgl::AimMode::Aim);
    assert(closeEnough(state.aim.yaw, billiardgl::kPi));

    state.aim.yaw = 1.23f;
    billiardgl::handleAimToggleKey(state);
    assert(state.aim.mode == billiardgl::AimMode::Observe);
    assert(closeEnough(state.aim.yaw, 1.23f));
}

void testCameraOrbitDoesNotChangeAimInObserveMode()
{
    billiardgl::GameState state;
    const float startAim = state.aim.yaw;
    const float startCameraX = state.camera.angleX;

    billiardgl::handleMouseButton(state, billiardgl::MouseButton::Left, billiardgl::ButtonState::Down, 100, 100);
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

    billiardgl::handleMouseButton(state, billiardgl::MouseButton::Left, billiardgl::ButtonState::Down, 100, 100);
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
    testObserveModeLeftDragOrbitsCamera();
    testObserveModeRightDragDoesNotOrbitCamera();
    testObserveModeMouseWheelZoomsCamera();
    testCameraAnchorToggleAndReturnToCueBall();
    testFreeLookPanMovesCameraTarget();
    testAimModeLeftClickRequestsHitWithoutCameraOrbit();
    testHelpBlocksMouseShotInput();
    testShotPowerChargesThroughInputState();
    testAimModeMouseWheelAdjustsShotPower();
    testAimToggleSwitchesModes();
    testEnteringAimModeUsesCameraForwardDirection();
    testCameraOrbitDoesNotChangeAimInObserveMode();
    testAimModePointerMovementChangesAimNotCamera();
    return 0;
}
