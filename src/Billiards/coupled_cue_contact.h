#pragma once

#include "cue_contact.h"
#include "frozen_cue_topology.h"

namespace billiardgl {

enum class CoupledCueContactStatus {
    Released,
    InvalidInput,
    NonfiniteState,
    PassiveEnergyGain,
    CompressionLimit,
    ForceLimit,
    ContactIslandLimit,
    PenetrationLimit,
    NoRelease,
    Nonconvergence
};

struct CoupledCueContactResult {
    GameState state;
    CueContactResult contact;
    CoupledCueContactStatus status = CoupledCueContactStatus::InvalidInput;
    std::string error;
};

double huntCrossleyNormalForce(double compressionM,
    double compressionRateMS, double stiffnessNPerM32,
    double dissipationSPerM);

CoupledCueContactResult solveCoupledCueContact(const GameState& state,
    const FrozenCueTopology& topology, const CueImpactInput& input,
    const PhysicsProfile& profile);

}  // namespace billiardgl
