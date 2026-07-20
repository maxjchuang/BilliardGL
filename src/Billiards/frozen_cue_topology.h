#pragma once

#include "contact_island.h"
#include "game_state.h"
#include "physics_boundary_mode.h"
#include "physics_profile.h"

#include <string>

namespace billiardgl {

enum class FrozenCueTopologyStatus {
    Valid,
    IslandLimit,
    ContradictoryTopology
};

struct FrozenCueTopology {
    FrozenCueTopologyStatus status = FrozenCueTopologyStatus::Valid;
    bool frozen = false;
    ContactIsland island;
    std::string error;
};

FrozenCueTopology detectFrozenCueTopology(
    const GameState& state, int cueBallIndex, const PhysicsProfile& profile,
    PhysicsBoundaryMode boundaryMode);

}  // namespace billiardgl
