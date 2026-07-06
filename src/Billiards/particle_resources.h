#pragma once

#include "game_state.h"
#include "render_resources.h"

namespace billiardgl {

void initializeParticleEmitters(RenderResources& resources, const GameState& state);
void destroyParticleEmitters(RenderResources& resources);

}  // namespace billiardgl
