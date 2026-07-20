#pragma once

#include "full_game_case_registry.h"

#include <cstdint>
#include <string>
#include <vector>

namespace fullgame {

struct FullGameBudgets {
    double maximumPenetrationCm = 0.5;
    double maximumResidualCmS = 0.001;
    double passiveEnergyToleranceJ = 1e-10;
    double maximumWallSeconds = 30.0;
    std::uint64_t maximumPeakRssBytes = 512ull * 1024ull * 1024ull;
};

struct FullGameInvariantResult {
    bool passed = false;
    std::vector<std::string> failures;
    std::string canonicalState;
    std::string deterministicHash;
    double maximumPenetrationCm = 0.0;
    double maximumResidualCmS = 0.0;
    int duplicateContacts = 0;
    int stepFailures = 0;
};

FullGameInvariantResult evaluateFullGameInvariants(
    const std::vector<billiardgl::PhysicsFrame>& frames,
    const std::vector<billiardgl::RuntimeEvent>& events,
    std::size_t droppedTraceFrames,
    double wallSeconds,
    std::uint64_t peakRssBytes,
    const FullGameBudgets& budgets = FullGameBudgets{});

std::string canonicalFullGameState(
    const std::vector<billiardgl::PhysicsFrame>& frames,
    const std::vector<billiardgl::RuntimeEvent>& events);
std::string sha256Hex(const std::string& bytes);

}  // namespace fullgame
