#include "input.h"

#include <cmath>

namespace billiardgl {

namespace {

float cameraForwardYawOnTable(const GameState& state)
{
    const float dx = state.camera.target[0] - state.camera.eye[0];
    const float dz = state.camera.target[2] - state.camera.eye[2];
    if (std::fabs(dx) < 0.0001f && std::fabs(dz) < 0.0001f) {
        return state.aim.yaw;
    }
    return std::atan2(dz, dx);
}

}  // namespace

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

void handleAimToggleKey(GameState& state)
{
    const bool enteringAimMode = state.aim.mode == AimMode::Observe;
    state.aim.mode = enteringAimMode ? AimMode::Aim : AimMode::Observe;
    if (enteringAimMode) {
        state.aim.yaw = cameraForwardYawOnTable(state);
    }
    state.players.aimingAtCueBall = state.aim.mode == AimMode::Aim;
    state.input.leftMouseDown = false;
    state.input.rightMouseDown = false;
    state.input.trackpadOrbit = false;
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

    if (state.hud.showHelp) {
        state.input.leftMouseDown = false;
        state.input.rightMouseDown = false;
        state.input.trackpadOrbit = false;
        state.input.waitingForHit = false;
        state.input.hitRequested = false;
        return;
    }

    const bool isDown = buttonState == ButtonState::Down;
    if (button == MouseButton::Left) {
        state.input.leftMouseDown = isDown;
        state.input.waitingForHit = false;
        if (state.aim.mode == AimMode::Aim && !isDown) {
            state.input.hitRequested = true;
        }
    } else if (button == MouseButton::Right) {
        state.input.rightMouseDown = false;
    }
}

void handleMouseWheel(GameState& state, int direction, float zoomStep, float powerStep, float maxPower)
{
    if (state.hud.showHelp || direction == 0) {
        return;
    }

    if (state.aim.mode == AimMode::Aim) {
        state.input.shotPower += static_cast<float>(direction) * powerStep;
        if (state.input.shotPower < 0.0f) {
            state.input.shotPower = 0.0f;
        }
        if (state.input.shotPower > maxPower) {
            state.input.shotPower = maxPower;
        }
        return;
    }

    state.camera.zoom -= static_cast<float>(direction) * zoomStep;
    if (state.camera.zoom < 10.0f) {
        state.camera.zoom = 10.0f;
    }
    if (state.camera.zoom > 500.0f) {
        state.camera.zoom = 500.0f;
    }
}

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

    if (state.input.leftMouseDown || state.input.trackpadOrbit) {
        state.camera.angleX += static_cast<float>(dx) * 0.01f;
        state.camera.angleY += static_cast<float>(dy) * 0.01f;
        clampCameraAngles(state);
    }
}

}  // namespace billiardgl
