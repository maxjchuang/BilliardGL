#pragma once

#include "full_game_case_registry.h"

namespace fullgame {

FullGameCaseResult runCadenceEquivalence(
    billiardgl::GameRuntime&, std::uint32_t, const FullGameRunOptions&);
FullGameCaseResult runHostLoadEquivalence(
    billiardgl::GameRuntime&, std::uint32_t, const FullGameRunOptions&);

}  // namespace fullgame
