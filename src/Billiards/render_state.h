#pragma once

#include "game_state.h"

namespace billiardgl {

// Builds a presentation-only state between two completed physics ticks.
// Discrete gameplay state always comes from current; only continuous visual
// quantities are interpolated.
GameState interpolateRenderState(const GameState& previous,
    const GameState& current, double alpha);

}  // namespace billiardgl
