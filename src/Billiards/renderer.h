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
