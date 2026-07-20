#pragma once

#include "full_game_case_registry.h"

namespace fullgame {

FullGameCaseResult runSeededBreak(
    billiardgl::GameRuntime&, std::uint32_t, const FullGameRunOptions&);
FullGameCaseResult runContinuousScoring(
    billiardgl::GameRuntime&, std::uint32_t, const FullGameRunOptions&);
FullGameCaseResult runCueBallScratch(
    billiardgl::GameRuntime&, std::uint32_t, const FullGameRunOptions&);
FullGameCaseResult runRandomizedLegalSequence(
    billiardgl::GameRuntime&, std::uint32_t, const FullGameRunOptions&);

}  // namespace fullgame
