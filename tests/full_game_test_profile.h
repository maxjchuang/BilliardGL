#pragma once

#include "game_runtime.h"
#include "physics_profile.h"

#include <stdexcept>

namespace fullgame {

inline billiardgl::GameRuntime phase3V5CandidateRuntime()
{
    billiardgl::GameRuntime runtime;
    const billiardgl::ActionResult installed = runtime.replaceStateForScenario(
        runtime.state(), billiardgl::phase3V5CandidatePhysicsProfile());
    if (!installed.ok) {
        throw std::runtime_error("cannot install the preserved Phase 3 v5 candidate");
    }
    return runtime;
}

}  // namespace fullgame
