#pragma once

#include "game_state.h"

namespace billiardgl {

// An actual, ordered solver mutation, not an estimated correction from an
// impulse or penetration depth. A two-ball projection has two adjacent records.
struct SolverPositionProjection {
    int ordinal = -1;
    int iteration = -1;
    int contactIndex = -1;
    int ballIndex = -1;
    std::uint64_t trajectoryGeneration = 0;
    Point3 beforeCm;
    Point3 afterCm;
};

} // namespace billiardgl
