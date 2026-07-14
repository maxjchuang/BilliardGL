#pragma once

#include "full_game_case_registry.h"

namespace fullgame {

FullGameCaseResult runCueCenterHit(
    billiardgl::GameRuntime&, std::uint32_t, const FullGameRunOptions&);
FullGameCaseResult runCueNearMiscue(
    billiardgl::GameRuntime&, std::uint32_t, const FullGameRunOptions&);
FullGameCaseResult runSlidingToRolling(
    billiardgl::GameRuntime&, std::uint32_t, const FullGameRunOptions&);
FullGameCaseResult runObliqueBallCollision(
    billiardgl::GameRuntime&, std::uint32_t, const FullGameRunOptions&);
FullGameCaseResult runRailRebound(
    billiardgl::GameRuntime&, std::uint32_t, const FullGameRunOptions&);
FullGameCaseResult runSidePocketCapture(
    billiardgl::GameRuntime&, std::uint32_t, const FullGameRunOptions&);

}  // namespace fullgame
