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
        state.input.waitingForHit = isDown;
        if (!isDown) {
            state.input.hitRequested = true;
        }
    } else if (button == MouseButton::Right) {
        state.input.rightMouseDown = isDown;
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

    if (state.input.rightMouseDown || state.input.trackpadOrbit) {
        state.camera.angleX += static_cast<float>(dx) * 0.01f;
        state.camera.angleY += static_cast<float>(dy) * 0.01f;
        clampCameraAngles(state);
    }
}

}  // namespace billiardgl
