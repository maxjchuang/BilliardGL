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
void beginTrackpadOrbit(GameState& state, int x, int y);
void endTrackpadOrbit(GameState& state);
void clampCameraAngles(GameState& state);

}  // namespace billiardgl
