#pragma once

#include "game_runtime.h"

#include <cstdint>
#include <string>
#include <vector>

namespace fullgame {

struct FullGameRunOptions {
    int cadenceTicks = 1;
    bool hostLoad = false;
    int repeats = 3;
    int ticksPerRepeat = 240;
};

struct FullGameCaseResult {
    bool passed = false;
    std::string failure;
    std::uint32_t seed = 0;
    double maximumPenetrationCm = 0.0;
    double maximumResidualCmS = 0.0;
    int duplicateContacts = 0;
    std::string deterministicHash;
};

typedef FullGameCaseResult (*FullGameCaseRun)(
    billiardgl::GameRuntime&, std::uint32_t, const FullGameRunOptions&);

struct FullGameCase {
    std::string id;
    FullGameCaseRun run = nullptr;
};

const std::vector<FullGameCase>& fullGameCases();
std::vector<std::string> fullGameCaseIds();
const FullGameCase* findFullGameCase(const std::string& id);

}  // namespace fullgame
