#pragma once

#include "game_state.h"
#include "render_resources.h"

namespace billiardgl {

void setupCameraFromGameState(const GameState& state);
void setupLights();
void renderScene(const GameState& state, RenderResources& resources);

}  // namespace billiardgl
